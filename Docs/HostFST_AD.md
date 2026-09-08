# Host FST resource forks: Native vs AppleDouble

Design notes for completing resource-fork (and Finder info / ProDOS type) storage on Linux, and for a portable sidecar mode on every host OS.

User-facing setup remains in [Host FST](HostFST.md). This page is the implementation plan.

## Current behavior

Host FST is Kelvin Sherlock’s GSPlus code (GPL 2.0), adapted in `src/devices/hostfst/`. GS/OS data-fork I/O hits the host file’s default stream. Resource forks and type/auxtype are stored in a **platform-native** side channel when one exists.

| Host OS | Mechanism | Resource-fork I/O | Finder info / type |
|---|---|---|---|
| **macOS** | Native named fork / xattr (`com.apple.ResourceFork`, `com.apple.FinderInfo`; open via `..namedfork/rsrc`) | Full | Full |
| **Windows** | NTFS Alternate Data Streams (`:AFP_Resource`, `:AFP_AfpInfo`) | Full | Full (`AFP_Info` blob) |
| **Linux** | Incomplete xattr sketch | **Stub** — `open_resource_fork` always returns `resForkNotFound` | Size/stat via `getxattr`; write uses a different xattr name than read |

Linux can *see* a resource-fork length if `user.com.apple.ResourceFork` is already present (e.g. from another tool). GS/OS cannot open or write the fork.

`Docs/HostFST.md` documents the Windows ADS convention and does not claim a Linux resource-fork story.

## Windows ADS (Native on NTFS)

NTFS files are a bag of named streams. The unnamed stream is the data fork. Host FST uses two extra streams, the same names as **Services for Macintosh** and **CiderPress**:

| Stream | Role |
|---|---|
| `file:AFP_Resource` | Raw resource-fork bytes |
| `file:AFP_AfpInfo` | 60-byte AFP metadata |

`AFP_AfpInfo` layout (`struct AFP_Info` in `host_common.h`, 60 bytes, pack 2):

- `magic` = `0x00504641` (`"AFP\0"` little-endian)
- `version` = `0x00010000`
- `file_id`, `backup_date`
- `finder_info[32]`
- `prodos_file_type` / `prodos_aux_type`
- `reserved[6]`

On read, magic/version are checked. Type comes from the ProDOS fields if set, otherwise from FinderInfo. If those disagree, ProDOS is the source of truth (`afp_synchronize(..., prefer_prodos)`).

A nonempty `:AFP_Resource` is reported as GS/OS `extendedFile`. Directories never get `:AFP_AfpInfo` (writing that stream on a directory without `FILE_FLAG_BACKUP_SEMANTICS` returns `ERROR_ACCESS_DENIED`; Finder shows “physically damaged”). If there is no `AFP_AfpInfo`, type/auxtype is guessed from the filename extension (`host_synthesize_file_xinfo`).

**NTFS only.** FAT, exFAT, and many network shares drop named streams. Copy to a USB stick or zip the folder and the resource fork plus type/auxtype vanish; the data fork still looks fine.

Windows is **not** xattr. Calling the Native mode “xattr” is wrong.

## GSPlus comparison

