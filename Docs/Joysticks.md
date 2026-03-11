# Joysticks

We support and emulate:

* Joystick & Paddles - Gamepad
* Joystick & Paddles - Mouse
* Sirius / Atari Joyport

## Joystick & Paddles (Gamepad)

GS2 supports up to two (2) Gamepad or joystick devices. These can be USB, Bluetooth, whatever your system supports.

GS2 will detect and activate multiple Gamepads if present.

Gamepad support: I have and have tested a 8BitDo Pro2 Gamepad. However, the underlying SDL library is known to work with hundreds of Gamepad models.

Joystick axes and buttons are mapped like so:

| Gamepad Control | Apple II Control |
|--|--|
| Gamepad 1 X | PDL(0) |
| Gamepad 1 Y | PDL(1) |
| Gamepad 1 Btn A | Button 0 |
| Gamepad 1 Btn B | Button 1 |
| Gamepad 2 X | PDL(2) |
| Gamepad 2 Y | PDL(3) |
| Gamepad 2 Btn A | Button 2 |
| Gamepad 2 Btn B | The Apple IIs don't have any more buttons |

## Joystick & Paddles (Mouse)

If you don't have a joystick or gamepad, use Joystick (Mouse) mode to use your mouse as a joystick or paddles. Works better for paddle emulation. It's not the easiest thing to use, since there is no bounceback or "return to center" springs.

You will want to enable Mouse Capture here, as with any use of the mouse with Apple II/IIgs software.

It's mapped as follows: 
* Left to Right is the horizontal axis PDL(0). Up to Down is the vertical axis PDL(1).
* The left mouse button is Button 0; right is Button 1. If you only have a 1-button mouse, you're limited to Button 0.

## Sirius / Atari "Joyport" - Atari Joystick games

In Joyport mode, GS2 emulates a Sirius Joyport. A variety of early to mid 80s games supported the Joyport, which allowed connecting Atari 2600 joysticks to the Apple II through a converter box. Reading the controls on the 2600 joysticks was done by multiplexing into the Apple II buttons and annunciators.

The real Joyport had a compatibility problem which would cause control-reset to always throw the machine into self-test mode, on IIes and IIgs.

Unlike the real Joyport, GS2's Joyport is compatible with II+, IIe, AND IIgs by disabling Joyport functions for 100 milliseconds after a reset.

Enable Atari Joyport mode by using the menu, the OSD Buttons, or by pressing F6 on your keyboard until the message says Joyport mode is activated. (Switch back to regular Apple Joystick mode by pressing F6 again).

Joyport emulation only works with Gamepads, not the Mouse.

The Gamepad D-PAD (+ shaped control) is what is used for the Atari Joystick - NOT the regular joystick. 

Currently, only the first Gamepad is recognized by the system for Joyport mode, so no multi-player games.

Almost all Joyport games will require configuration in-game to enable Joyport support. Many Sirius Software games use Control-Shift-@ (Apple IIe Emulation) or Control-Shift-P (Apple II+ Emulation) to enable Joyport. Other games have setup menus. You will have to check the manual for the game.
