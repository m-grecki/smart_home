#define FALSE 0
#define TRUE 1

// DEBUGGING
#define INT_MARK_ON __asm \
  .if gt TEST 1 \
  setb P1.7 \
  .endif \
__endasm;
#define INT_MARK_OFF __asm \
  .if gt TEST 1 \
  clr P1.7 \
  .endif \
__endasm;

// Hardware
#define TRANS_DIR P3.7
#define C_TRANS_DIR P3_7   // doubled with "C_" prefix to use in C code
#define TRANS_SBUF SBUF
#define TRANS_TI TI
#define TRANS_RI RI
#define TRANS_SCON SCON
#define OW_BUS P3.5
#define C_OW_BUS P3_5   // doubled with "C_" prefix to use in C code


// DEFINES ==========================================================
#define COMM_BESC   0x5a  // frame start marker
#define COMM_BSHIFT 0xa5  // "shifted byte" marker
#define COMM_SESC   0x55  // "shifted" COMM_BESC  
#define COMM_SSHIFT 0xaa  // "shifted" COMM_BSHIFT  (COMM_BESC xor COMM_BSHIFT must result 0xff)
//#define MAXDEL (((MASTERDEL+UNITDEL*256)*CLKFREQ)/11059)
#define COMM_NOERR          0   // no error
#define COMM_ERR_BADCOMMAND 1   // wrong frame type
#define COMM_ERR_BADSEQUENCE2   // wrong frame number
#define COMM_ERR_BADCRC     3   // wrong crc
#define COMM_ERR_BADSIZE    4   // frame does not fit into the buffer (to long)
#define COMM_ERR_OW_NOPRESENCE    5   // 1-wire bus does not respond to reset

//
//#define CLOCK_FREQ          11059200
#define CLOCK_FREQ          22118400
#define CLOCK_1SEC          (CLOCK_FREQ/12/256)
#define CLOCK_MULT          (CLOCK_FREQ/11059200)


// frame types (commands)
#define COMM_CMD_IGNORE     0   // ignore, do not send
#define COMM_CMD_GETINFO    1   // please, send the own time (32bits) and signature (TBD)
                                // answer (COMM_CMD_SIGNATURE) data:
                                // 2b: msec/1000*3600
                                // 4B: sec - 6b, min - 6b, hour - 5b, day - 5b, month - 4b,
                                // year - 6b (counted from 2020)
                                // 2B: device type:
                                //   0 - test device
                                //   1 - temperature sensor
#define COMM_CMD_SETTIME    2   // set time, time in the form of 48 bits number (LSB first): 2B with tick_cnt (ms/1000*3600),
                                // 4B: sec - 6b (0-59), min - 6b (0-69), hour - 5b (0-23), day - 5b (1-31), month - 4b (1-12),
                                // year - 6b (counted from 2020) - send as a broadcast, no answer needed
#define COMM_CMD_SETPAR     3   // set parameter(s) (addr, values...)
#define COMM_CMD_OWSEARCH   4   // scan 1-wire bus, found addresses are send in following frames (possibly more than 1!)
#define COMM_CMD_OW_TEMP    5   // measure temperature


// Prototypes =======================================================
void putchar(char a);
void puts(char *ptr);
void sendFrame();
unsigned short crc_ccitt(unsigned short crc, unsigned char a);
//void serial_helper();
//void monputchar(char a);

//     Description      Vector Address
// 0   External 0       0x0003
// 1   Timer 0          0x000b
// 2   External 1       0x0013
// 3   Timer 1          0x001b
// 4   Serial           0x0023
// 5   Timer 2 (8052)   0x002b
// ...
// n                    0x0003 + 8*n


// ISR-Prototypes ===================================================
//void External0_ISR(void) __interrupt (0) __using (0); // ISR for the external input INT0
//void Timer0_ISR(void)    interrupt 1 using 1; // ISR for Timer0 overflow
//void External1_ISR(void) __interrupt (2) __using (0); // ISR for the external input INT1
//void Timer1_ISR(void)    interrupt 3; // ISR for Timer1 overflow
//void Serial_ISR(void)    __interrupt (4) __using (0); // ISR for serial reception
//void Timer2_ISR(void)    interrupt 5; // ISR for Timer2 overflow
//void Serial_1_ISR(void)    __interrupt (7) __using (0); // ISR for serial reception
// END ISR-Prototypes ===============================================



struct frame {
  unsigned char frm_type;
  unsigned char frm_length;
  unsigned char frm_data[1];
};

struct comm_frame {
  unsigned char frm_receiver;
  unsigned char frm_sender;
  unsigned char frm_numbers;  // frame number and confirmed frame number
  struct frame frm;
  };
