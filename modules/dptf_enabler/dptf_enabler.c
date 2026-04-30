// SPDX-License-Identifier: GPL-2.0-or-later
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/memremap.h>
#include <linux/unaligned.h>
#include <acpi/acpi_bus.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel DPTF device enabler");
MODULE_AUTHOR("Filip Stanis");

static bool enable_fans;
module_param(enable_fans, bool, 0444);
MODULE_PARM_DESC(enable_fans,
	"Enable fan participants FND1-3 (broken: CFSP/DFSP/GFSP EC fields "
	"missing, causes AE_NOT_FOUND in dmesg)");

static bool enable_sensors;
module_param(enable_sensors, bool, 0444);
MODULE_PARM_DESC(enable_sensors,
	"Enable thermal sensor participants S1DE-S6DE and charger CHGE "
	"(broken: TSR1/2/4/5 and FCHG EC fields missing)");

/* DPTF _OSC UUID (from SSDT1 IETM._OSC method) */
#define DPTF_OSC_UUID		"b23ba85d-c8b7-3542-88de-8de2ffcfd698"
#define DPTF_OSC_CAPS		0x0F

/* ACPI path to the DPTF manager device */
#define IETM_PATH		"\\_SB.IETM"

/* GNVS byte offsets for DPTF participant _STA gates (see README for details) */
static const struct { u16 off; const char *name; } flags[] = {
	{ 0x072, "DPTF" },
	{ 0x1F4, "PWRE" },
	{ 0x32F, "BATR" },
	{ 0x969, "PCHE" },
};

/* Fan participants -- disabled by default (missing EC fields, see README) */
static const struct { u16 off; const char *name; } fan_flags[] = {
	{ 0x076, "FND1" },
	{ 0xB54, "FND2" },
	{ 0xB55, "FND3" },
};

/* Thermal/charger participants -- disabled by default (missing EC fields, see README) */
static const struct { u16 off; const char *name; } sensor_flags[] = {
	{ 0x327, "CHGE" },
	{ 0x333, "S1DE" },
	{ 0x334, "S2DE" },
	{ 0x335, "S3DE" },
	{ 0x336, "S4DE" },
	{ 0x337, "S5DE" },
	{ 0xB56, "S6DE" },
};

/*
 * Find GNVS OperationRegion in DSDT AML.
 * Scans for: ExtOpPrefix(5B) OpRegionOp(80) "GNVS" SystemMemory(00)
 */
static int find_gnvs(phys_addr_t *addr, u32 *len)
{
	struct acpi_table_header *dsdt;
	acpi_status status;
	u8 *aml, *end, *p;

	status = acpi_get_table(ACPI_SIG_DSDT, 0, &dsdt);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	aml = (u8 *)dsdt + sizeof(*dsdt);
	end = (u8 *)dsdt + dsdt->length;

	while (aml < end - 12) {
		if (aml[0] == 0x5B && aml[1] == 0x80 &&
		    aml[2] == 'G' && aml[3] == 'N' &&
		    aml[4] == 'V' && aml[5] == 'S' &&
		    aml[6] == 0x00) {
			p = aml + 7;

			switch (*p) {
			case 0x0C:
				*addr = get_unaligned_le32(p + 1);
				p += 5;
				break;
			case 0x0E:
				*addr = get_unaligned_le64(p + 1);
				p += 9;
				break;
			default:
				acpi_put_table(dsdt);
				return -EINVAL;
			}

			switch (*p) {
			case 0x0A: *len = *(p + 1); break;
			case 0x0B: *len = get_unaligned_le16(p + 1); break;
			case 0x0C: *len = get_unaligned_le32(p + 1); break;
			default:
				acpi_put_table(dsdt);
				return -EINVAL;
			}

			acpi_put_table(dsdt);
			return 0;
		}
		aml++;
	}

	acpi_put_table(dsdt);
	return -ENOENT;
}

static int __init dptf_enabler_init(void)
{
	struct acpi_osc_context osc = {
		.uuid_str = DPTF_OSC_UUID,
		.rev = 1,
	};
	acpi_handle ietm;
	acpi_status status;
	u32 caps[2];
	phys_addr_t gnvs_addr;
	u32 gnvs_len;
	void *gnvs;
	int i, n = 0;

	if (find_gnvs(&gnvs_addr, &gnvs_len))
		return -ENODEV;

	gnvs = memremap(gnvs_addr, gnvs_len, MEMREMAP_WB);
	if (!gnvs)
		return -ENOMEM;

	#define SET_FLAGS(arr) do { \
		for (i = 0; i < ARRAY_SIZE(arr); i++) { \
			u8 *p = (u8 *)gnvs + arr[i].off; \
			if (!*p) { \
				*p = 1; \
				n++; \
				pr_info("set %s\n", arr[i].name); \
			} \
		} \
	} while (0)

	SET_FLAGS(flags);
	if (enable_fans)
		SET_FLAGS(fan_flags);
	if (enable_sensors)
		SET_FLAGS(sensor_flags);

	#undef SET_FLAGS

	memunmap(gnvs);

	if (!n) {
		pr_info("all flags already set\n");
		return 0;
	}

	/* Inform firmware that a DPTF-aware OS is present */
	status = acpi_get_handle(NULL, IETM_PATH, &ietm);
	if (ACPI_SUCCESS(status)) {
		caps[OSC_QUERY_DWORD] = 0;
		caps[OSC_SUPPORT_DWORD] = DPTF_OSC_CAPS;
		osc.cap.length = sizeof(caps);
		osc.cap.pointer = caps;

		status = acpi_run_osc(ietm, &osc);
		if (ACPI_SUCCESS(status))
			kfree(osc.ret.pointer);
	}

	/* Discover newly-enabled devices */
	acpi_bus_scan(ACPI_ROOT_OBJECT);

	pr_info("enabled %d DPTF device(s)\n", n);
	return 0;
}

static void __exit dptf_enabler_exit(void) {}

module_init(dptf_enabler_init);
module_exit(dptf_enabler_exit);
