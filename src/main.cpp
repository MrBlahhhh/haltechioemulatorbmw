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

// BMW PT-CAN IDs
constexpr uint32_t BMW_WHEEL_ID = 0x0CE;  // wheel speeds (1/16 km/h, LE)
constexpr uint32_t BMW_SAS_ID   = 0x0C4;  // steering angle (0.1°/bit, signed LE)

// Latest wheel speeds from BMW (1/16 km/h)
static uint16_t fl_kph16 = 0, fr_kph16 = 0, rl_kph16 = 0, rr_kph16 = 0;

// Correct scaling — gives ~550 Hz at 40 mph like a real PD16A
constexpr float HZ_PER_KPH = 8.545f;

// Steering angle state
static int16_t  steer_raw    = 0;
static bool     steer_valid  = false;
static uint32_t steer_last_t = 0;

// Steering angle encoding:
// BMW raw is signed 16-bit, 0.1°/bit, range ±7200 (±720°).
// We map ±540° to 0–5000 mV for the Haltech AVI1 input.
// 0° = 2500 mV (mid-scale), +540° = 5000 mV, -540° = 0 mV.
// Haltech NSP calibration table:
//   0 mV    → -540.0°
//   2500 mV →    0.0°
//   5000 mV →  +540.0°
constexpr float STEER_MV_CENTER  = 2500.0f;
constexpr float STEER_MV_PER_DEG = 2500.0f / 540.0f;  // ~4.63 mV/°

unsigned long tConfig = 0, tTele = 0, tStatus = 0, tEcr = 0, lastLed = 0;
unsigned long lastPrint = 0;

void setup() {
  delay(8000);
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  Serial.println(F("\nPD16A EMULATOR – WHEEL SPEEDS + STEERING ON AVI1 (mux 0x80)\n"));

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

  // ── Receive BMW PT-CAN frames ─────────────────────────────────────────────
  twai_message_t rx;
  while (twai_receive(&rx, 0) == ESP_OK) {

    // Wheel speeds (0x0CE) — unchanged from original
    if (rx.identifier == BMW_WHEEL_ID && rx.data_length_code == 8) {
      fl_kph16 = (rx.data[1] << 8) | rx.data[0];
      fr_kph16 = (rx.data[3] << 8) | rx.data[2];
      rl_kph16 = (rx.data[5] << 8) | rx.data[4];
      rr_kph16 = (rx.data[7] << 8) | rx.data[6];
    }

    // Steering Angle Sensor (0x0C4) — signed 16-bit LE, 0.1°/bit
    if (rx.identifier == BMW_SAS_ID && rx.data_length_code >= 2) {
      steer_raw    = (int16_t)(((uint16_t)rx.data[1] << 8) | rx.data[0]);
      steer_valid  = true;
      steer_last_t = now;
    }
  }

  // Stale guard: no SAS frame for 2000 ms → hold centre
  if (steer_valid && (now - steer_last_t > 2000)) {
    steer_valid = false;
  }

  // ── 20 Hz PD16A telemetry ─────────────────────────────────────────────────
  if (now - tTele >= 50) {
    tTele = now;

    // Wheel speed → Hz (unchanged from original)
    float fl_kph = fl_kph16 / 16.0f;
    float fr_kph = fr_kph16 / 16.0f;
    float rl_kph = rl_kph16 / 16.0f;
    float rr_kph = rr_kph16 / 16.0f;

    uint16_t fl_hz = (uint16_t)(fl_kph * HZ_PER_KPH + 0.5f);
    uint16_t fr_hz = (uint16_t)(fr_kph * HZ_PER_KPH + 0.5f);
    uint16_t rl_hz = (uint16_t)(rl_kph * HZ_PER_KPH + 0.5f);
    uint16_t rr_hz = (uint16_t)(rr_kph * HZ_PER_KPH + 0.5f);

    // Steering → millivolts (0–5000 mV)
    // steer_raw is in 0.1° units, so divide by 10 to get degrees
    float steer_deg = steer_raw * 0.1f;
    float steer_mV  = STEER_MV_CENTER + (steer_deg * STEER_MV_PER_DEG);
    steer_mV = constrain(steer_mV, 0.0f, 5000.0f);
    uint16_t avi1_mV = steer_valid ? (uint16_t)(steer_mV + 0.5f) : 2500; // hold centre if no signal

    // Debug print
    if (now - lastPrint >= 500) {
      lastPrint = now;
      float avg_mph = ((fl_kph + fr_kph + rl_kph + rr_kph) / 4.0f) * 0.621371f;
      Serial.printf("Speed: %.1f mph → FL:%u FR:%u RL:%u RR:%u Hz | Steer: %.1f° = %u mV (%s)\n",
                    avg_mph, fl_hz, fr_hz, rl_hz, rr_hz,
                    steer_deg, avi1_mV, steer_valid ? "OK" : "NO SIG");
    }

    // ── SPI1–4 frames (mux 0x60–0x63) — unchanged from original ─────────
    uint8_t tele[8] = {
      0x60,             // SPI1
      0x01,
      0x13, 0x88,       // 5000 mV
      0x01, 0xF4,       // 50% duty
      highByte(fl_hz),
      lowByte(fl_hz)
    };
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);

    tele[0] = 0x61;    // SPI2
    tele[6] = highByte(fr_hz);
    tele[7] = lowByte(fr_hz);
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);

    tele[0] = 0x62;    // SPI3
    tele[6] = highByte(rl_hz);
    tele[7] = lowByte(rl_hz);
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);

    tele[0] = 0x63;    // SPI4
    tele[6] = highByte(rr_hz);
    tele[7] = lowByte(rr_hz);
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);

    // ── AVI1 frame (mux 0x80) — steering angle in mV ─────────────────────
    // Per Haltech reference: bytes 2-3 = mV, bytes 4-7 = 0x00
    uint8_t avi[8] = {
      0x80,                              // AVI1 mux
      (uint8_t)(steer_valid ? 0x01 : 0x00), // state
      (uint8_t)(avi1_mV >> 8),           // mV high byte
      (uint8_t)(avi1_mV & 0xFF),         // mV low byte
      0x00, 0x00, 0x00, 0x00             // bytes 4-7 always zero for AVI
    };
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, avi);
  }

  // ── Keep-alive messages — unchanged from original ─────────────────────────
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
