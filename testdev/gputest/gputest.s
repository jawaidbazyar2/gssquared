* GPUTEST — GS/OS S16 bounce demo for Second Sight GPU mode
* SetMode($5C,$03), three 128x128 RGB555 textures, CSB Clear/Draw/Present/End.

         rel
         dsk   GPUTEST
         typ   S16
         mx    %00
         xc
         xc

GPUTEST  START
         phk
         plb
         clc
         xce
         rep   #$30
         longa on
         longi on

         jsr   GetStatus

         lda   #$005C
         sta   ss_mode
         lda   #$0003
         sta   ss_flag
         jsr   SetMode
         jsr   ScreenOn
         jsr   GetGpuInfo

         jsr   FillSolid
         jsr   UploadTex
         sta   h1
         jsr   FillCheck
         jsr   UploadTex
         sta   h2
         jsr   FillGrad
         jsr   UploadTex
         sta   h3

         lda   h1
         sta   csb+6
         lda   h2
         sta   csb+13
         lda   h3
         sta   csb+20

         lda   #40
         sta   x0
         lda   #80
         sta   y0
         lda   #200
         sta   x1
         lda   #40
         sta   y1
         lda   #320
         sta   x2
         lda   #200
         sta   y2
         lda   #3
         sta   dx0
         lda   #2
         sta   dy0
         lda   #$FFFE            ; -2
         sta   dx1
         lda   #3
         sta   dy1
         lda   #4
         sta   dx2
         lda   #$FFFD            ; -3
         sta   dy2

         lda   #29
         sta   csblen
         stz   csblen+2

frame    jsr   PatchCSB
         jsr   ExecCmdBuf
         jsr   Bounce
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

quit     lda   h1
         jsr   FreeTex
         lda   h2
         jsr   FreeTex
         lda   h3
         jsr   FreeTex
         lda   #$0000
         sta   ss_mode
         sta   ss_flag
         jsr   SetMode
         jsl   $E100A8          ; GS/OS _Quit (does not return)
         dw    $2029
         adrl  quitpb

PatchCSB anop
         lda   x0
         sta   csb+8
         lda   y0
         sta   csb+10
         lda   x1
         sta   csb+15
         lda   y1
         sta   csb+17
         lda   x2
         sta   csb+22
         lda   y2
         sta   csb+24
         rts

* Load X/Y first so ADC is last before BncDo. LDX/LDY would otherwise
* clobber N from ADC; BMI would miss negatives and unsigned CMP bmax
* would snap to the opposite wall (wrap instead of bounce).
Bounce   anop
         lda   #512
         sta   bmax
         ldx   #x0
         ldy   #dx0
         lda   x0
         clc
         adc   dx0
         jsr   BncDo
         ldx   #x1
         ldy   #dx1
         lda   x1
         clc
         adc   dx1
         jsr   BncDo
         ldx   #x2
         ldy   #dx2
         lda   x2
         clc
         adc   dx2
         jsr   BncDo
         lda   #352
         sta   bmax
         ldx   #y0
         ldy   #dy0
         lda   y0
         clc
         adc   dy0
         jsr   BncDo
         ldx   #y1
         ldy   #dy1
         lda   y1
         clc
         adc   dy1
         jsr   BncDo
         ldx   #y2
         ldy   #dy2
         lda   y2
         clc
         adc   dy2
         jsr   BncDo
         rts

* A = new pos, X = &pos, Y = &vel, bmax = inclusive max
BncDo    bmi   bneg
         cmp   bmax
         beq   bok
         bcc   bok
         lda   bmax
         sta   |$0000,x
         lda   |$0000,y
         eor   #$FFFF
         inc   a
         sta   |$0000,y
         rts
bok      sta   |$0000,x
         rts
bneg     lda   #0
         sta   |$0000,x
         lda   |$0000,y
         eor   #$FFFF
         inc   a
         sta   |$0000,y
         rts

FillSolid anop
         ldy   #0
         lda   #$7C00            ; red RGB555
fs1      sta   texbuf,y
         iny
         iny
         cpy   #32768
         bne   fs1
         rts

FillCheck anop
         stz   py
fcy      stz   px
fcx      lda   px
         lsr
         lsr
         lsr
         sta   tmp
         lda   py
         lsr
         lsr
         lsr
         eor   tmp
         and   #1
         beq   fcblue
         lda   #$03E0            ; green
         bra   fcst
fcblue   lda   #$001F            ; blue
fcst     pha
         lda   py
         asl
         asl
         asl
         asl
         asl
         asl
         asl                     ; py*128
         clc
         adc   px
         asl
         tax
         pla
         sta   texbuf,x
         inc   px
         lda   px
         cmp   #128
         bcc   fcx
         inc   py
         lda   py
         cmp   #128
         bcc   fcy
         rts

FillGrad anop
         stz   py
fgy      stz   px
fgx      lda   px
         lsr
         lsr
         lsr
         and   #$001F
         asl
         asl
         asl
         asl
         asl
         sta   tmp               ; r<<5
         lda   py
         lsr
         lsr
         lsr
         and   #$001F
         ora   tmp
         asl
         asl
         asl
         asl
         asl                     ; (r<<10)|(g<<5)
         sta   tmp
         lda   px
         clc
         adc   py
         lsr
         lsr
         lsr
         lsr
         and   #$001F
         ora   tmp
         pha
         lda   py
         asl
         asl
         asl
         asl
         asl
         asl
         asl
         clc
         adc   px
         asl
         tax
         pla
         sta   texbuf,x
         inc   px
         lda   px
         cmp   #128
         bcc   fgx
         inc   py
         lda   py
         cmp   #128
         bcc   fgy
         rts

         put   ss.s

quitpb   dw    0                 ; pCount = 0

ss_mode  ds    2
ss_flag  ds    2
rethandle ds   2
h1       ds    2
h2       ds    2
h3       ds    2
x0       ds    2
y0       ds    2
x1       ds    2
y1       ds    2
x2       ds    2
y2       ds    2
dx0      ds    2
dy0      ds    2
dx1      ds    2
dy1      ds    2
dx2      ds    2
dy2      ds    2
px       ds    2
py       ds    2
tmp      ds    2
bmax     ds    2
csblen   ds    4
statusbuf ds   16
gpuinfo  ds    20

texhdr   dw    128,128
         db    $02,$00,$00,$00   ; RGB555, flags 0

* Clear $FF202040, three DrawTexture, Present VBL, End
* 29 bytes: Clear(5) + 3*DrawTexture(7) + Present(2) + End(1)
csb      hex   01402020FF040000000000000400000000000004000000000000020100

texbuf   ds    32768

         END
