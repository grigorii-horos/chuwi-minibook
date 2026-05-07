# EC Register Map

ITE IT5570E embedded controller. Host-visible through the 256-byte ACPI EC
window at `\_SB.PC00.LPCB.H_EC` (ports 0x62/0x66).

## ACPI EC window (0x00-0xFF)

| Offset        | Field | Size | Description                                                                                                                                  |
| ------------- | ----- | ---- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| `0x00`        | ECMV  | 1    | EC major version                                                                                                                             |
| `0x01`        | ECSV  | 1    | EC minor version                                                                                                                             |
| `0x02`        | KBVS  | 1    | Keyboard firmware version                                                                                                                    |
| `0x03`        | ECTV  | 1    | Touchpad firmware version                                                                                                                    |
| `0x04`        | OSFG  | 1    | OS awake flag. Set to 1 by ACPI `_REG`/`RWAK`, cleared to 0 by `RPTS`.                                                                       |
| `0x07`        |       | 1    | BIOS scratch. Saved to CMOS 0x20 on S4/S5, restored on resume.                                                                               |
| `0x08`        | MSFG  | 1    | Modern standby flag. 1 = entering S0ix, 0 = exiting.                                                                                         |
| `0x0B`        |       | 1    | BIOS scratch. Saved to CMOS 0x22 on S4/S5, restored on resume.                                                                               |
| `0x0C`        | TPTL  | 1    | Touchpad toggle. `0x11` = on, `0x22` = off. Written by EC firmware on Fn+F1; exposed by `minibook_ec` as `touchpad_enabled`.                 |
| `0x0D`        | KBCD  | 1    | Keyboard mode. `0x00` = laptop, `0x03` = slate. Written by ACPI method `\_SB.ACMK.LTSM`; exposed by `minibook_ec` as `keyboard_enabled`.     |
| `0x0E`        |       | 1    | BIOS scratch. Saved to CMOS 0x23 on S4/S5, restored on resume.                                                                               |
| `0x11`        |       | 1    | BIOS scratch. Saved to CMOS 0x1F on S4/S5, restored on resume.                                                                               |
| `0x15`        | TSR3  | 1    | Ambient thermistor, degrees C. The only working DPTF thermal sensor (SEN3). Slow-reacting.                                                   |
| `0x41`-`0x48` |       | 8    | EC firmware build time, ASCII `HH:MM:SS`.                                                                                                    |
| `0x4B`-`0x54` |       | 10   | EC firmware build date, ASCII `YYYY/MM/DD`.                                                                                                  |
| `0x64`        | TSHT  | 1    | Thermal sensor high threshold.                                                                                                               |
| `0x65`        | TSLT  | 1    | Thermal sensor low threshold.                                                                                                                |
| `0x70`        |       | 1    | SoC thermistor, degrees C. Fast-reacting (seconds, not minutes). Not in DSDT; exposed by `minibook_ec` as the `minibook_soc` thermal zone.   |
| `0x7F`        | LSTE  | 1    | Lid state. Bit 0: 0 = closed, 1 = open. Read by `LID0._LID`.                                                                                 |
| `0x80`        | ECPS  | 1    | Power status. Bit 0 = AC present, bit 1 = battery present. Read by `ADP1._PSR` and `BAT0._STA`.                                              |
| `0x81`        | B1MN  | 1    | Battery manufacturer code.                                                                                                                   |
| `0x82`        | B1SN  | 2    | Battery serial number.                                                                                                                       |
| `0x84`        | B1DC  | 2    | Battery design capacity (mAh).                                                                                                               |
| `0x86`        | B1DV  | 2    | Battery design voltage (mV).                                                                                                                 |
| `0x88`        | B1FC  | 2    | Battery full charge capacity (mAh).                                                                                                          |
| `0x8A`        | B1TP  | 2    | Battery type/chemistry.                                                                                                                      |
| `0x8C`        | B1ST  | 1    | Battery status (charge state bits).                                                                                                          |
| `0x8D`        | B1PR  | 2    | Battery present rate (mA).                                                                                                                   |
| `0x8F`        | B1RC  | 2    | Battery remaining capacity (mAh).                                                                                                            |
| `0x91`        | B1PV  | 2    | Battery present voltage (mV).                                                                                                                |
| `0x93`        | B1RP  | 1    | Battery remaining percentage (0-100).                                                                                                        |
| `0xF0`        |       | 1    | BIOS unlock. Write `0xAA` to expose hidden BIOS menus on next boot. Volatile (lost on power-off). Exposed by `minibook_ec` as `bios_unlock`. |
| `0xF3`        |       | 1    | BIOS password clear. Write `0xAA`, reboot; BIOS clears the password hash and writes `0xDD` to acknowledge.                                   |

