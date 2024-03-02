#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DCCA_PIN         5
#define DCCB_PIN         6
#define DCC_SIGNAL_PIN   4

#define DCC_1_HALFPERIOD 58
#define DCC_0_HALFPERIOD 100

// Define a structure to hold DCC packet data
typedef struct {
  uint8_t preamble[2];  // Two bytes representing the 12-bit preamble
  uint8_t address;      // Address byte of the locomotive
  uint8_t data[6];       // Array to hold up to 6 data bytes
  uint8_t checksum;     // XOR checksum of address and data bytes
} dcc_packet_t;


// uint8_t dcc_packet_calc_checksum(dcc_packet_t *packet) {
//   uint8_t checksum = 0;
//   for (int i = 0; i < sizeof(packet->address) + sizeof(packet->data); i++) {
//     checksum ^= ((uint8_t *)packet)[i + 1]; // +1 to skip preamble
//   }
//   packet->checksum = checksum;
//   return checksum;
// }

// void dcc_packet_init_idle(dcc_packet_t *packet) {
//   memset(packet->preamble, 0xFF, sizeof(packet->preamble));
//   packet->address = 0;
//   memset(packet->data, 0, sizeof(packet->data));
//   packet->checksum = 0;
// }

#define DCC_PULSE(a,b,us) \
  do { \
    REG_WRITE(GPIO_OUT_W1TS_REG, 1 << a); \
    REG_WRITE(GPIO_OUT_W1TC_REG, 1 << b); \
    ets_delay_us(us); \
    REG_WRITE(GPIO_OUT_W1TC_REG, 1 << a); \
    REG_WRITE(GPIO_OUT_W1TS_REG, 1 << b); \
    ets_delay_us(us); \
  } while (0)

#define DCC_PULSE1(a,b)\
  do { DCC_PULSE(a,b,DCC_1_HALFPERIOD*2 +10); } while(0)
#define DCC_PULSE0(a,b)\
  do { DCC_PULSE(a,b,DCC_0_HALFPERIOD*2 +10); } while(0)

#define DCC_PULSEX(a,b,us) \
  do { \
    REG_WRITE(GPIO_OUT_REG, (1 << a) | ~(1 << b)); \
    ets_delay_us(us); \
    REG_WRITE(GPIO_OUT_REG, ~(1 << a) | (1 << b)); \
    ets_delay_us(us); \
  } while (0)
#define DCC_PULSEX0(a,b)\
  do { DCC_PULSEX(a,b,DCC_0_HALFPERIOD); } while(0)
#define DCC_PULSEX1(a,b)\
  do { DCC_PULSEX(a,b,DCC_1_HALFPERIOD); } while(0)
#define DCC_PULSEXX(a,b,x) \
  do { \
  if (x==0)DCC_PULSEX(a,b,DCC_0_HALFPERIOD); \
  else DCC_PULSEX1(a,b); \
  } while(0)




void 
rmt_dcc_decoder_main(void *pvParameters);


#ifdef __cplusplus
}
#endif
