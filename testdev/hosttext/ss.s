* Slot 3 Second Sight I/O (Host Text demo)
* Included inside HOSTTEXT START. 16-bit assembly; 8-bit A around I/O.

         mx    %00

WR_CMD    equ  $E0C0B0
WR_DATA   equ  $E0C0B1
RD_DATA   equ  $E0C0B2
HANDSHAKE equ  $E0C0B8

CMD_Status    equ $00
CMD_SetMode   equ $01
CMD_Caps      equ $10
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

* GetCapabilities: u16 length + records into capbuf.
GetCapabilities anop
         php
         sei
         lda   #CMD_Caps
         jsr   WriteCmd
         jsr   WaitHSOn
         sep   #$20
         longa off
         lda   >RD_DATA
         sta   capbuf
         sta   capleft
         lda   >RD_DATA
         sta   capbuf+1
         sta   capleft+1
         ldx   #2
gclp     lda   capleft
         ora   capleft+1
         beq   gcdone
         lda   >RD_DATA
         sta   capbuf,x
         inx
         lda   capleft
         bne   gcdec
         dec   capleft+1
gcdec    dec   capleft
         bra   gclp
gcdone   rep   #$20
         longa on
         jsr   WaitHSOff
         plp
         rts

* A = required mode. CLC if GSVGA, version>=$20, and mode is in the catalog.
CheckCard anop
         sta   wantmode
         jsr   GetStatus
         sep   #$20
         longa off
         lda   statusbuf
         cmp   #'G'
         bne   ssfail
         lda   statusbuf+1
         cmp   #'S'
         bne   ssfail
         lda   statusbuf+2
         cmp   #'V'
         bne   ssfail
         lda   statusbuf+3
         cmp   #'G'
         bne   ssfail
         lda   statusbuf+4
         cmp   #'A'
         bne   ssfail
         lda   statusbuf+6
         cmp   #$20
         bcc   ssfail
         rep   #$20
         longa on
         jsr   GetCapabilities
         lda   wantmode
         jsr   FindMode
         rts
ssfail   rep   #$20
         longa on
         sec
         rts

* A = mode. CLC if present in capbuf.
FindMode anop
         sep   #$20
         longa off
         sta   wantmode
         lda   capbuf
         sta   capleft
         lda   capbuf+1
         sta   capleft+1
         ldx   #2
fmloop   lda   capleft
         ora   capleft+1
         beq   fmno
         lda   capbuf,x
         sta   reclen
         inx
         lda   capbuf,x
         inx
         sta   captmp
         lda   capleft
         sec
         sbc   #2
         sta   capleft
         bcs   fm2
         dec   capleft+1
fm2      lda   capleft
         sec
         sbc   captmp
         sta   capleft
         bcs   fm3
         dec   capleft+1
fm3      lda   reclen
         cmp   wantmode
         beq   fmyes
         txa
         clc
         adc   captmp
         tax
         bra   fmloop
fmyes    rep   #$20
         longa on
         clc
         rts
fmno     rep   #$20
         longa on
         sec
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
