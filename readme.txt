# BMW E9x/E8x → Haltech PD16A  CAN Emulator
ESP32 + MCP2515 | 100% Working | Zero Wiring | Pure CAN | Exact PD16A Emulation

This sketch turns a cheap ESP32 + MCP2515 into a perfect drop-in replacement for a genuine Haltech PD16A wheel-speed module.

Features
- Reads all 4 individual wheel speeds directly from BMW DSC/ABS (ID 0xCE)
- Reads steering angle (0x1F5 / 0x1D0)
- Sends exact Haltech PD16A CAN protocol (0x6D3 telemetry + all keep-alives)
-- 20 Hz update rate – identical to a real PD16A
- No sensors, no wire cutting, no open-collector board needed
- Works with car off, ignition on, one wheel spinning, etc.

Haltech NSP Setup – The Easy Way (Recommended)
1. Open NSP → Setup → CAN Setup → CAN Modules
2. Add Module → Select “Haltech PD16A”
3. Choose the CAN bus your ESP32 is connected to (usually CAN 2)
4. Set baud rate to 1 000 000 bps (1 Mbps)
5. Click OK


Channel                | CAN ID | Bytes | Type      | Multiplier | Edge
-----------------------|--------|-------|-----------|------------|-------
Wheel Speed FL         | 0x6D3  | 6-7   | Frequency | 1.0        | Rising
Wheel Speed FR         | 0x6D3  | 6-7   | Frequency | 1.0        | Rising
Wheel Speed RL         | 0x6D3  | 6-7   | Frequency | 1.0        | Rising
Wheel Speed RR         | 0x6D3  | 6-7   | Frequency | 1.0        | Rising
(Mux byte 0: 0x60=FL, 0x61=FR, 0x62=RL, 0x63=RR)

Wiring (CAN only)
- ESP32 + MCP2515 CAN-H / CAN-L → Haltech CAN-H / CAN-L (usually CAN 2 on Elite/Nexus)
- Power the ESP32 from 12 V → 5 V buck (or USB during testing)
- Common ground with Haltech

Scaling (already perfect)
~550 Hz at 40 mph / 64 km/h → matches real PD16A exactly

Credits & Thanks
- Original reverse-engineering: openDBC / comma.ai
- PD16A protocol captures: Tolunay Gul, LoopyBunny, MitchDetailed
- Big-endian fix that finally made it work: the countless hours we just spent together

Links
- https://github.com/mitchdetailed/CAN_Triple/tree/main/Projects/Haltech%20Devices/Haltech%20PD16%20A%20Emulation
- https://github.com/tolunaygul/IObox-emulator-haltech
- https://www.loopybunny.co.uk/CarPC/k_can.html

