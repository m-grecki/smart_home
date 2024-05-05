//===================================================================
//                                                                  =
// Program description - program sterownika inteligentnego domu     =
//                                                                  =
// Compiler options                                                 =
// Compiler version                                                 =
//                                                                  =
//===================================================================



// INCLUDES =========================================================
//#include <stdlib.h>
//#include <stdio.h>
//#include <setjmp.h>
//#include <string.h>
//#include <ctype.h>

#include <string.h>
#include <8052.h>       // Definitions of registers, SFRs and Bits
#if TEST
#include "../DS89Cxxx.h"  // DS89Cxxx specific definitions
#endif
#include "../ihmg.h"    // general definitions for the system
// END INCLUDES =====================================================

// DEFINES ==========================================================
#define DEV_ID 1
// END DEFINES ======================================================



/* **************************************************************** */
/* Function    : Subroutine_header()                            SUB */
/*------------------------------------------------------------------*/
/* Description :                                                    */
/*                                                                  */
/*------------------------------------------------------------------*/
/* Author      : Mariusz Grecki                                     */
/*------------------------------------------------------------------*/
/* Input       : none                                               */
/*------------------------------------------------------------------*/
/* Returnvalue : none                                               */
/*------------------------------------------------------------------*/
/* History     : mm/yy    V1.0 Basic routine                        */
/*                                                                  */
/* **************************************************************** */



// END Prototypes ===================================================


// Globals ==========================================================
#if DEBUG>0
const char * __code VERSION = "IHS_test V0.2 2024 by MG";  // Version control
#endif

volatile char r_fptr;   // currently received byte number
volatile char s_fptr;   // currently transmitted byte index
volatile char r_cnt;    // received frame length (for counting receiving frame bytes)
volatile char s_cnt;    // sending frame counter (for counting sending frame bytes)
volatile unsigned r_crc;  // crc of receiving frame
volatile unsigned s_crc;  // crc of transmitted frame
volatile unsigned char frm_nums;   // (confirmed frame number | sending frame number) two 4b frame counters
volatile unsigned tick_cnt;   // counter for 1s tick (4 MS bits are unused)
volatile char s_shift;    // normally set to 0x00 and then it is ignored, otherwise set to COMM_SESC or COMM_SSHIFT and is sent as a second (shifted) byte 

//volatile __bit ser_idle;        // serial port idle
//volatile __bit ser1_idle;       // serial port idle
volatile __bit frame_received;  // received new frame
volatile __bit in_frame;        // during frame reception
volatile __bit was_shift;       // data byte "slshift" active (COMM_BSHIFT received in previous byte)
volatile __bit my_frame;        // mark, the frame is addressed for me
volatile __bit send_error;      // set, when sending error detected (e.g. line is busy)
//volatile __bit sending;
//__sbit __at (0xB5) sending ;    // transmitter control and "sending" flag, for "production version" 0xB7 (P3.7)
//__sbit __at (0xB3) ow_bus;      // ow_transmission and reception bus, for "production version" 0xB7 (P3.7)
//volatile __bit sent_shift;    // data byte "shift" active (COMM_BSHIFT sent in previous byte)
volatile __bit s_crc_h;         // transmitting crc_h
volatile __bit s_crc_l;         // transmitting crc_l


// time - 32b
volatile unsigned long time;    // 32 bits for keeping the current time (adjusted in timer interrupt)
unsigned alarm;                 // 16 bits for storing of the alarm time (alarm bit is set when time>alarm_time)
// sec - 6b   time&0x3f
// min - 6b   (time>>6)&0x3f      
// hour - 5b  (time>>12)&0x1f
// day - 5b   (time>>17)&0x1f
// month - 4b (time>>22)&0x0f
// year - 6b (number of years from 2022 - range 2022-2085)  time>>26
volatile __bit newalarm; // alarm time passed by
volatile __bit newsec;
volatile __bit newmin;
volatile __bit newhour;
volatile __bit newday;
volatile __bit newmonth;
volatile __bit newyear; // markers for time change

//char dbg_buf[16]; // debug characters bufferCLOCK_FREQ
char r_buf[16];   // receiving frame buffer
char f_buf[16];   // received frame buffer
char s_buf[16];   // send(ing) frame buffer
//unsigned char dowcrc;
//char ow_serial[8]; // one-wire serial number and general purpose buffer
volatile char s_cache; // next byte to send
volatile char s_verify1; // previous (-1) byte to verify
volatile char s_verify2; // previous (-2) byte to verify
volatile char r_verify; // read byte to verify

__data struct comm_frame * __code const r_frame=(__data struct comm_frame *)&f_buf; // pointer in code memory pointing object in data memory
__data struct comm_frame * __code const s_frame=(__data struct comm_frame *)&s_buf; // pointer in code memory pointing object in data memory
__code char days_in_months[]={0,31,28,31,30,31,30,31,31,30,31,30,31};

// parameters (at fixed locations)
//__data unsigned __at (0x18) time_1s;  // counting value for 1s (does not make sense, even with correction the time diverges quickly...)

//char spi_ptr;
//char spo_head;
//char spo_tail;
// globals to save memory space
//unsigned h;
signed char i;   // general purpose 8bit int (used mostly as counter)
unsigned h;  // general purpose 16b unsigned

// debugging
#if DEBUG>0
__data unsigned __at (0x84) dptr1;
const char * __code dbg_ser1intR = "s1iR";  // Version control
const char * __code dbg_ser1intT = "s1iT";  // Version control
const char * __code dbg_cmp = "cmp:";  // Version control
const char * __code dbg_owa = "owa:";  // one wire addres
__xdata char __at (0xf000) disp;
#endif

//};
// END Globals ======================================================





// sdcc_external_startup ============================================
signed char _sdcc_external_startup()
{
// chip configuration:
// serial port: 19200b/s by timer 1 in mode 2, interrupts
// timer 0 in mode 3:
// timer 0 H - counting time
// timer 0 L - trigger timeouts (control by TR0)
// 
PCON |= 0x80;  // double speed UART0
TH1 = 255;     // baud rate: 232:1200, 244:2400, 250:4800, 253:9600, 253+double:19200, 255:28800, 255+double:57600 
TMOD = 0x23;   // timer 1: count clocks, 8 bits, autoreload, timer 0: mode 3, count clocks, 2x 8 bits timers
TCON = 0x55;   // timer 0 running, timer 1 running, external interrupts IT0,IT1 edge triggered
SCON = 0x5e;   // UART0: mode 1, 10 bits, timer 1 generates baud rate
//TR1 = 1;       // timer 1 enabled
//TR0 = 1;       // timer 0 disabled

// for DS89C430 for debugging
#if DEBUG>0
SCON1 = 0x5c; // UART1: mode 1, 10 bits, timer 1 generates baud rate
SMOD_1 = 1;    // double speed UART1
PMR|=1; // for turning on xram memory (0-3ffh)
DPS=0;
dptr1=0x9000;
#endif

ES=1;          // interrupt serial port 0
//ES1=1;         // interrupt serial port 1
ET0=1;
ET1=1;

return 1;
}


