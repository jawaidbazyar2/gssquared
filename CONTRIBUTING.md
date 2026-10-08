# Contributing to GSSquared

Bug reports, notes from real hardware, documentation, and pull requests are
welcome. GSSquared is trying to match real Apple II machines, and to stay
buildable on macOS, Windows, and Linux.

Build instructions are in the [README](README.md).

## Reporting bugs

Search the existing issues before opening a new one. If the bug is already
filed, add what you saw.

A useful report includes:

- The GSSquared version, or the git commit if you built from source.
- The host operating system.
- The guest machine (][, ][+, //e, //e Enhanced, IIgs) and the ROM revision,
  if you know it.
- The disk image or software, and the steps that show the problem.

A screenshot, a saved state, or a short trace helps. Matching another emulator
is a clue. It is not, by itself, evidence of correct behavior.

## Suggesting a change

A small fix can go straight to a pull request.

Open an issue first for a new machine, a disk format, or a change to timing,
memory, or I/O. Describe the hardware behavior you want to match. Settling
the approach before the code means the review is about the patch.

## Pull requests

Branch from `main` and open the pull request against `main`. See
[Branches and releases](#branches-and-releases).

Keep the pull request to one concern. A drive-light fix and a CPU change are
two pull requests. Split a large change into pieces that each stand on their
own.

The change should be finished enough to build and to review. You should
understand it and be able to explain what it does, including the edge cases.
Review comments are a conversation with you.

Follow the style of the file you are editing.

Leave `vendored/` alone. Those trees are third-party sources (SDL and
others). A platform limit gets a workaround in GSSquared code, or a note in
the docs.

### Changes to emulator behavior

GSSquared aims to match real hardware. If a change alters current emulator
behavior (CPU, MMU, video, interrupts, I/O, timing, and so on), test it
against real hardware first, or against a test that has already been run on
real hardware.

A short note of what you ran, on which machine and ROM, and what you observed
is enough. A lab notebook is not required. The note does need to show that
the change is grounded in the machine, and not only in another emulator or a
plausible reading of the code.

Documentation, UI, build, and other changes that do not affect guest behavior
do not need hardware tests.

### Discuss before writing code

Open an issue before starting on:

- CPU timing, the MMU, video timing, or interrupt behavior
- Disk formats, and the IWM or SmartPort path
- Anything under `vendored/`
- The release packaging scripts under `scripts/release/`

User interface, documentation, and build fixes can come in as a pull request.

## Licensing

Source files are under the GNU General Public License, version 3 or any later
version, as stated in their headers. A contribution sent to the project is
licensed on those same terms. There is no separate contributor license
agreement.

Code that cannot be licensed that way cannot be merged. GPL version 2-only
code is in that category.

## Branches and releases

`main` is the development branch. It stays in a state that can be shipped.
Every pull request targets `main`.

Each change uses a short-lived topic branch, named for the work, for example
`fix/iwm-write-protect` or `feat/woz-flux`. After the pull request merges,
that branch is finished. GSSquared does not keep a standing feature branch or
a standing bugfix branch. Those would leave `main` behind the real work, and
incoming pull requests would be stale on arrival.

### Where a release tag goes

A release is a git tag on the commit that was built and shipped. Existing
tags are named `v0.7.3`, `v1.0.0`, and so on.

When that commit is `main`, the tag is on `main`. That is how `v0.7.0`
through `v1.0.0` were cut, and it is the normal way to ship a new version.

A `release/1.0` branch is created from the `v1.0.0` tag only when the 1.0.x
line needs commits that `main` should not contain yet:

- a short freeze before a tag, if `main` has to keep moving, or
- a later patch, such as `v1.0.1`, after newer work has already landed on
  `main`.

The tag for that patch is placed on `release/1.0`, because that branch holds
the commit that was shipped. Fixes from the release branch are merged back to
`main`. The release branch takes bugfixes for that version line. It is
retired when the line is finished. The next version (`v1.1.0`, for example)
is tagged on `main` again, unless that line also needs a branch of its own.

Open pull requests against `main`. A maintainer cherry-picks onto
`release/1.0` when that line needs the fix.
