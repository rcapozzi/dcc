/**
 * * RTOS Task
 * * RMT for decoding
 */
#include "rmt_dcc_decoder.h"

extern "C" {
  void ets_delay_us(uint32_t us);
}

uint8_t dcc_packet_calc_checksum(dcc_packet_t *packet) {
  uint8_t checksum = 0;
  for (int i = 0; i < sizeof(packet->address) + sizeof(packet->data); i++) {
    checksum ^= ((uint8_t *)packet)[i + 1]; // +1 to skip preamble
  }
  packet->checksum = checksum;
  return checksum;
}

void pulseIdle(){
  dcc_packet_t packet;
  packet.address = 0;
  memset(packet.data, 0, sizeof(packet.data));

  for (int i = 0; i < 12; i++) {
    DCC_PULSEX1(DCCA_PIN, DCCB_PIN);
  }
  DCC_PULSEX0(DCCA_PIN, DCCB_PIN);

  // Address
  for (size_t i = 0; i < 8; i++){ 
    DCC_PULSEXX(DCCA_PIN, DCCB_PIN, ((packet.address >> 0) & 1));
  }
  DCC_PULSEX0(DCCA_PIN, DCCB_PIN);

  // Data
  for (int i = 0; i < sizeof(packet.data); i++) {
    for (int j = 0; j < 8; j++) {
      DCC_PULSEXX(DCCA_PIN, DCCB_PIN, ((packet.data[i] >> j) & 1));
    }
  }
  DCC_PULSEX0(DCCA_PIN, DCCB_PIN);

  // Checksum
  dcc_packet_calc_checksum(&packet);
  for (int i = 0; i < 8; i++) {
    DCC_PULSEXX(DCCA_PIN, DCCB_PIN, ((packet.checksum >> i) & 1));
  }
  // end-of-transmission
  DCC_PULSEX1(DCCA_PIN, DCCB_PIN);
}

/**
 * Create square wave of 1 and 0.
 */
void packetWriterTask(void *pvParameters) {
  TickType_t lastWakeTime = xTaskGetTickCount();

  REG_WRITE(GPIO_OUT_W1TC_REG, 1 << DCCA_PIN);
  REG_WRITE(GPIO_OUT_W1TC_REG, 1 << DCCB_PIN);
  Serial.printf("packetWriterTask starting. Channels A & B\n");

  while (1) {
    // pulseIdle();
    DCC_PULSE1(DCCA_PIN, DCCB_PIN);
    DCC_PULSE0(DCCA_PIN, DCCB_PIN);

    DCC_PULSEX1(DCCA_PIN, DCCB_PIN);
    DCC_PULSEX0(DCCA_PIN, DCCB_PIN);

    // Turn off LEDs
    REG_WRITE(GPIO_OUT_REG, GPIO_OUT_REG  & (~(1 << DCCA_PIN) | ~(1 << DCCB_PIN)));
    Serial.printf("packetWriterTask X reg=%d\n", REG_READ(GPIO_OUT_REG));
    vTaskDelayUntil(&lastWakeTime, 2000 / portTICK_PERIOD_MS );
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("==================================");
  Serial.println("Demo_DCC_Server_01_RMT setup enter");
  Serial.println("==================================");
  delay(1500);

  pinMode(DCCA_PIN, OUTPUT);
  pinMode(DCCB_PIN, OUTPUT);
  pinMode(DCC_SIGNAL_PIN, INPUT);

  xTaskCreate(packetWriterTask, "packetWriter", 2048, NULL, 1, NULL);
  xTaskCreate(rmt_dcc_decoder_main, "rmt_dcc_decoder_main", 2048, NULL, 1, NULL);

  char msg[256];
  sprintf(msg, "setup returns A=%d B=%d SIGNAL=%d", DCCA_PIN, DCCB_PIN, DCC_SIGNAL_PIN);
  Serial.println(msg);
}

void loop() {
  unsigned long now_t = millis();
  static unsigned prev_t = 0;;
  if ((now_t - prev_t) > 10000 ){
    Serial.printf("Demo_DCC_Server_01_RMT Heartbeat A=%d B=%d SIGNAL=%d\n", DCCB_PIN, DCCB_PIN, digitalRead(DCC_SIGNAL_PIN));
    prev_t = now_t;
  }

  static TickType_t lastWakeTime = xTaskGetTickCount();
  delay(50);
  // vTaskDelayUntil(&lastWakeTime, 5000 / portTICK_PERIOD_MS);
}
