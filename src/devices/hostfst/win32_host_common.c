
#define _WIN32_WINNT 0x0600 // vista+
#include <Windows.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "defc_shim.h"
#include "gsos.h"

#include "host_common.h"

#ifdef _MSC_VER
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define strdup _strdup
#endif



void afp_init(struct AFP_Info *info, word16 file_type, word32 aux_type) {
  //static_assert(sizeof(AFP_Info) == 60, "Incorrect AFP_Info size");
  memset(info, 0, sizeof(*info));
  info->magic = 0x00504641;
  info->version = 0x00010000;
  info->prodos_file_type = file_type;
  info->prodos_aux_type = aux_type;
  if (file_type || aux_type)
    host_file_type_to_finder_info(info->finder_info, file_type, aux_type);
}

BOOL afp_verify(struct AFP_Info *info) {
  if (!info) return 0;

  if (info->magic != 0x00504641) return 0;
  if (info->version != 0x00010000) return 0;

  return 1;
}


int afp_to_filetype(struct AFP_Info *info, word16 *file_type, word32 *aux_type) {
  // check for prodos ftype/auxtype...
  if (info->prodos_file_type || info->prodos_aux_type) {
    *file_type = info->prodos_file_type;
    *aux_type = info->prodos_aux_type;
    return 0;
  }
  int ok = host_finder_info_to_filetype(info->finder_info, file_type, aux_type);
  if (ok == 0) {
    info->prodos_file_type = *file_type;
    info->prodos_aux_type = *aux_type;
  }
  return 0;
}

void afp_synchronize(struct AFP_Info *info, int preference) {
  // if ftype/auxtype is inconsistent between prodos and finder info, use
  // prodos as source of truth.
  word16 f;
  word32 a;
  if (host_finder_info_to_filetype(info->finder_info, &f, &a) != 0) return;
  if (f == info->prodos_file_type && a == info->prodos_aux_type) return;
  if (preference == prefer_prodos)
    host_file_type_to_finder_info(info->finder_info, info->prodos_file_type, info->prodos_aux_type);
  else {
    info->prodos_file_type = f;
    info->prodos_aux_type = a;
  }
}



static DWORD root_file_id[3] = { 0 };