// External0-ISR (INT0) =============================================
#if 0
void External0_ISR(void) __interrupt (0) __using (0) __naked
{
// Safety-routine
   sendFrame(0,256,'@');
   while(1);
}
#endif
// END External0-ISR ================================================

/********************************************************************/
/* Function    : ISR_header()                                   ISR */
/*------------------------------------------------------------------*/
/* Description :                                                    */
/*                                                                  */
/*------------------------------------------------------------------*/
/* Author      : Mariusz Grecki                                     */
/*------------------------------------------------------------------*/
/* History     :                                                    */
/*                                                                  */
/********************************************************************/
// Timer0-ISR =======================================================
//#pragma EXCLUDE r0,r1,r2,r3,r4,r5,r6,r7
#if 1
void Timer0_ISR(void) __interrupt (1) __using (0) __naked
{
INT_MARK_ON
// Safety-routine
__asm
      jnb   TRANS_DIR,1$
      push  acc
      push  psw
; sending, checks what was recently send and received
.if gt DEBUG 2
          push  dpl
          push  dph
          push  ar0
          push  ar1
          mov   dptr,#_dbg_cmp
          acall _get_code_ptr
          acall _puts
          mov   dpl,_s_verify2
          acall _hex
          mov   r1,dpl
          mov   dpl,dph
          acall _putchar
          mov   dpl,r1
          acall _putchar
          mov   dpl,#' ' ; 5$?
;in_frame=0;
          acall _putchar
          mov   dpl,_r_verify
          acall _hex
          mov   r1,dpl
          mov   dpl,dph
          acall _putchar
          mov   dpl,r1
          acall _putchar
          mov   dpl,#' '
          acall _putchar
          pop   ar1
          pop   ar0
          pop   dph
          pop   dpl0x0e
.endif
      mov   a,_r_verify
      cjne  a,_s_verify2,2$
      sjmp  3$
2$:   clr   TRANS_DIR          ; disable transmitter
      setb  _send_error
3$:   pop   psw
      pop   acc
1$:   clr   TR0
__endasm;
INT_MARK_OFF
__asm
      reti
__endasm;
}
// END Timer0-ISR ===================================================
//#pragma EXCLUDE NONE
#endif

// External1-ISR (INT1) =============================================
#if 0
void External1_ISR(void) __interrupt 2 __using 0 __naked
{
}
#endif
// END External1-ISR ================================================

void Timer1_ISR(void)  __interrupt (3) __using (0) __naked
{
INT_MARK_ON
__asm
              push  psw
              push  acc
              push  ar7
              push  ar6
              push  ar0
; count to 7200/3600 (0x1c20/0xe10) (22.118/11.0592 ticks divided by 12*256) gives 1s
              mov   a,_tick_cnt
              mov   r6,a
              mov   r7,_tick_cnt+1
              jb    _newalarm,20$       ; skip, if alarm still active
              cjne  a,_alarm,20$        ; no alarm if differs
              mov   r0,#0x1f
              mov   a,r7
              anl   a,r0
              xch   a,r0
              anl   a,_alarm+1
              cjne  a,ar0,20$           ; no alarm if differs
; alarm? check the 3 bits MSB
              mov   a,_alarm+1          
              anl   a,#0xe0
;              jz    21$                 ; check, if still some seconds to wait, if 0 then alarm!
              clr   c
              subb  a,#20
              mov   _alarm+1,a
              mov   _alarm,#0
              jnc   20$               ; no alarm
; alarm!
              setb  _newalarm
20$:          mov   a,r6
              inc   a
              mov   r6,a
              jnz   3$
;              swap  a                 ; to save 4 MSB bits
;              add   a,#0x10
;              swap  a
              inc   r7
3$:           mov   a,r6
              clr   c
              subb  a,#<CLOCK_1SEC
              mov   a,r7
              anl   a,#0x1f
              subb  a,#>CLOCK_1SEC       ; subbtract 7200/3600 from ticks
              jnc   12$
              mov   _tick_cnt,r6
              mov   _tick_cnt+1,r7     ; tick_cnt<3600, save and continue
              ajmp  1$
12$:          mov   a,r7
              anl   a,#0xe0
              mov   _tick_cnt+1,a
              clr   a
              mov   _tick_cnt,a         ; tick_cnt=3600, save 0 and increase time
              mov   r6,a        ; 5$?
; add 1 second to the time variable (32b)
              mov   r0,#_time
              inc   @r0
              setb  _newsec
              mov   a,@r0
              anl   a,#0x3f ; acall _seconds
              clr   c
              subb  a,#60
              jc    13$      ; below 60 seconds, OK (jump to next test and jump since not enough long is the oryginal jump)
              mov   a,@r0
              anl   a,#0xc0 ; set 0 seconds
              add   a,#0x40 ; increase minutes
              mov   @r0,a
              inc   r0
              mov   a,@r0
              addc  a,r6
              mov   @r0,a
              setb  _newmin
              acall _minutes
              clr   c
              subb  a,#60
13$:          jc    11$      ; below 60 minutes, OK
;              dec   r0
;              mov   a,r0
;              anl   a,#0x3f ; set 2 minutes lower bits to 0
;              mov   @r0,a
;              inc   r0
              anl   _time,#0x3f
              mov   a,@r0
              anl   a,#0xf0 ; set 4 uppper minutes bits to 0
              add   a,#0x10 ; increase hours
              mov   @r0,a
              inc   r0
              mov   a,@r0
              addc  a,r6
              mov   @r0,a
              setb  _newhour
              acall _hours
              clr   c
              subb  a,#24
              jc    11$      ; below 24 hours, OK
;              dec   r0
;              mov   a,@r0
;              anl   a,#0x0f ; set 4 lower hours bits to 0
;              mov   @r0,a
;              inc   r0
              anl   _time+1,#0x0f
              mov   a,@r0
              anl   a,#0xfe ; set 1 uppper hours bits to 0
              add   a,#0x02 ; increase days
              mov   @r0,a
              setb  _newday
              acall _months
              cjne  a,#2,2$
              acall _years
              add   a,#22   ; year+2022 (C is 0 after this addition)
              anl   a,#0x3
              jnz   2$      ; no leap year
              setb  c       ; leap year
              mov   a,#2
2$:           push  dpl
              push  dph
              mov   dptr,#_days_in_months
              movc  a,@a+dptr
              pop   dph
              pop   dpl
              addc  a,#0    ; correction for February leap year (if C is set)
              mov   r7,a
              acall _days
              setb  c       ; days counts 1-30(31,28,29)
              subb  a,r7
              jc    11$      ; still the same month and year...
              mov   a,@r0
              anl   a,#0xc1
              add   a,#0x42 ; set 1st day of the month and increase month
              mov   @r0,a
              inc   r0
              mov   a,@r0
              addc  a,#0
              mov   @r0,a
              setb  _newmonth
              acall _months
              clr   c
              subb  a,#13
              jnc   11$      ; below 13 month, OK
              anl   _time+2,#0x3f
              orl   _time+2,#0x40
              mov   a,@r0
              anl   a,#0xfc
              add   a,#0x04 ; set month to 1 and increase year
              setb  _newyear
; czas zaktualizowany   
11$:
1$:           pop   ar0
              pop   ar6
              pop   ar7
              pop   acc
              pop   psw
__endasm;
INT_MARK_OFF
__asm
      reti
__endasm;
}
// END Timer1-ISR ===================================================


