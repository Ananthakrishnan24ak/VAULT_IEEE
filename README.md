# VAULT\_IEEE





**1. MAIN ESP32 (WROOM) CONNECTIONS**

**---------------------------------------------------------------------**

**Component         | ESP32 Pin | Direction | Description**

**------------------|-----------|-----------|--------------------------**

**Solenoid Relay    | GPIO 4    | OUTPUT    | Relays power to open lock**

**Serial2 RX        | GPIO 16   | INPUT     | Connect to XIAO TX (D6)**

**Serial2 TX        | GPIO 17   | OUTPUT    | Connect to XIAO RX (D7)**

**Keypad Row 1      | GPIO 13   | IN/OUT    | 4x4 Keypad Row 1**

**Keypad Row 2      | GPIO 12   | IN/OUT    | 4x4 Keypad Row 2**

**Keypad Row 3      | GPIO 14   | IN/OUT    | 4x4 Keypad Row 3**

**Keypad Row 4      | GPIO 27   | IN/OUT    | 4x4 Keypad Row 4**

**Keypad Col 1      | GPIO 26   | IN/OUT    | 4x4 Keypad Column 1**

**Keypad Col 2      | GPIO 25   | IN/OUT    | 4x4 Keypad Column 2**

**Keypad Col 3      | GPIO 33   | IN/OUT    | 4x4 Keypad Column 3**

**Keypad Col 4      | GPIO 32   | IN/OUT    | 4x4 Keypad Column 4**

**TFT SPI SCK       | GPIO 18   | OUTPUT    | Clock for ST7789/ILI9341**

**TFT SPI MOSI      | GPIO 23   | OUTPUT    | Data line for TFT Display**

**TFT SPI MISO      | GPIO 19   | INPUT     | Display Master In Slave Out**

**---------------------------------------------------------------------**



**\*Note: TFT CS, DC, and RST pin routing depends on your local configuration**

&#x20;**settings inside the TFT\_eSPI library "User\_Setup.h" file.**



**2. XIAO ESP32-S3 SENSE CONNECTIONS**

**---------------------------------------------------------------------**

**External Communications Header:**

**Component         | XIAO Label | GPIO Num  | Direction | Description**

**------------------|------------|-----------|-----------|-------------**

**Serial1 RX        | D7         | GPIO 44   | INPUT     | Connect to ESP32 TX2 (17)**

**Serial1 TX        | D6         | GPIO 43   | OUTPUT    | Connect to ESP32 RX2 (16)**



**Internal Camera Board Pins (Ribbon Interface Mapping):**

**Camera Signal     | XIAO GPIO  | Description**

**------------------|------------|-------------------------------------**

**FLASH\_LED\_PIN     | GPIO 21    | On-board Flash Expansion LED (Active LOW)**

**XCLK              | GPIO 10    | External Clock Input**

**SIOD (SDA)        | GPIO 40    | SCCB Data (I2C Protocol)**

**SIOC (SCL)        | GPIO 39    | SCCB Clock (I2C Protocol)**

**VSYNC             | GPIO 38    | Vertical Sync Pulse**

**HREF              | GPIO 47    | Horizontal Reference**

**PCLK              | GPIO 13    | Pixel Clock**

**Data Bus (Y2-Y9)  | 15,17,18,  | 8-bit Parallel Video Stream Bus**

&#x20;                 **| 16,14,12,  |** 

&#x20;                 **| 11, 48     |** 

**PWDN / RESET      | -1         | Unused/Tied directly on Sense Board**

**---------------------------------------------------------------------**





**3. SYSTEM INTER-BOARD SERIAL WIRING (CRITICAL)**

**---------------------------------------------------------------------**

**To allow communication, connect the cross-over lines directly:**



&#x20;       **\[MAIN ESP32 WROOM]                  \[XIAO ESP32-S3 SENSE]**

&#x20;    **=======================              =======================**

&#x20;    **TX2 (GPIO 17) --------- ----------> RX1 (D7 / GPIO 44)**

&#x20;    **RX2 (GPIO 16) <--------- ---------- TX1 (D6 / GPIO 43)**

&#x20;    **GND           --------------------> GND (Common Ground)**



**⚠️ WARNING: Ensure that both boards share a common ground line (GND).** 

**Without a shared ground wire, data signals over the RX/TX lines** 

**will become corrupted or unreadable.**

**=====================================================================**







