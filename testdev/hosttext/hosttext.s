* HOSTTEXT — GS/OS S16 demo for Second Sight Host Text mode
* SetMode($03,$04), ANSI font, planar 80x50 wrap scroll, HGR1/HGR2 page-flip.

         rel
         dsk   HOSTTEXT
         typ   S16
         mx    %00
         xc
         xc

HOSTTEXT START
         phk
         plb
         clc
         xce
         rep   #$30
         longa on
         longi on

         jsr   GetStatus

         lda   #$0003
         sta   ss_mode
         lda   #$0004
         sta   ss_flag
         jsr   SetMode
         jsr   ScreenOn

         lda   #$0002
         sta   fontidx
         jsr   SetTextFont

         jsr   InitPalette
         lda   #$2000
         sta   pgbase
         stz   fstyle
         jsr   FillPage
         lda   #$4000
         sta   pgbase
         lda   #1
         sta   fstyle
         jsr   FillPage
         jsr   InitCtrl

         lda   #$00E0
         sta   ctrl_lo
         lda   #$003F
         sta   ctrl_hi
         stz   ctrl_aux
         jsr   SetTextCtrl

         stz   startln
         stz   frameno
         stz   pagef
         stz   curx
         stz   scdiv
         stz   paldiv
         lda   #1
         sta   curdx

frame    jsr   WaitVBL
         jsr   TickScroll
         jsr   UpdateHUD
         lda   paldiv
         inc   a
         sta   paldiv
         cmp   #16
         bcc   nopal
         stz   paldiv
         jsr   RotPalette
nopal    jsr   TickCursor
         jsr   MaybeFlip

         sep   #$20
         longa off
         lda   >$00C000
         bpl   nokey
         sta   >$00C010
         rep   #$20
         longa on
         bra   quit
nokey    rep   #$20
         longa on
         bra   frame

quit     lda   #$0000
         sta   ss_mode
         sta   ss_flag
         jsr   SetMode
         jsl   $E100A8
         dw    $2029
         adrl  quitpb

* $C019 bit7=1 while drawing, 0 in VBL
WaitVBL  anop
         sep   #$20
         longa off
wv1      lda   >$00C019
         bpl   wv1
wv2      lda   >$00C019
         bmi   wv2
         rep   #$20
         longa on
         rts

TickScroll anop
         lda   scdiv
         inc   a
         sta   scdiv
         cmp   #8
         bcc   tsout
         stz   scdiv
         lda   startln
         inc   a
         cmp   #48
         bcc   tsok
         lda   #0
tsok     sta   startln
         sep   #$20
         longa off
         sta   >$E03FE4
         rep   #$20
         longa on
tsout    rts

TickCursor anop
         lda   curx
         clc
         adc   curdx
         bmi   cxlo
         cmp   #80
         bcc   cxok
         lda   #79
         sta   curx
         lda   #$FFFF
         sta   curdx
         bra   cxwr
cxlo     lda   #0
         sta   curx
         lda   #1
         sta   curdx
         bra   cxwr
cxok     sta   curx
cxwr     sep   #$20
         longa off
         lda   curx
         sta   >$E03FE5
         lda   #0
         sta   >$E03FE6
         rep   #$20
         longa on
         rts

MaybeFlip anop
         lda   frameno
         inc   a
         sta   frameno
         cmp   #120
         bcc   mfout
         stz   frameno
         lda   pagef
         eor   #1
         sta   pagef
         beq   mfa
         lda   #$4000
         ldx   #$4FA0
         bra   mfwr
mfa      lda   #$2000
         ldx   #$2FA0
mfwr     sta   bufaddr
         stx   attraddr
         sep   #$20
         longa off
         lda   bufaddr
         sta   >$E03FEA
         lda   bufaddr+1
         sta   >$E03FEB
         lda   attraddr
         sta   >$E03FEC
         lda   attraddr+1
         sta   >$E03FED
         rep   #$20
         longa on
mfout    rts

UpdateHUD anop
         lda   pagef
         beq   uha
         lda   #$4000
         bra   uhgo
uha      lda   #$2000
uhgo     sta   hudbase
         jsr   BuildHUD
         lda   hudbase
         jsr   CopyHUD
         lda   hudbase
         jsr   FillHUDAttr
         lda   hudbase
         clc
         adc   #3920
         jsr   CopyHUD
         lda   hudbase
         clc
         adc   #3920
         jsr   FillHUDAttr
         rts

* A = dest in bank $E0; 80 chars from hudbuf
CopyHUD  anop
         tax
         ldy   #0
         sep   #$20
         longa off
chlp     lda   hudbuf,y
         sta   >$E00000,x
         inx
         iny
         cpy   #80
         bcc   chlp
         rep   #$20
         longa on
         rts

* A = char dest in bank $E0; 80 attr bytes $1F (white on blue)
FillHUDAttr anop
         mx    %00
         clc
         adc   #$0FA0
         tax
         ldy   #80
         sep   #$20
         mx    %10
         lda   #$1F
fha      sta   >$E00000,x
         inx
         dey
         bne   fha
         rep   #$20
         mx    %00
         rts

BuildHUD anop
         ldx   #0
bh1      lda   hudtmpl,x
         sta   hudbuf,x
         inx
         inx
         cpx   #80
         bcc   bh1
         lda   startln
         lsr
         lsr
         lsr
         lsr
         jsr   HexN
         sep   #$20
         longa off
         sta   hudbuf+19
         rep   #$20
         longa on
         lda   startln
         and   #$000F
         jsr   HexN
         sep   #$20
         longa off
         sta   hudbuf+20
         lda   pagef
         clc
         adc   #'A'
         sta   hudbuf+32
         rep   #$20
         longa on
         rts

