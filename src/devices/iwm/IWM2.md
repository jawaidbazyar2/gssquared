So the 3.5 physical layer basically works the same as the 5.25.however the interface has some enhancements.
the IWM has a number of mode switches to deal with 5.25. the big one is:
diskreg[6]"E	Enables 3.5-inch drive support:			0 = 5.25-inch drive and smartport devices available
			1 = 3.5-inch drive available"

this likely toggles the line on the 19pin connector labeled "3.5". there is also a "HDSEL" which is likely to be the smartport  (ok, it is)
the second tier of mode switches for using 3.5 vs 5.25 is the IWM Mode Register:
The bits of the mode register are laid out as follows:
	  7   6   5   4   3   2   1   0  
	+---+---+---+---+---+---+---+---+
	| R | R | R | S | C | M | H | L |
	+---+---+---+---+---+---+---+---+

With the various bit meanings described below:
	Bit	Function
	---	--------
	 R	Reserved
	 S	Clock speed:
			0 = 7 MHz.    // this is for Apple II
			1 = 8 MHz     // this is for Macs.
		Should always be 0.
	 C	Bit cell time:
			0 = 4 usec/bit (for 5.25 drives)
			1 = 2 usec/bit (for 3.5 drives). // twice as fast on a 3.5, double bit density
	 M	Motor-off timer:
			0 = leave drive on for 1 sec after program turns
			    it off
			1 = no delay.  // 3.5 drive off turns motor off immediately
		Should be 0 for 5.25 and 1 for 3.5.
	 H	Handshake protocol:
			0 = synchronous (software must supply proper
			    timing for writing data)
			1 = asynchronous (IWM supplies timing)
		Should be 0 for 5.25 and 1 for 3.5.
	 L	Latch mode:
			0 = read-data stays valid for about 7 usec
			1 = read-data stays valid for full byte time
		Should be 0 for 5.25 and 1 for 3.5.

So basically, the whole thing, $00 for 5.25, and $0F for 3.5. the big one here we're concerned with is instead of 
the wp sense bit shifting into the data register, has been replaced with the status register.(Q6=1, Q7=0)

The 3.5 also has various status and controls that are signaled with the phase lines (renamed CA0-2 + diskreg[sel]and LSTRB). So set the four bits and strobe. the result is in the status[7].

So, I am feeling like we could have iwm_controller. It will be very similar to diskii_controller. in fact we could instance a diskii_controller and vector between it and iwm_controller based on the 5.25/3.5 switch. 

the things that will be different between 5.25 and 3.5 in controller are:fast_forwardthe way the register loads are bytewise for reading and writing (i.e. check for completion byte at a time instead of bit at a time?)


## Floppy Differences between 5.25 and 3.5

the floppy525_woz->floppy35_woz changes are:

the fast_forward speed per cycle. currently head_position += cycles * 2. head position is bit with a , so this advances by 1/4 or 1 bit cell.however, for 3.5, it runs twice as fast, so we would do head_positoin += cycles * 4 (2 cpu cycles per bit cell). this will return more bits_to_sim per invocation. (the iwm should be clocked at 1mhz, not cpu clock). 

This could be a parameter from disk_controller. 

OR, maybe I can template the 3.5 and 5.25 off the (mostly) same code base. 

I think the read pulse, etc. are all the same. 

other differences: the 3.5 set_phase
this . the SEL bit from DISKREG needs to go to the 3.5 port? it must be on one of the pins. Research this more. We can just push this into floppy. Ah, it could be the HDSEL pin. I assumed this was "hard drive select" but Head Select makes more sense, given the firmware reference suggests this controls which head is used. (Probably mistake).

the way phase, SEL behave is of course radically different from the 5.25. but the controller interface is still those pins.
basically depending on how those are setup, the selected status is read on the write protect sense line. which is -always- this status bit on a 3.5.
