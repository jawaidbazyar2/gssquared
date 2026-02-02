# Clock abstraction

Probably the next thing I should do is figure out and implement the Apple IIgs RAM/ROM/MegaII cycle timing.

Rather than putting in increasingly more complex logic into incr_cycle, I should probably do this:

incr_cycle() is a very simple that just does this:

inline void Clock::incr_cycle() {
  if (cycle_handler) (*cycle_handler)();
}

Then we can stuff different cycle handlers into it, e.g. switching in the LS handler when we want, having different handlers for IIe/IIgs. The inline and this being a very small routine, should make this inlinable throughout the CPU code.

What does a new clock abstraction look like?

Set the cpu clock speed.
interrogate information about the clock.
It's allocated separately, injected where needed.
Tracks:
  CPU cycle ticks
  14M cycle ticks
  (maybe ensoniq cycle ticks?)
Keeps track of "user desired accelerated speed" vs "current speed selection".
Tracks "frame" values (e.g. start of current frame, end of current frame) in 14M units
returns info about the clock state, and clock speed names, etc.

Having the handler like that is icky, what I should do is use, you know, proper class stuff. Have a abstract base class that does nothing, then have a variety of clock subclasses that do increasingly useful stuff.
Have a factory class that returns the correct clock object based on a selection from the Platform. (like how the CPU works).

The clock object will be created in computer and passed around where needed. have it be a header-file implementation to aid in inlining, and I expect a lot of the methods will be very short and inlined? no they can't if they're virtual. well we can have incr_cycle itself be inlinable and check a flag to call the real handler. like we do now with incr_cycle and slow_incr_cycle.

For clock sync, it's the MMU that knows when we're making a call into the megaii, or making some other call.
in read/write, set "clocktype=fast". Wherever we switch gears and call the megaii, set "clocktype=megaii". now, this is inside the clock object, which we have of course overridden for iigs. have these variables and their getter/setters in all clock class but only use them in mmu_iigs.

So how this works overall is:
1. cpu does a memory access, calls mmu.
1. mmu does the memory access, sets fast/megaii flag in clock.
1. cpu then calls incr_cycle
1. the incr_cycle in a GS checks the flag, and sync's accordingly.

In theory we ought to be able to get to a point where we can also simulate accelerators on IIe, and arbitrary CPU Clock speeds, by carefully calculating the values automatically in the clock table.

Should we inject Clock into every device? Or let a device grab it from Computer if it wants? Let's do latter, many but not all devices access 

OK, done! That wasn't that bad actually.

Now I need to do the IIgs-specific stuff.
