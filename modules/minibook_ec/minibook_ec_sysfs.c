// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Sysfs attributes: touchpad toggle, keyboard mode, EC version,
 * BIOS menu unlock.
 */

#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>

#include "minibook_ec.h"

static ssize_t touchpad_enabled_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	u8 val;
	int ret;

	ret = ec_read(EC_REG_TPTL, &val);
	if (ret < 0)
		return ret;

	return sysfs_emit(buf, "%d\n", val != TPTL_OFF);
}

static ssize_t touchpad_enabled_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret < 0)
		return ret;

	ret = ec_write(EC_REG_TPTL, enable ? TPTL_ON : TPTL_OFF);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR_RW(touchpad_enabled);

static ssize_t keyboard_enabled_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	u8 val;
	int ret;

	ret = ec_read(EC_REG_KBCD, &val);
	if (ret < 0)
		return ret;

	return sysfs_emit(buf, "%d\n", val == KBCD_LAPTOP);
}

static ssize_t keyboard_enabled_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	union acpi_object arg = { .type = ACPI_TYPE_INTEGER };
	struct acpi_object_list args = { .count = 1, .pointer = &arg };
	bool enable;
	acpi_status status;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret < 0)
		return ret;

	arg.integer.value = enable ? 0 : 1;

	status = acpi_evaluate_object(NULL, LTSM_PATH, &args, NULL);
	if (ACPI_FAILURE(status))
		return -EIO;

	return count;
}

static DEVICE_ATTR_RW(keyboard_enabled);

static ssize_t ec_version_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	u8 ecmv, ecsv, kbvs, ectv;
	int ret;

	ret = ec_read(EC_REG_ECMV, &ecmv);
	if (ret < 0)
		return ret;

	ret = ec_read(EC_REG_ECSV, &ecsv);
	if (ret < 0)
		return ret;

	ret = ec_read(EC_REG_KBVS, &kbvs);
	if (ret < 0)
		return ret;

	ret = ec_read(EC_REG_ECTV, &ectv);
	if (ret < 0)
		return ret;

	return sysfs_emit(buf, "%u.%u kb=%u tp=%u\n", ecmv, ecsv, kbvs, ectv);
}

static DEVICE_ATTR_RO(ec_version);

static ssize_t bios_unlock_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	bool unlock;
	int ret;

	ret = kstrtobool(buf, &unlock);
	if (ret < 0)
		return ret;

	if (!unlock)
		return -EINVAL;

	ret = ec_write(EC_REG_UNLOCK, UNLOCK_MAGIC);
	if (ret < 0)
		return ret;

	dev_info(dev,
		 "BIOS unlock flag set — reboot into BIOS setup to access hidden menus\n");
	return count;
}

static DEVICE_ATTR_WO(bios_unlock);

static struct attribute *minibook_ec_attrs[] = {
	&dev_attr_touchpad_enabled.attr,
	&dev_attr_keyboard_enabled.attr,
	&dev_attr_ec_version.attr,
	&dev_attr_bios_unlock.attr,
	NULL,
};

static const struct attribute_group minibook_ec_group = {
	.attrs = minibook_ec_attrs,
};

int minibook_ec_register_sysfs(struct minibook_ec *ec)
{
	return devm_device_add_group(&ec->pdev->dev, &minibook_ec_group);
}
