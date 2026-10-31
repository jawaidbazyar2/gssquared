MERLIN32 ?= $(HOME)/src/Merlin32_v1.2_b2/MacOs/Merlin32
MACROS   ?= $(HOME)/src/Merlin32_v1.2_b2/Library
CP2      ?= $(HOME)/src/cp2_1.0.5_osx-x64_sc/cp2
SYSTEM   ?= $(HOME)/Downloads/apple_II/images/masters/prodos/PRODOS-8 v4.0.2 System.dsk

.PHONY: all clean

all: adbtest.po

ADBTEST: adbtest.s
	$(MERLIN32) $(MACROS) adbtest.s
	test -f ADBTEST

adbtest.po: ADBTEST
	rm -f adbtest.po
	$(CP2) create-disk-image adbtest.po 800k prodos
	$(CP2) copy "$(SYSTEM)" PRODOS BASIC.SYSTEM adbtest.po
	$(CP2) add --overwrite adbtest.po ADBTEST
	$(CP2) set-attr adbtest.po type=SYS,aux=0x2000 ADBTEST
	$(CP2) catalog adbtest.po

clean:
	rm -f ADBTEST adbtest.po error_output.txt _FileInformation.txt \
		ADBTEST_S01_Segment1_Output.txt ADBTEST_S01_Segment1_Output_Error.txt
