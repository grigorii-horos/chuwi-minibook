// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * I2EC (Indirect EC) host access to IT5570E internal registers.
 *
 * The 256-byte ACPI EC window (0x00-0xFF) only exposes EC SRAM bytes the
 * firmware chose to publish. The full 16-bit EC internal address space
 * (including hardware modules like PWM/ADC/GPIO/SMBus) is reachable from
 * the host via PNPCFG Depth-2 I/O at port 0x4E/0x4F. The IT5570E datasheet
 * section 6.3.2.9 describes this interface.
 *
 * BADRSEL on this board selects the 0x4E base (not the default 0x2E).
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/mutex.h>

#include "minibook_ec.h"

#define PNPCFG_INDEX		0x4E
#define PNPCFG_DATA		0x4F
#define PNPCFG_IO_SIZE		2
#define PNPCFG_D2ADR		0x2E
#define PNPCFG_D2DAT		0x2F
#define I2EC_SUB_ADDR_L		0x10
#define I2EC_SUB_ADDR_H		0x11
#define I2EC_SUB_DATA		0x12

#define IT5570E_ECHIPID1	0x2000
#define IT5570E_ECHIPID2	0x2001
#define IT5570E_ID1		0x55
#define IT5570E_ID2		0x70

static void pnpcfg_select(u8 index, u8 sub_index)
{
	outb(index, PNPCFG_INDEX);
	outb(sub_index, PNPCFG_DATA);
}

u8 minibook_ec_i2ec_read(struct minibook_ec *ec, u16 addr)
{
	u8 val;

	mutex_lock(&ec->i2ec_lock);

	pnpcfg_select(PNPCFG_D2ADR, I2EC_SUB_ADDR_H);
	pnpcfg_select(PNPCFG_D2DAT, (addr >> 8) & 0xFF);

	pnpcfg_select(PNPCFG_D2ADR, I2EC_SUB_ADDR_L);
	pnpcfg_select(PNPCFG_D2DAT, addr & 0xFF);

	pnpcfg_select(PNPCFG_D2ADR, I2EC_SUB_DATA);
	outb(PNPCFG_D2DAT, PNPCFG_INDEX);
	val = inb(PNPCFG_DATA);

	mutex_unlock(&ec->i2ec_lock);
	return val;
}

void minibook_ec_i2ec_write(struct minibook_ec *ec, u16 addr, u8 val)
{
	mutex_lock(&ec->i2ec_lock);

	pnpcfg_select(PNPCFG_D2ADR, I2EC_SUB_ADDR_H);
	pnpcfg_select(PNPCFG_D2DAT, (addr >> 8) & 0xFF);

	pnpcfg_select(PNPCFG_D2ADR, I2EC_SUB_ADDR_L);
	pnpcfg_select(PNPCFG_D2DAT, addr & 0xFF);

	pnpcfg_select(PNPCFG_D2ADR, I2EC_SUB_DATA);
	pnpcfg_select(PNPCFG_D2DAT, val);

	mutex_unlock(&ec->i2ec_lock);
}

int minibook_ec_i2ec_init(struct minibook_ec *ec)
{
	if (!request_region(PNPCFG_INDEX, PNPCFG_IO_SIZE, DRIVER_NAME)) {
		dev_warn(&ec->pdev->dev,
			 "PNPCFG I/O region 0x%X busy, I2EC unavailable\n",
			 PNPCFG_INDEX);
		return -EBUSY;
	}
	ec->pnpcfg_available = true;

	if (minibook_ec_i2ec_read(ec, IT5570E_ECHIPID1) != IT5570E_ID1 ||
	    minibook_ec_i2ec_read(ec, IT5570E_ECHIPID2) != IT5570E_ID2) {
		dev_warn(&ec->pdev->dev,
			 "IT5570E chip ID not detected via I2EC\n");
		release_region(PNPCFG_INDEX, PNPCFG_IO_SIZE);
		ec->pnpcfg_available = false;
		return -ENODEV;
	}
	return 0;
}

void minibook_ec_i2ec_cleanup(struct minibook_ec *ec)
{
	if (ec->pnpcfg_available) {
		release_region(PNPCFG_INDEX, PNPCFG_IO_SIZE);
		ec->pnpcfg_available = false;
	}
}