// Serial-ISR =======================================================
void Serial_0_ISR(void) __interrupt (4) __using (0) __naked
{
INT_MARK_ON
__asm
          push  psw
          push  acc
          push  ar0
          push  ar1
          push  dpl
          push  dph
          push  b

100$:     jbc   TRANS_TI,101$           ; check and reset "transmitter ready" bit
;===============================================================
          ajmp  2$                  ; goto check reception
105$:     setb  _s_crc_l
          mov   _s_verify2,_s_verify1
          mov   _s_verify1,_s_cache
          ajmp  2$
108$:     clr   TRANS_DIR
          clr   _s_crc_l
          clr   _s_crc_h
          mov   TL0,#256-50*CLOCK_MULT             ; is it worth to test last byte? If error what to do anyway? 
          clr   TF0
          setb  TR0                 ; set timer 0 for sending/reception comparison
          ajmp  2$
101$:     jnb   TRANS_DIR,2$         ; not sending, ignore interrupt
          jb    _s_crc_l,108$
          mov   TRANS_SBUF,_s_cache     ; send prepared byte
          mov   a,_s_cache
          cjne  a,#COMM_BESC,22$
          mov   _s_verify1,a
          mov   a,#7                ; header + crc
          add   a,_s_buf+4          ; + payload
          mov   _s_cnt,a            ; set bytes counter to the frame length
          clr   a                   ; just sending frame marker, so the transmission begins
          mov   _s_fptr,a           ; set pointer
          mov   _s_crc,a
          mov   _s_crc+1,a          ; initialize crc to zero
          mov   _s_shift,a          ; clear "shift" flag 
          clr   _send_error
          sjmp  109$
22$:      mov   TL0,#256-50*CLOCK_MULT
          clr   TF0
          setb  TR0                 ; set timer 0 for sending/reception comparison
109$:     clr   a
          xch   a,_s_shift          ; clear "shift" flag, send shifted byte if needed
          jnz   104$                ; jump if shifted byte is to be send
          jb    _s_crc_h,105$
          mov   a,_s_cnt
          clr   c
          subb  a,#3
          jnc   102$
          add   a,#_s_crc+2         ; points to crc
          setb  c
          ajmp  103$
102$:     mov   a,_s_fptr           ; index of the byte to send 
          add   a,#_s_buf
          inc   _s_fptr             ; update data pointer
103$:     mov   r0,a
          mov   a,@r0               ; get byte to send
          mov   r0,a
          jc    107$
          mov   dpl,_s_crc
          mov   dph,_s_crc+1
          push  ar0
          acall _crc16_ccitt        ; crc
          pop   ar0
          mov   _s_crc,dpl
          mov   _s_crc+1,dph
107$:     djnz  _s_cnt,106$
          setb  _s_crc_h
          nop
;          ajmp  2$                  ; all prepared and sent, just do finishing
106$:     mov   a,r0
          cjne  a,#COMM_BESC,21$
          mov   a,#COMM_SESC        ; send COMM_BSHIFT and then COMM_SESC
          sjmp  26$
21$:      cjne  a,#COMM_BSHIFT,104$
          mov   a,#COMM_SSHIFT      ; send COMM_BSHIFT and COMM_SSHIFT
26$:      mov   _s_shift,a
          mov   a,#COMM_BSHIFT

104$:     mov   _s_verify2,_s_verify1
          mov   _s_verify1,_s_cache
          mov   _s_cache,a

          sjmp  2$

; *************************************************************

2$:       jbc  TRANS_RI,12$
44$:      ajmp  4$
12$:
          mov   a,TRANS_SBUF
          mov   _r_verify,a
          jb    TRANS_DIR,44$
13$:      mov   r0,a
.if gt DEBUG 3
          push  ar0
          mov   dptr,#_dbg_ser1intR
          acall _get_code_ptr
          acall _puts
          pop   ar0
          mov   a,r0
          mov   dpl,a
          acall _hex
          mov   r1,dpl
          mov   dpl,dph
          acall _putchar
          mov   dpl,r1
          acall _putchar
          mov    dpl,#' '
          acall _putchar
.endif
          cjne  r0,#COMM_BESC,3$  ; is start frame?
; frame start
          clr   a
          mov   _r_fptr,a    ; f_len=0
          mov   _r_cnt,#10    ; initial reset frame payload bytes counter (count backwards!), will be set after frame length is received
          mov   _r_crc,a
          mov   _r_crc+1,a    ; crc initialized
          setb  _in_frame
          setb  _my_frame
          clr   _was_shift
          ajmp  4$
3$:       jnb   _in_frame,4$; byte reveived outside frame - ignore
          jbc   _was_shift,6$ ; is shift enabled?
          cjne  r0,#COMM_BSHIFT,5$  ; is shift byte?
          setb  _was_shift
          ajmp  4$
; wrong shifted byte, error
9$:       clr   _in_frame   ; cancel frame reception
          ajmp  4$          ; ignore received data
6$:       mov   a,r0
          xrl   a,#COMM_SESC
          jz    7$          ; shifted COMM_BESC received?
.if eq (COMM_SESC ^ COMM_SSHIFT) 0
.error!
.endif
          inc   a
          jnz   9$
          mov   r0,#COMM_BSHIFT
          sjmp  5$
7$:       mov   r0,#COMM_BESC
; store received byte
5$:       mov   dpl,_r_crc
          mov   dph,_r_crc+1
          push  ar0
          acall _crc16_ccitt
          pop   ar0
          mov   _r_crc,dpl
          mov   _r_crc+1,dph
          mov   r1,#_r_fptr
          jnb   _my_frame,10$ ; if frame not addressed to me, skip data storage
          mov   a,_r_cnt
          setb  c
          subb  a,#2        ; compare, if r_cnt is 2 or less (do not store crc!)
          jc    10$
          mov   a,@r1       ; f_len
          add   a,#_r_buf    ; location in buffer
          xch   a,r0        ; a - received data, r0 - pointer to buffer location
          mov   @r0,a
          mov   r0,a        ; received data
10$:      mov   a,@r1
          inc   a
          xch   a,@r1
          jnz   8$          ; not the first byte? (destination)
; first byte - destination
; frame (bytes): COMM_BESC, dest, source, (frm_conf<<4) | frm_no & 0x0f, frm_type, frm_length, payload[0-254], crc16
          mov   a,#DEV_ID
          xrl   a,r0
          jz    4$            ; is the frame addressed to me?
          mov   a,#0xff
          xrl   a,r0
          jz    4$            ; is the frame broadcast?
          clr   _my_frame     ; not my frame
          ajmp  4$
8$:       xrl   a,#4
          jnz   11$           ; frame length received?
          inc   r0
          inc   r0            ; include the payload and crc
          mov   _r_cnt,r0     ; bytes counter (2-256)
4$:       mov   a,TRANS_SCON
          anl   a,#3
          jz    14$
          ajmp  100$
; end of frame received, including crc!
11$:      djnz   _r_cnt,4$
; all bytes received (including crc)
          clr   _in_frame
          mov   a,_r_crc
          orl   a,_r_crc+1
          jnz   4$            ; crc not 0?, ignore frame
; frame received correctly
          mov   r0,#_r_buf
          mov   r1,#_f_buf
          push  ar7
          mov   r7,#16
15$:      mov   a,@r0
          mov   @r1,a
          inc   r0
          inc   r1
          djnz  r7,15$
          pop   ar7
          setb  _frame_received
          sjmp  4$
14$:      pop   b
          pop   dph
          pop   dpl
          pop   ar1
          pop   ar0
          pop   acc
          pop   psw
__endasm;
INT_MARK_OFF
__asm
      reti
__endasm;
}
// END Serial-ISR ===================================================


