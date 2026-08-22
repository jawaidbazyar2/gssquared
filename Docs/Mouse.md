# Mouse

As the Apple II series progressed in time, mouse support became more and more integrated into the platform until in the //gs it became an indispensible base part of the system.

GS2 supports a Mouse in two ways:

* Via **[Apple Mouse III](Cards_AppleMouse.md)** card on Apple //e (and similar)
* Via **ADB** (Apple Desktop Bus) mouse on Apple IIgs

## Mouse Capture

When you're using emulated Mouse software, you will probably want to turn on Mouse Capture.

Do this by using the Mouse Capture menu, Mouse Capture button, or by pressing F1.

When you enter Mouse Capture, a helpful message will appear at the top of the screen for a few seconds reminding you how to exit Mouse Capture.

To release Mouse Capture, you must press **F1**, or Alt-Tab to switch windows. (Alt-Tab may be a bit different depending on your host operating system.)

Opening the Control Panel via **F4** or the Control Panel button will also temporarily release Mouse Capture. When you close the Control Panel, the original Mouse Capture state will be restored.

Mouse capture also turns off the "mouse movement shows on-screen controls" feature. It locks you into that keyboard-oriented Apple II experience.


## IIgs - GS/OS Mouse Tracking

When you boot a GS/OS image, or other application that starts the IIgs Event Manager, GSSquared detects this and changes how it processes mouse movement events. It will calculate the mouse movement that the IIgs needs to see, in order to match where the Host (your computer) mouse . Then it synthesizes the appropriate events to get the cursor there.

This provides seamless movement of the mouse cursor in and out of the IIgs desktop, making the mouse work much more naturally, and avoiding the need for Mouse Capture mode with IIgs desktop apps.

You will still want to use Mouse Capture mode when using many games, or 8-bit programs, that don't use the Event Manager to read the mouse.

ROM 01 and ROM 03 IIgs platforms both use this tracking path.

Host X is mapped onto the guest’s 320- or 640-pixel SHR coordinate space using line 0’s Scan Control Byte (bit 7). That byte lives at CPU `$E1/9D00`; when `$C029` SHR/linearize is on it is stored at the interleaved Mega II physical address, not at raw Mega II `$E1/9D00`. Reading the untranslated location samples a pixel and can make a 640 desktop track as 320 (host cursor escapes at the right edge while the IIgs pointer is only halfway across).
