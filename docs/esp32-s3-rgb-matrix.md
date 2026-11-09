# Waveshare ESP32-S3-RGB-Matrix (HUB75 Driver)

Hardware guide and architecture documentation for the [Waveshare ESP32-S3-RGB-Matrix](https://www.waveshare.com/esp32-s3-rgb-matrix.html).

Schematic reference: [ESP32-S3-RGB-Matrix Schematics (PDF)](https://github.com/waveshareteam/ESP32-S3-RGB-Matrix/blob/main/hardware/schematics/ESP32-S3-RGB-Matrix-Schematics.pdf).

---

## 1. Hardware Architecture Overview

* **MCU Module:** ESP32-S3-WROOM-2-N32R16V
  * 32 MB Octal SPI Flash
  * 16 MB Octal SPI PSRAM (`MALLOC_CAP_SPIRAM`)
  * Dual-core Xtensa LX7 up to 240 MHz
* **Display Interface:** HUB75 / HUB75E RGB matrix panel driver via DMA.
* **Audio Subsystem:** Everest Semiconductor duplex codec pair (ES7210 ADC + ES8311 DAC) sharing I2S clocks, plus NS4150B Class-D speaker amplifier.
* **Onboard Sensors & RTC:** SHTC3 temperature/humidity, QMI8658 6-axis IMU, PCF85063 RTC on a shared I2C bus.

---

## 2. Shared I2C Bus Map (GPIO 47 SDA / GPIO 48 SCL)

The board routes a single master I2C bus for all onboard peripherals, managed through the `SystemHardwareHub` singleton.

| 7-bit Address | Component | Function | Status / Notes |
| --- | --- | --- | --- |
| `0x18` | **ES8311** | Audio DAC / Playback | Shares I2S clocks with ES7210; feeds NS4150B amp |
| `0x40` | **ES7210** | Audio ADC / Microphones | Dual-channel analog mic capture via `ES7210` driver |
| `0x51` | **PCF85063** | Real-Time Clock | Standard NXP/Waveshare RTC |
| `0x6B` | **QMI8658** | 6-Axis IMU (Accel/Gyro) | **SA0 is pulled high**, placing it at `0x6B` (not `0x6A`) |
| `0x70` | **SHTC3** | Temperature & Humidity | Sensirion sensor with clock stretching |

> [!NOTE]
> **GPIO 47 & 48 vs. Octal Flash:** Early documentation warned that GPIO 47/48 might conflict with OPI flash lines. On the N32R16V module, the Octal memory interface uses GPIO 33–37. GPIO 47 (`I2C_SDA_PIN`) and GPIO 48 (`I2C_SCL_PIN`) are dedicated solely to the onboard peripheral bus and work reliably with standard ESP-IDF I2C drivers.

---

## 3. Audio Subsystem (ES7210 ADC + ES8311 DAC)

The audio architecture uses a shared-clock, split-data I2S topology:

```
                  +---------------------------+
                  |         ESP32-S3          |
                  |  (I2S Master Clock Source)|
                  +---+-------------+-----+---+
                      |             |     |
         MCLK (GPIO 12)             |     |
         BCLK (GPIO 43)             |     |
         LRCK (GPIO 38)             |     |
                      |             |     |
            +---------+---+         |     |
            |             |         |     |
            v             v         |     |
      +-----------+ +-----------+   |     |
      |  ES7210   | |  ES8311   |   |     |
      | (Mic ADC) | | (Spk DAC) |   |     |
      +-----+-----+ +-----+-----+   |     |
            |             ^         |     |
   DIN (ASDOUT)           |         |     |
      GPIO 39 ------------+         |     |
                                    |     |
                          DOUT (DSDIN)    |
                             GPIO 21 <----+
                                          |
                              PA_EN (GPIO 11)
                                          |
                                          v
                                    +-----------+
                                    |  NS4150B  |
                                    | (Amp PA)  |
                                    +-----------+
```

### Critical Audio Bring-up Rules

1. **Clock Generation Order:** The ES7210 will not exit reset or acknowledge register writes properly unless external clocks are running. The ESP32 I2S channel must be initialized and enabled as master (`MCLK = 256 * Fs = 6.144 MHz` at 24 kHz) **before** calling `ES7210::Configure()`.
2. **Format:** Standard Philips I2S, 16-bit stereo (`I2S_DATA_BIT_WIDTH_16BIT`, `I2S_SLOT_MODE_STEREO`). For two microphones (MIC1 + MIC2), register `0x11` is set to `0x60` and `0x12` to `0x00`. (TDM mode `0x12 = 0x02` is only used for 3 or 4 microphones).
3. **PGA & Software Gain:**
   * Analog PGA Gain: **30.0 dB** (`SetGain(kMic1 | kMic2, 30.0f)`).
   * Digital Scaling: **1:1** (`kES7210SoftwareGain = 1`) into the 16-bit sample buffer. This preserves full dynamic range without clipping on normal speech.
4. **DSP & AGC Tuning (`kParamsES7210`):**
   * `quietEnvFloorGate = 100000.0f`: Cleanly gates out quiet room silence (~20k energy) while keeping soft music and speech responsive.
   * `energyMaxEnvCap = 0.0f`: Uncapped envelope prevents flatline clipping across frequency bands on loud sound.
   * `frameSNRGate = 0.0f`: Disabled to prevent chattering/pulsing on ambient noise floor.
   * `compressGamma = (1.0f / 3.0f)`: Cube-root perceptual energy compression.
5. **Speaker Amplifier Control:** GPIO 11 (`AUDIO_PA_PIN`) controls the NS4150B power amplifier. Held `LOW` during microphone-only operation to avoid speaker hiss and amplifier power consumption.
6. **Telemetry & Overhead:** Debug telemetry in `soundanalyzer.cpp` is enclosed in `#if DEBUG_AUDIO_PARAMS`. In production (`0`), all sample scanning loops and hashing are completely eliminated at compile time.

---

## 4. Complete GPIO Pinout Map

| GPIO | Function | Subsystem | Notes |
| --- | --- | --- | --- |
| **0** | `BOOT` Button | User Input | Active LOW, pulled high externally |
| **1** | `SD_MMC_CLK` | MicroSD | 1-bit high-speed SDMMC clock |
| **2** | `HUB75_OE` | Matrix | Output Enable (Active LOW) |
| **3** | `HUB75_C` | Matrix | Row Select Bit C |
| **4** | `HUB75_R1` | Matrix | Upper Half Red Data |
| **5** | `HUB75_G1` | Matrix | Upper Half Green Data |
| **6** | `HUB75_B1` | Matrix | Upper Half Blue Data |
| **7** | `HUB75_R2` | Matrix | Lower Half Red Data |
| **8** | `HUB75_B` | Matrix | Row Select Bit B |
| **9** | `HUB75_E` | Matrix | Row Select Bit E (for 64x64 / 1/32 scan) |
| **10** | Unused / Reserved | — | Available test trace |
| **11** | `AUDIO_PA_PIN` | Audio | NS4150B amplifier enable (Active HIGH) |
| **12** | `I2S_MCLK_PIN` | Audio | Master Clock to ES7210 & ES8311 (6.144 MHz) |
| **13** | `SD_MMC_CMD` / `IMU_INT` | MicroSD / IMU | Shared command line / interrupt |
| **14** | `SD_MMC_D0` | MicroSD | SDMMC Data line 0 |
| **15** | `HUB75_G2` | Matrix | Lower Half Green Data |
| **16** | `HUB75_B2` | Matrix | Lower Half Blue Data |
| **17** | Status LED | Indicator | Onboard LED |
| **18** | `HUB75_A` | Matrix | Row Select Bit A |
| **19** | `USB_D-` | USB | Native USB JTAG / Serial |
| **20** | `USB_D+` | USB | Native USB JTAG / Serial |
| **21** | `I2S_DOUT_PIN` | Audio | ESP32 DOUT -> ES8311 DSDIN (Speaker DAC) |
| **26–37** | **RESERVED** | System | Dedicated to Octal SPI Flash & PSRAM bus |
| **38** | `I2S_WS_PIN` | Audio | Word Select / LRCK |
| **39** | `INPUT_PIN` | Audio | ES7210 ASDOUT -> ESP32 DIN (Microphones) |
| **40** | `HUB75_LAT` | Matrix | Data Latch |
| **41** | `HUB75_CLK` | Matrix | Shift Register Clock |
| **42** | `HUB75_D` | Matrix | Row Select Bit D |
| **43** | `I2S_BCLK_PIN` | Audio | Bit Clock / Serial Clock |
| **47** | `I2C_SDA_PIN` | System I2C | Shared peripheral bus SDA |
| **48** | `I2C_SCL_PIN` | System I2C | Shared peripheral bus SCL |

---

## 5. Software & PlatformIO Configuration

Target environment in `platformio.ini`: `[env:ws_s3_hub75-64x64]`

```ini
[env:ws_s3_hub75-64x64]
build_flags = ${feature_espmpdma.build_flags}
              -DMATRIX_WIDTH=64
              -DMATRIX_HEIGHT=64
              -DUSE_I2S_AUDIO=1
              -DI2S_MCLK_PIN=12
              -DI2S_WS_PIN=38
              -DI2S_BCLK_PIN=43
              -DI2S_DOUT_PIN=21
              -DINPUT_PIN=39
              -DAUDIO_PA_PIN=11
              -DI2C_SDA_PIN=47
              -DI2C_SCL_PIN=48
              -DENABLE_AUDIO=1
```

Flash upload command:
```sh
pio run -t upload -e ws_s3_hub75-64x64 --upload-port /dev/cu.usbmodem101
```