#if 0
// Timer2-ISR =======================================================
void Timer2_ISR(void) interrupt 5
{
// Safety-routine
_asm
  nop
_endasm;

return;
}
// END Timer2-ISR ===================================================
#endif




// sec - 6b   time&0x3f           time_0&0x3f
// min - 6b   (time>>6)&0x3f      
// hour - 5b  (time>>12)&0x1f
// day - 5b   (time>>17)&0x1f
// month - 4b (time>>22)&0x0f
// year - 6b  time>>26
unsigned seconds() __naked
{
__asm
          mov   a,_time
          anl   a,#0x3f
          ret
__endasm;
}
unsigned minutes() __naked
{
__asm
          push  ar0
          mov   a,_time
          mov   r0,_time+1
          rlc   a
          xch   a,r0
          rlc   a
          xch   a,r0
          rlc   a
          xch   a,r0
          rlc   a
          anl   a,#0x3f
          pop   ar0
          ret
__endasm;
}

unsigned hours() __naked
{
__asm
          mov   a,_time+2
          mov   c,acc.0
          mov   a,_time+1
          swap  a
          mov   acc.4,c
          anl   a,#0x1f
          ret
__endasm;
}

unsigned days() __naked
{
__asm
          mov   a,_time+2
          rr    a
          anl   a,#0x1f
          ret
__endasm;
}

unsigned months() __naked
{
__asm
          push  ar0
          mov   r0,_time+3
          mov   a,_time+2
          rlc   a
          xch   a,r0
          rlc   a
          xch   a,r0
          rlc   a
          xch   a,r0
          rlc   a
          anl   a,#0x0f
          pop   ar0
          ret
__endasm;
}

unsigned years() __naked
{
__asm
          mov   a,_time+3
          rr    a
          rr    a
          anl   a,#0x3f
          ret
__endasm;
}


// ***********************************************************************************
#pragma save
#pragma disable_warning 85
#pragma nooverlay
//unsigned crc16_ccitt(unsigned crc, unsigned char data) __naked
unsigned crc16_ccitt(unsigned crc) __naked
{
__asm
; inputs: dptr: crc
;         r0: data
; output: dptr: crc
        mov   a,r0
        xrl   a,dph
        mov   dph,a   ; x = data ^ (crc >> 8);
        swap  a
        anl   a,#0x0f
        xrl   a,dph   ; x ^= (x >> 4);
        swap  a
        mov   r0,a
        anl   a,#0xf0
        xrl   a,dpl   ; (crc & 255) ^ (x << 4)
        mov   dpl,a
        mov   a,r0
        rl    a
        anl   a,#0x1f ; (x >> 3)
        xrl   a,dpl   ; (crc & 255) ^ (x >> 3) ^ (x << 4)
        mov   dph,a
        mov   a,r0
        rl    a
        anl   a,#0xe0 ; (x << 5)
        xch   a,r0
        swap  a       ; x
        xrl   a,r0
        mov   dpl,a
        ret
__endasm;
//unsigned char msb = crc >> 8;
//unsigned char lsb = crc & 255;

//unsigned char x = a ^ msb;
//x ^= (x >> 4);
//msb = (lsb ^ (x >> 3) ^ (x << 4)) & 255;
//lsb = (x ^ (x << 5)) & 255;

//return (msb << 8) + lsb;
}
#pragma restore


#if 1
void div16_16(void)
  {
// R1/R0 will be divided by R3/R2 (high/low), on output R3/R2 - quotient of division, R1/R0 - reminder
__asm
  CLR C       ;Clear carry initially
  MOV R4,#0x00 ;Clear R4 working variable initially
  MOV R5,#0x00 ;CLear R5 working variable initially
  MOV B,#0x00  ;Clear B since B will count the number of left-shifted bits
1$:
  INC B      ;Increment counter for each left shift
  MOV A,R2   ;Move the current divisor low byte into the accumulator
  RLC A      ;Shift low-byte left, rotate through carry to apply highest bit to high-byte
  MOV R2,A   ;Save the updated divisor low-byte
  MOV A,R3   ;Move the current divisor high byte into the accumulator
  RLC A      ;Shift high-byte left high, rotating in carry from low-byte
  MOV R3,A   ;Save the updated divisor high-byte
  JNC 1$   ;Repeat until carry flag is set from high-byte
2$:        ;Shift right the divisor
  MOV A,R3   ;Move high-byte of divisor into accumulator
  RRC A      ;Rotate high-byte of divisor right and into carry
  MOV R3,A   ;Save updated value of high-byte of divisor
  MOV A,R2   ;Move low-byte of divisor into accumulator
  RRC A      ;Rotate low-byte of divisor right, with carry from high-byte
  MOV R2,A   ;Save updated value of low-byte of divisor
  CLR C      ;Clear carry, we do not need it anymore
  MOV ar7,R1 ;Make a safe copy of the dividend high-byte
  MOV ar6,R0 ;Make a safe copy of the dividend low-byte
  MOV A,R0   ;Move low-byte of dividend into accumulator
  SUBB A,R2  ;Dividend - shifted divisor = result bit (no factor, only 0 or 1)
  MOV R0,A   ;Save updated dividend 
  MOV A,R1   ;Move high-byte of dividend into accumulator
  SUBB A,R3  ;Subtract high-byte of divisor (all together 16-bit substraction)
  MOV R1,A   ;Save updated high-byte back in high-byte of divisor
  JNC 3$   ;If carry flag is NOT set, result is 1
  MOV R1,ar7 ;Otherwise result is 0, save copy of divisor to undo subtraction
  MOV R0,ar6
3$:
  CPL C      ;Invert carry, so it can be directly copied into result
  MOV A,R4 
  RLC A      ;Shift carry flag into temporary result
  MOV R4,A   
  MOV A,R5
  RLC A
  MOV R5,A		
  DJNZ B,2$ ;Now count backwards and repeat until "B" is zero
  MOV R3,ar5  ;Move result to R3/R2
  MOV R2,ar4  ;Move result to R3/R2
__endasm;
}
#endif


