# Contributing to GSSquared

We love collaboration and pull requests. If you want to help make Apple II
emulation better, you are welcome here.

## Pull requests

Please open a PR. Bug fixes, features, docs, and "I noticed this while using a
real machine" reports are all appreciated.

### Changes to emulator behavior

GSSquared aims to match real hardware. If a change alters current emulator
behavior (CPU, MMU, video, interrupts, I/O, timing, and so on), we would like
to see it tested against real hardware first — or against a test that has
already been run on real hardware.

A short note of what you ran, on which machine and ROM, and what you observed
is enough. We do not need a lab notebook; we do need to know the change is not
only matching another emulator or a plausible reading of the code.

Documentation, UI, build, and other changes that do not affect guest behavior
do not need hardware tests.
