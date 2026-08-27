MERLIN32 ?= $(HOME)/src/Merlin32_v1.2_b2/MacOs/Merlin32
MACROS   ?= $(HOME)/src/Merlin32_v1.2_b2/Library
CP2      ?= $(HOME)/src/cp2_1.0.5_osx-x64_sc/cp2

.PHONY: all clean

all: gputest.po

GPUTEST: gputest.s ss.s
	$(MERLIN32) $(MACROS) gputest.s
	test -f GPUTEST

gputest.po: GPUTEST
	rm -f gputest.po
	$(CP2) create-disk-image gputest.po 800k prodos
	$(CP2) add --overwrite gputest.po GPUTEST
	$(CP2) set-attr gputest.po type=S16,aux=0x0000 GPUTEST
	$(CP2) catalog gputest.po

clean:
	rm -f GPUTEST gputest.po error_output.txt _FileInformation.txt \
		GPUTEST_S01_Segment1_Output.txt GPUTEST_S01_Segment1_Output_Error.txt
