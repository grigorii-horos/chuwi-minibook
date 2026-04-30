// SPDX-License-Identifier: GPL-2.0-or-later
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/suspend.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DesignWare I2C spike suppression fixup for Intel LPSS");
MODULE_AUTHOR("Filip Stanis");

/*
 * DesignWare I2C register offsets.  IC_FS_SPKLEN / IC_HS_SPKLEN set the
 * minimum pulse width (in ic_clk cycles) that the spike-suppression logic
 * will pass through.  Linux's i2c-designware driver never programs them,
 * leaving whatever power-on default the silicon has (usually 1 — far too
 * short for fast-mode buses).
 *
 * Default values match the Intel LPSS generic-platform table for Alder Lake.
 */
#define DW_IC_ENABLE		0x6c
#define DW_IC_ENABLE_STATUS	0x9c
#define DW_IC_FS_SPKLEN		0xa0
#define DW_IC_HS_SPKLEN		0xa4
#define DW_IC_CON		0x00
#define DW_IC_CON_SPEED_MASK	0x06
#define DW_IC_CON_SPEED_STD	0x02
#define DW_IC_CON_SPEED_FAST	0x04
#define DW_IC_CON_SPEED_HIGH	0x06

/* LPSS private register space sits at BAR0 + 0x200 */
#define LPSS_PRIV_OFFSET	0x200
#define LPSS_PRIV_CLKGATE	0x38	/* dynamic clock gating control */

static unsigned int fs_spklen;
module_param(fs_spklen, uint, 0444);
MODULE_PARM_DESC(fs_spklen,
	"IC_FS_SPKLEN value (0 = auto-detect from speed mode)");

static unsigned int hs_spklen;
module_param(hs_spklen, uint, 0444);
MODULE_PARM_DESC(hs_spklen,
	"IC_HS_SPKLEN value (0 = auto-detect from speed mode)");

static bool clkgate = true;
module_param(clkgate, bool, 0444);
MODULE_PARM_DESC(clkgate, "Enable LPSS dynamic clock gating (default: true)");

/* Alder Lake-N I2C PCI device IDs */
static const unsigned short adl_n_i2c_ids[] = {
	0x54e8, 0x54e9, 0x54ea, 0x54eb,	/* I2C 0-3 */
	0x54c5, 0x54c6,			/* I2C 4-5 */
	0x54d8, 0x54d9,			/* UART (share LPSS block) */
	0
};

#define MAX_CONTROLLERS 8

struct lpss_i2c_fixup {
	void __iomem *base;
	void __iomem *priv;
	struct pci_dev *pdev;
	u8 spklen_fs;
	u8 spklen_hs;
};

static struct lpss_i2c_fixup controllers[MAX_CONTROLLERS];
static int nr_controllers;

static u8 pick_fs_spklen(void __iomem *base)
{
	u32 con;

	if (fs_spklen)
		return fs_spklen;

	con = readl(base + DW_IC_CON);
	if ((con & DW_IC_CON_SPEED_MASK) == DW_IC_CON_SPEED_STD)
		return 1;
	return 6;
}

static u8 pick_hs_spklen(void __iomem *base)
{
	u32 con;

	if (hs_spklen)
		return hs_spklen;

	con = readl(base + DW_IC_CON);
	if ((con & DW_IC_CON_SPEED_MASK) == DW_IC_CON_SPEED_HIGH)
		return 2;
	return 0;
}

static bool wait_controller_disabled(void __iomem *base)
{
	int timeout = 100;

	while (readl(base + DW_IC_ENABLE_STATUS) & 1) {
		if (--timeout <= 0)
			return false;
		udelay(25);
	}
	return true;
}

