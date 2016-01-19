//
// laser_access.h
//


#define VERSION F("0.1.4-github")
#define FIRMWARE F("FabLab laser access control")


#define CARD_TIMEOUT     10  // after 10 seconds try to re-validate card
#define OFF_TIMEOUT      20  // after 10+10==20 seconds w/o valid card: power off
#define DIM_TIMEOUT     300  // after 300 seconds dim tft down
#define ENABLE_TIMEOUT  300  // after 300 seconds disable privileged mode

// Power PINs
#define PIN_POWER     12  // power relais
#define PIN_CHAIN     11  // unused

// Sense PINs
#define SENSE_0    4  // service hatches
#define SENSE_1    5  // front hatch
#define SENSE_2    6  // chiller
#define SENSE_3    7  // compressed air
#define SENSE_4    8  // reserved
#define SENSE_5    9  // reserved


// ESP8266 reset pin
#define PIN_ESP_RESET 43

// PWM PINs
//#define PIN_INT_LED   13
#define PIN_TFT       46  // pwm TFT backlight
#define FREQ 50000    // pwm frequency (in Hz)

// mode
#define MODE_OFF          0  // power off
#define MODE_ON           1  // power on
#define MODE_CARD         2  // power still on, re-validating card
#define MODE_MAINTENANCE  99 // 

// I2C addresses
#define I2C_RTC_ADDR     0x68 // 104 RTC
#define I2C_EE_ADDR      0x54 // 80 64k eeprom 4 logging
#define I2C_TOUCH_ADDR   0x41 // touch controller
#define I2C_NFC_ADDR     0x24 // useless here: set in adafruit_nfc library
//#define I2C_RTCEE_ADDR   0x50 // unused ?
//#define I2C__ADDR     0x78 // unused ?

//#define TFT_MISO 50
//#define TFT_MOSI 51
//#define TFT_CLK 52
//#define TFT_RST -1
#define TFT_DC  42
#define TFT_CS  53 // SS

//#define TFT_IRQ 3
#define SD_CS   48

#define NFC_IRQ    3
#define NFC_RESET -1  // Not connected

//#define CTRL_Z 26

//#define byte unsigned char
//#define byte uint8_t

// internal eeprom offsets
#define EE_MASTER 0
#define EE_ENABLE_LEN       80  // length of enable pw stored 80
#define EE_ENABLE_MAX_LEN   12  // max 12 digits (81...93)
#define EE_ENABLE_PW        81  // store enable pw in 81...93
// first user  @#96 ...leaves room for 1000 (id(4Byte) users
#define EE_NEXT             94  // (94 + 95) store next=last user id here
#define EE_USER             96  // start of user db here
#define EE_MAX            4096  // 4k internal eeprom

//#define EE_LOG_MIN       0  // log space from here
//#define EE_LOG_MAX   32760  // log space till here
//#define EE_LOG_START_ADDR 32764  // (32764 + 32765) store start of logs here
//#define EE_LOG_END_ADDR   32766  // (32766 + 32767) store end of logs here
////#define EXT_EE_MAX       32768  // absolute maximum

#define EE_LOG_MIN       0  // log space from here
#define EE_LOG_MAX   65530  // log space till here
#define EE_LOG_START_ADDR 65532  // (65532 + 65533) store start of logs here
#define EE_LOG_END_ADDR   65534  // (65534 + 65535) store end of logs here
//#define EXT_EE_MAX       65536  // absolute maximum


// Color definitions
#define ILI9341_BLACK       0x0000      /*   0,   0,   0 */
#define ILI9341_NAVY        0x000F      /*   0,   0, 128 */
#define ILI9341_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define ILI9341_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define ILI9341_MAROON      0x7800      /* 128,   0,   0 */
#define ILI9341_PURPLE      0x780F      /* 128,   0, 128 */
#define ILI9341_OLIVE       0x7BE0      /* 128, 128,   0 */
#define ILI9341_LIGHTGREY   0xC618      /* 192, 192, 192 */
#define ILI9341_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define ILI9341_BLUE        0x001F      /*   0,   0, 255 */
#define ILI9341_GREEN       0x07E0      /*   0, 255,   0 */
#define ILI9341_CYAN        0x07FF      /*   0, 255, 255 */
#define ILI9341_RED         0xF800      /* 255,   0,   0 */
#define ILI9341_MAGENTA     0xF81F      /* 255,   0, 255 */
#define ILI9341_YELLOW      0xFFE0      /* 255, 255,   0 */
#define ILI9341_WHITE       0xFFFF      /* 255, 255, 255 */
#define ILI9341_ORANGE      0xFD20      /* 255, 165,   0 */
#define ILI9341_GREENYELLOW 0xAFE5      /* 173, 255,  47 */
#define ILI9341_PINK        0xF81F




// non standard :) time_t
typedef struct {
  unsigned int hour;
  unsigned int minute;
  unsigned int second;
  unsigned int year;
  unsigned int month;
  unsigned int day;
  unsigned long unixtime;
} time_t;




// select * from users
// id|name|phone|tag|zugang|amlaser|zinglaser|ulti|ulti2|res1|res2|comment
// tag:
//     0xAABBCCDD
//     0| long(4B) |
typedef struct {
  unsigned int id;
  char name[33];
  char phone[32];
  unsigned char serial[4];
} user_t;


typedef struct {
  //unsigned long b[4];
  unsigned char b[4];
} tag_t;