HexN     anop
         and   #$000F
         cmp   #10
         bcc   hnd
         clc
         adc   #'A'-10
         rts
hnd      clc
         adc   #'0'
         rts

InitPalette anop
         ldx   #0
         sep   #$20
         longa off
ip1      lda   ibmpal,x
         sta   >$E03FA0,x
         inx
         cpx   #48
         bcc   ip1
         rep   #$20
         longa on
         rts

RotPalette anop
         php
         sep   #$30
         mx    %11
         lda   >$E03FA3
         sta   savr
         lda   >$E03FA4
         sta   savg
         lda   >$E03FA5
         sta   savb
         ldx   #0
rp1      lda   >$E03FA6,x
         sta   >$E03FA3,x
         inx
         cpx   #42
         bcc   rp1
         lda   savr
         sta   >$E03FCD
         lda   savg
         sta   >$E03FCE
         lda   savb
         sta   >$E03FCF
         plp
         mx    %00
         rts

InitCtrl anop
         mx    %00
         ldx   #0
         sep   #$20
         mx    %10
         lda   #$17
         sta   >$E03FE0
         lda   #80
         sta   >$E03FE1
         lda   #25
         sta   >$E03FE2
         lda   #50
         sta   >$E03FE3
         lda   #0
         sta   >$E03FE4
         sta   >$E03FE5
         sta   >$E03FE6
         lda   #1
         sta   >$E03FE7
         sta   >$E03FE8
         lda   #0
         sta   >$E03FE9
         lda   #$00
         sta   >$E03FEA
         lda   #$20
         sta   >$E03FEB
         lda   #$A0
         sta   >$E03FEC
         lda   #$2F
         sta   >$E03FED
         lda   #$A0
         sta   >$E03FEE
         lda   #$3F
         sta   >$E03FEF
         lda   #0
icpad    sta   >$E03FF0,x
         inx
         cpx   #16
         bcc   icpad
         rep   #$20
         mx    %00
         rts

FillPage anop
         stz   frow
fcrow    stz   fcol
fccol    jsr   PlotCell
         inc   fcol
         lda   fcol
         cmp   #80
         bcc   fccol
         inc   frow
         lda   frow
         cmp   #50
         bcc   fcrow
         rts

PlotCell anop
         lda   frow
         asl   a
         asl   a
         asl   a
         asl   a
         sta   tmp               ; *16
         asl   a
         asl   a                 ; *64
         clc
         adc   tmp               ; *80
         clc
         adc   fcol
         sta   off

         lda   fstyle
         bne   pstar
         jsr   CreditChar
         bra   pwrch
pstar    jsr   StarChar
pwrch    pha
         lda   pgbase
         clc
         adc   off
         tax
         pla
         sep   #$20
         longa off
         sta   >$E00000,x
         rep   #$20
         longa on

         lda   fstyle
         bne   attrst
         lda   frow
         beq   attrsh
         cmp   #49
         beq   attrsh
         lda   fcol
         cmp   #8
         bcc   attrsh
         cmp   #72
         bcs   attrsh
         lda   #$000F
         bra   attrwr
attrsh   lda   #$0008
         bra   attrwr
attrst   lda   #$000E
attrwr   pha
         lda   pgbase
         clc
         adc   #$0FA0
         clc
         adc   off
         tax
         pla
         sep   #$20
         longa off
         sta   >$E00000,x
         rep   #$20
         longa on
         rts

CreditChar anop
         lda   frow
         beq   ccblk
         cmp   #49
         beq   ccblk
         lda   fcol
         cmp   #8
         bcc   ccblk
         cmp   #72
         bcs   ccblk
         lda   frow
         and   #7
         asl   a
         asl   a
         asl   a
         clc
         adc   fcol
         sec
         sbc   #8
         and   #$003F
         tax
         lda   msg,x
         and   #$00FF
         rts
ccblk    lda   frow
         clc
         adc   fcol
         and   #3
         tax
         lda   shades,x
         and   #$00FF
         rts

StarChar anop
         lda   frow
         asl   a
         clc
         adc   fcol
         adc   frow
         and   #$001F
         cmp   #2
         bcc   scstar
         lda   fcol
         adc   frow
         and   #3
         tax
         lda   shades,x
         and   #$00FF
         rts
scstar   lda   #'*'
         rts

         put   ss.s

quitpb   dw    0

ss_mode  ds    2
ss_flag  ds    2
fontidx  ds    2
ctrl_lo  ds    2
ctrl_hi  ds    2
ctrl_aux ds    2
statusbuf ds   16

pgbase   ds    2
frow     ds    2
fcol     ds    2
fstyle   ds    2
off      ds    2
tmp      ds    2
startln  ds    2
frameno  ds    2
pagef    ds    2
scdiv    ds    2
paldiv   ds    2
curx     ds    2
curdx    ds    2
bufaddr  ds    2
attraddr ds    2
hudbase  ds    2
savr     ds    1
savg     ds    1
savb     ds    1
hudbuf   ds    80

hudtmpl  asc   ' HOST TEXT   LINE=00  PAGE=A  HW-SCROLL + PAGE-FLIP + COLOR '
         ds    20

shades   hex   B0B1B2DB

msg      asc   'GSSQUARED HOST TEXT -- FAST SCROLL  PAGE-FLIP  16-COLOR ATTR'
         asc   'ANSI PLASMA IN HGR -- PRESS ANY KEY TO EXIT               '

ibmpal   hex   0000000000AA00AA0000AAAAAA0000AA00AAAA5500AAAAAA
         hex   5555555555FF55FF5555FFFFFF5555FF55FFFFFF55FFFFFF

         END
