MERLIN32 ?= $(HOME)/src/Merlin32_v1.2_b2/MacOs/Merlin32
MACROS   ?= $(HOME)/src/Merlin32_v1.2_b2/Library
CP2      ?= $(HOME)/src/cp2_1.0.5_osx-x64_sc/cp2

.PHONY: all clean

all: hosttext.po

HOSTTEXT: hosttext.s ss.s
	$(MERLIN32) $(MACROS) hosttext.s
	test -f HOSTTEXT

hosttext.po: HOSTTEXT
	rm -f hosttext.po
	$(CP2) create-disk-image hosttext.po 800k prodos
	$(CP2) add --overwrite hosttext.po HOSTTEXT
	$(CP2) set-attr hosttext.po type=S16,aux=0x0000 HOSTTEXT
	$(CP2) catalog hosttext.po

clean:
	rm -f HOSTTEXT hosttext.po error_output.txt _FileInformation.txt \
		HOSTTEXT_S01_Segment1_Output.txt HOSTTEXT_S01_Segment1_Output_Error.txt
