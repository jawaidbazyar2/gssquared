# Mouse

As the Apple II series progressed in time, mouse support became more and more integrated into the platform until in the //gs it became an indispensible base part of the system.

GS2 supports a Mouse in two ways:

* Via Mouse Card on Apple //e
* Via ADB (Apple Desktop Bus) mouse on Apple IIgs

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
