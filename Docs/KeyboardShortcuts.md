# Using a Keyboard

GS2 maps your modern computer keyboard, as best it can, to Apple II Keyboard functions.

The below tables explain how this is done, where it isn't obvious.

NOTE: Some platforms "eat" keystrokes intended for the Apple II window. E.g. Control-ALT(OA)-ESC on Windows minimizes the window (it's control-ESC). On Windows, doing a mouse capture (F1) can also help capture these keystrokes and let them do the Apple II thing instead of the Windows thing.

| Key | Platform | Action |
| --- | --- | --- |
| F1 | All | Release mouse cursor when captured by window |
| F2 | All | Toggle between Composite; RGB; and Monochrome displays |
| F3 | All | Toggle between fullscreen and windowed mode |
| F4 | All | Open/Close Control Panel display |
| F5 | All | Toggle between new display rendering (NTSC accurate) and old display rendering |
| Ctrl + F5 | All | Toggle between linear interpolation display rendering (slight blurring) and nearest neighbor display rendering (sharper) |
| F6 | All | Toggle between Joystick, Joyport, Mouse-emulated Joystick modes |
| F7 | All | Toggle CRT Shader (when the GPU shader is available) |
| F9 | All | Increase speed - 1MHz, 2.8MHz, 7.1MHz, 14.3MHz, and Ludicrous Speed |
| Shift - F9 | All | Decrease speed |
| F10 | All | Open/close the debugger window |
| PrintScreen | All | Copy Screen to the host clipboard |
| Shift + PrintScreen | All | Save Screenshot to a PNG on the Desktop |
| Shift + Insert | All | Paste Text from the host clipboard into the emulated keyboard |
| Ctrl + F12 | MacOS,Windows | Reset |
| Ctrl + BREAK | Linux | Reset (sometimes, this key is labelled Pause) |

# Apple Keys - Modifier Keys

Linux / Windows

| Key | //e Modifier | IIgs Modifier |
| --- | --- | --- |
| ALT | Open Apple | Command |
| Windows | Closed Apple | Option |
| Shift | Shift | Shift |
| Caps Lock | Caps Lock | Caps Lock |

MacOS

| Key | //e Modifier | IIgs Modifier |
| --- | --- | --- |
| Command | Open Apple | Command |
| Option | Closed Apple | Option |
| Shift | Shift | Shift |
| Caps Lock | Caps Lock | Caps Lock |

# IIgs keyboard layouts (including French)

On the Apple IIgs, the **keyboard layout** is selected inside the emulated machine (IIgs Control Panel / keyboard language), not from a GSSquared host menu.

GSSquared maps your host keys through the layout the guest has selected. **French (AZERTY)** is supported: when the IIgs is set to French, typing follows the French layout and character set expected by French GS/OS and apps.

To use French:

1. Boot an Apple IIgs (ROM 01 or ROM 03).
2. In the guest IIgs Control Panel (or equivalent), set the keyboard / layout language to **French**.
3. Type normally on your host keyboard; GSSquared applies the French ADB mapping.
