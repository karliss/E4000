#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <string.h>

extern "C" {
#include "usbdrv.h"
}

#include "matrix_mapping.h"

// TODO:rewrite
const PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = 
{
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)(224)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)(231)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs) ; Modifier byte
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs) ; Reserved byte
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs) ; LED report
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs) ; LED report padding
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0Xff,                    //   LOGICAL_MAXIMUM (255)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))(0)
    0x29, 0xff,                    //   USAGE_MAXIMUM (Keyboard Application)(255)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};

typedef struct
{
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keycode[6];
} keyboard_report_t;

static keyboard_report_t keyboard_report;
volatile static uchar LED_state = 0xff;
static uchar idleRate; 

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
    usbRequest_t *rq = reinterpret_cast<usbRequest_t*>(data);

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS)
    {
        switch(rq->bRequest) {
        case USBRQ_HID_GET_REPORT:
            usbMsgPtr = reinterpret_cast<uchar*>(&keyboard_report);
            keyboard_report.modifier = 0;
            keyboard_report.keycode[0] = 0;
            return sizeof(keyboard_report);
        case USBRQ_HID_SET_REPORT: //TODO: check correctly
            return (rq->wLength.word == 1) ? USB_NO_MSG : 0;
        case USBRQ_HID_GET_IDLE:
            usbMsgPtr = &idleRate;
            return 1;
        case USBRQ_HID_SET_IDLE:
            idleRate = rq->wValue.bytes[1];
            return 0;
        }
    }
    
    return 0;
}

#define NUM_LOCK 1
#define CAPS_LOCK 2
#define SCROLL_LOCK 4
#define F_LOCK 8

static const uint8_t LED_MASK = 0xf0;

static void setLeds(uint8_t v)
{
    PORTD = (PORTD & (~LED_MASK))|((v << 4) & LED_MASK);
}


extern "C" usbMsgLen_t usbFunctionWrite(uint8_t * data, uchar len) {
    if (data[0] == LED_state)
        return 1;
    
    LED_state = data[0];
    
    setLeds(LED_state);
    //PORTD = (PORTD & (~LED_MASK))|((LED_state << 4) & LED_MASK);

    return 1;
}

static uint8_t charToCode(char c)
{
    if (c >= 'a' && c <= 'z')
        return 4 + c - 'a';
    if (c >= '1' && c <= '9')
        return 0x1e + c - '1';
    if (c == '0')
        return 0x27;
    if (c == '\n')
        return 0x28;
    if (c == ' ')
        return 0x2c;
        
    return 0;
}

static void buildReport(uchar send_key)
{
    keyboard_report.modifier = 0;
    keyboard_report.keycode[0] = charToCode(send_key);
}

#define STATE_WAIT 0
#define STATE_SEND_KEY 1
#define STATE_RELEASE_KEY 2

static inline uint8_t getRows() {
    uint8_t result = (((uint8_t)PINC) & (uint8_t)0x3fU) | (((uint8_t)(PIND)) << 6);
    return ~result;
}

const uint8_t SRCLK = (1 << PB0);
const uint8_t SRD = (1 << PB1);
const uint8_t SRCLR = (1 << PB2);

const uint8_t SHIFTR_MASK = SRCLK | SRCLR | SRD;

static void initHardware()
{
    DDRC = 0; // rows in
    PORTC |= 0x3f; // rows pullups
    
    // ROW7 ROW8, LED1 LED2 LED3 LED4
    DDRD = (0 << PD0) | (0 << PD1) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);
    PORTD = 0x3;
    
    DDRB = SHIFTR_MASK;
    PORTB = SRCLK;
}


const uint8_t MATRIX_ROWS = 24;

static uint8_t MATRIX[MATRIX_ROWS];

static void readMatrix()
{
    PORTB &= ~SRCLR; // clear all rows
    PORTB |= SRD;
    PORTB |= SRCLR;
    
    PORTB &= ~SRCLK;
    PORTB |= SRCLK;
    PORTB &= ~SRD;
    PORTB &= ~SRCLK;
    PORTB |= SRCLK;
    for (int i = 0; i < MATRIX_ROWS; i++)
    {
        _delay_us(20);
        cli();
        uint8_t tmp = DDRC;
        DDRC = tmp | 0x3f;
        DDRC = tmp;
        DDRD |= 0x01;
        DDRD &= ~0x01;
        DDRD |= 0x02;
        DDRD &= ~0x02;
        sei();
        /*DDRD |= 0x3;
        DDRD &= ~0x3f;*/
        MATRIX[i] = getRows();
        PORTB &= ~SRCLK;
        PORTB &= ~SRD;
        PORTB |= SRCLK;
    }
    PORTB &= ~SRCLR;
    PORTB |= SRCLR;
}

static uint8_t cMatrix[32];

