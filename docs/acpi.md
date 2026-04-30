# ACPI Tables

Decompiled from live system tables (`acpidump` + `iasl -d`).

## Table inventory

| Table    | SSDT   | Description                                 |
| -------- | ------ | ------------------------------------------- |
| DSDT     |        | Device tree, EC fields, methods             |
| DptfTabl | ssdt1  | DPTF thermal framework participants         |
| Pmax_Dev | ssdt2  | Platform max power device                   |
| CpuSsdt  | ssdt3  | CPU device definitions                      |
| SaSsdt   | ssdt4  | System Agent (graphics, memory controller)  |
| IgfxSsdt | ssdt5  | Intel Graphics (backlight, display outputs) |
| Ther_Rvp | ssdt6  | Thermal zone TZ00                           |
| Adl_DDR4 | ssdt7  | Alder Lake DDR4 board config                |
| PtidDevc | ssdt8  | Platform thermal ID device                  |
| TbtTypeC | ssdt9  | Type-C subsystem (USB4/TBT port topology)   |
| xh_adl_N | ssdt10 | USB host controller port map                |
| SocGpe   | ssdt11 | SoC GPE handlers                            |
| SocCmn   | ssdt12 | SoC common devices                          |
| ADebTabl | ssdt13 | ACPI debug interface (SMI port 0xB2)        |
| Cpu0Cst  | ssdt14 | CPU 0 C-state definitions                   |
| Cpu0Ist  | ssdt15 | CPU 0 P-state (SpeedStep)                   |
| Cpu0Psd  | ssdt16 | CPU 0 P-state dependencies                  |
| Cpu0Hwp  | ssdt17 | CPU 0 HWP (Hardware P-states)               |
| ApIst    | ssdt18 | Application Processor P-states              |
| ApHwp    | ssdt19 | AP HWP                                      |
| ApPsd    | ssdt20 | AP P-state dependencies                     |
| ApCst    | ssdt21 | AP C-states                                 |
| LPIT     |        | Low Power Idle Table                        |
| DMAR     |        | DMA Remapping (IOMMU)                       |
| NHLT     |        | Non-HDA Link Table (audio)                  |

## EC OperationRegion

Path: `\_SB.PC00.LPCB.H_EC`, region `ECF2` (0x00-0xFF).

The DSDT declares a subset of the 256-byte EC window. See
[ec-registers.md](ec-registers.md) for the full register map including
undocumented registers.

| Offset    | Field | Size | Description                    |
| --------- | ----- | ---- | ------------------------------ |
| 0x00      | ECMV  | 1    | EC major version               |
| 0x01      | ECSV  | 1    | EC minor version               |
| 0x02      | KBVS  | 1    | Keyboard firmware version      |
| 0x03      | ECTV  | 1    | Touchpad firmware version      |
| 0x04      | OSFG  | 1    | OS awake flag                  |
| 0x08      | MSFG  | 1    | Modern standby flag            |
| 0x0C      | TPTL  | 1    | Touchpad toggle                |
| 0x0D      | KBCD  | 1    | Keyboard mode                  |
| 0x15      | TSR3  | 1    | Ambient thermistor (degrees C) |
| 0x64      | TSHT  | 1    | Thermal sensor high threshold  |
| 0x65      | TSLT  | 1    | Thermal sensor low threshold   |
| 0x7F      | LSTE  | 1    | Lid state (bit 0)              |
| 0x80      | ECPS  | 1    | Power status                   |
| 0x81-0x93 | B1xx  |      | Battery registers              |

## DPTF participants

IETM (manager), TPWR, TPCH and BAT1 are directly under `\_SB`.
SEN1--SEN5, DGPU, TFN1--TFN3 and CHRG are under
`\_SB.PC00.LPCB.H_EC`. TCPU is a PCI device at 00:04.0
(`\_SB.PC00.TCPU`) with DPTF methods added by the DptfTabl SSDT.

Only SEN3 returns valid data; the other EC-based sensors depend on
fields the BIOS never defined in the EC OperationRegion. HIDs are
resolved dynamically by the `GHID` method in `\_SB.IETM`.

| Device | HID      | EC field    | Description                                                   |
| ------ | -------- | ----------- | ------------------------------------------------------------- |
| IETM   | INTC1041 |             | DPTF manager                                                  |
| TCPU   | (PCI)    |             | Processor thermal (00:04.0)                                   |
| SEN1   | INTC1046 | TSR1        | Thermistor PCH VR (EC field missing)                          |
| SEN2   | INTC1046 | TSR2        | Thermistor GT VR (EC field missing)                           |
| SEN3   | INTC1046 | TSR3 (0x15) | Thermistor ambient                                            |
| SEN4   | INTC1046 | TSR4        | Thermistor battery charger (EC field missing)                 |
| SEN5   | INTC1046 | TSR5        | Thermistor memory (EC field missing)                          |
| DGPU   | INTC1046 | TSR1        | Discrete GPU sensor (EC field missing, shares TSR1 with SEN1) |
| TFN1   | INTC1048 | CFSP        | CPU fan (EC field missing)                                    |
| TFN2   | INTC1048 | DFSP        | DDR fan (EC field missing)                                    |
| TFN3   | INTC1048 | GFSP        | GFX fan (EC field missing)                                    |
| CHRG   | INTC1046 | FCHG        | Charger (EC field missing)                                    |
| TPWR   | INTC1060 |             | Platform power                                                |
| TPCH   | INTC1049 |             | Intel PCH FIVR                                                |
| BAT1   | INTC1061 |             | Battery                                                       |

