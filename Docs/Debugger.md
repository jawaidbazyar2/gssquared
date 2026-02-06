# Debugger

The purpose of the debugger is to allow the user who might be diagnosing a bug in the emulator, or, a bug in software they are writing, to have a way to see what is happening in the virtual Apple II.

Debugger will operate in a separate window from the main screen. It will be an "always on top" window.

It should have the following features:

* setting breakpoints in the CPU / memory
* Display of last 10 - 20 disassembled instructions.
* speculative disassembly of next 10-20 instructions, IF execution is paused. Have to assume straght-line execution (no conditional branches). Could follow JSR / JMP or BRA.
* Display of CPU registers & flags
* As we have now, display the data value that went out to, or came in from, memory.

Might be nice to see states of common hardware registers, like keyboard, annunciators, 

Apple2ts has a nice thing where it shows the state of the various bank-switched memory regions. It also has the ability to dump memory ranges.

It would be ideal, to be able to define memory watch ranges, which will display the data in the range and be updated every time an instruction is executed.

now, if we're free running, we can't update display every instruction. So we would 'snapshot' every frame (1/60th second) and just draw what's there.

For controls, there should be normal debugger controls:
* step over - run JSR'd subroutine at full speed, stop when we get back
* step into - step and follow JSR
* step out - run until a RTS or RTI
* continue

I think the above can be handled easily by automatically setting appropriate breakpoints. e.g., step into is "set breakpoint at next instruction, then run".

## Architecture

So, the historical disassembly would be in a circular buffer.


# Monitor / Debugger interface

Tracing is working pretty well so far. Can trace on/off, can single-step and resume execution. Got a lot of great ideas on FB:
```
* Memory window(s). In particular I recently found myself debugging in an emulator that only let me watch one contiguous chunk of memory, but I wanted to watch two as the date being manipulated were in very different regions.
* a disassembly window centered on the current line of code
* text screen
* lo res screen
* hi res screen
It would also be nice if the tabs you proposed were tear away tabs, so that you could look at multiple at the same time by moving each window around on your monitors as needed.
-Memory view. It’s nice to show different “formats”. Hex, Text (maybe ASCII and Apple II encoding).
- Setting memory breakpoints. When someone reads (or writes) from $A0, stop.
- View the current Text screens, GR, HGR, HGR2, DHGR, even if they are not active. Extra credit to render and arbitrary bit if memory (looking for sprite data, etc) as hgr/gr/etc.
- Disassembly window that shows upcoming instructions. Lets you set breakpoints.
- A panel that shows all soft-switches values.
- Load symbols for code complied in CC65.
- I like the idea of a little monitor too.
- Load a blob of compiled code into memory and execute it. (bload from local machine). You could make a very smooth development workflow. Compile on the PC, load into mem and debug.
- Save / Restore state of the whole system. No need to wait to boot something. Save your games, etc.
```

I had a UI idea: when the mouse is moving over the window, show some controls. One of them will be a triangle that opens the OSD. That same tab also closes the OSD. It will be on the upper right corner sticking out, so it will always be clear what's up with it.

[x] implement the "temporary message at bottom of screen" idea I had; use it for mouse capture.   
[ ] use general mono_color_table for rendering videx too. Probably bring those variables out and centralize them somewhere.  

Mike sugggested the "save machine state". We would need to save:
* contents of memory
* cycle counter
* PC, registers, etc. of course
* values of softswitches

To restore machine state, seems like the copymaster etc cards had to have done this. Instead of trying to save all of the emulator's state, perhaps we can "play back" the soft switch settings in some cases, i.e. call the routines via the memory bus interface. The audio modules (speaker, mb) are going to be tricky.

So multiple requests for "show upcoming instructions". That's probably straightforward to do. Draw those in a different color to indicate they're not executed yet. Show a highlight or arrow or other indicator. Kicks in when you enter step-wise mode. The trace is everything that -has- executed. The disassembly would be everything that -will- be executed, including whatever is at the current program counter.
Definitely implement a check that doesn't require re-rendering the debug window if nothing has changed. (e.g., sitting and staring at stepwise).

