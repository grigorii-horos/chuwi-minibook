// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Thermal zones:
 *   - minibook_soc:     EC register 0x70 (undocumented CPU/SoC thermistor)
 *   - minibook_charger: IT5570E ADC channel 0 via I2EC — warms during charging.
 *                       Likely the charger IC thermistor (possibly battery pack).
 */

#include <linux/acpi.h>
#include <linux/errno.h>
#include <linux/thermal.h>

#include "minibook_ec.h"

#define ADC_CH0_CTL		0x1904
#define ADC_CH0_DATL		0x1918
#define ADC_CH0_DATM		0x1919

#define ADC_GLITCH		0x1FF
#define ADC_ZERO_C_OFFSET	644
#define ADC_RAW_PER_DEGREE	7

static int soc_temp_get(struct thermal_zone_device *tz, int *temp)
{
	u8 val;
	int ret;

	ret = ec_read(EC_REG_SOC_TEMP, &val);
	if (ret < 0)
		return ret;

	if (val == 0 || val == 0xFF)
		return -ENODATA;

	*temp = val * 1000;
	return 0;
}

static const struct thermal_zone_device_ops soc_tz_ops = {
	.get_temp = soc_temp_get,
};

static u16 adc_read_raw(struct minibook_ec *ec)
{
	u8 ctl = minibook_ec_i2ec_read(ec, ADC_CH0_CTL);
	u8 datl, datm;
	u16 raw;

	if (ctl == 0)
		return 0xFFFF;

	datl = minibook_ec_i2ec_read(ec, ADC_CH0_DATL);
	datm = minibook_ec_i2ec_read(ec, ADC_CH0_DATM);
	raw = ((u16)datm << 8) | datl;

	if (raw == ADC_GLITCH || raw == 0)
		return 0xFFFF;
	return raw;
}

static int charger_temp_get(struct thermal_zone_device *tz, int *temp)
{
	struct minibook_ec *ec = thermal_zone_device_priv(tz);
	u16 raw = adc_read_raw(ec);
	int deg_c;

	if (raw == 0xFFFF)
		return -ENODATA;

	if (raw > ADC_ZERO_C_OFFSET)
		raw = ADC_ZERO_C_OFFSET;
	deg_c = (ADC_ZERO_C_OFFSET - raw) / ADC_RAW_PER_DEGREE;
	*temp = deg_c * 1000;
	return 0;
}

static const struct thermal_zone_device_ops charger_tz_ops = {
	.get_temp = charger_temp_get,
};

static int register_soc_zone(struct minibook_ec *ec)
{
	int temp;

	if (soc_temp_get(NULL, &temp))
		return -ENODEV;

	ec->soc_tz = thermal_tripless_zone_device_register("minibook_soc",
							   ec, &soc_tz_ops,
							   NULL);
	if (IS_ERR(ec->soc_tz))
		return PTR_ERR(ec->soc_tz);

	thermal_zone_device_enable(ec->soc_tz);
	return 0;
}

static int register_charger_zone(struct minibook_ec *ec)
{
	if (!ec->pnpcfg_available)
		return -ENODEV;

	if (adc_read_raw(ec) == 0xFFFF)
		return -ENODATA;

	ec->charger_tz = thermal_tripless_zone_device_register(
		"minibook_charger", ec, &charger_tz_ops, NULL);
	if (IS_ERR(ec->charger_tz))
		return PTR_ERR(ec->charger_tz);

	thermal_zone_device_enable(ec->charger_tz);
	return 0;
}

int minibook_ec_register_thermal(struct minibook_ec *ec)
{
	int ret;

	ret = register_soc_zone(ec);
	if (ret)
		dev_warn(&ec->pdev->dev, "SoC thermal zone unavailable: %d\n",
			 ret);

	ret = register_charger_zone(ec);
	if (ret)
		dev_warn(&ec->pdev->dev,
			 "charger thermal zone unavailable: %d\n", ret);

	if (ec->soc_tz || ec->charger_tz)
		return 0;
	return -ENODEV;
}

void minibook_ec_unregister_thermal(struct minibook_ec *ec)
{
	if (ec->soc_tz)
		thermal_zone_device_unregister(ec->soc_tz);
	if (ec->charger_tz)
		thermal_zone_device_unregister(ec->charger_tz);
}
