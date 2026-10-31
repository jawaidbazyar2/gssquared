*
* ADBTEST — ProDOS 8, Apple IIgs.
*
* Boot the image, and at the BASIC prompt type ADBTEST.
*
* Same path Wolfenstein 3D uses. Autopoll off does not stop
* interrupts: the keyboard still asserts SRQ, and the firmware
* interrupt manager polls every device on the SRQ list. This
* program does not hook E1/0010. It asks the ADB tool set to
* put the keyboard on that list.
*
*   SendInfo setModes ($04) with bit 0 set — keyboard autopoll off
*   SRQPoll ($1409) for ADB address 2
*
* The microcontroller reports SRQ to the host only in the status
* byte of a bus command the host issued. Nothing under ProDOS 8
* issues one, so once per video frame the main loop sends
* AsyncADBReceive ($0D09) Talk R3 to the mouse. The command is the
* microcontroller's form, $C0 + reg*16 + addr = $F3. The keyboard's
* SRQ rides back on that response and starts the firmware's SRQ
* list poll.
*
* The completion routine is entered in 8-bit native mode with the
* data-buffer pointer at direct-page offset 9 (three extra return
* bytes under the usual poll frame). Each non-$FF byte is sent
* back with SendInfo keyCode ($11). Then $C000 / $C025 are queued
* for the main loop:
*   KEY  WAS  NOW  MOD
* KEY is the ADB byte (bit 7 set = key up). WAS/NOW are $C000
* before and after the $11. MOD is $C025 after the $11.
* ESC (keydown) removes the SRQ handler, clears modes bit 0,
* and returns to BASIC.
*
         xc
         xc
         org   $2000
         dsk   ADBTEST
         typ   SYS
         mx    %11

bufp     equ   $0B

ADBTEST  start
         lda   #<ban1
         ldy   #>ban1
         jsr   puts
         lda   $FBB3
         cmp   #$06
         beq   isgs
         lda   #<needgs
         ldy   #>needgs
         jsr   puts
         rts

isgs     sei
         jsr   setup
         bcc   ready
         cli
         rts

ready    cli
         lda   #<hdr
         ldy   #>hdr
         jsr   puts

main     jsr   beat
         jsr   drawst
         lda   qhead
         cmp   qtail
         beq   mainq
         jsr   prrec
mainq    lda   quitf
         beq   main

         ldx   #0
         ldy   #0
hbwait   lda   hbbusy
         beq   hbidle
         dex
         bne   hbwait
         dey
         bne   hbwait
hbidle   sei
         jsr   teardown
         cli
         lda   #<bye
         ldy   #>bye
         jsr   puts
         rts

*
* Native, 16-bit. setModes bit 0, then SRQPoll address 2.
* Carry set: message already printed, back in emulation.
*
setup    clc
         xce
         rep   #$30
         mx    %00
         phb
         phk
         plb
         pea   $0001
         pea   $0000
         pea   modeb
         pea   $0004
         ldx   #$0909
         jsl   $E10000
         bcc   setok
         sta   errw
         stz   which
         plb
         jmp   report

setok    pea   $0000
         pea   srq
         pea   $0002
         ldx   #$1409
         jsl   $E10000
         bcc   setdone
         sta   errw
         pea   $0001
         pea   $0000
         pea   modeb
         pea   $0005
         ldx   #$0909
         jsl   $E10000
         lda   #1
         sta   which
         plb
         jmp   report

setdone  plb
         jsr   emul
         clc
         rts

*
* Leave native 16-bit and return in emulation, 8-bit.
*
emul     sep   #$30
         mx    %11
         sec
         xce
         rts

*
* SRQRemove address 2, then clearModes bit 0.
* Entered in emulation with interrupts off.
*
teardown clc
         xce
         rep   #$30
         mx    %00
         phb
         phk
         plb
         pea   $0002
         ldx   #$1509
         jsl   $E10000
         pea   $0001
         pea   $0000
         pea   modeb
         pea   $0005
         ldx   #$0909
         jsl   $E10000
         plb
         jmp   emul

*
* On each rising edge of $C019 bit 7, if the last one finished,
* AsyncADBReceive Talk R3 address 3. Emulation on entry and exit.
*
beat     mx    %11
         lda   hbbusy
         bne   bdone
         lda   $C019
         and   #$80
         tax
         eor   vblprev
         beq   bdone
         stx   vblprev
         txa
         beq   bdone
         lda   #1
         sta   hbbusy
         clc
         xce
         rep   #$30
         mx    %00
         pea   $0000
         pea   hbdone
         pea   $00F3
         ldx   #$0D09
         jsl   $E10000
         bcc   bok
         sta   lasterr
         sep   #$30
         mx    %11
         stz   hbbusy
         inc   hberr
         bra   bsent
