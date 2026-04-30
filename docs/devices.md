# Device Inventory

## Platform

|           |                                                             |
| --------- | ----------------------------------------------------------- |
| System    | CHUWI MiniBook X                                            |
| Board     | NexGo (ODM), Alder Lake-N DDR4                              |
| CPU       | Intel N150, stepping A0, 4C/4T (all E-cores), up to 3.6 GHz |
| PCH       | PCH-N, stepping A0, SSKU Unlocked (SSKU01)                  |
| RAM       | 4x 4 GB LPDDR5 @ 4000 MT/s, Micron                          |
| SPI Flash | 16 MB, single component                                     |

## Firmware

| Component | Version                        |
| --------- | ------------------------------ |
| BIOS      | AMI DNN20A V2.51 (2025-04-16)  |
| FSP       | OC.02.89.40                    |
| ME FW     | 16.50.12.1465 (Consumer)       |
| PMC FW    | 160.50.0.1010                  |
| IOM FW    | 23000900 (Type-C/USB4)         |
| EC FW     | 0.18, IT5570E-128 (2024-04-17) |
| Microcode | 0x1A (CPUID 0xB06E0)           |
| IGFX GOP  | 21.1.6                         |

---

## Active devices

### Processor and chipset

| Device                           | IDs             | Bus         | Notes                   |
| -------------------------------- | --------------- | ----------- | ----------------------- |
| Intel N150 CPU                   | `8086:461c`     | Host bridge | 4C/4T, up to 3.6 GHz    |
| Intel UHD Graphics               | `8086:46d4`     | PCI 00:02.0 | Gen12.2                 |
| Intel DPTF Processor Participant | `8086:461d`     | PCI 00:04.0 | Requires `dptf_enabler` |
| Intel HECI / Management Engine   | `8086:54e0`     | PCI 00:16.0 | ME FW 16.50.12.1465     |
| Intel PTT (fTPM 2.0)             | ACPI `MSFT0101` | MMIO        | Firmware TPM            |

### Storage

| Device                             | IDs         | Bus         | Notes               |
| ---------------------------------- | ----------- | ----------- | ------------------- |
| AirDisk 512GB NVMe (MAXIO MAP1202) | `1e4b:1202` | PCI 01:00.0 | DRAM-less, 476.9 GB |
| Intel eMMC Controller              | `8086:54c4` | PCI 00:1a.0 | No media present    |

### Networking

| Device                | IDs             | Bus                | Notes                                  |
| --------------------- | --------------- | ------------------ | -------------------------------------- |
| Intel Wi-Fi 6 AX201   | `8086:54f0`     | PCI 00:14.3 (CNVi) | 802.11ax 2x2, Bluetooth 5.2 integrated |
| Intel AX201 Bluetooth | USB `8087:0026` | USB bus 1          |                                        |

### Display

| Device                     | IDs                     | Bus | Notes                                                 |
| -------------------------- | ----------------------- | --- | ----------------------------------------------------- |
| 10.51" MIPI-DSI Panel      | DRM `DSI-1`             | DSI | 1200x1920 portrait. See [vbt-patch.md](vbt-patch.md). |
| Intel Backlight Controller | sysfs `intel_backlight` |     | PWM backlight for internal panel                      |

### Audio

| Device                    | IDs                         | Bus         | Notes                            |
| ------------------------- | --------------------------- | ----------- | -------------------------------- |
| Intel HD Audio Controller | `8086:54c8`                 | PCI 00:1f.3 |                                  |
| Everest ES8326 Codec      | ACPI `ESSX8326`, I2C `0x19` | I2C1        | Jack detect + power enable GPIOs |

### Input

| Device                     | IDs                         | Bus       | Notes                         |
| -------------------------- | --------------------------- | --------- | ----------------------------- |
| Goodix Touchscreen         | ACPI `GDIX1002`, I2C `0x5D` | I2C2      | Patched by `goodix_ts` module |
| I2C HID Touchpad           | ACPI `XXXX0000`, I2C `0x2C` | I2C3      | HID-over-I2C                  |
| Pixart Keyboard Controller | ACPI `093A3854`, I2C `0x2C` | I2C1      | HID-over-I2C at 1 MHz         |
| Intel HID Events           | ACPI `INTC1070`             | ACPI      | Power/volume/home buttons     |
| Lid Switch                 | ACPI `PNP0C0D`              | ACPI      |                               |
| AT Keyboard                | ACPI `MSFT0001`             | i8042/PS2 | Legacy interface via EC       |

### Sensors

| Device              | IDs                        | Bus         | Notes                                                                                 |
| ------------------- | -------------------------- | ----------- | ------------------------------------------------------------------------------------- |
| Memsic MXC6655 (x2) | ACPI `MDA6655`, I2C `0x15` | I2C0 + I2C1 | Dual accelerometers (display + base). See [iio-sensor-proxy.md](iio-sensor-proxy.md). |

### Camera

| Device          | IDs             | Bus       | Notes                      |
| --------------- | --------------- | --------- | -------------------------- |
| HYGD USB Webcam | USB `1b0a:2bc9` | USB bus 1 | 1600x1200 UVC + microphone |

### Power

| Device                    | IDs                 | Bus      | Notes                                                                               |
| ------------------------- | ------------------- | -------- | ----------------------------------------------------------------------------------- |
| Battery (BAT0)            | ACPI `PNP0C0A`      | EC       | 38 Wh, 7.6 V design                                                                 |
| AC Adapter (ADP1)         | ACPI `ACPI0003`     | EC       |                                                                                     |
| ANX7447 USB-PD Controller | I2C `0x37` + `0x2B` | EC SMBus | PD negotiation, handled autonomously by EC. See [ec-registers.md](ec-registers.md). |

