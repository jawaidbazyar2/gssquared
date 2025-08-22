# Sirius Joyport

This was an adapter for the Apple II to let it use Atari type mechanical joysticks.

It also allowed two sets of Apple paddles / joysticks to be connected, and to programmatically switch back and forth between the two joysticks so you can read two buttons from each set. (Normally, the Apple II game controller input only has three button inputs).

https://ia903106.us.archive.org/6/items/siriusjoyportmanual/Sirius%20Joyport%20Manual_text.pdf

list of games that support Joyport:

https://nerdlypleasures.blogspot.com/2021/08/digital-joysticks-and-apple-ii.html



The mappings are very straightforward. However, we will need some user-interface.

We need a mode button for game controller, for:

* Apple Joystick (the current, standard behavior)
* Atari Joysticks via Joyport
* Two Apple Joysticks via Joyport

The code will need access to annunciator record, which is easily done. And each of the pushbutton functions will need to switch behavior based on the UI setting.

## Atari Mode

Atari joysticks have 5 pushbutton inputs. The fire button, and 4 more for up down left right. (Atari joysticks don't have fine control, they're either on or off in a particular direction).

When comparing to a modern Gamepad, that is the nintendo-style "+" control.

So we can support either that, or the joystick mapped to a particular direction. Or even both.

In Atari mode, two Annunciator settings can be used.

If either left or right controller is used (controller select switch to left or right), the following applies:

| Annunciator 0 | C058 - C059 |
| Annunciator 1 | C05A - C05B |

| Controller Select | Annunciator #1 | Button 0 $C061 | Button 1 $C062 | Button 2 $C063 |
|--|--|--|--|--|
| Left | On | Fire-1 | Up-1 | Down-1 |
| | Off | Fire-1 | Left-1 | Right-1 |
| Right | On | Fire-2 | Up-2 | Down-2 |
| | Off | Fire-2 | Left-2 | Right-2 |

I.e., Annunciator #1 selects between vertical (off) and horizontal (on) readings.

If the Controller Select button is in the middle, then additionally Annunciator #0 lets you choose programmatically between the two controllers.

| Annunciator #0 | Annunciator #1 | Button 0 $C061 | Button 1 $C062 | Button 2 $C063 |
|--|--|--|--|--|
| On | On | Fire-1 | Up-1 | Down-1 |
| | Off | Fire-1 | Left-1 | Right-1 |
| Off | On | Fire-2 | Up-2 | Down-2 |
| | Off | Fire-2 | Left-2 | Right-2 |

## Control-Reset

I am thinking what we need to do on a control-reset, is suspend the joyport function until some time has elapsed past the reset. Even something like 100ms or 200ms would ensure normal pushbutton operation past the monitor routines that check PB0 and PB1.

## Two Joysticks

Now we need to support two gamepads/joysticks, through use of the annunciator #0 option.