Sources were copied from Kelvin’s fork ([ksherlock/gsplus](https://github.com/ksherlock/gsplus/tree/master/src)) into `testdir/hostfstorigin/` for a side-by-side. The current [digarok/gsplus](https://github.com/digarok/gsplus) tree is a later KEGS rewrite and no longer has these files.

`unix_host_common.c` matches except `defc.h` → `defc_shim.h`. Linux `open_resource_fork` in origin is the same stub:

```c
#elif defined __linux__
static int open_resource_fork(const char *path, word16 *access, word16 *error) {
  *error = resForkNotFound;
  return -1;
}
```

There is **no** missing Linux implementation to port. Completing Linux is new work.

`fst.h` / `gsos.h` match. Unix diffs in our tree are later GSSquared fixes (remount, `fd_head` clear, 31-character name limit, skip long / `:` names, `HOST_FST_LOG`). Windows diffs are 64-bit robustness (error mapping, share modes, no ADS on directories).

The one origin file we never ported is **`host_mli.c`** (ProDOS 8 `/HOST` via ATINIT). That path says `/* ignore resource fork */`. P8 has no GS/OS resource-fork model; it is not relevant to this work.

## Why Linux xattr cannot be Native

Linux `XATTR_SIZE_MAX` is 65536. ext4 is often one filesystem block (~4KB) unless large xattrs are enabled. IIgs resource forks (fonts, `rSound`, `rPict`, etc.) routinely exceed that.

The existing Linux xattr names are also inconsistent: read `user.com.apple.ResourceFork` / `user.com.apple.FinderInfo`, write `user.apple.FinderInfo`. Do not finish that path as the Linux store.

AppleDouble (or another sidecar) is the size-safe choice.

## Proposed toggle: Native vs AppleDouble

| Mode | What it does |
|---|---|
| **Native** | macOS named forks / xattr; Windows NTFS ADS. Unavailable on Linux (and on FAT/exFAT / many shares). |
| **AppleDouble** | `._filename` sidecar. Same bytes on Mac, Windows, and Linux. |

Defaults:

- Mac / Windows: **Native** (current behavior; CiderPress / Finder interop)
- Linux: **AppleDouble** (only working store)

Forcing AppleDouble on Mac/Windows is still useful for a folder shared via git/Dropbox/USB, or a host folder that is not NTFS.

Suggested setting, same pattern as `g_cfg_host_crlf` / `g_cfg_host_merlin`:

```
g_cfg_host_forks = native | appledouble
```

Persist next to Host Folder. Changing it should remount `:Host`, same as changing the folder.

### Toggle rules

- **Native on Linux:** refuse or ignore; AppleDouble is the only working store.
- **One store at a time.** AppleDouble mode must not also write xattr/ADS (split-brain).
- Directory listings already skip names starting with `.`, so `._*` stay hidden from GS/OS.

## AppleDouble as portable C++ file I/O

AppleDouble v2 (`._` files as written by macOS, Netatalk, and CiderPress) is a regular file: 26-byte header, entry list, then FinderInfo (32 bytes) and the resource-fork payload. No xattr, no ADS, no `attropen`. `std::filesystem` plus a file stream (or POSIX `open` / `pread` / `pwrite`) is enough and can be one module used on every platform.

FinderInfo (32 bytes) already maps to ProDOS type/auxtype via `host_finder_info_to_filetype`. The 60-byte `AFP_Info` blob is Windows-Native only.

Create / rename / destroy must move the sidecar with the data file.

### The fd-model wrinkle

`open_resource_fork` today returns a Unix `fd` or a Win32 `HANDLE`. `read` / `write` / `lseek` / `SetEndOfFile` treat byte 0 as the start of the fork.

In AppleDouble the fork lives at an **offset inside** `._file`. `lseek(fd, 0, SEEK_END)` on the sidecar would include the header. `ftruncate` / `SetEndOfFile` would smash the header. SetEOF on the resource fork must update the entry-list length, not truncate the whole file.

Do **not** hand the C layer a raw fd onto `._file`. Put a small C++ handle behind the existing `safe_read` / `safe_write` / mark / EOF path:

- `host_ad_open_rsrc` / `read` / `write` / `seek` / `seteof` / `geteof`
- `host_ad_get/set_finderinfo`
- `host_ad_resource_eof` for `GetFileInfo`

Keep a **canonical layout**: header + 2 entries + 32-byte FinderInfo + resource bytes at the end. Growing the fork is append + patch the length field. That matches typical macOS `._` files.

Native stays the existing `#ifdef` Mac/Win code. AppleDouble is the new cross-platform piece and the Linux default.

## Related

- [Host FST](HostFST.md) — user setup
- `src/devices/hostfst/` — current implementation
- `testdir/hostfstorigin/` — GSPlus sources used for the comparison (`SOURCE.txt`)