### Thermal

| Sensor                | hwmon          | Notes                                                       |
| --------------------- | -------------- | ----------------------------------------------------------- |
| CPU Package + 4 Cores | `coretemp`     | Per-core die sensors                                        |
| NVMe SSD              | `nvme`         | 3 zones                                                     |
| Wi-Fi                 | `iwlwifi_1`    | Radio temperature                                           |
| EC SoC thermistor     | `minibook_ec`  | Register 0x70. See [minibook-ec.md](minibook-ec.md).        |
| EC Charger thermistor | `minibook_ec`  | ADC channel 0. See [minibook-ec.md](minibook-ec.md).        |
| EC Fan                | `minibook_fan` | Read-only RPM + duty. See [minibook-ec.md](minibook-ec.md). |

### Bus controllers

| Device                     | IDs                | Bus                  | Notes                                           |
| -------------------------- | ------------------ | -------------------- | ----------------------------------------------- |
| Intel xHCI USB 3.2 Gen 2   | `8086:54ed`        | PCI 00:14.0          | 10 Gbps                                         |
| Intel I2C #0-#5            | `8086:54e8`-`54c6` | PCI 00:15.x, 00:19.x | DesignWare. Patched by `i2c_designware_spklen`. |
| Intel eSPI Controller      | `8086:5481`        | PCI 00:1f.0          | Bridge to EC                                    |
| Intel SPI Flash Controller | `8086:54a4`        | PCI 00:1f.5          | BIOS flash                                      |
| Genesys GL3523 USB Hub     | USB `05e3:0625`    | USB bus 1            | Internal hub, fans out to physical ports        |
| Intel GPIO Controller      | ACPI `INTC1057`    | ACPI                 | Alder Lake-N pin controller                     |

### Embedded controller

| Device      | IDs            | Notes                                                                        |
| ----------- | -------------- | ---------------------------------------------------------------------------- |
| ITE IT5570E | ACPI `PNP0C09` | See [minibook-ec.md](minibook-ec.md) and [ec-registers.md](ec-registers.md). |

---

## Disabled ACPI devices

Defined in the Alder Lake-N reference DSDT but not populated on this
board (`_STA = 0`). These are alternate component options from the
reference design.

### Sensors

| Device              | ACPI HID   | I2C address        |
| ------------------- | ---------- | ------------------ |
| Bosch BMA2x2        | `BOSC0200` | I2C0 @ 0x19        |
| Kionix KXCJ9        | `KIOX000A` | I2C0 @ 0x0F        |
| Kionix KXCJ9        | `KIOX010A` | I2C0 @ 0x0E        |
| Kionix KXTJ3        | `KIOX020A` | I2C0 @ 0x0F        |
| Sensortek STK3311   | `STK3311`  | I2C2 @ 0x48        |
| Sensortek STK8321   | `STK8321`  | I2C0 @ 0x1F        |
| NSA2513             | `NSA2513`  | I2C0 @ 0x27        |
| Lite-On LTR-303     | `LTER0303` | I2C3 @ 0x29        |
| Capella CM3218      | `CPLM3218` | I2C3 @ 0x10        |
| Bosch SMO8B51       | `SMO8B51`  | I2C0 @ 0x6B        |
| Memsic MXC6655 (x2) | `MXC6655`  | I2C0 + I2C2 @ 0x15 |

### Touch and input

| Device                | ACPI HID   | I2C address |
| --------------------- | ---------- | ----------- |
| Elan Touchscreen      | `ELAN4246` | I2C2 @ 0x10 |
| MSSL Touchscreen      | `MSSL1680` | I2C2 @ 0x40 |
| FocalTech Touchscreen | `FTSC1000` | I2C2 @ 0x38 |
| ILTK Touchscreen      | `ILTK0001` | I2C2 @ 0x41 |

### Audio

| Device         | ACPI HID   | I2C address |
| -------------- | ---------- | ----------- |
| Everest ES8336 | `ESSX8336` | I2C1 @ 0x11 |

### Other

| Device                               | ACPI HID   | Notes                              |
| ------------------------------------ | ---------- | ---------------------------------- |
| Goodix Fingerprint                   | `GXFP3200` | SPI2                               |
| Sony IMX135 Camera                   | `INT3471`  | I2C2 @ 0x10                        |
| OmniVision OV2740 Camera             | `INT3474`  | I2C4 @ 0x36                        |
| Microchip PAC1934 Power Monitor (x4) | `MCHP1930` | I2C5 @ 0x18                        |
| BAT1, BAT2                           | `PNP0C0A`  | Extra battery slots, not populated |

---

## How this was gathered

Active devices were enumerated from `/sys` (PCI, USB, I2C, ACPI, DRM,
hwmon, power_supply) using `tools/detect-hardware.sh`. Battery, NVMe,
Wi-Fi, camera and ME details were read from sysfs and standard tools
(`iw`, `lsusb`, `bluetoothctl`). Disabled devices, I2C bus assignments,
GPIO pins and DPTF participants were identified from decompiled ACPI
tables (DSDT + SSDTs). EC registers and the ANX7447 were mapped through
firmware decompilation and I2EC probing.

See [acpi.md](acpi.md) for the
full ACPI analysis.