bok      sep   #$30
         inc   hbsent
bsent    sec
         xce
bdone    rts

*
* AsyncADBReceive completion. 8-bit native, bank unknown.
*
hbdone   mx    %11
         lda   >hbcnt
         clc
         adc   #1
         sta   >hbcnt
         lda   #0
         sta   >hbbusy
         rtl

*
* Status line on the bottom row, written straight to the Mega II
* text page so it does not scroll. 80 columns: even columns are
* in E1, odd in E0.
*
drawst   stz   spos
         lda   #<stl1
         ldy   #>stl1
         jsr   stputs
         lda   hbsent
         jsr   sthex
         lda   #<stl2
         ldy   #>stl2
         jsr   stputs
         lda   hbcnt
         jsr   sthex
         lda   #<stl3
         ldy   #>stl3
         jsr   stputs
         lda   hberr
         jsr   sthex
         lda   #<stl4
         ldy   #>stl4
         jsr   stputs
         lda   lasterr+1
         jsr   sthex
         lda   lasterr
         jsr   sthex
         lda   #<stl5
         ldy   #>stl5
         jsr   stputs
         lda   srqn
         jsr   sthex
         lda   #<stl6
         ldy   #>stl6
         jsr   stputs
         lda   srqz
         jsr   sthex
         lda   #<stl7
         ldy   #>stl7
         jsr   stputs
         lda   $C027
         jsr   sthex
         rts

stputs   sta   stlda+1
         sty   stlda+2
         ldy   #0
stlda    lda   $FFFF,y
         beq   stpdn
         jsr   stput
         iny
         bne   stlda
stpdn    rts

sthex    pha
         lsr
         lsr
         lsr
         lsr
         jsr   stnib
         pla
stnib    and   #$0F
         cmp   #10
         bcc   stnibd
         adc   #6
stnibd   adc   #'0'

stput    ora   #$80
         pha
         lda   $C01F
         bmi   st80
         ldx   spos
         pla
         sta   >$E007D0,x
         inc   spos
         rts
st80     lda   spos
         lsr
         tax
         pla
         bcs   stodd
         sta   >$E107D0,x
         inc   spos
         rts
stodd    sta   >$E007D0,x
         inc   spos
         rts

*
* Back in emulation. which = 0 setModes failed, 1 SRQPoll failed.
* Entered native 16-bit.
*
report   mx    %11
         jsr   emul
         lda   which
         bne   repfail
         lda   #<sfail
         ldy   #>sfail
         bra   repsay
repfail  lda   #<pfail
         ldy   #>pfail
repsay   jsr   puts
         jsr   perr
         sec
         rts

*
* SRQ list completion. 8-bit native, I=1, bank and DP unknown.
* Data buffer pointer is at DP+9 after PHD / TSC / TCD; the PHB
* and PHX ahead of the PHD move it to DP+11.
* Byte 0 nonzero means data. Bytes 1 and 2 are the keyboard's
* register 0 in reverse order: byte 2 is the first key.
*
srq      mx    %11
         phb
         phk
         plb
         phx
         inc   srqn
         phd
         tsc
         tcd
         lda   [bufp]
         beq   srnone
         ldy   #2
         lda   [bufp],y
         sta   kbytes
         dey
         lda   [bufp],y
         sta   kbytes+1
         pld
         lda   kbytes
         jsr   onekey
         lda   kbytes+1
         jsr   onekey
         bra   srout
srnone   pld
         inc   srqz
srout    plx
         plb
         rtl

*
* A = one ADB byte. $FF is an empty slot.
*
onekey   cmp   #$FF
         beq   onedone
         jmp   sendkey
onedone  rts

*
* A = one ADB byte. SendInfo keyCode, then queue C000/C025.
* Preserves Y. 8-bit on entry and exit.
*
sendkey  phy
         sta   raw
         sta   raww
         lda   >$E0C000
         sta   was
         rep   #$30
         mx    %00
         lda   raww
         and   #$00FF
         sta   raww
         ldx   #8
