#include <util/atomic.h>
#include "iface_nrf24l01.h"
#include <string.h>

#define PPM_pin 2  // PPM in
//SPI Comm.pins with nRF24L01
#define MOSI_pin 3   // MOSI - D3
#define SCK_pin 4    // SCK  - D4
#define CE_pin 5     // CE   - D5
#define MISO_pin A0  // MISO - A0
#define CS_pin A1    // CS   - A1

#define ledPin 13  // LED  - D13

// SPI outputs
#define MOSI_on PORTD |= _BV(3)    // PD3
#define MOSI_off PORTD &= ~_BV(3)  // PD3
#define SCK_on PORTD |= _BV(4)     // PD4
#define SCK_off PORTD &= ~_BV(4)   // PD4
#define CE_on PORTD |= _BV(5)      // PD5
#define CE_off PORTD &= ~_BV(5)    // PD5
#define CS_on PORTC |= _BV(1)      // PC1
#define CS_off PORTC &= ~_BV(1)    // PC1
// SPI input
#define MISO_on (PINC & _BV(0))  // PC0

#define RF_POWER TX_POWER_80mW

// PPM stream settings
#define CHANNELS 12  // number of channels in ppm stream, 12 ideally
enum chan_order {
  THROTTLE,
  AILERON,
  ELEVATOR,
  RUDDER,
  AUX1,  // (CH5)  led light, or 3 pos. rate on CX-10, H7, or inverted flight on H101
  AUX2,  // (CH6)  flip control
  AUX3,  // (CH7)  still camera (snapshot)
  AUX4,  // (CH8)  video camera
  AUX5,  // (CH9)  headless
  AUX6,  // (CH10) calibrate Y (V2x2), pitch trim (H7), RTH (Bayang, H20), 360deg flip mode (H8-3D, H22)
  AUX7,  // (CH11) calibrate X (V2x2), roll trim (H7)
  AUX8,  // (CH12) Reset / Rebind
};

#define PPM_MIN 1000
#define PPM_SAFE_THROTTLE 1050
#define PPM_MID 1500
#define PPM_MAX 2000
#define PPM_MIN_COMMAND 1300
#define PPM_MAX_COMMAND 1700
#define GET_FLAG(ch, mask) (ppm[ch] > PPM_MAX_COMMAND ? mask : 0)

// supported protocols
enum {
  PROTO_CX10_BLUE = 2,  // Cheerson CX-10 blue board, newer red board, CX-10A, CX-10C, Floureon FX-10, CX-Stars (todo: add DM007 variant)
  PROTO_CX10_GREEN      // Cheerson CX-10 green board
};

uint16_t overrun_cnt = 0;
uint8_t transmitterID[4];
uint8_t current_protocol;
static volatile bool ppm_ok = false;
uint8_t packet[21];
//uint8_t raw_packet[21];
static bool reset = true;
volatile uint16_t Servo_data[12];
static uint16_t ppm[12] = {
  PPM_MIN,
  PPM_MID,
  PPM_MID,
  PPM_MID,
  PPM_MID,
  PPM_MID,
  PPM_MID,
  PPM_MID,
  PPM_MID,
  PPM_MID,
  PPM_MID,
  PPM_MID,
};

void setup() {

  randomSeed((analogRead(A4) & 0x1F) | (analogRead(A5) << 5));
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);  //start LED off

  pinMode(PPM_pin, INPUT);
  pinMode(MOSI_pin, OUTPUT);
  pinMode(SCK_pin, OUTPUT);
  pinMode(CS_pin, OUTPUT);
  pinMode(CE_pin, OUTPUT);
  pinMode(MISO_pin, INPUT);

  // PPM ISR setup
  //attachInterrupt(PPM_pin - 2, ISR_ppm, CHANGE);
  TCCR1A = 0;  //reset timer1
  TCCR1B = 0;
  TCCR1B |= (1 << CS11);  //set timer1 to increment every 1 us @ 8MHz, 0.5 us @16MHz
  // Serial port input/output setup
  Serial.begin(115200);
}

void loop() {
  uint32_t timeout;
  // reset / rebind
  //Serial.println("begin loop");
  if (reset || ppm[AUX8] > PPM_MAX_COMMAND) {
    reset = false;
    Serial.println("selecting protocol");
    selectProtocol();
    Serial.println("selected protocol.");
    NRF24L01_Reset();
    Serial.println("nrf24l01 reset.");
    NRF24L01_Initialize();
    Serial.println("nrf24l01 init.");
    init_protocol();
    Serial.println("init protocol complete.");
  }

  switch (current_protocol) {
    case PROTO_CX10_GREEN:
    case PROTO_CX10_BLUE:
      timeout = CX10RX_process();  // returns micros()+6000 for time to next packet.
      break;
  }

  while (micros() < timeout)  // timeout for CX-10 blue = 6000microseconds.
  {
    //overrun_cnt+=1;
  };
}

void selectProtocol() {
  //set_txid(true);
  current_protocol = PROTO_CX10_BLUE;
}

void init_protocol() {
  switch (current_protocol) {
    case PROTO_CX10_GREEN:
    case PROTO_CX10_BLUE:
      CX10RX_init();
      Serial.println("binding...");
      CX10RX_bind();
      break;
  }
}
