# Using a Keyboard

GS2 maps your modern computer keyboard, as best it can, to Apple II Keyboard functions.

The below tables explain how this is done, where it isn't obvious.

NOTE: Some platforms "eat" keystrokes intended for the Apple II window. E.g. Control-ALT(OA)-ESC on Windows minimizes the window (it's control-ESC). On Windows, doing a mouse capture (F1) can also help capture these keystrokes and let them do the Apple II thing instead of the Windows thing.

On the Mac, disable “Pressing Option 5 Times enables Mouse Keys” and Dictation. Otherwise pressing Control or Option repeatedly can interfere with the emulator.

| Key | Platform | Action |
| --- | --- | --- |
| F1 | All | Release (or capture) the mouse cursor |
| F2 | All | Cycle Composite, RGB, and Monochrome display engines |
| F3 | All | Toggle fullscreen and windowed mode |
| F4 | All | Open/Close Control Panel |
| F5 | All | Toggle pixel-blur (analog upscale) and rectangular (square) scaling |
| F6 | All | Cycle Joystick, Joyport, and mouse-emulated joystick modes |
| F7 | All | Toggle CRT Shader (when the GPU shader is available) |
| F9 | All | Increase speed — 1 MHz, 2.8 MHz, 7.1 MHz, 14.3 MHz, and Ludicrous Speed |
| Shift - F9 | All | Decrease speed |
| F10 | All | Open/close the debugger window |
| PrintScreen | All | Copy Screen to the host clipboard |
| Shift + PrintScreen | All | Save Screenshot to a PNG on the Desktop |
| Shift + Insert | All | Paste Text from the host clipboard into the emulated keyboard |
| Ctrl + F12 | MacOS,Windows | Reset |
| Ctrl + BREAK | Linux | Reset (sometimes this key is labelled Pause) |

F5 is a **scaling** preference, not a switch between “accurate NTSC” and a legacy renderer. Composite / RGB / mono is **F2**. See [Displays](Displays.md).

# Apple Keys - Modifier Keys

**Settings → Apple Keys** chooses which host keys are Open Apple and Closed Apple. The choice is stored in `system_settings.toml` as `keyboard.apple_keys` and is remembered across sessions. The default depends on the platform: Command = Open Apple on macOS, Alt = Open Apple on Windows and Linux, and Left Option = Open Apple in the browser.

| Menu | Settings file | Open Apple | Closed Apple |
| --- | --- | --- | --- |
| Command = Open Apple | `command_open_apple` | Command / Windows (both) | Option / Alt (both) |
| Alt = Open Apple | `alt_open_apple` | Alt / Option (both) | Command / Windows (both) |
| Left Option = Open Apple | `left_option_open_apple` | Left Option | Right Option |

On a Mac keyboard, Command is the GUI key and Option is Alt. On a PC keyboard, the Windows key is the GUI key. Left Option uses only the left Alt key for Open Apple and the right Alt key for Closed Apple, so browser Command shortcuts stay with the browser.

Shift and Caps Lock are unchanged on every layout.

# Platform keyboards

## Apple II+

Control, Shift, Escape, Reset, Backspace, and left/right arrows work. A real II+ has no up/down arrows, so those keys do nothing in II+ mode. There is no **REPT** key — use your host keyboard’s autorepeat.

**Ctrl-OpenApple-Reset** (Machine → Restart) force-reboots a II+ even though Open-Apple and Closed-Apple are not mapped to the game-port buttons on a real II+. That is a GSSquared convenience.

On a real II+ keyboard, **@** is printed on the **P** key and **^** is printed on the **N** key. Holding Control with those keys produces the control code for the shifted glyph, the same way Control-A produces `$01`. A modern keyboard puts those glyphs on the number row instead, so GSSquared accepts both the II+ key positions and the modern ones:

| Host keys | Apple II character | Code |
| --- | --- | --- |
| Control-Shift-P, or Control-Shift-2 | Control-@ | `$00` (NUL) |
| Control-Shift-N, or Control-Shift-6 | Control-^ | `$1E` |

`$00` is the only way to type a null on the II+. Software that asks for Control-@ (for example some game cheats and monitor tricks) will take either chord.

## Apple IIe

Control, Shift, Escape, Reset, all four arrows, Open-Apple, and Closed-Apple work. Backspace is mapped to **0x7F Delete** on the IIe keyboard. No REPT key; host autorepeat is used instead.

## Apple IIgs (ADB)

IIgs input uses the ADB keyboard (and mouse). Layout is selected **inside the guest** (IIgs Control Panel / keyboard language), not from a GSSquared host menu.

**French (AZERTY)** is supported: when the IIgs is set to French, typing follows the French layout and character set expected by French GS/OS and apps.

1. Boot an Apple IIgs (ROM 01 or ROM 03).
2. In the guest IIgs Control Panel (or equivalent), set the keyboard / layout language to **French**.
3. Type normally on your host keyboard; GSSquared applies the French ADB mapping.

Full international keyboard coverage beyond French is planned and not complete yet.