sktry    phx
         pea   $0001
         pea   $0000
         pea   raww
         pea   $0011
         ldx   #$0909
         jsl   $E10000
         plx
         bcc   skok
         cmp   #$0982
         bne   skbad
         dex
         bne   sktry
skbad    sep   #$30
         mx    %11
         lda   #$EE
         sta   was
         sta   now
         sta   modb
         jsr   enqueue
         ply
         rts
skok     sep   #$30
         mx    %11
         jsr   settle
         lda   >$E0C000
         sta   now
         lda   >$E0C025
         sta   modb
         sta   >$E0C010
         jsr   enqueue
         lda   raw
         and   #$7F
         cmp   #$35
         bne   skdone
         lda   raw
         bmi   skdone
         lda   #1
         sta   quitf
skdone   ply
         rts

*
* The microcontroller handles $11 after SendInfo returns. Wait for
* the $C000 strobe, or about 4 ms at 2.8 MHz, before reading.
*
settle   ldy   #10
         ldx   #0
setlp    lda   >$E0C000
         bmi   setdn
         dex
         bne   setlp
         dey
         bne   setlp
setdn    rts

enqueue  lda   qtail
         clc
         adc   #1
         and   #$1F
         cmp   qhead
         beq   enqfull
         lda   qtail
         asl
         asl
         tax
         lda   raw
         sta   qbuf,x
         lda   was
         sta   qbuf+1,x
         lda   now
         sta   qbuf+2,x
         lda   modb
         sta   qbuf+3,x
         lda   qtail
         clc
         adc   #1
         and   #$1F
         sta   qtail
enqfull  rts

perr     lda   errw+1
         jsr   phex
         lda   errw
         jsr   phex
         lda   #$0D
         jmp   cout

prrec    lda   qhead
         asl
         asl
         tax
         lda   qbuf,x
         jsr   phex
         jsr   sp2
         lda   qbuf+1,x
         jsr   phex
         jsr   sp2
         lda   qbuf+2,x
         jsr   phex
         jsr   sp2
         lda   qbuf+3,x
         jsr   phex
         lda   #$0D
         jsr   cout
         lda   qhead
         clc
         adc   #1
         and   #$1F
         sta   qhead
         rts

sp2      lda   #' '
         jsr   cout
         jmp   cout

phex     pha
         lsr
         lsr
         lsr
         lsr
         jsr   pnib
         pla
pnib     and   #$0F
         cmp   #10
         bcc   pnibd
         adc   #6
pnibd    adc   #'0'
         jmp   cout

puts     sta   plda+1
         sty   plda+2
         ldy   #0
plda     lda   $FFFF,y
         beq   putsdn
         jsr   cout
         iny
         bne   plda
putsdn   rts

cout     phx
         phy
         ora   #$80
         jsr   $FDED
         ply
         plx
         rts

ban1     asc   'ADBTEST'
         dfb   $0D
         asc   'SRQPoll keyboard, send $11, read C000.'
         dfb   $0D,0
needgs   asc   'Needs an Apple IIgs.'
         dfb   $0D,0
sfail    asc   'SetModes failed '
         dfb   0
pfail    asc   'SRQPoll failed '
         dfb   0
hdr      asc   'KEY  WAS  NOW  MOD'
         dfb   $0D
         asc   'bit7=up  ESC quits  EE=timeout'
         dfb   $0D
         asc   'Bottom: H sent D done E err/code'
         dfb   $0D
         asc   'S SRQ calls Z empty C C027'
         dfb   $0D,0
stl1     asc   'H'
         dfb   0
stl2     asc   ' D'
         dfb   0
stl3     asc   ' E'
         dfb   0
stl4     asc   '/'
         dfb   0
stl5     asc   ' S'
         dfb   0
stl6     asc   ' Z'
         dfb   0
stl7     asc   ' C'
         dfb   0
bye      dfb   $0D
         asc   'Back to BASIC.'
         dfb   $0D,0

modeb    dfb   $01,0
which    dfb   0,0
raww     dfb   0,0
errw     dfb   0,0
quitf    dfb   0
hbbusy   dfb   0
vblprev  dfb   0
hbsent   dfb   0
hbcnt    dfb   0
hberr    dfb   0
lasterr  dfb   0,0
srqn     dfb   0
srqz     dfb   0
spos     dfb   0
klen     dfb   0
raw      dfb   0
was      dfb   0
now      dfb   0
modb     dfb   0
qhead    dfb   0
qtail    dfb   0
kbytes   ds    8
qbuf     ds    128
         end
