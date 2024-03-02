/**
 * The DCC-EX Command station flips directions on the motor shield to create pulse/waves. This technique results in voltage flipping.
 * The naive method shown here only
 */
#define DCCA_PIN         4
#define DCCB_PIN         5
#define DCC_SIGNAL_PIN   6
#define DCC_1_HALFPERIOD 58
#define DCC_0_HALFPERIOD 100

extern "C" {
  void ets_delay_us(uint32_t us);
}

// Define a structure to hold DCC packet data
typedef struct {
  uint8_t preamble[2];  // Two bytes representing the 12-bit preamble
  uint8_t address;      // Address byte of the locomotive
  uint8_t data[6];       // Array to hold up to 6 data bytes
  uint8_t checksum;     // XOR checksum of address and data bytes
} dcc_packet_t;


uint8_t dcc_packet_calc_checksum(dcc_packet_t *packet) {
  uint8_t checksum = 0;
  for (int i = 0; i < sizeof(packet->address) + sizeof(packet->data); i++) {
    checksum ^= ((uint8_t *)packet)[i + 1]; // +1 to skip preamble
  }
  packet->checksum = checksum;
  return checksum;
}

void dcc_packet_init_idle(dcc_packet_t *packet) {
  memset(packet->preamble, 0xFF, sizeof(packet->preamble));
  packet->address = 0;
  memset(packet->data, 0, sizeof(packet->data));
  packet->checksum = 0;
}

/**
 * Create a wave. Duration determines digit. The two pins are always opposed to each other.
 */
void pulsePingPong(int pinA, int pinB){
  digitalWrite(pinB, LOW);
  digitalWrite(pinA, HIGH);
  ets_delay_us(50);

  digitalWrite(pinA, LOW);
  digitalWrite(pinB, HIGH);
  ets_delay_us(50);
}

void pulse(int pinA, int pinB, int digit){
  int us = digit == 1 ? DCC_1_HALFPERIOD : DCC_0_HALFPERIOD;
  // TODO: For ESP32, avoid digital write.
  // ets_delay_us(1); // Optional delay for stability, adjust as needed
  // GPIO.out_w1tc = (1 << pinB); // Set the pin low
  // GPIO.out_w1ts = (1 << pinB); // Set the pin high

  digitalWrite(pinB, LOW);
  digitalWrite(pinA, HIGH);
  ets_delay_us(us);

  digitalWrite(pinA, LOW);
  digitalWrite(pinB, HIGH);
  ets_delay_us(us);
}

void pulsePreamble(int pinA, int pinB){
  for (int i = 0; i < 12; i++) {
    pulse(pinA, pinB, 1);
  }
}

void pulseAddress(int pinA, int pinB, dcc_packet_t *packet){  
  pulse(pinA, pinB, ((packet->address >> 0) & 1));
  pulse(pinA, pinB, ((packet->address >> 1) & 1));
  pulse(pinA, pinB, ((packet->address >> 2) & 1));
  pulse(pinA, pinB, ((packet->address >> 3) & 1));
  pulse(pinA, pinB, ((packet->address >> 4) & 1));
  pulse(pinA, pinB, ((packet->address >> 5) & 1));
  pulse(pinA, pinB, ((packet->address >> 6) & 1));
  pulse(pinA, pinB, ((packet->address >> 7) & 1));
  // pulse(pinA, pinB, ((address & (1 << 0)) >> 0));
}

void pulseData(int pinA, int pinB, dcc_packet_t *packet){  
  for (int i = 0; i < sizeof(packet->data); i++) {
    for (int j = 0; j < 8; j++) {
      pulse(pinA, pinB, ((packet->data[i] >> j) & 1));
    }
  }
}

// TODO: Remove checksum from dcc_packet_t?
void pulseChecksum(int pinA, int pinB, dcc_packet_t *packet){
  uint8_t checksum = dcc_packet_calc_checksum(packet);
  for (int i = 0; i < 8; i++) {
    pulse(pinA, pinB, ((packet->checksum >> i) & 1));
  }
}

void pulsePacket(int pinA, int pinB, dcc_packet_t *packet){
  pulsePreamble(DCCA_PIN, DCCB_PIN);
  pulse(pinA, pinB, 0);
  pulseAddress(pinA, pinB, packet);
  pulse(pinA, pinB, 0);
  pulseData(pinA, pinB, packet);
  pulse(pinA, pinB, 0);
  pulseChecksum(pinA, pinB, packet);
  pulse(pinA, pinB, 1);
}

void pulseIdle(int pinA, int pinB){
  dcc_packet_t packet;
  dcc_packet_init_idle(&packet);
  dcc_packet_calc_checksum(&packet);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Demo_DCC_Server_00 Setup enter");
  pinMode(DCCA_PIN, OUTPUT);
  pinMode(DCCB_PIN, OUTPUT);
  pinMode(DCC_SIGNAL_PIN, OUTPUT);

  digitalWrite(DCCA_PIN, LOW);
  digitalWrite(DCCB_PIN, LOW);
  digitalWrite(DCC_SIGNAL_PIN, HIGH);
  

  char msg[256];
  sprintf(msg, "Demo DCC signa using A=%d B=%d STATUS%d", DCCB_PIN, DCCB_PIN, LED_BUILTIN);
  Serial.println(msg);
}

void loop() {
  static int state = 1;
  // static TickType_t lastWakeTime = xTaskGetTickCount();

  unsigned long now_t = millis();
  static unsigned prev_t = 0;;
  if ((now_t - prev_t) > 5000 ){
    Serial.println("Heartbeat from loop()");
    prev_t = now_t;
  }

  if (Serial.available()) { // if there is data comming
    Serial.println("Serial available");
    String command = Serial.readStringUntil('\n');
    if (command == "1"){
      digitalWrite(DCC_SIGNAL_PIN, HIGH);
      state = 1;
      Serial.println("Setting state");
    } else {
      state = 0;
      digitalWrite(DCC_SIGNAL_PIN, LOW);
    }
  }

  if (state == 1){
    pulseIdle(DCCA_PIN, DCCB_PIN);
    pulsePingPong(DCCA_PIN, DCCB_PIN);
    pulse(DCCA_PIN, DCCB_PIN, 0);
  }
  //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  //digitalWrite(DCCA_PIN, !digitalRead(DCCA_PIN));
  //digitalWrite(DCCB_PIN, !digitalRead(DCCB_PIN));
}
