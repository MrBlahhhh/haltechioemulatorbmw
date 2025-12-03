#include <Arduino.h>
#include <driver/twai.h>
#include <mcp_can.h>

#define LED       2
#define CAN1_TX   GPIO_NUM_7
#define CAN1_RX   GPIO_NUM_6
#define CAN2_SCK  12
#define CAN2_MISO 13
#define CAN2_MOSI 11
#define CAN2_CS   10

MCP_CAN CAN2(CAN2_CS);

constexpr uint32_t PD16_CONFIG_ID  = 0x6D1;
constexpr uint32_t PD16_ECR_ID     = 0x6D2;
constexpr uint32_t PD16_TELE_ID    = 0x6D3;
constexpr uint32_t PD16_STATUS_ID  = 0x6D5;

// Latest wheel speeds from BMW (1/16 km/h)
static uint16_t fl_kph16 = 0, fr_kph16 = 0, rl_kph16 = 0, rr_kph16 = 0;

// Correct scaling — gives ~550 Hz at 40 mph like a real PD16A
constexpr float HZ_PER_KPH = 8.545f;

unsigned long tConfig = 0, tTele = 0, tStatus = 0, tEcr = 0, lastLed = 0;
unsigned long lastPrint = 0;

void setup() {
  delay(8000);
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  Serial.println(F("\nPD16A EMULATOR – BIG-ENDIAN FIXED VERSION\n"));

  twai_general_config_t g_cfg = TWAI_GENERAL_CONFIG_DEFAULT(CAN1_TX, CAN1_RX, TWAI_MODE_NORMAL);
  twai_timing_config_t  t_cfg = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t  f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g_cfg, &t_cfg, &f_cfg) != ESP_OK || twai_start() != ESP_OK) {
    while(1) { digitalWrite(LED, !digitalRead(LED)); delay(100); }
  }

  SPI.begin(CAN2_SCK, CAN2_MISO, CAN2_MOSI, CAN2_CS);
  if (CAN2.begin(MCP_ANY, CAN_1000KBPS, MCP_16MHZ) != CAN_OK) {
    while(1) { digitalWrite(LED, !digitalRead(LED)); delay(100); }
  }
  CAN2.setMode(MCP_NORMAL);

  tConfig = tTele = tStatus = tEcr = millis();
}

void loop() {
  unsigned long now = millis();

  // Heartbeat LED
  if (now - lastLed >= 250) {
    lastLed = now;
    digitalWrite(LED, !digitalRead(LED));
  }

  // Receive BMW wheel speeds (0xCE)
  twai_message_t rx;
  while (twai_receive(&rx, 0) == ESP_OK) {
    if (rx.identifier == 0xCE && rx.data_length_code == 8) {
      fl_kph16 = (rx.data[1] << 8) | rx.data[0];
      fr_kph16 = (rx.data[3] << 8) | rx.data[2];
      rl_kph16 = (rx.data[5] << 8) | rx.data[4];
      rr_kph16   = (rx.data[7] << 8) | rx.data[6];
    }
  }

  // 20 Hz telemetry — matches real PD16A exactly
  if (now - tTele >= 50) {
    tTele = now;

    float fl_kph = fl_kph16 / 16.0f;
    float fr_kph = fr_kph16 / 16.0f;
    float rl_kph = rl_kph16 / 16.0f;
    float rr_kph = rr_kph16 / 16.0f;

    uint16_t fl_hz = (uint16_t)(fl_kph * HZ_PER_KPH + 0.5f);
    uint16_t fr_hz = (uint16_t)(fr_kph * HZ_PER_KPH + 0.5f);
    uint16_t rl_hz = (uint16_t)(rl_kph * HZ_PER_KPH + 0.5f);
    uint16_t rr_hz = (uint16_t)(rr_kph * HZ_PER_KPH + 0.5f);

    // Debug print
    if (now - lastPrint >= 500) {
      lastPrint = now;
      float avg_mph = ((fl_kph + fr_kph + rl_kph + rr_kph) / 4.0f) * 0.621371f;
      Serial.printf("Speed: %.1f mph → FL:%u FR:%u RL:%u RR:%u Hz\n", avg_mph, fl_hz, fr_hz, rl_hz, rr_hz);
    }

    // === CORRECT BIG-ENDIAN PD16A TELEMETRY FRAMES ===
    uint8_t tele[8] = {
      0x60,             // Mux FL
      0x01,             // State = active
      0x13, 0x88,       // 5000 mV
      0x01, 0xF4,       // 50% duty
      highByte(fl_hz),  // ← HIGH BYTE FIRST (big-endian)
      lowByte(fl_hz)    // ← low byte second
    };
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);

    tele[0] = 0x61;
    tele[6] = highByte(fr_hz);
    tele[7] = lowByte(fr_hz);
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);

    tele[0] = 0x62;
    tele[6] = highByte(rl_hz);
    tele[7] = lowByte(rl_hz);
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);

    tele[0] = 0x63;
    tele[6] = highByte(rr_hz);
    tele[7] = lowByte(rr_hz);
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);
  }

  // Keep-alive messages (unchanged — already correct)
  if (now - tConfig >= 500) { 
    tConfig = now;
    uint8_t cfg[8] = {0x60,0x03,0x00,0x00,0x88,0x13,0x01,0x00};
    for (int i = 0; i < 4; i++) { 
      CAN2.sendMsgBuf(PD16_CONFIG_ID, 0, 8, cfg); 
      cfg[0]++; 
    }
  }
  if (now - tEcr >= 490) { 
    tEcr = now;
    uint8_t ack[8] = {0,0,0,0,0,0,0,1};
    CAN2.sendMsgBuf(PD16_ECR_ID, 0, 8, ack);
  }
  if (now - tStatus >= 500) { 
    tStatus = now;
    uint8_t st[8] = {0,0,0,0,1,2,3,4};
    CAN2.sendMsgBuf(PD16_STATUS_ID, 0, 8, st);
  }
}