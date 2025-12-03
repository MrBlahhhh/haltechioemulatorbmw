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
#define CAN2_INT  3

MCP_CAN CAN2(CAN2_CS);

#define ID_WHEEL_SPEED    0xCE
#define ID_STEERING_ANGLE 0xC4

constexpr uint32_t PD16_CONFIG_ID = 0x6D1;
constexpr uint32_t PD16_ECR_ID    = 0x6D2;
constexpr uint32_t PD16_TELE_ID   = 0x6D3;
constexpr uint32_t PD16_STATUS_ID = 0x6D5;
constexpr uint32_t STEERING_ID    = 0x2CF;

uint16_t fl_pulses = 0, fr_pulses = 0, rl_pulses = 0, rr_pulses = 0;
uint16_t fl_prev = 0, fr_prev = 0, rl_prev = 0, rr_prev = 0;

unsigned long tConfig = 0, tTele = 0, tStatus = 0, tEcr = 0, lastLed = 0;

void fastBlink() { while(1) { digitalWrite(LED, !digitalRead(LED)); delay(50); } }

void setup() {
  delay(8000);
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  Serial.println(F("\nPD16 EMULATOR – WORKING 2025 VERSION\n"));

  twai_general_config_t g_cfg = TWAI_GENERAL_CONFIG_DEFAULT(CAN1_TX, CAN1_RX, TWAI_MODE_NORMAL);
  twai_timing_config_t  t_cfg = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t  f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g_cfg, &t_cfg, &f_cfg) != ESP_OK || twai_start() != ESP_OK) fastBlink();

  SPI.begin(CAN2_SCK, CAN2_MISO, CAN2_MOSI, CAN2_CS);
  if (CAN2.begin(MCP_ANY, CAN_1000KBPS, MCP_16MHZ) != CAN_OK) fastBlink();
  CAN2.setMode(MCP_NORMAL);

  tConfig = tTele = tStatus = tEcr = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastLed >= 250) { lastLed = now; digitalWrite(LED, !digitalRead(LED)); }

  // BMW wheel pulses
  twai_message_t rx;
while (twai_receive(&rx, 0) == ESP_OK) {
  if (rx.identifier == 0xCE && rx.data_length_code == 8) {
    // These are already km/h × 16 (0.0625 km/h per LSB)
    uint16_t fl_kph16 = (rx.data[1] << 8) | rx.data[0];  // little-endian!
    uint16_t fr_kph16 = (rx.data[3] << 8) | rx.data[2];
    uint16_t rl_kph16 = (rx.data[5] << 8) | rx.data[4];
    uint16_t rr_kph16 = (rx.data[7] << 8) | rx.data[6];

    // Convert to real Hz for PD16 DPI telemetry
    // 48 pulses/rev, 275/35R18 → 2.042 m/rev → 489.7 pulses/km
    // → Hz = (km/h × 16) / (3600 / 489.7) ≈ (km/h × 16) × 0.136
    uint16_t fl_hz = (uint32_t)fl_kph16 * 136 / 1000;  // rounded coefficient
    uint16_t fr_hz = (uint32_t)fr_kph16 * 136 / 1000;
    uint16_t rl_hz = (uint32_t)rl_kph16 * 136 / 1000;
    uint16_t rr_hz = (uint32_t)rr_kph16 * 136 / 1000;

    fl_prev = fl_hz; fr_prev = fr_hz; rl_prev = rl_hz; rr_prev = rr_hz;
    // No delta/rollover needed — just use directly
  }
}



  // CONFIG – 2 Hz
  if (now - tConfig >= 500) {
    tConfig = now;
    uint8_t cfg[8] = {0x60, 0x03, 0x00, 0x00, 0x88, 0x13, 0x01, 0x00};
    CAN2.sendMsgBuf(PD16_CONFIG_ID, 0, 8, cfg);
    cfg[0]++; CAN2.sendMsgBuf(PD16_CONFIG_ID, 0, 8, cfg);
    cfg[0]++; CAN2.sendMsgBuf(PD16_CONFIG_ID, 0,8, cfg);
    cfg[0]++; CAN2.sendMsgBuf(PD16_CONFIG_ID, 0,8, cfg);
  }

  // ECR ACK – 2 Hz
  if (now - tEcr >= 490) {
    tEcr = now;
    uint8_t ack[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01};
    CAN2.sendMsgBuf(PD16_ECR_ID, 0, 8, ack);
  }

  // STATUS – normal mode immediately
  if (now - tStatus >= 500) {
    tStatus = now;
    uint8_t st[8] = {0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04};
    CAN2.sendMsgBuf(PD16_STATUS_ID, 0, 8, st);
  }

  // TELEMETRY – 20 Hz – BIG-ENDIAN VOLTAGE & DUTY
  if (now - tTele >= 50) {
    tTele = now;

    uint16_t fl_d = (fl_pulses >= fl_prev) ? fl_pulses - fl_prev : fl_pulses + 65536 - fl_prev;
    uint16_t fr_d = (fr_pulses >= fr_prev) ? fr_pulses - fr_prev : fr_pulses + 65536 - fr_prev;
    uint16_t rl_d = (rl_pulses >= rl_prev) ? rl_pulses - rl_prev : rl_pulses + 65536 - rl_prev;
    uint16_t rr_d = (rr_pulses >= rr_prev) ? rr_pulses - rr_prev : rr_pulses + 65536 - rr_prev;

    uint16_t fl_hz = fl_d * 20;  // 50 ms interval
    uint16_t fr_hz = fr_d * 20;
    uint16_t rl_hz = rl_d * 20;
    uint16_t rr_hz = rr_d * 20;

    fl_prev = fl_pulses; fr_prev = fr_pulses; rl_prev = rl_pulses; rr_prev = rr_pulses;

    uint8_t tele[8] = {
      0x60, 0x01,              // Mux + State ON
      0x13, 0x88,              // VOLTAGE 5000 mV – BIG ENDIAN
      0x01, 0xF4,              // DUTY 500 – BIG ENDIAN → 50.0 %
      lowByte(fl_hz), highByte(fl_hz)   // Frequency – little-endian (as normal)
    };
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);

    tele[0] = 0x61; tele[6] = lowByte(fr_hz); tele[7] = highByte(fr_hz);
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);

    tele[0] = 0x62; tele[6] = lowByte(rl_hz); tele[7] = highByte(rl_hz);
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);

    tele[0] = 0x63; tele[6] = lowByte(rr_hz); tele[7] = highByte(rr_hz);
    CAN2.sendMsgBuf(PD16_TELE_ID, 0, 8, tele);
  }
}