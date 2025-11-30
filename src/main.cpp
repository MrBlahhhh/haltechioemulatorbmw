#include <Arduino.h>
#include "driver/twai.h"
#include <mcp_can.h>

// ============================ PINS ============================
#define LED       2
#define CAN1_TX   GPIO_NUM_7
#define CAN1_RX   GPIO_NUM_6
#define CAN2_SCK  12
#define CAN2_MISO 13
#define CAN2_MOSI 11
#define CAN2_CS   10
#define CAN2_INT  3

MCP_CAN CAN2(CAN2_CS);

// ============================ BMW IDs ============================
#define ID_WHEEL_SPEED    0xCE    // 206 decimal — raw pulses
#define ID_STEERING_ANGLE 0xC4    // 196 decimal

// ============================ HALTECH DPI CAN IDs ============================
constexpr uint32_t DPI1_ID      = 0x2C3;   // FL + FR
constexpr uint32_t DPI2_ID      = 0x2C5;   // RL + RR
constexpr uint32_t STEERING_ID  = 0x2CF;
constexpr uint32_t KEEP_ID      = 0x2C7;

// ============================ Variables ============================
uint16_t fl_pulses = 0, fr_pulses =0, rl_pulses =0, rr_pulses =0;
float    steering_deg = 0.0f;

unsigned long tKeep = 0, tSend = 0, tDebug = 0, lastLed = 0;
bool ledState = false;
bool firstKeepSent = false;

unsigned long bmwMsgCount = 0, lastStatsTime = 0;

// ============================ FAST BLINK ============================
void fastBlink() {
  while (1) {
    digitalWrite(LED, HIGH); delay(50);
    digitalWrite(LED, LOW);  delay(50);
  }
}

// ============================ SETUP ============================
void setup() {
  delay(8000);
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  Serial.println(F("\n=== BMW E9x to Haltech IO12 – RAW PULSE to DPI CAN (FINAL) ==="));

  // CAN1 – BMW PT-CAN
  twai_general_config_t g_cfg = TWAI_GENERAL_CONFIG_DEFAULT(CAN1_TX, CAN1_RX, TWAI_MODE_NORMAL);
  twai_timing_config_t  t_cfg = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t  f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g_cfg, &t_cfg, &f_cfg) != ESP_OK || twai_start() != ESP_OK) {
    Serial.println("CAN1 failed!");
    fastBlink();
  }
  Serial.println("CAN1 BMW OK");

  // CAN2 – Haltech 1 Mbps
  SPI.begin(CAN2_SCK, CAN2_MISO, CAN2_MOSI, CAN2_CS);
  if (CAN2.begin(MCP_ANY, CAN_1000KBPS, MCP_16MHZ) != CAN_OK) {
    Serial.println("CAN2 failed!");
    fastBlink();
  }
  CAN2.setMode(MCP_NORMAL);
  pinMode(CAN2_INT, INPUT_PULLUP);
  Serial.println("CAN2 Haltech OK\n");

  lastStatsTime = millis();
}

// ============================ LOOP ============================
void loop() {
  unsigned long now = millis();

  // LED heartbeat
  if (now - lastLed >= 250) {
    lastLed = now;
    ledState = !ledState;
    digitalWrite(LED, ledState);
  }

  // Keep-alive
  if (now - tKeep >= 85) {
    tKeep = now;
    uint8_t keep[5] = {0x10, 0x09, 0x0B, 0x01, 0x00};
    if (CAN2.sendMsgBuf(KEEP_ID, 0, 5, keep) == CAN_OK && !firstKeepSent) {
      Serial.println("First Keep-Alive sent – IO12 Box B online");
      firstKeepSent = true;
    }
  }

  // Send data every 50 ms (20 Hz)
  if (now - tSend >= 50) {
    tSend = now;

    // Send raw BMW pulses ×10 → Haltech scales with ×0.1
    uint16_t fl_send = fl_pulses * 10;
    uint16_t fr_send = fr_pulses * 10;
    uint16_t rl_send = rl_pulses * 10;
    uint16_t rr_send = rr_pulses * 10;

    uint8_t msg1[8] = {
      lowByte(fl_send), highByte(fl_send), 0, 0,
      lowByte(fr_send), highByte(fr_send), 0, 0
    };
    uint8_t msg2[8] = {
      lowByte(rl_send), highByte(rl_send), 0, 0,
      lowByte(rr_send), highByte(rr_send), 0, 0
    };

    CAN2.sendMsgBuf(DPI1_ID, 0, 8, msg1);
    CAN2.sendMsgBuf(DPI2_ID, 0, 8, msg2);

    // Steering angle ×10
    int16_t steer_x10 = (int16_t)round(steering_deg * 10.0f);
    uint8_t steer_msg[8] = {lowByte(steer_x10), highByte(steer_x10), 0,0,0,0,0,0};
    CAN2.sendMsgBuf(STEERING_ID, 0, 8, steer_msg);
  }

  // Receive BMW messages
  twai_message_t rx;
  while (twai_receive(&rx, 0) == ESP_OK) {
    bmwMsgCount++;

    if (rx.identifier == ID_WHEEL_SPEED && rx.data_length_code == 8) {
      fl_pulses = ((uint16_t)rx.data[0] << 8) | rx.data[1];
      fr_pulses = ((uint16_t)rx.data[2] << 8) | rx.data[3];
      rl_pulses = ((uint16_t)rx.data[4] << 8) | rx.data[5];
      rr_pulses = ((uint16_t)rx.data[6] << 8) | rx.data[7];
    }

    else if (rx.identifier == ID_STEERING_ANGLE && rx.data_length_code >= 2) {
      int16_t raw = (int16_t)((rx.data[0] << 8) | rx.data[1]);
      steering_deg = raw * 0.04395f;
    }
  }

  // Debug every 5 s
  if (now - tDebug >= 5000) {
    tDebug = now;
    float elapsed = (now - lastStatsTime) / 1000.0f;
    Serial.printf("[%.0fs] BMW %.0fHz | Steer %.1f degrees | Pulses FL:%u FR:%u RL:%u RR:%u\n",
                  now/1000.0f, bmwMsgCount / elapsed, steering_deg,
                  fl_pulses, fr_pulses, rl_pulses, rr_pulses);
    bmwMsgCount = 0;
    lastStatsTime = now;
  }
}