// ***********************************************************************************
#pragma save
#pragma disable_warning 85
void set_alarm(unsigned a)
{
__asm
      push  dpl
      push  dph
__endasm;
#if 0
i=a/1000;
h=(a % 1000)*CLOCK_1SEC/1000;
#endif
__asm
      pop   ar1     ;dph
      pop   ar0     ;dpl
      mov   r0,dpl
      mov   r1,dph
      mov   r2,#<1000
      mov   r3,#>1000
      acall _div16_16
      mov   _i,r2
      mov   r7,#CLOCK_1SEC/100
      mov   a,r7
      mov   b,r0
      mul   ab
      mov   r2,a
      mov   r3,b
      mov   a,r7
      mov   b,r1
      mul   ab
      add   a,r3
      mov   r3,a
      mov   a,#0
      addc  a,b
      rrc   a
      mov   a,r3
      rrc   a
      mov   r1,a
      mov   a,r2
      rrc   a
      mov   r0,a
      mov   r2,#5
      mov   r3,#0
      acall _div16_16
      mov   _h,r2
      mov   _h+1,r3

;      mov   c,EA
;      mov   f0,c
      clr   EA
.if 1
      mov   a,_tick_cnt
      add   a,_h
      mov   r6,a
      mov   a,_tick_cnt+1
      addc  a,_h+1
      mov   r7,a
      clr   c           ; c is rather cleared here...
      mov   a,r6
      subb  a,#<CLOCK_1SEC
      mov   r4,a
      mov   a,r7
      subb  a,#>CLOCK_1SEC     ; 7200/3600
      mov   r5,a
      jc    1$
      mov   a,r4
      mov   r6,a
      mov   a,r5
      mov   r7,a
      inc   _i
1$:   mov   _alarm,r6
      mov   a,_i
      swap  a
      rl    a
      anl   a,#0xe0
      orl   a,r7
      mov   _alarm+1,a
.endif
      clr   _newalarm
;      mov   c,f0
;      mov   EA,c
      setb  EA
__endasm;
}
#pragma restore


#if DEBUG>1

void get_code_ptr() __naked
{
__asm
  clr	a
  movc	a,@a+dptr
  mov   r0,a
  mov	  a,#0x01
  movc	a,@a+dptr
  mov	  r1,a
  mov	  a,#0x02
  movc	a,@a+dptr
  mov   b,a
  mov   dpl,r0
  mov   dph,r1
  ret
__endasm;
}

#pragma save
#pragma disable_warning 85
#if 1
void putchar(char a) __naked
{
__asm
      mov   a,_DPH1
      subb  a,#0xf0
      jnc   1$
      mov   c,EA
      clr   EA
      mov   a,dpl
      mov   _DPS,#0x11
      movx  @dptr,a
      mov   _DPS,#0
      mov   a,dpl
      mov   EA,c
;     ljmp  #0x267      ; just use monitor function (no interrupts, no buffer)
1$:   ret
__endasm;
}

#else

void putchar(char a) __naked
{
__asm
        mov   a,dpl
        ljmp  #0x267      ; just use monitor function (no interrupts, no buffer)
.if 0
        push  ar1
        mov   r1,#_dbg_ptrs
        mov   a,@r1       ; head | tail
        swap  a           ; tail | head
        inc   a           ; tail | head+1
        anl   a,#0x0f     ; head+1
        mov   dph,a       ; head+1
1$:
        mov   a,@r1       ; head | tail
        anl   a,#0x0f     ; tail
        cjne  a,dph,2$    ; wait  until there is a free space in the buffer
        ajmp  1$
2$:
        mov   a,dph
        dec   acc         ; head (b3-b0)
        anl   a,#0x0f     ; head       
        add   a,#_dbg_buf ; ptr in dbg_buf
        xch   a,r1
        mov   @r1,dpl     ; store data in buf
        xch   a,r1
; stop interrupts for atomic index adjustments
;        mov   c,ea
;        mov   f0,c
;        clr   ea
        mov   a,@r1
        swap  a
        anl   a,#0xf0
        orl   a,dph
        swap  a
        mov   @r1,acc
;        mov   c,f0
;        mov   ea,c
        mov   c,_ser_idle
        mov   TI,c
        pop   ar1
        ret
.endif
__endasm;
}
#endif
#pragma restore



#pragma save
#pragma disable_warning 59
unsigned hex(char a)
{
__asm
          mov   a,dpl
          swap  a
          anl   a,#0x0f
          add   a,#0x90
          da    a
          addc  a,#0x40
          da    a
          mov   dph,a
          mov   a,dpl
          anl   a,#0x0f
          add   a,#0x90
          da    a
          addc  a,#0x40
          da    a
          mov   dpl,a
          ret
__endasm;
}

#pragma restore



char getc()
{
__asm
  acall   #0x236
  mov     dpl,a
  mov     dph,#0
__endasm;
}

void puts(char *ptr) __naked
{
__asm
2$:     acall	__gptrget
        jz    1$
        push  dpl
        mov   dpl,a
        acall _putchar
        pop   dpl
        inc   dptr
        sjmp  2$    
1$:     ret
__endasm;
//RS0=1;
//for(;*ptr;ptr++)
//  putchar(*ptr);
//RS0=0;
}

void sendInfo()
{
   puts("\r\n");
   puts("**********************************\r");
   puts("**     MGHN TEST_DEV V 0.1      **\r");
   puts("**     (C) M.Grecki, 2023       **\r");
   puts("**********************************\r\n\n");
}

void print_frame(struct comm_frame *f)
{
//unsigned h;

puts("\n\n\rreceiver:");
h=hex(f->frm_receiver);
putchar(h>>8);
putchar(h&0xff);
puts("\n\rsender:");
h=hex(f->frm_sender);
putchar(h>>8);
putchar(h&0xff);

//f_nums&=0x0f;
//f_nums|=(f->frm_numbers<<4)&0xf0;

puts("\n\rfrm_conf:");
h=hex(f->frm_numbers>>4);
putchar(h&0xff);
puts("\n\rfrm_no:");
h=hex(f->frm_numbers&0x0f);
putchar(h&0xff);
puts("\n\rtype:");
h=hex(f->frm.frm_type);
putchar(h>>8);
putchar(h&0xff);
puts("\n\rlength:");
h=hex(f->frm.frm_length);
putchar(h>>8);
putchar(h&0xff);
puts("\n\rpayload:");
for(i=0; i<(int)f->frm.frm_length;i++)
  {
  h=hex(f->frm.frm_data[i]);
  putchar(h>>8);
  putchar(h&0xff);
  putchar(' ');
  }
puts("\n\r");
}

#endif  // DEBUG>0


void sendFrame()
{
char __data *s;
//unsigned h;

  i=frm_nums+1;
  frm_nums&=0xf0;
  frm_nums|=i&0x0f;           // increase frm_no

  s_frame->frm_receiver=r_frame->frm_sender;
  s_frame->frm_sender=DEV_ID;
  s_frame->frm_numbers=frm_nums;

#if DEBUG>0
  puts("\n\rframe to send: 5A ");
  s=(char*)s_frame;
//      crc=0;
  for(i=0; i<5+(int)s_frame->frm.frm_length;i++)
    {
    h=hex(*s++);
    putchar(h>>8);
    putchar(h&0xff);
    putchar(' ');
    }
  puts("+crc\n\r");

  print_frame(s_frame);
#endif


// sends frame from s_frame
__asm
1$:         jb    TRANS_DIR,1$
            setb  TRANS_DIR
            nop
            nop
            mov   _s_cache,#COMM_BESC
            setb  TRANS_TI
            ret
__endasm;
}

