# ImageWriter II Printer Emulation

Research notes. The spec that came out of them is [PrinterEmulation.md](PrinterEmulation.md).

ImageWriter II was Apple's standard printer for the Apple II series, dot matrix.

Contains a link to a copy of the ImageWriter II ROM:

https://reneeharke.com/roms.html

it's 32KB, so it might take some doing to find the character data in it.

May not need to, the ImageWriter manual :
https://mirrors.apple2.org.za/ftp.apple.asimov.net/documentation/hardware/printers/Apple%20ImageWriter%20II%20Technical%20Reference%20Manual.pdf

has a section in the back where it shows the character matrix for the several character sets.

That is a lot of data entry. But it might be easier than trying to decode the ROM.

The ImageWriter II has a 9-pin print head.

Print Shop lets you select ImageWriter and Parallel port. Which couldn't happen in real life, but, works in the emulator.

Chars 32 - 126 are printable ASCII; 128-255 are "high ascii" and contain special characters.

ESC is the typical command character. If the character following ESC is invalid the printer ignores the ESC and it.

there's a test program in applesoft on page 10 of the manual and sample output.

The implementation will be to specify the fonts in a PNG file. The horizontal dimension is the characters; the vertical dimension is lines but also different character sets.

There are two "software switches" that act like dip switch banks. A and B. 8 bits each. They are settable by ESC commands:

* ESC Z a b - sets switches to open (off)
* ESC D a b - switches to closed (on)

(a are switches for A and b for B, capiche?)

In graphics mode, only the top 8 pins are used, because graphics data is 8-bit bytes.

A "line" will be a buffer of some number of pretend scanlines. For instance 8, or 9. Once we have rendered a line, we "send it to the printer" which means convert into data being sent to the PDF File.

This approach would generate a PDF that is all rasterized data. 

An alternative approach would embed and use postscript fonts in the PDF, fonts such as this one:
https://fontstruct.com/fontstructions/show/393784/imgwriter_draft
https://fontstruct.com/fontstructions/show/473428/imgwriter_nlq_1
he's got some other ImageWriter fonts too.
The benefit of this would be that the PDF will be smaller; text will be selectable and copyable.

However, there is a "user defined custom character set" feature. So, I think we'll have to go with the rasterized approach.

Print head wire spacing is 1/72 inch. However you can move the paper 1/144 inch at a time. This lets you do sort of double density output.

GIMP can show a pixel grid that will make it quite easy to draw in these characters according to the bitmaps in the manual. It will be time consuming because there are just so many of them.

Instead of doing a line at a time, we need to do a page at a time, maybe even multiple, since they can scroll up and down, and probably across pages.

There aren't that many different control codes. There are the graphics codes, things for setting tabs, which character set, etc. Should be pretty straightforward.
