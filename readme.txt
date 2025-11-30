# BMW E9x/E8x → Haltech IO12 Box B CAN Bridge  
**ESP32-CAN-X2 | 100 % Working | No Wiring | Pure CAN | 100 Hz Update**

### Features
- Reads **four individual wheel speeds** directly from BMW PT-CAN  
- Reads **steering wheel angle**  
- Sends **perfect Haltech-compatible DPI frequency data** over CAN  
- Sends **keep-alive** so the IO12 Box B stays online  
- **100 Hz** update rate (every 10 ms) — faster than factory  
- No physical wheel speed sensors needed  
- No cutting wires  
- Works with ignition off / engine off / one wheel spinning  

### Haltech NSP Setup (Do this once)

| Channel        | CAN ID | Bytes | Type           | Multiplier | Edge    |
|----------------|--------|-------|----------------|------------|---------|
| Wheel Speed FL | 0x2C3  | 0–1   | Frequency      | **0.1**    | Falling |
| Wheel Speed FR | 0x2C3  | 4–5   | Frequency      | **0.1**    | Falling |
| Wheel Speed RL | 0x2C5  | 0–1   | Frequency      | **0.1**    | Falling |
| Wheel Speed RR | 0x2C5  | 4–5   | Frequency      | **0.1**    | Falling |
| Steering Angle | 0x2CF  | 0–1   | Generic Value  | **0.1**    | —       |

### Credits
- Original CAN research: openDBC / comma.ai
https://github.com/tolunaygul/IObox-emulator-haltech