Each participant's `_STA` is gated by a GNVS flag: `DPTF`+`IN34`
(IETM), `SADE` (TCPU), `S1DE`--`S5DE` (SEN1--SEN5), `S6DE` (DGPU),
`FND1`--`FND3` (TFN1--TFN3), `CHGE` (CHRG), `PWRE` (TPWR), `PCHE`
(TPCH), `BATR` (BAT1). The `dptf_enabler` module sets these flags at
runtime so the kernel instantiates the devices. See the main
[README](../README.md).

## Thermal zone TZ00

Trip points from GNVS (populated by BIOS from `Setup` NVRAM):

| Method | GNVS variable  | Default |
| ------ | -------------- | ------- |
| \_CRT  | CRTT           | 119 C   |
| \_AC0  | ACTT           | 71 C    |
| \_PSV  | PSVT           | 95 C    |
| \_TMP  | (returns PSVT) | 95 C    |

## Accelerometer (ACMK)

Path: `\_SB.ACMK`, HID `MDA6655`.

Two MXC6655 accelerometers on separate I2C buses (both at address 0x15):

| Bus  | Role         |
| ---- | ------------ |
| I2C0 | Base half    |
| I2C1 | Display half |

I2C speed: 400 kHz. The `LTSM` method switches keyboard mode:
`LTSM(0)` = laptop (KBCD=0x00), `LTSM(1)` = slate (KBCD=0x03).

## USB port map

From xh_adl_N (ssdt10). GUPC type 0xFF = any device.

### XHCI

| Port | Active | Visible | Position | Assignment                                 |
| ---- | ------ | ------- | -------- | ------------------------------------------ |
| HS01 | Yes    | Yes     | 1        | USB-A (paired with SS02)                   |
| HS02 | Yes    | Yes     | 2        | USB-C                                      |
| HS04 | Yes    | Yes     | 4        | Bluetooth                                  |
| HS05 | Yes    |         |          | Webcam                                     |
| HS06 | Yes    | Yes     | 6        | USB-C HS companion (paired with SS01+SS04) |
| HS08 | Yes    | No      | 8        | Internal                                   |
| HS10 | Yes    | No      | 10       | Internal                                   |
| SS01 | Yes    | Yes     | 6        | USB-C SuperSpeed                           |
| SS02 | Yes    | Yes     | 1        | USB-A SuperSpeed (paired with HS01)        |
| SS03 | Yes    | Yes     | 4        | SuperSpeed                                 |
| SS04 | Yes    | Yes     | 6        | USB-C SS alt mode                          |

HS03, HS07, HS09 are disabled. Type-C TBT/USB4 ports (TXHC SS01/SS02)
are disabled on this SKU.

## Low power idle (LPIT)

Three native C-state entries, all MWAIT hint 0x60 (C6):

| ID  | Residency | Latency | Counter                          |
| --- | --------- | ------- | -------------------------------- |
| 0   | 30 ms     | 3 ms    | MSR 0x632 (C10 residency)        |
| 1   | 30 ms     | 3 ms    | MMIO 0xFE00193C (SLP_S0 counter) |
| 2   | 30 ms     | 3 ms    | None                             |

## DMA remapping (DMAR)

| Scope        | Device             | Path    |
| ------------ | ------------------ | ------- |
| PCI Endpoint | Intel UHD Graphics | 00:02.0 |
| IOAPIC       | PCH IOAPIC         | 00:1E.7 |
| HPET         | PCH HPET           | 00:1E.6 |

## Audio (NHLT)

OEM: NexGo, Table ID: 1CD2.

| Endpoint | Direction | Link       | Codec     |
| -------- | --------- | ---------- | --------- |
| 0        | Render    | SSP1 (I2S) | 8086:AE34 |
| 1        | Capture   | SSP1 (I2S) | 8086:AE34 |

ES8326 codec on I2C1 @ 0x19, connected via SSP1.

## ACPI device paths

| Path                       | HID      | Device                       |
| -------------------------- | -------- | ---------------------------- |
| `\_SB.PC00.LPCB.H_EC`      | PNP0C09  | Embedded controller          |
| `\_SB.PC00.LPCB.H_EC.BAT0` | PNP0C0A  | Battery                      |
| `\_SB.PC00.LPCB.H_EC.ADP1` | ACPI0003 | AC adapter                   |
| `\_SB.PC00.LPCB.H_EC.LID0` | PNP0C0D  | Lid switch                   |
| `\_SB.IETM`                | INTC1041 | DPTF manager                 |
| `\_SB.PC00.LPCB.H_EC.SEN3` | INTC1046 | DPTF ambient sensor          |
| `\_SB.ACMK`                | MDA6655  | Dual accelerometer           |
| `\_SB.PC00.I2C1.HDAC`      | ESSX8326 | Audio codec                  |
| `\_SB.PC00.I2C2.TPL1`      | GDIX1002 | Touchscreen                  |
| `\_SB.PC00.I2C3.TPD0`      | XXXX0000 | Touchpad                     |
| `\_SB.PC00.I2C1.LLKB`      | 093A3854 | Keyboard controller          |
| `\_SB.HIDD`                | INT33D5  | Intel HID Event Filter       |
| `\_SB.PEPD`                | INT33A1  | Power Engine Plugin          |
| `\_SB.PC00.GFX0`           |          | Intel Graphics (00:02.0)     |
| `\_SB.PC00.XHCI`           |          | USB 3.2 controller (00:14.0) |

## Serial I/O bus assignments

| Device | PCI     | Assignment                        |
| ------ | ------- | --------------------------------- |
| I2C0   | 00:15.0 | Accelerometer (MXC6655 base)      |
| I2C1   | 00:15.1 | Audio codec + keyboard controller |
| I2C2   | 00:15.2 | Touchscreen                       |
| I2C3   | 00:15.3 | Touchpad                          |
| I2C4   | 00:19.0 | Available                         |
| I2C5   | 00:19.1 | Available                         |