void errorFrame(char err)
{
s_frame->frm.frm_type=0xff;   
s_frame->frm.frm_data[0]=err;       // error code
s_frame->frm.frm_length=1;
}

#if 0
unsigned char __code dscrc_table[] = {
0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
157,195, 33,127,252,162, 64, 30, 95, 1,227,189, 62, 96,130,220,
35,125,159,193, 66, 28,254,160,225,191, 93, 3,128,222, 60, 98,
190,224, 2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89, 7,
219,133,103, 57,186,228, 6, 88, 25, 71,165,251,120, 38,196,154,
101, 59,217,135, 4, 90,184,230,167,249, 27, 69,198,152,122, 36,
248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91, 5,231,185,
140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
202,148,118, 40,171,245, 23, 73, 8, 86,180,234,105, 55,213,139,
87, 9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

#pragma save
#pragma disable_warning 59
unsigned char ow_crc(unsigned char x)
{

__asm
;        mov   a,dpl
        xrl   a,r2
        mov   dptr,#_dscrc_table
        movc  a,@a+dptr
        mov   r2,a
;        mov   dpl,a
__endasm;
}
#pragma restore
char byte_crc(char crc, char byte)
{
//  int i;
  char b;
  for (i = 0; i < 8; i++)
  {
    b = crc ^ byte;
    crc >>= 1;
    if (b & 0x01)
      crc ^= 0x8c;
    byte >>= 1;
  }
  return crc;
}

#endif

#if 1
#pragma save
#pragma disable_warning 59
unsigned char ow_crc(unsigned char x){
__asm
        push  ar7
        push  ar6
        mov   r7,#8
        mov   r6,a
2$:     mov   a,r6
        xrl   a,r2
        xch   a,r2
        clr   c
        rrc   a
        xch   a,r2
        xch   a,r6
;        clr   c
        rrc   a
        xch   a,r6
        jnb   acc.0,1$
        mov   a,#0x8c
        xrl   a,r2
        mov   r2,a
1$:     djnz  r7,2$
        pop   ar6
        pop   ar7
__endasm;
}
#pragma restore

#else

unsigned char ow_crc(unsigned char x)
{
__asm
        push  ar7
        mov   r7,#8
2$:     xrl   a,r2
        clr   c
        rlc   a
        jnc   1$
        xrl   a,#0x31
1$:     djnz  r2,2$
        mov   r2,a
        pop   ar7
__endasm;

#if 0
uint8_t crc = 0xff;
size_t i, j;
for (i = 0; i < len; i++)
  {
  crc ^= data[i];
  for (j = 0; j < 8; j++)
    {
    if ((crc & 0x80) != 0)
      crc = (uint8_t)((crc << 1) ^ 0x31);
    else
      crc = crc << 1;
    }
  }
  return crc;

#endif
}
#endif


#pragma save
#pragma disable_warning 59
void wait_3us(char a)
{
__asm
1$:   nop
      djnz  dpl,1$
__endasm;
}


// wait for OW_BUS to became 0, waiting time returned in a (*8us) if c==0, c==1 means timeout (2ms)
void wait_ow_0()
{
__asm
      push  acc
      clr   a
1$:   inc   a
      jz    2$
      jb    OW_BUS, 1$
      clr   c
      sjmp  3$
2$:   setb  c
3$:   mov   dpl,a
      pop   acc
__endasm;
}

void wait_ow_1()
{
__asm
      push  acc
      clr   a
1$:   inc   a
      jz    2$
      jnb   OW_BUS, 1$
      clr   c
      sjmp  3$
2$:   setb  c
3$:   mov   dpl,a
      pop   acc
__endasm;
}


void ow_write(unsigned char a)
{
__asm
        push  ar7
        mov   a,dpl
        mov   r7,#8
1$:     rrc   a
        acall _ow_write_slot
        djnz  r7,1$
        pop ar7
__endasm;
}

char ow_read(void)
{
__asm
        push  ar7
        mov   r7,#8
1$:     acall _ow_read_slot
        rrc   a
        djnz  r7,1$
        mov   dpl,a
        pop   ar7
__endasm;
}

void ow_read_slot()
{
__asm
;        mov   c,ea
;        mov   f0,c
        clr   ea
        clr   OW_BUS
        nop
;        nop
        setb  OW_BUS
        mov   dpl,#3
        acall _wait_3us
        mov   c,OW_BUS
;        mov   _F1,c
        setb  ea
        mov   dpl,#27
        acall _wait_3us
;        mov   c,f0
;        mov   ea,c
;        mov   c,_F1
__endasm;
}

void ow_write_slot()
{
__asm
;        mov   _F1,c
;        mov   c,ea
;        mov   f0,c
        clr   ea
        clr   OW_BUS
        mov   dpl,#2
        acall _wait_3us
;        mov   c,_F1
        mov   OW_BUS,c
        setb  ea
        mov   dpl,#28
        acall _wait_3us
        setb  OW_BUS
;        mov   c,f0
;        mov   ea,c
__endasm;
}


char ow_reset() __naked
{
__asm
; reset
        clr   OW_BUS
; wait 500us
        mov   dpl,#215
        acall _wait_3us
        setb  OW_BUS
1$:     jnb   OW_BUS,1$
;        nop
        nop
        acall _wait_ow_0    ; check a (15-60us)
        jc    99$
        acall _wait_ow_1    ; check a (60-240us)
        jc    99$
        mov   dpl,#0
        ret                 ; success (at least one device answered)
99$:
        mov   dpl,#1
        ret                 ; error, timeout happened
__endasm;
}
#pragma restore


char ow_search_dev(int par)
{
__asm
          mov r5,#1                 ; bit mask
          mov r4,#3                 ; usefull constant
          mov r6,dph               ; last collision location
101$:
          mov r7,#64                ; bit counter
          mov r3,#0xff              ; reset position marker
          mov r2,#0                 ; crc
          mov r0,dpl                ; bit buffer
100$:
          acall _ow_reset
          mov   a,dpl
          jnz   1$                  ; reset error!
          mov   dpl,#0xf0
          acall _ow_write
10$:      acall _ow_read_slot
          rlc   a
          mov   dpl,#3
          acall _wait_3us
          acall _ow_read_slot
          rlc   a
          anl   a,r4                ; anl a,#3
          jz    1$                  ; collision
          xrl   a,r4                ; xrl a,#3
          jz    3$                  ; no device - exit
; no collision
          rrc   a
12$:      acall _ow_write_slot
          rrc   a
          mov   a,r5
          jnc    4$
          cpl   a
          anl   a,@r0
          mov   @r0,a
          sjmp  5$
; collision
1$:       mov   a,r6
          cjne  a,ar7,8$
          setb  c                   ; second attempt, now try "1"
          sjmp  9$
8$:       jnc   11$                 ; new collision, try "0"
          mov   a,@r0               ; restore bit tried before
          anl   a,r5
          jnz   9$                  ; c set to "1" already
          clr   c
7$:       jc    9$
11$:      mov   a,r7
          mov   r3,a                ; save position marker
9$:       mov   acc.0,c
          cpl   a
          sjmp  12$
4$:       orl   a,@r0
          mov   @r0,a
5$:       mov   a,r5
          rl    a
          mov   r5,a
          dec   a
          jnz   6$                  ; mask!=1
          mov   a,@r0
          acall _ow_crc             ; calculate crc
          inc   r0                  ; mask==1
6$:       djnz  r7,10$
          mov   a,r2
          jnz   3$
.if 0
          mov   dptr,#_dbg_owa
          acall _get_code_ptr
          acall _puts
          mov   r7,#8
          mov   r0,#_ow_serial
20$:      mov   dpl,@r0
          acall _hex
          mov   r1,dpl
          mov   dpl,dph
          acall _putchar
          mov   dpl,r1
          acall _putchar
          mov   dpl,#' '
          acall _putchar
          inc   r0
          djnz  r7,20$
.endif
          mov   dpl,r3
          sjmp  2$
;          mov   r6,a
;          inc   a
;          jnz   101$
3$:       mov   dpl,#0xff
2$:
__endasm;
}









// MAIN =============================================================
void main(void)
{
char __data *s;
//unsigned h;
__bit send_frame;



//byte_crc(0x3f, 0xe4);

__asm
        mov   a,#0xe4
        mov   r2,#0x3f
        acall _ow_crc
;        mov   a,#0xe4
;        mov   r2,#0x3f
;        acall _ow_crc1
__endasm;



C_TRANS_DIR=0;   // driver disable and set "sending" flag
C_OW_BUS=1;  // one wire bus
EA=0;   // global interrupt disable

__asm
   nop
.if 0
100$:
        inc   dptr
        mov   p1,dpl
        mov   p3,dph
        sjmp  100$
.endif

.if 0
        mov   a,#100
999$:   mov   dpl,a
        setb  P1.7
998$:   nop
        djnz  dpl,998$
        mov   dpl,a
        clr   P1.7
997$:   nop
        djnz  dpl,997$
        sjmp  999$
.endif


   nop
   nop
	mov	a,sp
	mov	r0,a
	inc	r0
1$:
	mov	@r0,#0x55
	inc	r0
	cjne	r0,#0x20,1$  

.if gt DEBUG 0
  mov   dpl,_DPL1
  mov   dph,_DPH1
2$:
  clr a
  movx @dptr,a
  inc dptr
  mov a,dph
  clr c
  subb a,#0xf0
  jc 2$
.endif

__endasm;

//dbg_ptrs=0;

#if DEBUG>0
  memcpy(&disp,"\x90\x90\x00\xE0\x70\x03\x02\x00\xB9\x12\x02\x67\xA3\x80\xF4\x00\x00", 17);
#endif

tick_cnt=0;
time=0x00420000; // set initial date 1.01.2020 00:00:00
s_shift=0;
//ser_idle=1;
frame_received=0;
newalarm=1;
in_frame=0;
EA=1;          // global interrupt enable
//TI=1;
TRANS_TI=0;

#if DEBUG>0
  sendInfo();
#endif

#if 0
  i=ow_reset();
  if(i)
    {
//    ow_write(0xcc);
//    ow_write(0x44);
//    for(i=0;i<200;i++)
//      wait_3us(0);
//    h=ow_reset();
//    ow_write(0xcc);
//    ow_write(0xbe);
    ow_write(0x33);
    for(i=0;i<9;i++)
      {
      h=hex(ow_read());
      putchar(h>>8);
      putchar(h&0xff);
      putchar(' ');
      }
    }
#endif



while(1)
  {
  if(frame_received)
    {
#if DEBUG>0
    print_frame(r_frame);
#endif

    if(r_frame->frm_receiver==DEV_ID || r_frame->frm_receiver==0xff) // frame for me or broadcast
      {
// TODO: check frame numbers and react in case of inconsistency

      send_frame=r_frame->frm_receiver==DEV_ID; // send back the frame only if frame sent directly to me, otherwise ignore
#pragma save
#pragma disable_warning 24
      switch(r_frame->frm.frm_type)
        {
        case COMM_CMD_GETINFO:  // send time and signature, TODO: atomic operation
          EA=0;
          s_frame->frm.frm_type=COMM_CMD_GETINFO;                    // response
          *((unsigned *)&(s_frame->frm.frm_data[0]))=tick_cnt;    // time
          *((unsigned long *)&(s_frame->frm.frm_data[2]))=time;   // time
          EA=1;
          s_frame->frm.frm_data[6]=0;
          s_frame->frm.frm_data[7]=0;                             // I am just test device (type 0)
          s_frame->frm.frm_length=8;
          break;
        case COMM_CMD_SETTIME:  // set time, TODO: atomic operation
          EA=0;
          time=*((unsigned long *)&(r_frame->frm.frm_data[2]));   // time
          tick_cnt=*((unsigned *)r_frame->frm.frm_data)+72;       // number of 1/3600 parts of second (72 is correction for duration of the time setup)
          EA=1;
          s_frame->frm.frm_type=COMM_CMD_SETTIME;                 // response
          s_frame->frm.frm_length=0;                              // no error
          break;
        case COMM_CMD_SETPAR: // set memory cells to the values
          EA=0;
          s=(char __data *)r_frame->frm.frm_data[0];              // destination
          for(i=1; i<(int)s_frame->frm.frm_length;i++)
            *s++=r_frame->frm.frm_data[i];
          EA=1;
          s_frame->frm.frm_type=COMM_CMD_SETPAR;                  // response
          s_frame->frm.frm_length=0;                              // no error
          break;
        case COMM_CMD_OWSEARCH:                                   // search 1-wire bus for devices (temperature sensors)
          if(r_frame->frm_receiver!=DEV_ID)
            {
no_send:    send_frame=0;
            break;
            }
          h=0xff;
          i=0;
          s_frame->frm.frm_length=8;
          s_frame->frm.frm_type=COMM_CMD_OWSEARCH;                // response
          while((h=ow_search_dev((h<<8)|(unsigned)(__data char *)&(s_frame->frm.frm_data[0])))!=0xff)
            sendFrame();
          break;
        case COMM_CMD_OW_TEMP:                                   // read 1-wire devices (temperature sensors)
          if(r_frame->frm_receiver!=DEV_ID)
            goto no_send;
          s_frame->frm.frm_length=9;
          s_frame->frm.frm_type=COMM_CMD_OW_TEMP;                // response
          i=ow_reset();
          if(i)
            goto err_ow_reset;
          ow_write(0x55);
          for(i=0;i<8;i++)
            ow_write(r_frame->frm.frm_data[i]);
          ow_write(0x44);
          set_alarm(750);   // clears newalarm flag
          while(!newalarm);

#if 0
__asm
          mov   a,#
11$:      mov   dpl,#0
          acall _wait_3us
          mov   dpl,#0
          acall _wait_3us
10$:      acall _ow_read_slot
          jnc   10$
__endasm;
#endif
          i=ow_reset();
          if(i)
            {
err_ow_reset:
            errorFrame(COMM_ERR_OW_NOPRESENCE);     // frame not understood
            break;
            }
          ow_write(0x55);
          for(i=0;i<8;i++)
            ow_write(r_frame->frm.frm_data[i]);
          ow_write(0xbe);
          for(i=0;i<9;i++)
            s_frame->frm.frm_data[i]=ow_read();
/*            {
            h=hex(ow_read());
            putchar(h>>8);
            putchar(h&0xff);
            putchar(' ');
            }
*/
          break;
        default:
          errorFrame(COMM_ERR_BADCOMMAND);     // frame not understood
          break;
        }
#pragma restore
      if(send_frame)                 
        sendFrame();
      }
    frame_received=0;
    }
  }
}

// Hardware (AT89C2051)
// 0. 11.0592 (or 22.1184) MHz clock
// 1. UART (RS485) (TxD, RxD) with timer 1 for baud rate generation (115200 or 57600 or 19200 baud)
// 2. TRANS_DIR (P3.7)  - Output pin for RS485 transmitter control (normally transmission is disabled, reception is always enabled)
// 3. OW_BUS (P3.5) - one-wire bus
// 3. Interrupt INT0 (P3.2), negative edge, connected to RxD (to detect start of data sending - busy state of the bus detection)
// 4. Timer 0 configured as 2 8b timers/counters
//    - TH0: time measurements (3600 x 256cycles gives 1s)
//    - TL0: the only available timer/counter (8b)
//       - to genereate timeouts (e.g. for synchronization of data transmission even when some devices do not work)
// available resources:
//    F0 (flag in PSW), P1, P3.3 (INT1), P3.4 (T0), one can connect I2C expander if needed

// test hardware:
// P1.7 - interrupt mark, P1.2,3 RxD, TxD, P3.2 - INT0, P3.3(INT1) - one-wire data, P3.5 - transmitter control


// architecture:
// devices, one manager (PC) and HA controllers are connected to the common RS485 bus
// each device has unique id=0-254 (manager id=0, id=255 is assigned to broadcast id) 
// the id set up the order in which the devices take the bus to transmit data
// the transmission sequence is self-synchronizing
// usually first the manager sends a bunch of frames to the slave devices, and then the slaves take the bus to respond or communicate each other
// manager knows the last device id, so after last frame the sequence can repeat
// if there is no manager, the devices take the bus in random moment (if it is free) after the transmission timeout (timer) is over
// any bus activity detection updates the timeout accordingly (e.g. wait for the frame reception and place itself in the sequence base on the frame received)
// potential collisions are detected by listening to the data on the bus and comparing to the sent data
// after collision detection, the devices wait the time related to it's own id (higher id - longer waiting)

// Each device set timeout, after that it tries to send own frame(s) (timer), during waiting the bus state is monitored,
// if the transmission is detected, device synchronize to the data stream present in the bus
// it traces the data transmission and waits for the last frame from the active device
// if the active on the bus device has an id directly preceding device id (active_device_id=id-1), then the frame is transmited
//   immediately, after receiwing last frame byte (2nd crc byte)
// if the active on the bus device has other id, then the new timout for data transmission is (id-active_device_id-1)*0.5*single_byte_data_transmission
//   that makes n*0.5 byte transmission time time slot for starting transmission from preceedeing devices

// communication protocol:
// sequence of frames: <main><slave_1><slave_2>...<slave_n>
// <.....>: frame:
//   frame (bytes): COMM_BESC, dest, source, frm_conf_4b | frm_no_4b, frm_type, frm_length, payload[0-254], crc16
// source - sender id (0-254), source=0: manager, bit 8 of source set: last frame frome source (no more follows), dest - destination id or 0xff (broadcast) 
// manager sends either 1 broadcast frame or a sequence of single frames to individual nodes, they cannot be combined (node may not be able to serve 2 frames in the sequence)
// no node sends more than 1 frame in a row (this is questionable, maybe this use case is needed?)
// while sending frame all COMM_BESC and COMM_BSHIFT bytes inside the frame (the whole frame except the first COMM_BESC byte
// - it ALWAYS indicate the frame start) MUST be substitutet with COMM_BSHIFT with following COMM_SESC (COMM_BESC) or COMM_SSHIFT (COMM_BSHIFT) respectively
// during frame reception the sequence "COMM_BSHIFT, n" MUST be restored respectively
// COMM_BESC=0x5a and COMM_BSHIFT=0xa5, COMM_SESC=0x55, COMM_SSHIFT=0xaa

// after last frame from device with id, the next device (id+1) can take the bus immediately after receiving the last byte and send its frame(s)
// if device id+1 does not transmit anything (nobody is taking the bus) during 0.5 of single byte transmission, the next device (id+2) can take the bus
// if device id+2 also does not work, then the device id+n waits (n-1)*0.5 single byte transmission, and so on...
// if sender recognizes transmission collision (what was received is not the same as sent) then it breaks transmission immediately and sets up the timeout according to the own id

// for frame type definitions see the ihm.h

// for frame error definitions see the ihm.h

// temperature sensors DS18B20
// 28 66 3C A2 9A 23 09 2E
// 28 E6 16 19 A0 23 07 2F
// 28 A5 2D 9F 9A 23 09 8C
// 28 2B 86 54 A0 23 08 71
// 28 7B 4E C7 9A 23 09 C0
// 28 67 8F CD A0 23 08 F8

// test of temperature readout (each sensow 2 times)
// 5a 00 01 0b 05 09 9b 01 4b 46 7f ff 0c 10 dc 66 7d 
// 5a 00 01 0c 05 09 9c 01 4b 46 7f ff 0c 10 0c 14 e0 
// 5a 00 01 0d 05 09 98 01 4b 46 7f ff 0c 10 19 c9 ee 
// 5a 00 01 0e 05 09 99 01 4b 46 7f ff 0c 10 a5 55 5f f5 
// 5a 00 01 0f 05 09 9e 01 4b 46 7f ff 0c 10 8a 26 56 
// 5a 00 01 00 05 09 9e 01 4b 46 7f ff 0c 10 8a 35 b5 
// 5a 00 01 01 05 09 98 01 4b 46 7f ff 0c 10 19 df 92 
// 5a 00 01 02 05 09 98 01 4b 46 7f ff 0c 10 19 da 0d 
// 5a 00 01 03 05 09 99 01 4b 46 7f ff 0c 10 a5 55 4a fc 
// 5a 00 01 04 05 09 99 01 4b 46 7f ff 0c 10 a5 55 42 b7 
// 5a 00 01 05 05 09 99 01 4b 46 7f ff 0c 10 a5 55 41 c2 
// 5a 00 01 06 05 09 99 01 4b 46 7f ff 0c 10 a5 55 44 5d 


// END MAIN =========================================================