unsigned host_startup(void) {
#if HOST_FST_DEBUG
  setvbuf(stderr, NULL, _IONBF, 0);
#endif

  if (!g_cfg_host_path) return invalidFSTop;
  if (!*g_cfg_host_path) return invalidFSTop;
  if (host_root) free(host_root);
  host_root = strdup(g_cfg_host_path);


  HANDLE h = CreateFile(host_root, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    host_win32_log("CreateFile:host_root", host_root, GetLastError());
    fprintf(stderr, "%s does not exist or is not accessible\n", host_root);
    return invalidFSTop;
  }
  FILE_BASIC_INFO fbi;
  BY_HANDLE_FILE_INFORMATION info;

  GetFileInformationByHandle(h, &info);
  GetFileInformationByHandleEx(h, FileBasicInfo, &fbi, sizeof(fbi));
  // can't delete volume root.
  CloseHandle(h);

  root_file_id[0] = info.dwVolumeSerialNumber;
  root_file_id[1] = info.nFileIndexHigh;
  root_file_id[2] = info.nFileIndexLow;


  if (!(fbi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
    fprintf(stderr, "%s is not a directory\n", host_root);
    return invalidFSTop;
  }

  return 0;
}

void host_shutdown(void) {
  if (host_root) free(host_root);
  host_root = NULL;
  memset(root_file_id, 0, sizeof(root_file_id));
}

int host_is_root(const BY_HANDLE_FILE_INFORMATION *info) {
  DWORD id[3] = { info->dwVolumeSerialNumber, info->nFileIndexHigh, info->nFileIndexLow };

  return memcmp(&id, &root_file_id, sizeof(root_file_id)) == 0;
}


/*
 * Date/time conversion
 */


static int dayofweek(int y, int m, int d) {
  /* 1 <= m <= 12,  y > 1752 (in the U.K.) */
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

/*
 * converts time_t to a gs/os readhextime date/time record.
 */

void host_set_date_time_rec(word32 ptr, FILETIME utc) {

  if (utc.dwLowDateTime == 0 && utc.dwHighDateTime == 0) {
    for (int i = 0; i < 8; ++i) set_memory_c(ptr++, 0, 0);
    return;
  }


  SYSTEMTIME tmLocal;
  SYSTEMTIME tmUTC;

  FileTimeToSystemTime(&utc, &tmUTC);
  SystemTimeToTzSpecificLocalTime(NULL, &tmUTC, &tmLocal);

  if (tmLocal.wYear < 1900 || tmLocal.wYear > 1900 + 255) {
    for (int i = 0; i < 8; ++i) set_memory_c(ptr++, 0, 0);
    return;
  }
  if (tmLocal.wSecond == 60) tmLocal.wSecond = 59;       /* leap second */

  int dow = dayofweek(tmLocal.wYear, tmLocal.wMonth, tmLocal.wDay);
  set_memory_c(ptr++, tmLocal.wSecond, 0);
  set_memory_c(ptr++, tmLocal.wMinute, 0);
  set_memory_c(ptr++, tmLocal.wHour, 0);
  set_memory_c(ptr++, tmLocal.wYear - 1900, 0);
  set_memory_c(ptr++, tmLocal.wDay - 1, 0);       // 1 = sunday
  set_memory_c(ptr++, tmLocal.wMonth - 1, 0);
  set_memory_c(ptr++, 0, 0);
  set_memory_c(ptr++, dow + 1, 0);
}

/*
 * converts time_t to a prodos16 date/time record.
 */
void host_set_date_time(word32 ptr, FILETIME utc) {

  if (utc.dwLowDateTime == 0 && utc.dwHighDateTime == 0) {
    for (int i = 0; i < 4; ++i) set_memory_c(ptr++, 0, 0);
    return;
  }

  SYSTEMTIME tmLocal;
  SYSTEMTIME tmUTC;

  FileTimeToSystemTime(&utc, &tmUTC);
  SystemTimeToTzSpecificLocalTime(NULL, &tmUTC, &tmLocal);

  if (tmLocal.wYear < 1940 || tmLocal.wYear > 2039) {
    for (int i = 0; i < 4; ++i) set_memory_c(ptr++, 0, 0);
    return;
  }
  if (tmLocal.wSecond == 60) tmLocal.wSecond = 59;       /* leap second */

  word16 tmp = 0;
  tmp |= (tmLocal.wYear % 100) << 9;
  tmp |= tmLocal.wMonth << 5;
  tmp |= tmLocal.wDay;

  set_memory16_c(ptr, tmp, 0);
  ptr += 2;

  tmp = 0;
  tmp |= tmLocal.wHour << 8;
  tmp |= tmLocal.wMinute;
  set_memory16_c(ptr, tmp, 0);
}


FILETIME host_get_date_time(word32 ptr) {

  FILETIME utc = {0, 0};

  word16 a = get_memory16_c(ptr + 0, 0);
  word16 b = get_memory16_c(ptr + 2, 0);
  if (!a && !b) return utc;


  SYSTEMTIME tmLocal;
  SYSTEMTIME tmUTC;
  memset(&tmLocal, 0, sizeof(tmLocal));
  memset(&tmUTC, 0, sizeof(tmUTC));

  tmLocal.wYear = ((a >> 9) & 0x7f) + 1900;
  tmLocal.wMonth = ((a >> 5) & 0x0f);
  tmLocal.wDay = (a >> 0) & 0x1f;

  tmLocal.wHour = (b >> 8) & 0x1f;
  tmLocal.wMinute = (b >> 0) & 0x3f;
  tmLocal.wSecond = 0;


  // 00 - 39 => 2000-2039
  // 40 - 99 => 1940-1999
  if (tmLocal.wYear < 40) tmLocal.wYear += 100;


  TzSpecificLocalTimeToSystemTime(NULL, &tmLocal, &tmUTC);
  if (!SystemTimeToFileTime(&tmUTC, &utc)) memset(&utc, 0, sizeof(utc));
 
  return utc;
}

FILETIME host_get_date_time_rec(word32 ptr) {

  FILETIME utc = {0, 0};

  byte buffer[8];
  for (int i = 0; i < 8; ++i) buffer[i] = get_memory_c(ptr++, 0);

  if (!memcmp(buffer, "\x00\x00\x00\x00\x00\x00\x00\x00", 8)) return utc;

  SYSTEMTIME tmLocal;
  SYSTEMTIME tmUTC;
  memset(&tmLocal, 0, sizeof(tmLocal));
  memset(&tmUTC, 0, sizeof(tmUTC));
  memset(&utc, 0, sizeof(utc));

  tmLocal.wSecond = buffer[0];
  tmLocal.wMinute = buffer[1];
  tmLocal.wHour = buffer[2];
  tmLocal.wYear = 1900 + buffer[3];
  tmLocal.wDay = buffer[4] + 1;
  tmLocal.wMonth = buffer[5] + 1;

  TzSpecificLocalTimeToSystemTime(NULL, &tmLocal, &tmUTC);
  if (!SystemTimeToFileTime(&tmUTC, &utc)) memset(&utc, 0, sizeof(utc));

  return utc;
}


word32 host_convert_date_time(FILETIME utc) {

  if (utc.dwLowDateTime == 0 && utc.dwHighDateTime == 0) {
    return 0;
  }

  SYSTEMTIME tmLocal;
  SYSTEMTIME tmUTC;

  FileTimeToSystemTime(&utc, &tmUTC);
  SystemTimeToTzSpecificLocalTime(NULL, &tmUTC, &tmLocal);

  if (tmLocal.wYear < 1940 || tmLocal.wYear > 2039) {
    return 0;
  }
  if (tmLocal.wSecond == 60) tmLocal.wSecond = 59;       /* leap second */

  word16 dd = 0;
  dd |= (tmLocal.wYear % 100) << 9;
  dd |= tmLocal.wMonth << 5;
  dd |= tmLocal.wDay;


  word16 tt = 0;
  tt |= tmLocal.wHour << 8;
  tt |= tmLocal.wMinute;

  return (tt << 16) | dd;
}


static word32 map_win32_error(DWORD e) {
  switch (e) {
    case ERROR_NO_MORE_FILES:
      return endOfDir;
    case ERROR_FILE_NOT_FOUND:
      return fileNotFound;
    case ERROR_PATH_NOT_FOUND:
      return pathNotFound;
    case ERROR_ACCESS_DENIED:
    case ERROR_INVALID_ACCESS:
      return invalidAccess;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
      return dupPathname;
    case ERROR_DISK_FULL:
      return volumeFull;
    case ERROR_INVALID_PARAMETER:
      return paramRangeErr;
    case ERROR_DRIVE_LOCKED:
      return drvrWrtProt;
    case ERROR_NEGATIVE_SEEK:
      return outOfRange;
    case ERROR_SHARING_VIOLATION:
      return fileBusy;                   // destroy open file, etc.
    case ERROR_DIR_NOT_EMPTY:
      return invalidAccess;
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_DIRECTORY:
    case ERROR_FILENAME_EXCED_RANGE:
      return badPathSyntax;
    default:
      return drvrIOError;
  }
}

void host_win32_log(const char *op, const char *path, DWORD e) {
#if HOST_FST_DEBUG
  char msg[512];
  DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, e, 0, msg, (DWORD)sizeof(msg), NULL);
  while (n > 0 && (msg[n - 1] == '\r' || msg[n - 1] == '\n' || msg[n - 1] == ' ')) {
    msg[--n] = 0;
  }
  if (n == 0) {
    snprintf(msg, sizeof(msg), "(no system message)");
  }

  word32 gs = map_win32_error(e);
  HOST_FST_LOG("Host FST WIN32: op=%s err=%lu (%s) gsos=$%02x %s path=%s\n",
               op ? op : "?",
               (unsigned long)e,
               msg,
               (unsigned)gs,
               host_error_name((word16)gs),
               path ? path : "(null)");
#else
  (void)op;
  (void)path;
  (void)e;
#endif
}

word32 host_win32_fail(const char *op, const char *path, DWORD e) {
  host_win32_log(op, path, e);
  return map_win32_error(e);
}

word32 host_map_win32_error(DWORD e) {
  return host_win32_fail("win32", NULL, e);
}

static word32 win32_enoent(const char *path) {
  /*
   * some op on path returned FILE_NOT_FOUND / PATH_NOT_FOUND.
   * Walk parents: missing ancestor => pathNotFound, else fileNotFound.
   */
  char *p = host_gc_strdup(path);
  for (;;) {
    char *slash = strrchr(p, '/');
    char *bslash = strrchr(p, '\\');
    char *sep = slash;
    if (bslash && (!sep || bslash > sep)) sep = bslash;
    if (!sep) break;
    if (sep == p) break;
    if (sep == p + 2 && p[1] == ':') break;
    *sep = 0;
    if (p[0] == 0) break;
    DWORD attr = GetFileAttributesA(p);
    if (attr == INVALID_FILE_ATTRIBUTES)
      return pathNotFound;
  }
  return fileNotFound;
}

word32 host_map_win32_error_path(DWORD e, const char *path) {
  if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
    word32 gs = win32_enoent(path);
    HOST_FST_LOG("Host FST WIN32: op=path err=%lu gsos=$%02x %s path=%s\n",
                 (unsigned long)e, (unsigned)gs, host_error_name((word16)gs),
                 path ? path : "(null)");
    return gs;
  }
  return host_win32_fail("path", path, e);
}


