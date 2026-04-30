// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Fan hwmon interface — reads IT5570E PWM module (base 0x1800h) via I2EC.
 * PWM duty exposed as pwm1_input (read-only). Fan RPM exposed as fan1_input.
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/hwmon.h>

#include "minibook_ec.h"

#define IT5570E_DCR_FAN		0x1809
#define IT5570E_FAN_TACH_L	0x181E
#define IT5570E_FAN_TACH_M	0x181F

#define TACH_RPM_CONSTANT	1875000

static long read_fan_rpm(struct minibook_ec *ec)
{
	u8 tach_l = minibook_ec_i2ec_read(ec, IT5570E_FAN_TACH_L);
	u8 tach_m = minibook_ec_i2ec_read(ec, IT5570E_FAN_TACH_M);
	u16 tach = ((u16)tach_m << 8) | tach_l;

	if (tach == 0 || tach == 0xFFFF)
		return 0;
	return TACH_RPM_CONSTANT / tach;
}

static umode_t fan_hwmon_visible(const void *drvdata,
				 enum hwmon_sensor_types type,
				 u32 attr, int channel)
{
	if (type == hwmon_fan && attr == hwmon_fan_input)
		return 0444;
	if (type == hwmon_pwm && attr == hwmon_pwm_input)
		return 0444;
	return 0;
}

static int fan_hwmon_read(struct device *dev,
			  enum hwmon_sensor_types type,
			  u32 attr, int channel, long *val)
{
	struct minibook_ec *ec = dev_get_drvdata(dev);

	if (type == hwmon_fan && attr == hwmon_fan_input) {
		*val = read_fan_rpm(ec);
		return 0;
	}
	if (type == hwmon_pwm && attr == hwmon_pwm_input) {
		*val = minibook_ec_i2ec_read(ec, IT5570E_DCR_FAN);
		return 0;
	}
	return -EOPNOTSUPP;
}

static const struct hwmon_ops fan_hwmon_ops = {
	.is_visible = fan_hwmon_visible,
	.read = fan_hwmon_read,
};

static const struct hwmon_channel_info *fan_hwmon_info[] = {
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT),
	HWMON_CHANNEL_INFO(pwm, HWMON_PWM_INPUT),
	NULL,
};

static const struct hwmon_chip_info fan_hwmon_chip = {
	.ops = &fan_hwmon_ops,
	.info = fan_hwmon_info,
};

int minibook_ec_register_fan(struct minibook_ec *ec)
{
	if (!ec->pnpcfg_available)
		return -ENODEV;

	ec->hwmon_dev = devm_hwmon_device_register_with_info(
		&ec->pdev->dev, "minibook_fan", ec, &fan_hwmon_chip, NULL);
	if (IS_ERR(ec->hwmon_dev))
		return PTR_ERR(ec->hwmon_dev);
	return 0;
}

void minibook_ec_unregister_fan(struct minibook_ec *ec)
{
	/* nothing to do — devm handles hwmon, i2ec handles PNPCFG region */
}