static void readCompressedMatrix()
{
    memset(cMatrix, 0, sizeof(cMatrix));
    readMatrix();
    uint8_t keys = 0;
    memset(&keyboard_report, 0, sizeof(keyboard_report));
    uint8_t cnt=0;
    for (uint8_t i=0; i<MATRIX_ROWS; i++)
    {
        uint8_t t = MATRIX[i];
        if (!t) continue;
        for (uint8_t j=0; j<8; j++)
        {
            if (t & 0x01)
            {
                uint8_t v = pgm_read_byte(&(MATRIX_MAPPING[i][j]));
                cnt++;
                if (keys < 6 && v)
                {
                    //keyboard_report.keycode[keys++] = charToCode('0' + j);
                    //keyboard_report.keycode[keys++] = charToCode('a' + i);
                    keyboard_report.keycode[keys++] = v;
                }
                /*uint8_t k = v & 0x07;
                v >>= 3;
                cMatrix[v] |= (1 << k);*/
            }
            t >>= 1;
        }
    }
   /* if (cnt) {
        keyboard_report.keycode[keys++] = charToCode('1' + cnt - 1);
    }*/
}
/*
static void readCompressedMatrix()
{
    memset(cMatrix, 0, sizeof(cMatrix));
    readMatrix();
    for (uint8_t i=0; i<MATRIX_ROWS; i++)
    {
        uint8_t t = MATRIX[i];
        if (!t) continue;
        for (int j=0; j<8; j++)
        {
            if (t & 0x01)
            {
                uint8_t v = MATRIX_MAPPING[i][j];
                uint8_t k = v & 0x07;
                v >>= 3;
                cMatrix[v] |= (1 << k);
            }
            t >>= 1;
        }
    }
}

static void fillReportFromcMatrix()
{
    uint8_t keys = 0;
    memset(&keyboard_report, 0, sizeof(keyboard_report));
    for (int i = 0; i < 32; i++)
    {
        uint8_t t = cMatrix[i];
        if (!t) continue;
        for (int j = 0; j < 8; j++)
        {
            if ((t & 0x01) && keys < 6)
            {
                uint8_t id = (i << 3) | (j);
                keyboard_report.keycode[keys++] = id;
            }
            t >>= 1;
        }
    }
}*/

char txt[32];
uint8_t dlen=0;
uint8_t cr = 0;
int main()
{
    uchar i, button_release_counter = 0, state = STATE_WAIT;
    wdt_disable();
    initHardware();
    setLeds(0);
 
    for(i=0; i<sizeof(keyboard_report); i++)
        ((uchar *)&keyboard_report)[i] = 0;

    usbInit();
    
    //usbDeviceDisconnect();
    for(i = 0; i<250; i++) {
        //wdt_reset();
        _delay_ms(2);
    }
    //usbDeviceConnect();
    setLeds(1);
    
    sei();
    uint8_t last_d = 0;
    uint8_t dt = 0;
    button_release_counter =0;
    txt[0] ='\0';
    //txt[1] ='b';
    //txt[2] ='\0';
    while(1) {
        //wdt_reset();
        //LED_state++;
        
        usbPoll();
        
        //uint8_t rows = getRows();
        char key = 'x';
       /* if(1 && dt == 0) { 
            //if(state == STATE_WAIT && button_release_counter == 255)
            //    state = STATE_SEND_KEY;
                
            uint8_t keyid = MATRIX[0];

            if (cr >= MATRIX_ROWS) {
                cr = 0;
            }
            uint8_t p = 0;
            txt[p++] = 0;
            if (cr == 0) {
                readMatrix();
            }
            txt[p++] = '\n';
            uint8_t cnt = 0;

            for (int j=0; j<8; j++) {
                char c = 'o';
                if (MATRIX[cr] & (1 << j)) {
                    c = 'x';
                }
                txt[p++] = ' ';
                txt[p++] = c;
            }
            if (cr == 0) {
                txt[p++] = '\n';
                txt[p++] = '\n';
            }
            dt = p;
            cr++;
        }*/
        key = 'z';
        button_release_counter++;
        //if(button_release_counter < 255) //TODO: do properly
        //    button_re lease_counter++;
       if (usbInterruptIsReady() /*&& dt > 0*/) {
           readCompressedMatrix();
           //fillReportFromcMatrix();
           usbSetInterrupt(reinterpret_cast<uchar*>(&keyboard_report), sizeof(keyboard_report));
           /*
           dt--;
           if (txt[dt]) {
                buildReport(txt[dt]);
           } else {
               buildReport('\0');
           }
           usbSetInterrupt(reinterpret_cast<uchar*>(&keyboard_report), sizeof(keyboard_report));*/
       }
       //setLeds(2);
       /*if (usbInterruptIsReady()) {
           uint8_t rows = getRows();
           if (rows)
               buildReport('z');
           else
               buildReport(0);

           usbSetInterrupt(reinterpret_cast<uchar*>(&keyboard_report), sizeof(keyboard_report));
           //setLeds(8);
       }*/
       //setLeds(4);
        /*
        if(usbInterruptIsReady() && state != STATE_WAIT && LED_state != 0xff){
            switch(state) {
            case STATE_SEND_KEY:
                buildReport(key);
                state = STATE_RELEASE_KEY;
                break;
            case STATE_RELEASE_KEY:
                buildReport('\0');
                state = STATE_WAIT;
                break;
            default:
                state = STATE_WAIT;
            }
            
            // start sending
            usbSetInterrupt(reinterpret_cast<uchar*>(&keyboard_report), sizeof(keyboard_report));
            

        }            */
    }
    
    return 0;
}