void host_get_file_xinfo(const char *path, struct file_info *fi) {

  HANDLE h;

  char *p = host_gc_append_string(path, ":AFP_Resource");
  LARGE_INTEGER size = { 0 };


  h = CreateFile(p, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (h != INVALID_HANDLE_VALUE) {
    GetFileSizeEx(h, &size);
    CloseHandle(h);
  }
  fi->resource_eof = size.LowPart;
  fi->resource_blocks = (size.LowPart + 511) / 512;


  p = host_gc_append_string(path, ":AFP_AfpInfo");

  h = CreateFile(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (h != INVALID_HANDLE_VALUE) {
    DWORD read = 0;
    if (ReadFile(h, &fi->afp, sizeof(struct AFP_Info), &read, NULL) && read == sizeof(struct AFP_Info)) {
      if (afp_verify(&fi->afp)) fi->has_fi = 1;
      afp_to_filetype(&fi->afp, &fi->file_type, &fi->aux_type);
    }
    CloseHandle(h);
  }
}

static word16 map_attributes(DWORD dwFileAttributes) {

  // 0x01 = read enable
  // 0x02 = write enable
  // 0x04 = invisible
  // 0x08 = reserved
  // 0x10 = reserved
  // 0x20 = backup needed
  // 0x40 = rename enable
  // 0x80 = destroy enable

  word16 access = 0;

  if (dwFileAttributes & FILE_ATTRIBUTE_READONLY)
    access = readEnable;
  else
    access = readEnable | writeEnable | renameEnable | destroyEnable;

  if (dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
    access |= fileInvisible;

  // map FILE_ATTRIBUTE_ARCHIVE to backup needed bit?

  return access;
}


word32 host_get_file_info(const char *path, struct file_info *fi) {


  HANDLE h = CreateFile(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (h == INVALID_HANDLE_VALUE) return host_win32_fail("CreateFile:get_file_info", path, GetLastError());

  //FILE_BASIC_INFO fbi;
  //FILE_STANDARD_INFO fsi;
  //FILE_ID_INFO fii;
  BY_HANDLE_FILE_INFORMATION info;


  memset(fi, 0, sizeof(*fi));
  //memset(&fbi, 0, sizeof(fbi));
  //memset(&fsi, 0, sizeof(fsi));

  GetFileInformationByHandle(h, &info);
  //GetFileInformationByHandleEx(h, FileBasicInfo, &fbi, sizeof(fbi));
  //GetFileInformationByHandleEx(h, FileStandardInfo, &fsi, sizeof(fsi));
  //GetFileInformationByHandleEx(h, FileIdInfo, &fii, sizeof(fii));

  word32 size = info.nFileSizeLow;

  fi->eof = size;
  fi->blocks = (size + 511) / 512;

  fi->create_date = info.ftCreationTime;
  fi->modified_date = info.ftLastWriteTime;

  if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    fi->storage_type = directoryFile;
    fi->file_type = 0x0f;

    if (host_is_root(&info)) fi->storage_type = 0x0f;
  } else {
    fi->file_type = 0x06;
    if (size < 0x200) fi->storage_type = seedling;
    else if (size < 0x20000) fi->storage_type = sapling;
    else fi->storage_type = tree;

    host_get_file_xinfo(path, fi);
    if (fi->resource_eof) fi->storage_type = extendedFile;

    if (!fi->has_fi) host_synthesize_file_xinfo(path, fi);
  }

  // 0x01 = read enable
  // 0x02 = write enable
  // 0x04 = invisible
  // 0x08 = reserved
  // 0x10 = reserved
  // 0x20 = backup needed
  // 0x40 = rename enable
  // 0x80 = destroy enable

  word16 access = 0;

  if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
    access = 0x01;
  else
    access = 0x01 | 0x02 | 0x40 | 0x80;

  if (info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
    access |= 0x04;

  // map FILE_ATTRIBUTE_ARCHIVE to backup needed bit?

  fi->access = map_attributes(info.dwFileAttributes);


  CloseHandle(h);
  return 0;
}


word32 host_set_file_info(const char *path, struct file_info *fi) {

  DWORD attrs = GetFileAttributesA(path);
  int is_dir = (attrs != INVALID_FILE_ATTRIBUTES) &&
               (attrs & FILE_ATTRIBUTE_DIRECTORY);

  /* Finder SetFileInfo after CreateGS leaves storage_type 0. Never write
   * AFP_AfpInfo onto a directory; CreateFile without BACKUP_SEMANTICS on a
   * directory returns ERROR_ACCESS_DENIED (Finder: "physically damaged"). */
  if (is_dir && fi->storage_type != 0x0f) {
    fi->storage_type = 0x0d;
  }

  if (fi->has_fi && !is_dir && fi->storage_type != 0x0d && fi->storage_type != 0x0f) {
    char *rpath = host_gc_append_string(path, ":AFP_AfpInfo");

    HANDLE h = CreateFile(rpath, GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return host_win32_fail("CreateFile:AFP_AfpInfo", rpath, GetLastError());

    DWORD written = 0;
    if (!WriteFile(h, &fi->afp, sizeof(struct AFP_Info), &written, NULL)) {
      host_win32_log("WriteFile:AFP_AfpInfo (ignored)", rpath, GetLastError());
    }
    CloseHandle(h);
  }


  if (fi->create_date.dwLowDateTime || fi->create_date.dwHighDateTime
      || fi->modified_date.dwLowDateTime || fi->modified_date.dwHighDateTime
      || fi->access) {
    /* FILE_FLAG_BACKUP_SEMANTICS is required to open directories. */
    HANDLE h;
    h = CreateFile(path, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                   FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) return host_win32_fail("CreateFile:set_file_info", path, GetLastError());

    FILE_BASIC_INFO fbi;
    FILE_BASIC_INFO newfbi;
    memset(&fbi, 0, sizeof(fbi));
    memset(&newfbi, 0, sizeof(newfbi));

    BOOL ok;
    ok = GetFileInformationByHandleEx(h, FileBasicInfo, &fbi, sizeof(fbi));

    int delta = 0;

    word16 old_access = map_attributes(fbi.FileAttributes);
    if (fi->access && fi->access != old_access) {
      newfbi.FileAttributes = fbi.FileAttributes;

      if (fi->access & fileInvisible) {
        delta = 1;
        newfbi.FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
      }
      // hfs fst only marks it read enable if all are clear.
      word16 locked = writeEnable | destroyEnable | renameEnable;
      if ((fi->access & locked) == 0) {
        delta = 1;
        newfbi.FileAttributes |= FILE_ATTRIBUTE_READONLY;
      }
    }

    // todo -- compare against nt file time to see if it's actually changed.
    // to prevent time stamp truncation.

    if (fi->create_date.dwLowDateTime || fi->create_date.dwHighDateTime) {
      delta = 1;
      newfbi.CreationTime.LowPart = fi->create_date.dwLowDateTime;
      newfbi.CreationTime.HighPart = fi->create_date.dwHighDateTime;
    }
    if (fi->modified_date.dwLowDateTime || fi->modified_date.dwHighDateTime) {
      delta = 1;
      newfbi.LastWriteTime.LowPart = fi->modified_date.dwLowDateTime;
      newfbi.LastWriteTime.HighPart = fi->modified_date.dwHighDateTime;
      //newfbi.ChangeTime.LowPart = fi->modified_date.dwLowDateTime; //?
      //newfbi.ChangeTime.HighPart = fi->modified_date.dwHighDateTime; //?
    }

    if (delta) {
      ok = SetFileInformationByHandle(h, FileBasicInfo, &newfbi, sizeof(newfbi));
      if (!ok) {
        host_win32_log("SetFileInformationByHandle (ignored)", path, GetLastError());
      }
    }
    CloseHandle(h);
  }
  return 0;
}


static int qsort_callback(const void *a, const void *b) {
  return strcasecmp(*(const char **)a, *(const char **)b);
}

unsigned host_scan_directory(const char *path, char ***out, size_t *entries, unsigned p8) {

  WIN32_FIND_DATA fdata;
  HANDLE h;
  char **data = NULL;
  size_t capacity = 0;
  size_t count = 0;
  DWORD e;

  path = host_gc_append_path(path, "*");

  h = FindFirstFile(path, &fdata);
  if (h == INVALID_HANDLE_VALUE) {
    e = GetLastError();
    if (e == ERROR_FILE_NOT_FOUND) {
      /* empty directory */
      *out = NULL;
      *entries = 0;
      return 0;
    }
    return host_win32_fail("FindFirstFile", path, e);
  }

  do {
    char *name = fdata.cFileName;
    if (name[0] == 0) continue;
    if (name[0] == '.') continue;
    if (p8) {
      int ok = 1;
      int n = strlen(name);
      if (n > 15) continue;
      /* check for invalid characters? */
      for (int i = 0; i < n; ++i) {
        unsigned char c = name[i];
        if (isalpha(c) || isdigit(c) || c == '.') continue;
        ok = 0;
        break;
      }
      if (!ok) continue;
    }
    if (count == capacity) {
      char **tmp;
      tmp = realloc(data, (capacity + 100) * sizeof(char *));
      if (!tmp) {
        FindClose(h);
        host_free_directory(data, count);
        return outOfMem;
      }
      data = tmp;
      for (int i = count; i < capacity; ++i) data[i] = 0;
      capacity += 100;
    }
    data[count++] = strdup(name);

  } while (FindNextFile(h, &fdata) != 0);

  e = GetLastError();
  FindClose(h);

  if (e && e != ERROR_NO_MORE_FILES) {
    host_free_directory(data, count);
    return host_win32_fail("FindNextFile", path, e);
  }
  qsort(data, count, sizeof(char *), qsort_callback);
  *entries = count;
  *out = data;

  return 0;
}


unsigned host_storage_type(const char *path, word16 *error) {
  if (!path) {
    *error = badPathSyntax;
    return 0;
  }
  HANDLE h;
  h = CreateFile(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    *error = host_win32_fail("CreateFile:storage_type", path, GetLastError());
    return 0;
  }

  BY_HANDLE_FILE_INFORMATION info;
  GetFileInformationByHandle(h, &info);
  CloseHandle(h);

  if (host_is_root(&info)) return 0x0f;

  if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    return directoryFile;

  return standardFile;
}
