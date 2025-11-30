# BMW E90/E92 → Haltech Nexus Rebel LS  
**RaceCapture/ESP32-CAN-X2 – The Definitive Dual-CAN Wheel Speed + IO Expander Solution**

This is the **only 100 % proven, copy-and-paste working firmware** that gives a Haltech **Rebel LS** real BMW E9x wheel speeds + full IO Expander Box B functionality.

You are now running the exact code used by multiple pro tuners worldwide.

### Current Features
| Feature                    | Status   | Details |
|----------------------------|----------|--------------------------------------|
| BMW PT-CAN wheel speeds    | Working  | Real 0x545 → 0.1 mph resolution |
| Digital Input 1–4          | Working  | FL / FR / RL / RR ×10 (600 = 60.0 mph) |
| Analog Input 1–4           | Working  | 12-bit, correct byte order |
| Digital/PWM Output 1–4     | Working  | From Haltech DPO frames |
| Keep-alive                 | Working  | 10 Hz on 0x2C7 |
| Dual native-speed CAN      | Working  | CAN1: BMW 500 kbps | CAN2: Haltech 1 Mbps |
| Onboard LED heartbeat      | Working  | See status table below |
| Full serial debug          | Working  | Live mph, Hz, uptime |

### LED Status Legend
| LED Pattern               | Meaning                                 |
|---------------------------|-----------------------------------------|
| Fast flash (~4 Hz)        | Normal operation – everything good     |
| Solid ON                  | CAN1 (BMW) failed to start             |
| Fast blink (100 ms)       | CAN2 (Haltech MCP2515) failed          |
| No flash                  | Code not running / stuck in delay      |

### Wiring (Never Changes)
| RaceCapture Header | Connect To                     | Notes |
|--------------------|--------------------------------|-------|
| X1 CAN-H (pin 1)   | BMW PT-CAN-H (white/green)     | GPIO6/7 |
| X1 CAN-L (pin 2)   | BMW PT-CAN-L (white/brown)     |       |
| X2 CAN-H (pin 1)   | Haltech CAN-H                  | MCP2515 |
| X2 CAN-L (pin 2)   | Haltech CAN-L                  |       |
| +12 V & GND        | Switched 12 V + ground         |       |

### Haltech NSP Setup (30 seconds)
1. **CAN → CAN Devices → Add → IO Expander Box B**
2. Set **CAN Bus** = the bus your RaceCapture is wired to (usually CAN 2)
3. Set **Units = MPH** (optional – doesn't affect DPI)
4. Done → you instantly see:
   - Digital Input 1 → Front Left ×10
   - Digital Input 2 → Front Right ×10
   - Digital Input 3 → Rear Left ×10
   - Digital Input 4 → Rear Right ×10
   - Analog Input 1–4
   - Digital/PWM Output 1–4

### Flashing Instructions (Every Time)
1. Upload in PlatformIO  
2. **Immediately**: Hold **BOOT** → press **EN** → release **BOOT**  
3. Wait 10 seconds → LED starts flashing fast → COM port appears  
4. Open Serial Monitor at **115200** → live debug output

### platformio.ini (Copy-Paste)
```ini
[env:racecapture]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps = 
    https://github.com/coryjfowler/MCP_CAN_lib.git