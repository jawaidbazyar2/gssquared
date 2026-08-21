# Clipboard

## Copy

"Copy" for now will do the following:

* Pre-allocate enough memory for conceivable Apple II bitmaps (640x216 should do it)

Call the Display Engine Copy routine

This routine (depending on display engine) will:

* Copy the screen buffer to the temporary memory after it.
* return the bitmap dimensions

"Copy" will then populate the bmp header for use when / if the clipboard callback routine is called.

## Paste

Paste is handled by the keyboard module. When a paste is done, copy the pasted string into a buffer.

**II / IIe:** each time `$C000` is read, if the strobe is clear (bit 7 = 0), inject the next character from the buffer. If the strobe is set, the guest has not processed the current keystroke. Shift+Insert, Edit → Paste Text, and debug-protocol `PASTE_TEXT` fill this buffer.

**IIgs (KeyGloo / ADB):** meter from the KeyGloo `frame_handler` (once per frame). If paste text remains and the `$C000` latch strobe is clear, inject one ASCII character via `store_key_to_buffer()` (`'\n'` becomes `'\r'`). Reset and keyboard flush abort the paste. Shift+Insert, Edit → Paste Text, and debug-protocol `PASTE_TEXT` all fill this buffer.

Implemented!
