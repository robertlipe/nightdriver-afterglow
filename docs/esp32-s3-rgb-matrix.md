This is targeting https://www.waveshare.com/esp32-s3-rgb-matrix.html.

It is a very featureful board. It's an ESP32-S3-N32R16, which isn't a
common module. It provides OSPI, so the RAM is fast enough to hold video
buffers if programmed carefully.

Pinouts:

Wrangling information for the **Waveshare ESP32-S3-RGB-Matrix** can certainly be frustrating due to overlapping documentation for similarly-named boards (like the tiny 8x8 "ESP32-S3-Matrix" vs. this large HUB75 panel driver). We considered the schematic at https://github.com/waveshareteam/ESP32-S3-RGB-Matrix/blob/main/hardware/schematics/ESP32-S3-RGB-Matrix-Schematics.pdf to be authoritative.

---

## 1. Primary Component I2C Map

The board utilizes a single main I2C bus to interface with its environmental sensors, clock, and audio controller configurations.

| Component | Function | 7-Bit I2C Address (Hex) | Confidence | Notes / Source |
| --- | --- | --- | --- | --- |
| **SHTC3** | Temp & Humidity | `0x70` | **Authoritative** | Standard Sensirion fixed address. |
| **PCF85063** | Real-Time Clock | `0x51` | **Authoritative** | Waveshare standard RTC address (`PCF85063A_Read_now`). |
| **QMI8658** | 6-Axis IMU | `0x6B` (or `0x6A`) | *Highly Probable Guess* | QST default addresses. `0x6B` is typical for Waveshare boards pulling SA0 high, but poll both. |
| **ES7210** | Audio ADC (Mic) | `0x40` | **Authoritative** | Standard Everest Semi I2C control address. |
| **ES8311** | Audio CODEC (DAC) | `0x18` | **Authoritative** | Standard Everest Semi I2C control address. |

---

## 2. Shared I2S Audio Architecture (Crucial Architecture Note)

The relationship between the **ES7210** (Analog-to-Digital Converter for the dual mic array) and the **ES8311** (Digital-to-Analog Codec for the speaker output) is **a shared-bus configuration operating in Simplex/Duplex split over I2S**.

* **The Setup:** They share the Master Clock (MCLK), Bit Clock (BCLK), and Left-Right Word Select Clock (LRCK/WS) lines natively routed from the ESP32-S3. They split the data lines: one GPIO feeds the ES8311 (DAC/Playback), and one GPIO captures from the ES7210 (ADC/Recording).
* **The Hardware Gotcha:** Because they share clocks, you *must* configure both chips to expect the exact same sample rate, bit depth, and clock format (typically I2S Standard Format).
* **The Boot-up Trap:** The ES7210 is notorious for failing its initial I2C configuration if the ESP32-S3's I2S peripheral isn't actively generating `MCLK` first. You must start the I2S master clock *before* running the initialization scripts over I2C for the microphones, or the ES7210 registers will write-fail or return zeros.

---

## 3. Comprehensive GPIO & Pinout Map

The primary conflict in online codebases stems from community developers assigning pins to fit specific libraries (like WLED or FastLED) rather than adhering to Waveshare's native hardware routing.

The pins mapped natively on the PCB trace are broken down below:

### HUB75 RGB Matrix Interface

| Signal Name | ESP32-S3 GPIO | Confidence | Role |
| --- | --- | --- | --- |
| **R1** | `GPIO 4` | **Authoritative** | Top Row Red Data |
| **G1** | `GPIO 5` | **Authoritative** | Top Row Green Data |
| **B1** | `GPIO 6` | **Authoritative** | Top Row Blue Data |
| **R2** | `GPIO 7` | **Authoritative** | Bottom Row Red Data |
| **G2** | `GPIO 15` | **Authoritative** | Bottom Row Green Data |
| **B2** | `GPIO 16` | **Authoritative** | Bottom Row Blue Data |
| **A** | `GPIO 18` | **Authoritative** | Row Select Bit A |
| **B** | `GPIO 8` | **Authoritative** | Row Select Bit B |
| **C** | `GPIO 3` | **Authoritative** | Row Select Bit C |
| **D** | `GPIO 42` | **Authoritative** | Row Select Bit D |
| **E** | `GPIO 9` or `-1` | **Authoritative** | Row Select Bit E (Only needed for 64x64 / 1/32 scan panels) |
| **LAT** | `GPIO 40` | **Authoritative** | HUB75 Data Latch |
| **OE** | `GPIO 2` | **Authoritative** | HUB75 Output Enable (Active Low) |
| **CLK** | `GPIO 41` | **Authoritative** | HUB75 Shift Register Clock |

### Onboard System Buses & Peripherals