static void apply_fixup(struct lpss_i2c_fixup *fix)
{
	u32 was_enabled;
	u32 old_fs, old_hs;

	old_fs = readl(fix->base + DW_IC_FS_SPKLEN);
	old_hs = readl(fix->base + DW_IC_HS_SPKLEN);

	if (old_fs == fix->spklen_fs && (!fix->spklen_hs || old_hs == fix->spklen_hs))
		return;

	was_enabled = readl(fix->base + DW_IC_ENABLE) & 1;

	if (was_enabled) {
		writel(0, fix->base + DW_IC_ENABLE);
		if (!wait_controller_disabled(fix->base)) {
			dev_warn(&fix->pdev->dev,
				 "timeout disabling controller\n");
			return;
		}
	}

	writel(fix->spklen_fs, fix->base + DW_IC_FS_SPKLEN);
	if (fix->spklen_hs)
		writel(fix->spklen_hs, fix->base + DW_IC_HS_SPKLEN);

	if (was_enabled)
		writel(1, fix->base + DW_IC_ENABLE);

	dev_info(&fix->pdev->dev, "FS_SPKLEN %u->%u HS_SPKLEN %u->%u\n",
		 old_fs, fix->spklen_fs,
		 old_hs, fix->spklen_hs ? fix->spklen_hs : old_hs);
}

static void apply_clkgate(struct lpss_i2c_fixup *fix)
{
	u32 old;

	if (!fix->priv)
		return;

	old = readl(fix->priv + LPSS_PRIV_CLKGATE);
	if (old == 1)
		return;

	writel(1, fix->priv + LPSS_PRIV_CLKGATE);
	dev_info(&fix->pdev->dev, "clock gating %u->1\n", old);
}

static void apply_all(void)
{
	int i;

	for (i = 0; i < nr_controllers; i++) {
		apply_fixup(&controllers[i]);
		if (clkgate)
			apply_clkgate(&controllers[i]);
	}
}

static int pm_handler(struct notifier_block *nb, unsigned long action,
		      void *data)
{
	if (action == PM_POST_SUSPEND || action == PM_POST_RESTORE) {
		pr_info("re-applying fixups after resume\n");
		apply_all();
	}
	return NOTIFY_OK;
}

static struct notifier_block pm_nb = {
	.notifier_call = pm_handler,
};

static bool is_adl_n_i2c(unsigned short devid)
{
	const unsigned short *p;

	for (p = adl_n_i2c_ids; *p; p++) {
		if (*p == devid)
			return true;
	}
	return false;
}

static int __init i2c_designware_spklen_init(void)
{
	struct pci_dev *pdev = NULL;
	resource_size_t bar_start, bar_len;
	int n = 0;

	while ((pdev = pci_get_device(PCI_VENDOR_ID_INTEL, PCI_ANY_ID, pdev))) {
		if (!is_adl_n_i2c(pdev->device))
			continue;
		if (n >= MAX_CONTROLLERS)
			break;

		bar_start = pci_resource_start(pdev, 0);
		bar_len = pci_resource_len(pdev, 0);
		if (!bar_start || !bar_len)
			continue;

		controllers[n].pdev = pdev;
		controllers[n].base = ioremap(bar_start, bar_len);
		if (!controllers[n].base) {
			dev_warn(&pdev->dev, "ioremap failed\n");
			continue;
		}

		if (bar_len >= LPSS_PRIV_OFFSET + 0x100)
			controllers[n].priv = controllers[n].base + LPSS_PRIV_OFFSET;

		controllers[n].spklen_fs = pick_fs_spklen(controllers[n].base);
		controllers[n].spklen_hs = pick_hs_spklen(controllers[n].base);
		pci_dev_get(pdev);
		n++;
	}

	nr_controllers = n;
	if (!n) {
		pr_info("no ADL-N I2C controllers found\n");
		return -ENODEV;
	}

	apply_all();
	register_pm_notifier(&pm_nb);

	pr_info("patched %d controller(s)\n", n);
	return 0;
}

static void __exit i2c_designware_spklen_exit(void)
{
	int i;

	unregister_pm_notifier(&pm_nb);

	for (i = 0; i < nr_controllers; i++) {
		if (controllers[i].base)
			iounmap(controllers[i].base);
		if (controllers[i].pdev)
			pci_dev_put(controllers[i].pdev);
	}
}

module_init(i2c_designware_spklen_init);
module_exit(i2c_designware_spklen_exit);
