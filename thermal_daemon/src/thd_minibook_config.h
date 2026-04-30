#ifndef THD_MINIBOOK_CONFIG_H_
#define THD_MINIBOOK_CONFIG_H_

/* MiniBook platform-specific configuration.
 * Values derived from GDDV firmware + ITMT3/NNPID .config files. */

/* SEN3 ambient trip (millidegrees C). GDDV has 55C; lowered for
 * earlier PL1 intervention. Consolidation adds a second trip at +1C. */
#define MINIBOOK_SEN3_TRIP_TEMP		50000

/* PPCC PL1 range (milliwatts). GDDV PPCC is locked (min==max==15W);
 * override with ITMT3 range. */
#define MINIBOOK_PPCC_PL1_MIN		8000
#define MINIBOOK_PPCC_PL1_MAX		17000
#define MINIBOOK_PPCC_STEP_SIZE		500

/* PID gains for RAPL cdev (error: millidegrees, output: microwatts).
 * Negative signs: overtemp → lower PL1. */
#define MINIBOOK_PID_KP			-1000
#define MINIBOOK_PID_KI			-100
#define MINIBOOK_PID_KD			-10

/* minibook_ec thermal zones. These are not DPTF participants, so
 * GDDV PSVT tables don't cover them. We install trips manually. */
#define MINIBOOK_SOC_ZONE		"minibook_soc"
#define MINIBOOK_CHARGER_ZONE		"minibook_charger"
#define MINIBOOK_SOC_TRIP_TEMP		80000	/* 80 C — SoC passive trip */
#define MINIBOOK_CHARGER_TRIP_TEMP	60000	/* 60 C — charger passive trip */

/* Hardware thermal limits (mirrors tools/verify-bios-tweaks.sh). */
#define MINIBOOK_TJMAX			105	/* fused TjMax (degrees C) */
#define MINIBOOK_STOCK_TCC_OFFSET	20	/* stock: throttle at 85 C */
#define MINIBOOK_TCC_SYSFS \
	"/sys/bus/pci/devices/0000:00:04.0/tcc_offset_degree_celsius"
#define MINIBOOK_RAPL_PL1_SYSFS \
	"/sys/class/powercap/intel-rapl:0/constraint_0_power_limit_uw"

#endif /* THD_MINIBOOK_CONFIG_H_ */