I'm liking the idea of maybe extending the width of the debug window to show:
* breakpoints
* monitored memory
* softswitch values per Mike above
* mike says maybe refer to compiler symbol table?
* show labels for well-known addresses - based on the effective address. would be hard for forward-disassembler.

I was thinking I like the idea of combining monitor and debugger interface. so you'd have the regular monitor interface commands: 
XXXX.YYYY to display memory. 
XXXX YYYY.ZZZZ M to move memory. XXXX:YY YY to set memory. etc.
But also things like "1234 BP", "1234.123 BP" <- set a breakpoint. 
how closely to make this mirror monitor? there aren't that many commands in the monitor. Well, a mini-assembler could be cool.
so, items are:
```
address
address.range
"string"
:val [..val, val, val]
commands are: move, set, clear, (breakpoint set / clear), L (list/disassemble), V (verify), R (from file specified in string), W ()
R would be:
load  "filename" 2000
save  "filename" 3000.4000
break C000
-break 3FE.3FF   - remove breakpoint 3FE.3FF
break    - by itself, list breakpoints
list / L 2000    (disassemble; L by itself, continue disassembly from last dis PC)
watch 2000  (memory monitor set)
nowatch 2000.2010  (memory monitor cancel)
watch     - list memory monitors
sload "label/file/path.lbl" - load a labels file
```

you would enter commands in a SDL "textedit" thing I just read about. and we can keep a small history with arrow key to get to them.

"words" will be parsed and placed in a linear list to execute.

Step out of: run until next RTS;
Step into: single step
Step over: single step, but if a JSR, run until RTS;

OK, so that's one approach. The other is more of a full GUI approach. That would be a lot of work. maybe there is a widget library I can lean on.
RmlUI is pretty sophisticated. Might do most of the things we want? Looks pretty snazzy too. ImGui looks more like stuff from 20 years ago. Square and blocky.


Instead of tabs, what if in the window we have panes, that the user can enable and disable by clicking buttons. Most screens today are very wide, so add panes horizontally. We can automatically increase/decrease the window width as they open/close.

disassembly pane
monitor pane
memory mon pane
video decode pane
devices pane

Certain monitor commands will automatically pop open panes. for example if you set a breakpoint.
List breakpoints in the disassembly pane.

reduce whitespace a bit between cycles and registers.
I don't need the horizontal spacing anywhere except the disassembly pane, so free up all that RE at the top.
Put control buttons at the top. These can be Button_t in a container!

That's coming together nicely.

## Text Input

so the monitor section, we need a text input widget. 

TextInputLine: Tile_t : Define a rectangular area. if you click inside the area, key down events start to be processed by that. If you click outside the area, key down events stop being processed. It can be a derivative of Tile like everything else. on Enter, a callback you set is called (just like we have done with click callbacks). need to handle delete and backspace. Track a cursor position. I don't think the SDL Text Input is needed here. that's primarily useful for mobile, which I don't care bout. Just do it by hand.

# Watch Pane

this pane contains a number of selectable diagnostic displays, that automatically update each frame. First to implement.

| watch name | Description | II | IIe | IIgs |
|-|-|-|-|-|
| mmugs | Apple IIgs Memory Map | ❌ | ❌ | ✅ |
| iiememory | Apple IIe Memory Map | ❌ | ✅ | ❌ |
| display | various display information | ✅ | ✅ | ✅ |
| scc8530 | IIgs Zilog Serial Chip | ❌ | ❌ | ✅ |

Maybe the thing to do here is, when we are mapping memory, we pass along a string to set the memory map description. Then we can just read the whole thing straight out of the MMU page table. That seems good.
Alternatively, can we just construct this from the softswitches? That requires info about system type. I kind of like just having 


# Debugger Feature Roadmap

[ ] Implement Memory Explorer that lets you jump to, and page through, memory, like looking for text and stuff  

[ ] Also, some way to put in a memory location and have it tell you which application it belongs to  

[ ] "save trace right now to file name" command  

[ ] change "debug" to "watch", keying on the string argument vs number argument.  

[ ] allow end a watch by clicking on an '[x]' in the upper right corner of that watch's area.

[ ] when we hit a breakpoint, make a beepy sound. This should be easy to do with a soundeffect.  