Battery registers (0x81-0x93) are populated by the EC from SMBus and read by
standard ACPI `_BIF`/`_BST` methods. Linux exposes them via
`/sys/class/power_supply/BAT0/`.

Offsets not listed are either unused gaps or EC-internal scratch that is not
meaningful to read from the host.

## IT5570E internal registers (I2EC)

These are in the EC's 16-bit address space, accessible from the host via the
PNPCFG Depth-2 I/O interface at port 0x4E/0x4F. The `minibook_ec` module uses
I2EC for fan, charger thermal zone and keyboard backlight.

### Chip ID (0x2000)

| Address  | Register | Description                         |
| -------- | -------- | ----------------------------------- |
| `0x2000` | ECHIPID1 | Chip ID byte 1 (`0x55` for IT5570). |
| `0x2001` | ECHIPID2 | Chip ID byte 2 (`0x70` for IT5570). |
| `0x2002` | ECHIPVER | Chip version.                       |

### PWM (0x1800)

| Address | Register | Description | | -------- | -------- |
------------------------------------------------------ | ------ | | `0x1803` |
DCR1 | Keyboard backlight PWM duty (0-255). | | `0x1809` | DCR7 | Fan PWM duty
(0-255). Firmware clamps to `0xB8` (72%). | | `0x181E` | F1TLRR | Fan tachometer
period LSB. | | `0x181F` | F1TMRR | Fan tachometer period MSB. RPM =
`1875000 / (MSB<<8    | LSB)`. |

### ADC (0x1900)

| Address  | Register | Description                                                                |
| -------- | -------- | -------------------------------------------------------------------------- |
| `0x1904` | VCH0CTL  | ADC channel 0 control. Non-zero when channel is active.                    |
| `0x1918` | VCH0DATL | ADC channel 0 data, low byte.                                              |
| `0x1919` | VCH0DATM | ADC channel 0 data, high byte. 10-bit reading used for charger thermistor. |

### GPIO (0x1611)

| Address  | Register | Description                                                                   |
| -------- | -------- | ----------------------------------------------------------------------------- |
| `0x1611` |          | Keyboard backlight output gate. Bit 6: 0 = output enabled, `0x40` = disabled. |

### Keyboard backlight firmware state (XDATA)

| Address  | Description                                                             |
| -------- | ----------------------------------------------------------------------- |
| `0x03E1` | Auto-off countdown timer. Initialized to `0x1E` (30 ticks) on activity. |
| `0x03E2` | Brightness setpoint. Firmware copies this to DCR1.                      |
| `0x0411` | Timer suspend flag. `0xEE` = skip countdown (always-on mode).           |

### SMBus (0x1C00)

Channel A communicates with an ANX7447 USB-PD controller at I2C addresses `0x37`
(PD policy) and `0x2B` (TCPCI). Channel B is unused (no responding devices). The
ANX7447 is the only peripheral on the EC's SMBus.

The EC handles PD negotiation and charger control autonomously. There is no
software-accessible charge limit or threshold -- the firmware does not implement
this feature.
