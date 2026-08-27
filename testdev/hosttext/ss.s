* Slot 3 Second Sight I/O (Host Text demo)
* Included inside HOSTTEXT START. 16-bit assembly; 8-bit A around I/O.

         mx    %00

WR_CMD    equ  $E0C0B0
WR_DATA   equ  $E0C0B1
RD_DATA   equ  $E0C0B2
HANDSHAKE equ  $E0C0B8

CMD_Status    equ $00
CMD_SetMode   equ $01
CMD_ScreenOff equ $04
CMD_ScreenOn  equ $05
CMD_SetFont   equ $0F
CMD_SetTextCtrl equ $50

WriteCmd anop
         php
         sep   #$20
         longa off
         sta   >WR_CMD
         plp
         longa on
         rts

WriteData anop
         php
         sep   #$20
         longa off
         sta   >WR_DATA
         plp
         longa on
         rts

WaitHSOn anop
         sep   #$20
         longa off
         lda   #1
hson1    cmp   >HANDSHAKE
         bne   hson1
         rep   #$20
         longa on
         rts

WaitHSOff anop
         sep   #$20
         longa off
         lda   #0
hsoff1   cmp   >HANDSHAKE
         bne   hsoff1
         rep   #$20
         longa on
         rts

WaitHSDone anop
         sep   #$20
         longa off
hsd1     lda   >HANDSHAKE
         cmp   #$A5
         beq   hsdok
         cmp   #$A6
         bne   hsd1
hsdok    rep   #$20
         longa on
         rts

GetStatus anop
         php
         sei
         lda   #CMD_Status
         jsr   WriteCmd
         jsr   WaitHSOn
         sep   #$20
         longa off
         lda   >RD_DATA
         ldx   #0
gst1     lda   >RD_DATA
         sta   statusbuf,x
         inx
         cpx   #11
         bne   gst1
         rep   #$20
         longa on
         jsr   WaitHSOff
         plp
         rts

SetMode  anop
         php
         sei
         lda   #CMD_SetMode
         jsr   WriteCmd
         jsr   WaitHSOn
         lda   ss_mode
         jsr   WriteData
         lda   ss_flag
         jsr   WriteData
         jsr   WaitHSOff
         jsr   WaitHSDone
         plp
         rts

ScreenOn anop
         php
         sei
         lda   #CMD_ScreenOn
         jsr   WriteCmd
         plp
         rts

ScreenOff anop
         php
         sei
         lda   #CMD_ScreenOff
         jsr   WriteCmd
         plp
         rts

SetTextFont anop
         php
         sei
         lda   #CMD_SetFont
         jsr   WriteCmd
         jsr   WaitHSOn
         lda   fontidx
         jsr   WriteData
         jsr   WaitHSOff
         plp
         rts

SetTextCtrl anop
         php
         sei
         lda   #CMD_SetTextCtrl
         jsr   WriteCmd
         jsr   WaitHSOn
         lda   ctrl_lo
         jsr   WriteData
         lda   ctrl_hi
         jsr   WriteData
         lda   ctrl_aux
         jsr   WriteData
         jsr   WaitHSOff
         jsr   WaitHSDone
         plp
         rts
