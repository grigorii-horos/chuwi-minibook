# CHUWI MiniBook Linux Support

Kernel modules, tools and tweaks for getting Linux working optimally on the CHUWI MiniBook laptops.

I personally own and use the MiniBook X N150 with
[CachyOS](https://cachyos.org/) which I recommend, as it ships a performance-tuned kernel, up-to-date Mesa and firmware and sane defaults for Intel hardware.

That said, everything here should work on other Arch-based distros and most of the instructions should be easily adaptable to other distros. Similarly, I expect the MiniBook X N100 shares the same board design and EC firmware, so most or all of this should apply to it as well.

**If you have tested this on a different MiniBook variant or a
different distro**, please
[open an issue](https://github.com/fstanis/chuwi-minibook/issues) and share your results.

## Getting started

See [GUIDE.md](GUIDE.md) for instructions on how to set up and install each individual component and tweak.

## Components

### [minibook_ec](docs/minibook-ec.md) -- EC platform driver

Exposes hardware behind the ITE IT5570E embedded controller: touchpad and
keyboard toggles, two thermal sensors (SoC and charger), fan RPM
monitoring, keyboard backlight control, EC version and BIOS unlock.

### [thermald](docs/thermald.md) -- patched thermal daemon

Fork of Intel's thermald with MiniBook-specific fixes. Unlocks the RAPL
power range, lowers trip points, enables PID control for smooth power
limit adjustments, adds minibook_ec thermal zones and parses power policy
tables the upstream ignores.

### [iio-sensor-proxy](docs/iio-sensor-proxy.md) -- screen rotation and tablet mode

Fork of iio-sensor-proxy with a dual-accelerometer driver for the two
MXC6655 chips (display + base). Computes the hinge angle for automatic
tablet mode detection and provides screen orientation for auto-rotation.

### [vbt_patch](docs/vbt-patch.md) -- display refresh rate

Patches the Video BIOS Table to increase the DSI panel's refresh rate
from the stock 50 Hz. The patched VBT loads from initramfs at early
boot -- no permanent firmware modification.

### goodix_ts -- touchscreen fix

DKMS module that patches the upstream Goodix touchscreen driver with a
resume retry loop and forced loading of the OEM noise-mitigation config.

### dptf_enabler -- unhide DPTF devices

DKMS module that enables BIOS-gated Intel DPTF devices by setting GNVS
flags at runtime. Required for thermald's adaptive mode.

### i2c_designware_spklen -- I2C spike suppression

DKMS module that programs the spike suppression registers on Intel LPSS
I2C controllers to prevent occasional transaction failures on the
touchscreen and sensor buses.

## Documentation

| Document                                     | Description                                                                                  |
| -------------------------------------------- | -------------------------------------------------------------------------------------------- |
| [Device inventory](docs/devices.md)          | Complete hardware inventory: components, firmware versions, active and disabled ACPI devices |
| [ACPI tables](docs/acpi.md)                  | ACPI table analysis: EC fields, DPTF participants, thermal zones, USB port map               |
| [EC register map](docs/ec-registers.md)      | IT5570E register map: ACPI window and internal I2EC registers                                |
| [minibook_ec](docs/minibook-ec.md)           | EC platform driver: thermal sensors, fan, keyboard backlight, sysfs interface                |
| [thermald](docs/thermald.md)                 | Patched thermal daemon: PID control, power limits, DPTF table fixes                          |
| [iio-sensor-proxy](docs/iio-sensor-proxy.md) | Dual-accelerometer driver: hinge angle, screen rotation, tablet mode                         |
| [VBT patcher](docs/vbt-patch.md)             | DSI panel refresh rate patcher and update-vbt-clock script                                   |
| [Installation guide](GUIDE.md)               | Status check, install instructions, GPU setup, BIOS tweaks                                   |

## License

See [LICENSE.md](LICENSE.md) for per-component licensing details.

## Similar or related projects

- https://github.com/greymouser/minibook-x-tools
- https://github.com/petitstrawberry/minibook-support
- https://github.com/sonnyp/linux-minibook-x
- https://github.com/lschans/chuwi-tablet
- https://github.com/juanjsebgarcia/chuwi-minibook-unlocked-bios
