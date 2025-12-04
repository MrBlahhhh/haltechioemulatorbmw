# racecapture esp32-can-x2 BMW E9x/E8x → Haltech PD16A  CAN Emulator

Features
- Reads all 4 individual wheel speeds directly from BMW DSC/ABS (ID 0xCE)
- Reads steering angle (0x1F5 / 0x1D0)
- Sends exact Haltech PD16A CAN protocol (0x6D3 telemetry + all keep-alives)
- Correct big-endian frequency bytes


sensor, wheelspeed Frequency Input Setup (if you don’t want to use the PD16A template)

Wheel              | PD16A SPI Channel | Mux byte (byte 0) | CAN ID | Bytes | Type      | Multiplier | Edge
-------------------|-------------------|-------------------|--------|-------|-----------|------------|-------
Front Left (FL)    | SPI 1             | 0x60              | 0x6D3  | 6-7   | Frequency | 1.0        | Rising
Front Right (FR)   | SPI 2             | 0x61              | 0x6D3  | 6-7   | Frequency | 1.0        | Rising
Rear Left (RL)     | SPI 3             | 0x62              | 0x6D3  | 6-7   | Frequency | 1.0        | Rising
Rear Right (RR)    | SPI 4             | 0x63              | 0x6D3  | 6-7   | Frequency | 1.0        | Rising

haltech can splitter into can2
bmw can hi/lo into can1


Credits & Thanks
- openDBC / comma.ai
- Tolunay Gul
- MitchDetailed
- LoopyBunny

Links
- https://github.com/mitchdetailed/CAN_Triple/tree/main/Projects/Haltech%20Devices/Haltech%20PD16%20A%20Emulation
- https://github.com/tolunaygul/IObox-emulator-haltech
- https://www.loopybunny.co.uk/CarPC/k_can.html