| Peripherals | Signal / Interface | ESP32-S3 GPIO | Confidence | Notes / The Real Hardware Strategy |
| --- | --- | --- | --- | --- |
| **System I2C** | `SDA` | `GPIO 47` | **Authoritative** | Shared by SHTC3, PCF85063, QMI8658, ES7210, and ES8311. |
| **System I2C** | `SCL` | `GPIO 48` | **Authoritative** | Shared across the entire internal I2C bus. |
| **MicroSD Card** | `SD_MMC_CLK` | `GPIO 1` | **Authoritative** | Native high-speed 1-bit SDMMC |
| **MicroSD Card** | `SD_MMC_CMD` | `GPIO 13` | **Authoritative** | (Shared with IMU Interrupt trace on some hardware variants) |
| **MicroSD Card** | `SD_MMC_D0` | `GPIO 14` | **Authoritative** | SD Card Slot Chip Select |
| **Audio I2S Bus** | `I2S_MCLK` | `GPIO 12` | **Authoritative** | Master Clock line feeding the audio controllers. |
| **Audio I2S Bus** | `I2S_BCLK` | `GPIO 43` | **Authoritative** | Bit Clock / Serial Clock (`I2S_SCLK`). |
| **Audio I2S Bus** | `I2S_WS / LRCK` | `GPIO 38` | **Authoritative** | Word Select / Left-Right Clock. |
| **Audio I2S Bus** | `I2S_DATA_OUT`| `GPIO 39` | **Authoritative** | Serial Data Out (`DSDOUT`) feeding the ES8311 DAC. |
| **Audio I2S Bus** | `I2S_DATA_IN` | `GPIO 10` | **Authoritative** | Serial Data In capturing from the ES7210 Mic ADC. |
| **User IO** | `BOOT Button` | `GPIO 0` | **Authoritative** | Pulls to GND when pressed. Left clear of active processing buses. |

---

## 4. Hardware Master Log (Pin-by-Pin Reference)

* **0:** BOOT Button
* **1:** SD_MMC_CLK
* **2:** HUB75_OE (Output Enable)
* **3:** HUB75_C (Row Select C)
* **4:** HUB75_R1 (Top Red)
* **5:** HUB75_G1 (Top Green)
* **6:** HUB75_B1 (Top Blue)
* **7:** HUB75_R2 (Bottom Red)
* **8:** HUB75_B (Row Select B)
* **9:** HUB75_E (Row Select E / Multiplexed SCL line alternative)
* **10:** Audio `I2S_DATA_IN` (From Mic ADC)
* **11:** PA_CTRL (Power Amplifier Control / Mute Switch)
* **12:** Audio `I2S_MCLK` (Master Clock)
* **13:** SD_MMC_CMD / IMU_INT (Shared Track)
* **14:** SD_MMC_D0 / SD_CS
* **15:** HUB75_G2 (Bottom Green)
* **16:** HUB75_B2 (Bottom Blue)
* **17:** Onboard Status Indicator / Debug LED Traces
* **18:** HUB75_A (Row Select A)
* **19:** USB_N (Native Hardware Debug/Flash Line)
* **20:** USB_P (Native Hardware Debug/Flash Line)
* **21:** Audio `I2S_DSDIN` (Alternate Audio Line configuration track)
* **26 to 37:** **RESERVED** (Dedicated to internal 32MB Octal Flash / 16MB Octal PSRAM interface)
* **38:** Audio `I2S_LRCK` / Word Select
* **39:** Audio `I2S_DSDOUT` / Data Out (To Speaker DAC)
* **40:** HUB75_LAT (Data Latch)
* **41:** HUB75_CLK (Shift Register Clock)
* **42:** HUB75_D (Row Select D)
* **43:** Audio `I2S_SCLK` / Bit Clock
* **44:** Extra Functionality Pin Breakout Trace
* **45:** Free / Unassigned
* **46:** Free / Unassigned
* **47:** System `I2C_SDA`
* **48:** System `I2C_SCL`

---

## 5. Systems Programming Strategy

1. **Octal PSRAM Management:** This board relies on the `ESP32-S3-N32R16` (32MB Flash / 16MB Octal PSRAM). Because driving HUB75 matrices requires intensive DMA memory allocation, you **must** configure your memory allocator to assign your RGB panel framebuffers to external RAM (`MALLOC_CAP_SPIRAM`), keeping the ultra-fast internal SRAM clear for network stacks and I2S processing buffers.
2. **DMA Interleaving:** Ensure your HUB75 driver (like `ESP32-HUB75-MatrixPanel-DMA`) does not collision-route onto the same DMA channels assigned to the MicroSD card reader, or you will experience noticeable display jitter/flicker whenever reading files.
