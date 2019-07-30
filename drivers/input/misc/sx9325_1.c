/*! \file sx9325_1.c
 * \brief	SX9325_1 Driver
 *
 * Driver for the SX9325_1
 * Copyright (c) 2011 Semtech Corp
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */
#define DEBUG
#define DRIVER_NAME "sx9325_1"

#define MAX_WRITE_ARRAY_SIZE 32
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include "sx9325.h"
#include <linux/fb.h>
#include <soc/qcom/socinfo.h> /*for pv-version check*/
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include <soc/qcom/vendor/board_id.h> /*@zte board id*/

#define IDLE 0
#define ACTIVE 1
#define USER_BUF_SIZE 100

#define SX9325_1_TAG					"==ZTE-SX9325_1=="
#define SX9325_1_DBG(fmt, args...)	printk(SX9325_1_TAG fmt, ##args)
#define SX9325_1_DBG2(fmt, args...)
#define SAR_GPIO_EINT_PIN	63
#define MSM_BOARD_ID_T3   7
#define MSM_BOARD_ID_POAD_T3  13

struct device *sx9325_1_dev_zte;

static struct smtc_reg_data sx9325_1_i2c_reg_setup[] = {
	/*Interrupt and config*/
	{
		.reg = SX9325_IRQ_ENABLE_REG,
		.val = 0x60,
	},
	{
		.reg = SX9325_IRQCFG0_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_IRQCFG1_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_IRQCFG2_REG,
		.val = 0x00,
	},
	/*--------General control*/
	{
		.reg = SX9325_CTRL0_REG,
		.val = 0x0F,
	},
	{
		.reg = SX9325_I2CADDR_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_CLKSPRD,
		.val = 0x00,
	},
	/*--------AFE Control*/
	{
		.reg = SX9325_AFE_CTRL0_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x80,/*changed by semtech 20180320*/
	#else
		.val = 0x80,
	#endif
	},
	{
		.reg = SX9325_AFE_CTRL1_REG,
		.val = 0x10,/*reserved*/
	},
	{
		.reg = SX9325_AFE_CTRL2_REG,
		.val = 0x00,/*reserved*/
	},
	{
		.reg = SX9325_AFE_CTRL3_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_AFE_CTRL4_REG,
		.val = 0x44,
	},
	{
		.reg = SX9325_AFE_CTRL5_REG,
		.val = 0x00,/*reserved*/
	},
	{
		.reg = SX9325_AFE_CTRL6_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_AFE_CTRL7_REG,
		.val = 0x44,
	},
	{
		.reg = SX9325_AFE_PH0_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x26,
	#elif (defined CONFIG_SAR_COPPER_SUPPORT)
		.val = 0x29,
	#elif defined(CONFIG_SAR_CS0_USED)
		.val = 0x01,
	#elif defined(CONFIG_SAR_CS1_USED)
		.val = 0x04,
	#else
		.val = 0x2A,
	#endif
	},
	{
		.reg = SX9325_AFE_PH1_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x00, /* All are HZ, not used */
	#else
		.val = 0x29,
	#endif
	},
	{
		.reg = SX9325_AFE_PH2_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x00, /* All are HZ, not used */
	#else
		.val = 0x00,
	#endif
	},
	{
		.reg = SX9325_AFE_PH3_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x00, /* All are HZ, not used */
	#else
		.val = 0x00,
	#endif
	},
	{
		.reg = SX9325_AFE_CTRL8,
		.val = 0x12,
	},
	{
		.reg = SX9325_AFE_CTRL9,
		.val = 0x08,
	},
	/*--------PROX control*/
	{
		.reg = SX9325_PROX_CTRL0_REG,
		.val = 0x0B,/*0x1C*/
	},
	{
		.reg = SX9325_PROX_CTRL1_REG,
		.val = 0x19,
	},
	{
		.reg = SX9325_PROX_CTRL2_REG,
		.val = 0x20,
	},
	{
		.reg = SX9325_PROX_CTRL3_REG,
		.val = 0x60,
	},
	{
		.reg = SX9325_PROX_CTRL4_REG,
		.val = 0x0C,
	},
	{
		.reg = SX9325_PROX_CTRL5_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x10,/*changed by semtech 20180320*/
	#else
		.val = 0x10,
	#endif
	},
	{
		.reg = SX9325_PROX_CTRL6_REG,
		.val = 0x3A,
	},
	{
		.reg = SX9325_PROX_CTRL7_REG,
		.val = 0x85,
	},
	/*--------Advanced control*/
	{
		.reg = SX9325_ADV_CTRL0_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL1_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL2_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL3_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL4_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL5_REG,
		.val = 0x05,
	},
	{
		.reg = SX9325_ADV_CTRL6_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL7_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL8_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL9_REG,
		.val = 0x80,
	},
	{
		.reg = SX9325_ADV_CTRL10_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL11_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL12_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL13_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL14_REG,
		.val = 0x80,
	},
	{
		.reg = SX9325_ADV_CTRL15_REG,
		.val = 0x0C,
	},
	{
		.reg = SX9325_ADV_CTRL16_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL17_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL18_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL19_REG,
		.val = 0xF0,
	},
	{
		.reg = SX9325_ADV_CTRL20_REG,
		.val = 0xF0,
	},
	/*--------Sensor enable*/
	{
		.reg = SX9325_CTRL1_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x21,/*enable PH0*/
	#else
		.val = 0x22,/*enable PH1 and PH2*/
	#endif
	},
};

/*@zte board id, begin*/
static struct smtc_reg_data sx9325_1_i2c_reg_temp[] = {
	/*Interrupt and config*/
	{
		.reg = SX9325_IRQ_ENABLE_REG,
		.val = 0x60,
	},
	{
		.reg = SX9325_IRQCFG0_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_IRQCFG1_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_IRQCFG2_REG,
		.val = 0x00,
	},
	/*--------General control*/
	{
		.reg = SX9325_CTRL0_REG,
		.val = 0x0F,
	},
	{
		.reg = SX9325_I2CADDR_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_CLKSPRD,
		.val = 0x00,
	},
	/*--------AFE Control*/
	{
		.reg = SX9325_AFE_CTRL0_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x80,/*changed by semtech 20180320*/
	#else
		.val = 0x80,
	#endif
	},
	{
		.reg = SX9325_AFE_CTRL1_REG,
		.val = 0x10,/*reserved*/
	},
	{
		.reg = SX9325_AFE_CTRL2_REG,
		.val = 0x00,/*reserved*/
	},
	{
		.reg = SX9325_AFE_CTRL3_REG,
		.val = 0x01,
	},
	{
		.reg = SX9325_AFE_CTRL4_REG,
		.val = 0xBC,
	},
	{
		.reg = SX9325_AFE_CTRL5_REG,
		.val = 0x00,/*reserved*/
	},
	{
		.reg = SX9325_AFE_CTRL6_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_AFE_CTRL7_REG,
		.val = 0x44,
	},
	{
		.reg = SX9325_AFE_PH0_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x26,
	#elif (defined CONFIG_SAR_COPPER_SUPPORT)
		.val = 0x29,
	#elif defined(CONFIG_SAR_CS0_USED)
		.val = 0x01,
	#elif defined(CONFIG_SAR_CS1_USED)
		.val = 0x04,
	#else
		.val = 0x2A,
	#endif
	},
	{
		.reg = SX9325_AFE_PH1_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x00, /* All are HZ, not used */
	#else
		.val = 0x29,
	#endif
	},
	{
		.reg = SX9325_AFE_PH2_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x00, /* All are HZ, not used */
	#else
		.val = 0x00,
	#endif
	},
	{
		.reg = SX9325_AFE_PH3_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x00, /* All are HZ, not used */
	#else
		.val = 0x00,
	#endif
	},
	{
		.reg = SX9325_AFE_CTRL8,
		.val = 0x12,
	},
	{
		.reg = SX9325_AFE_CTRL9,
		.val = 0x08,
	},
	/*--------PROX control*/
	{
		.reg = SX9325_PROX_CTRL0_REG,
		.val = 0x1C,
	},
	{
		.reg = SX9325_PROX_CTRL1_REG,
		.val = 0x19,
	},
	{
		.reg = SX9325_PROX_CTRL2_REG,
		.val = 0x20,
	},
	{
		.reg = SX9325_PROX_CTRL3_REG,
		.val = 0x60,
	},
	{
		.reg = SX9325_PROX_CTRL4_REG,
		.val = 0x0C,
	},
	{
		.reg = SX9325_PROX_CTRL5_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x10,/*changed by semtech 20180320*/
	#else
		.val = 0x10,
	#endif
	},
	{
		.reg = SX9325_PROX_CTRL6_REG,
		.val = 0x3A,
	},
	{
		.reg = SX9325_PROX_CTRL7_REG,
		.val = 0x85,
	},
	/*--------Advanced control*/
	{
		.reg = SX9325_ADV_CTRL0_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL1_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL2_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL3_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL4_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL5_REG,
		.val = 0x05,
	},
	{
		.reg = SX9325_ADV_CTRL6_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL7_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL8_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL9_REG,
		.val = 0x80,
	},
	{
		.reg = SX9325_ADV_CTRL10_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL11_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL12_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL13_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL14_REG,
		.val = 0x80,
	},
	{
		.reg = SX9325_ADV_CTRL15_REG,
		.val = 0x0C,
	},
	{
		.reg = SX9325_ADV_CTRL16_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL17_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL18_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL19_REG,
		.val = 0xF0,
	},
	{
		.reg = SX9325_ADV_CTRL20_REG,
		.val = 0xF0,
	},
	/*--------Sensor enable*/
	{
		.reg = SX9325_CTRL1_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x21,/*enable PH0*/
	#else
		.val = 0x22,/*enable PH1 and PH2*/
	#endif
	},
};
/*@zte board id, end*/

#if 0
/*@zte board id, begin*/
static struct smtc_reg_data sx9325_1_i2c_reg_sleep[] = {
	/*Interrupt and config*/
	{
		.reg = SX9325_IRQ_ENABLE_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_IRQCFG0_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_IRQCFG1_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_IRQCFG2_REG,
		.val = 0x00,
	},
	/*--------General control*/
	{
		.reg = SX9325_CTRL0_REG,
		.val = 0x0F,
	},
	{
		.reg = SX9325_I2CADDR_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_CLKSPRD,
		.val = 0x00,
	},
	/*--------AFE Control*/
	{
		.reg = SX9325_AFE_CTRL0_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x80,/*changed by semtech 20180320*/
	#else
		.val = 0x80,
	#endif
	},
	{
		.reg = SX9325_AFE_CTRL1_REG,
		.val = 0x10,/*reserved*/
	},
	{
		.reg = SX9325_AFE_CTRL2_REG,
		.val = 0x00,/*reserved*/
	},
	{
		.reg = SX9325_AFE_CTRL3_REG,
		.val = 0x01,
	},
	{
		.reg = SX9325_AFE_CTRL4_REG,
		.val = 0xBC,
	},
	{
		.reg = SX9325_AFE_CTRL5_REG,
		.val = 0x00,/*reserved*/
	},
	{
		.reg = SX9325_AFE_CTRL6_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_AFE_CTRL7_REG,
		.val = 0x44,
	},
	{
		.reg = SX9325_AFE_PH0_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x26,
	#elif (defined CONFIG_SAR_COPPER_SUPPORT)
		.val = 0x29,
	#elif defined(CONFIG_SAR_CS0_USED)
		.val = 0x01,
	#elif defined(CONFIG_SAR_CS1_USED)
		.val = 0x04,
	#else
		.val = 0x2A,
	#endif
	},
	{
		.reg = SX9325_AFE_PH1_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x00, /* All are HZ, not used */
	#else
		.val = 0x29,
	#endif
	},
	{
		.reg = SX9325_AFE_PH2_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x00, /* All are HZ, not used */
	#else
		.val = 0x00,
	#endif
	},
	{
		.reg = SX9325_AFE_PH3_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x00, /* All are HZ, not used */
	#else
		.val = 0x00,
	#endif
	},
	{
		.reg = SX9325_AFE_CTRL8,
		.val = 0x12,
	},
	{
		.reg = SX9325_AFE_CTRL9,
		.val = 0x08,
	},
	/*--------PROX control*/
	{
		.reg = SX9325_PROX_CTRL0_REG,
		.val = 0x1C,
	},
	{
		.reg = SX9325_PROX_CTRL1_REG,
		.val = 0x19,
	},
	{
		.reg = SX9325_PROX_CTRL2_REG,
		.val = 0x20,
	},
	{
		.reg = SX9325_PROX_CTRL3_REG,
		.val = 0x60,
	},
	{
		.reg = SX9325_PROX_CTRL4_REG,
		.val = 0x0C,
	},
	{
		.reg = SX9325_PROX_CTRL5_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x10,/*changed by semtech 20180320*/
	#else
		.val = 0x10,
	#endif
	},
	{
		.reg = SX9325_PROX_CTRL6_REG,
		.val = 0x3A,
	},
	{
		.reg = SX9325_PROX_CTRL7_REG,
		.val = 0x85,
	},
	/*--------Advanced control*/
	{
		.reg = SX9325_ADV_CTRL0_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL1_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL2_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL3_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL4_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL5_REG,
		.val = 0x05,
	},
	{
		.reg = SX9325_ADV_CTRL6_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL7_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL8_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL9_REG,
		.val = 0x80,
	},
	{
		.reg = SX9325_ADV_CTRL10_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL11_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL12_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL13_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL14_REG,
		.val = 0x80,
	},
	{
		.reg = SX9325_ADV_CTRL15_REG,
		.val = 0x0C,
	},
	{
		.reg = SX9325_ADV_CTRL16_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL17_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL18_REG,
		.val = 0x00,
	},
	{
		.reg = SX9325_ADV_CTRL19_REG,
		.val = 0xF0,
	},
	{
		.reg = SX9325_ADV_CTRL20_REG,
		.val = 0xF0,
	},
	/*--------Sensor enable*/
	{
		.reg = SX9325_CTRL1_REG,
	#ifdef CONFIG_BOARD_SMILE8917
		.val = 0x21,/*enable PH0*/
	#else
		.val = 0x00,/*enable PH1 and PH2*/
	#endif
	},
};
/*@zte board id, end*/
#endif
#ifdef BOARD_USES_SAR_CHIP
static struct _buttonInfo psmtcButtons[] = {
	{
	.keycode = KEY_F20,
	.mask = SX9325_TCHCMPSTAT_TCHSTAT0_FLAG,
	},
	{
	.keycode = KEY_F22,
	.mask = SX9325_TCHCMPSTAT_TCHSTAT1_FLAG,
	},
	{
	.keycode = KEY_F21,
	.mask = SX9325_TCHCMPSTAT_TCHSTAT2_FLAG,
	},
	{
	.keycode = KEY_F23,
	.mask = SX9325_TCHCMPSTAT_TCHCOMB_FLAG,
	},
};
#endif
typedef struct sx93XX sx93XX_t, *psx93XX_t;
struct sx93XX {
	void *bus;
	struct device *pdev;
	spinlock_t	lock;
	int irq;
	int board_id;
	char irq_disabled;
	struct delayed_work dworker;
	struct input_dev *pinput;
	struct _buttonInfo *buttons;
	int buttonSize;
	struct pinctrl				*pinctrl;
	struct pinctrl_state		*sx9325_1_active;
	struct pinctrl_state		*sx9325_1_sleep;
	struct regulator			 *sx9325_1_vdd;
	struct regulator			 *sx9325_1_svdd;
};

psx93XX_t g_sx9325_1_chip = 0;
unsigned char sx9235_1_g_enable_log = 0;
int sx9235_1_disable_sar_in_call = 0;
int sx9325_1_need_input_index = -1;
static void sx9325_1_touchProcess(psx93XX_t chip);

static void ForcetoTouched(psx93XX_t chip)
{
	struct input_dev *input = NULL;
	struct _buttonInfo *pCurrentButton	= NULL;

	if (chip) {
		dev_dbg(chip->pdev, "ForcetoTouched()\n");

		pCurrentButton = &chip->buttons[0];
		input = chip->pinput;

		input_report_key(input, pCurrentButton->keycode, 1);
		pCurrentButton->state = ACTIVE;

		input_sync(input);

		dev_dbg(chip->pdev, "Leaving ForcetoTouched()\n");
	}
}

static void ForcetoInput(psx93XX_t chip, int val)
{
	struct input_dev *input = NULL;
	struct _buttonInfo *pCurrentButton	= NULL;

	if (chip) {
		dev_dbg(chip->pdev, "ForcetoInput() val = %d\n", val);

		if (val == 0) {
			pCurrentButton = &chip->buttons[0];
		} else if (sx9325_1_need_input_index >= 0 && val == 1) {
			pCurrentButton = &chip->buttons[sx9325_1_need_input_index];
		}
		input = chip->pinput;
		input_report_key(input, pCurrentButton->keycode, val);
		input_sync(input);
		dev_dbg(chip->pdev, "Leaving ForcetoInput()\n");
	}
}

static int write_register(psx93XX_t chip, u8 address, u8 value)
{
	struct i2c_client *i2c = 0;
	char buffer[2];
	int returnValue = 0;

	buffer[0] = address;
	buffer[1] = value;
	returnValue = -ENOMEM;
	if (chip && chip->bus) {
	i2c = chip->bus;

	returnValue = i2c_master_send(i2c, buffer, 2);
	dev_dbg(&i2c->dev, "write_register Address: 0x%x Value: 0x%x Return: %d\n",
			address, value, returnValue);
	}
	if (returnValue < 0) {
	ForcetoTouched(chip);
	dev_info(chip->pdev, "Write_register-ForcetoTouched()\n");
	}
	return returnValue;
}

static int read_register(psx93XX_t chip, u8 address, u8 *value)
{
	struct i2c_client *i2c = 0;
	s32 returnValue = 0;

	if (chip && value && chip->bus) {
	i2c = chip->bus;
	returnValue = i2c_smbus_read_byte_data(i2c, address);
		dev_dbg(&i2c->dev, "read_register Address: 0x%x Return: 0x%x\n", address, returnValue);
	if (returnValue >= 0) {
		*value = returnValue;
		return 0;
	} else {
		return returnValue;
	}
	}
	ForcetoTouched(chip);
	dev_info(chip->pdev, "read_register-ForcetoTouched()\n");
	return -ENOMEM;
}

static int manual_offset_calibration(psx93XX_t chip)
{
	s32 returnValue = 0;

	returnValue = write_register(chip, SX9325_STAT2_REG, 0x0F);
	return returnValue;
}

static ssize_t manual_offset_calibration_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	u8 reg_value = 0;
	psx93XX_t chip = dev_get_drvdata(dev);

	dev_dbg(chip->pdev, "Reading IRQSTAT_REG\n");
	read_register(chip, SX9325_IRQSTAT_REG, &reg_value);
	return snprintf(buf, sizeof(buf) - 1, "%d\n", reg_value);
}

static ssize_t manual_offset_calibration_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	psx93XX_t chip = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;
	if (val) {
	dev_info(chip->pdev, "Performing manual_offset_calibration()\n");
	manual_offset_calibration(chip);
	}
	return count;
}

static DEVICE_ATTR(calibrate, 0664, manual_offset_calibration_show, manual_offset_calibration_store);

unsigned char sx9325_1_g_debug_reg = 0;

static ssize_t regval_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	psx93XX_t chip = dev_get_drvdata(dev);

	unsigned char debug_reg[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
	0x10, 0x11, 0x14, 0x15,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
	0x2A, 0x2B, 0x2C, 0x2D,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
	0x9F, 0xFA, 0xFE
	};
	int debug_index = 0;
	unsigned char debug_reg_val;
	int reg_count = 0;
	int return_size = 0;

	reg_count = sizeof(debug_reg);
	for (debug_index = 0; debug_index < reg_count; debug_index++) {
		read_register(chip, debug_reg[debug_index], &debug_reg_val);
		/* pr_err("jiangfeng %s, sx9502 reg 0x%x value = 0x%x\n",
					__func__, debug_reg[debug_index], debug_reg_val); */
		return_size += snprintf(buf + return_size, 32,
		"sx9502 reg 0x%x value = 0x%x\n", debug_reg[debug_index], debug_reg_val);
	}
	/* return sprintf(buf, "success\n"); */
	return return_size;
}

static ssize_t regval_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	psx93XX_t chip = dev_get_drvdata(dev);
	unsigned long val;
	unsigned reg_val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;
	reg_val = val;
	write_register(chip, sx9325_1_g_debug_reg, val);
	return count;
}
static DEVICE_ATTR(regval, 0664, regval_show,
	regval_store);

static ssize_t reg_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 64, "0x%x\n", sx9325_1_g_debug_reg);
}

static ssize_t reg_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	sx9325_1_g_debug_reg = val;

	return count;
}
static DEVICE_ATTR(reg, 0664, reg_show, reg_store);

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	psx93XX_t chip = dev_get_drvdata(dev);
	u8 touchStatus = 0;

	/*if (socinfo_get_ftm_flag())
		return snprintf(buf, 64, "0\n");*/
	pr_err("=status_show==read_sx9235_1_disable_sar_in_call: Value: %d\n", sx9235_1_disable_sar_in_call);
	if (sx9235_1_disable_sar_in_call == 1)
		return snprintf(buf, 64, "0\n");

	read_register(chip, SX9325_IRQ_ENABLE_REG, &touchStatus);
	if (touchStatus == 0)
		return snprintf(buf, 64, "0\n");

	write_register(chip, SX9325_CPSRD, 1);/*read ph1/CS0 default*/

	read_register(chip, SX9325_STAT0_REG, &touchStatus);
	pr_err("=status_show==read_register SX9325_1_STAT0_REG: Value: 0x%x\n", touchStatus);
	/*dump_register(chip);*/

	if (touchStatus&0x2)	{
		return snprintf(buf, 64, "1\n");
	} else {
		return snprintf(buf, 64, "0\n");
	}
}

static DEVICE_ATTR(status, 0444, status_show,
					NULL);

static ssize_t sx9325_1_enable_log_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 64, "0x%x\n", sx9235_1_g_enable_log);
}

static ssize_t sx9325_1_enable_log_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	unsigned long val;
	psx93XX_t chip = dev_get_drvdata(dev);

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	sx9235_1_g_enable_log = val;
	if (sx9235_1_g_enable_log)	{
		write_register(chip, SX9325_IRQ_ENABLE_REG, 0x7f);
		enable_irq_wake(chip->irq);
	} else {
		write_register(chip, SX9325_IRQ_ENABLE_REG, 0x60);
		disable_irq_wake(chip->irq);
	}

	return count;
}
static DEVICE_ATTR(sx9325_1_enable_log, 0664, sx9325_1_enable_log_show, sx9325_1_enable_log_store);

/*added by chenhui for enable/disable sar irq*/
static ssize_t sx9325_1_enable_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	u8 reg_value = 0;
	psx93XX_t chip = dev_get_drvdata(dev);

	dev_dbg(chip->pdev, "Reading SX9325_1_IRQ_ENABLE_REG\n");
	read_register(chip, SX9325_IRQ_ENABLE_REG, &reg_value);

	return snprintf(buf, sizeof(buf) - 1, "%d\n", reg_value);
}

static ssize_t sx9325_1_enable_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	unsigned long val;
	psx93XX_t chip = dev_get_drvdata(dev);

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	dev_dbg(chip->pdev, "enable_store val %d\n", (int)val);

	if (val)	{
		write_register(chip, SX9325_IRQ_ENABLE_REG, 0x60);
	} else {
		write_register(chip, SX9325_IRQ_ENABLE_REG, 0x00);
	}

	return count;
}
static DEVICE_ATTR(sx9325_1_enable, 0664, sx9325_1_enable_show, sx9325_1_enable_store);

/*
static ssize_t sx9235_1_disable_sar_in_call_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	psx93XX_t chip = dev_get_drvdata(dev);

	dev_dbg(chip->pdev, "Reading sx9235_1_disable_sar_in_call_show\n");
	return snprintf(buf, sizeof(buf) - 1, "%d\n", sx9235_1_disable_sar_in_call);
}

static ssize_t sx9235_1_disable_sar_in_call_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	unsigned long val;
	psx93XX_t chip = dev_get_drvdata(dev);

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	dev_dbg(chip->pdev, "Setting sx9235_1_disable_sar_in_call val %d\n", (int)val);

	if (val) {
		sx9235_1_disable_sar_in_call = 1;
		ForcetoTouched(chip);
	} else {
		sx9235_1_disable_sar_in_call = 0;
		dev_dbg(chip->pdev, "Set disable_sar_in_call sx9325_1_index %d\n", sx9325_1_need_input_index);
		if (sx9325_1_need_input_index >= 0)
			ForcetoInput(chip);
	}

	return count;
}
static DEVICE_ATTR(sx9235_1_disable_sar_in_call, 0664, sx9235_1_disable_sar_in_call_show, \
sx9235_1_disable_sar_in_call_store);
*/
static ssize_t sx9235_1_disable_sar_in_call_show(struct file *file,
	char __user *buffer, size_t size, loff_t *ppos)
{
	int count = 0;
	int ret = 0;
	char data_buf[USER_BUF_SIZE] = {0};
	psx93XX_t chip = dev_get_drvdata(sx9325_1_dev_zte);

	dev_dbg(chip->pdev, "Reading sx9235_1_disable_sar_in_call_show\n");

	if (*ppos)
		return 0;

	count = snprintf(data_buf, sizeof(data_buf), "%u\n", sx9235_1_disable_sar_in_call);
	ret = copy_to_user(buffer, data_buf, count);
	if (ret)
		return -EINVAL;
	*ppos += count;

	return count;
}

static ssize_t sx9235_1_disable_sar_in_call_store(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	unsigned int input;
	char data_buf[USER_BUF_SIZE] = {0};
	psx93XX_t chip = dev_get_drvdata(sx9325_1_dev_zte);

	ret = copy_from_user(data_buf, buffer, count);
	if (ret)
		return -EINVAL;
	ret = kstrtouint(data_buf, 0, &input);
	if (ret)
		return -EINVAL;

	input = input > 0 ? 1 : 0;
	dev_dbg(chip->pdev, "stone Setting sx9235_1_disable_sar_in_call val %d\n", (int)input);

	if (input) {
		sx9235_1_disable_sar_in_call = 1;
		ForcetoInput(chip, 0);
	} else {
		sx9235_1_disable_sar_in_call = 0;
		dev_dbg(chip->pdev, "stone disable_sar_in_call sx9325_1_index %d\n", sx9325_1_need_input_index);
		if (sx9325_1_need_input_index >= 0)
			ForcetoInput(chip, 1);
	}

	return count;
}
/*end*/
static int read_regStat(psx93XX_t chip)
{
	u8 data = 0;

	if (chip) {
	if (read_register(chip, SX9325_IRQSTAT_REG, &data) == 0)
	return (data & 0x00FF);
	}
	return 0;
}

/*-----------------------dev_atrribute operations-------------------------------*/
#ifdef BOARD_USES_SAR_CHIP
static ssize_t sx9325_1_register_write_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int reg_address = 0, val = 0;
	psx93XX_t this = dev_get_drvdata(dev);

	if (sscanf(buf, "%x,%x", &reg_address, &val) != 2) {
		pr_err("The number of data are wrong\n");
		return -EINVAL;
	}

	write_register(this, (unsigned char)reg_address, (unsigned char)val);
	dev_info(this->pdev, "%s - Register(0x%x) data(0x%x)\n", __func__, reg_address, val);

	return count;
}

static ssize_t sx9325_1_register_read_store(struct device *dev,
			   struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val = 0;
	int regist = 0;
	psx93XX_t this = dev_get_drvdata(dev);
	int val_gpio = -1;

	dev_info(this->pdev, "Reading register\n");

	if (kstrtoint(buf, 10, &regist) != 1) {
		pr_err("The number of data are wrong\n");
		return -EINVAL;
	}

	read_register(this, regist, &val);
	dev_info(this->pdev, "%s - Register(0x%2x) data(0x%2x)\n", __func__, regist, val);

	mdelay(50);
	val_gpio = gpio_get_value(SAR_GPIO_EINT_PIN);
	dev_info(this->pdev, "irq gpio val_gpio=%d\n", val_gpio);

	return count;
}


static ssize_t sx9325_1_raw_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 msb = 0, lsb = 0;
	u8 i;
	s32 useful;
	s32 average;
	s32 diff;
	u16 offset;
	psx93XX_t this = dev_get_drvdata(dev);

	for (i = 0; i < 3; i++) {
		write_register(this, SX9325_CPSRD, i);
		read_register(this, SX9325_USEMSB, &msb);
		read_register(this, SX9325_USELSB, &lsb);
		useful = (s32)((msb << 8) | lsb);

		read_register(this, SX9325_AVGMSB, &msb);
		read_register(this, SX9325_AVGLSB, &lsb);
		average = (s32)((msb << 8) | lsb);

		read_register(this, SX9325_OFFSETMSB, &msb);
		read_register(this, SX9325_OFFSETLSB, &lsb);
		offset = (u16)((msb << 8) | lsb);

		read_register(this, SX9325_DIFFMSB, &msb);
		read_register(this, SX9325_DIFFLSB, &lsb);
		diff = (s32)((msb << 8) | lsb);

		if (useful > 32767)
			useful -= 65536;
		if (diff > 32767)
			diff -= 65536;
		if (average > 32767)
			average -= 65536;
		dev_info(this->pdev, "PH[%d]: Useful : %d Average : %d, Offset : %d, DIFF : %d\n",
				i, useful, average, offset, diff);
	}
	return 0;
}

static ssize_t chipinfo_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 reg_value[2] = {0};
	psx93XX_t this = dev_get_drvdata(dev);

	if (this == NULL) {
		pr_err("i2c client is null!!\n");
		return 0;
	}

	read_register(this, SX9325_WHOAMI_REG, &reg_value[0]);
	read_register(this, SX9325_REV_REG, &reg_value[1]);
	return snprintf(buf, 128, "WHOAMI:%d REV:%d\n", reg_value[0], reg_value[1]);
}

static ssize_t diff_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 msb = 0, lsb = 0;
	s32 useful_cs0, useful_cs1;
	s32 average_cs0, average_cs1;
	s32 diff_cs0, diff_cs1;
	psx93XX_t this = dev_get_drvdata(dev);

/*main channel*/
	write_register(this, SX9325_CPSRD, 0);
	read_register(this, SX9325_USEMSB, &msb);
	read_register(this, SX9325_USELSB, &lsb);
	useful_cs0 = (s32)((msb << 8) | lsb);

	read_register(this, SX9325_AVGMSB, &msb);
	read_register(this, SX9325_AVGLSB, &lsb);
	average_cs0 = (s32)((msb << 8) | lsb);

	read_register(this, SX9325_DIFFMSB, &msb);
	read_register(this, SX9325_DIFFLSB, &lsb);
	diff_cs0 = (s32)((msb << 8) | lsb);
/*wifi channel*/
	write_register(this, SX9325_CPSRD, 1);
	read_register(this, SX9325_USEMSB, &msb);
	read_register(this, SX9325_USELSB, &lsb);
	useful_cs1 = (s32)((msb << 8) | lsb);

	read_register(this, SX9325_AVGMSB, &msb);
	read_register(this, SX9325_AVGLSB, &lsb);
	average_cs1 = (s32)((msb << 8) | lsb);

	read_register(this, SX9325_DIFFMSB, &msb);
	read_register(this, SX9325_DIFFLSB, &lsb);
	diff_cs1 = (s32)((msb << 8) | lsb);

	if (useful_cs0 > 32767)
		useful_cs0 -= 65536;
	if (diff_cs0 > 32767)
		diff_cs0 -= 65536;
	if (average_cs0 > 32767)
		average_cs0 -= 65536;
	if (useful_cs1 > 32767)
		useful_cs1 -= 65536;
	if (diff_cs1 > 32767)
		diff_cs1 -= 65536;
	if (average_cs1 > 32767)
		average_cs1 -= 65536;

	return snprintf(buf, 128, "%d,%d,%d,%d,%d,%d\n",
		useful_cs1, average_cs1, diff_cs1, useful_cs0, average_cs0, diff_cs0);
	dev_info(this->pdev, "Useful0 : %d Average0 : %d, DIFF0 : %d Useful1 : %d Average1 : %d, DIFF1 : %d\n",
			useful_cs0, average_cs0, diff_cs0, useful_cs1, average_cs1, diff_cs1);
	return 0;
}

static DEVICE_ATTR(register_write,  0664, NULL, sx9325_1_register_write_store);
static DEVICE_ATTR(register_read, 0664, NULL, sx9325_1_register_read_store);
static DEVICE_ATTR(raw_data, 0664, sx9325_1_raw_data_show, NULL);
static DEVICE_ATTR(chipinfo, 0664, chipinfo_value_show, NULL);
static DEVICE_ATTR(diff, 0444, diff_value_show, NULL);

static struct attribute *sx9325_1_attributes[] = {
	&dev_attr_register_write.attr,
	&dev_attr_register_read.attr,
	&dev_attr_raw_data.attr,
	&dev_attr_chipinfo.attr,
	NULL,
};
static struct attribute_group sx9325_1_attr_group = {
	.attrs = sx9325_1_attributes,
};
#endif
static void hw_init(psx93XX_t chip)
{
	int i = 0;
	unsigned char tempval;
	int num = ARRAY_SIZE(sx9325_1_i2c_reg_setup);

	/* configure device */
	dev_dbg(chip->pdev, "Going to Setup I2C Registers! chip->board_id (%d)\n", chip->board_id);
	if (chip) {
		while (i < num) {
			if (chip->board_id == MSM_BOARD_ID_T3) {
				write_register(chip, sx9325_1_i2c_reg_temp[i].reg, sx9325_1_i2c_reg_temp[i].val);
				read_register(chip, sx9325_1_i2c_reg_temp[i].reg, &tempval);
				i++;
			} /*else if (chip->board_id >= MSM_BOARD_ID_POAD_T3) {
				write_register(chip, sx9325_1_i2c_reg_sleep[i].reg, sx9325_1_i2c_reg_sleep[i].val);
				read_register(chip, sx9325_1_i2c_reg_sleep[i].reg, &tempval);
				i++;
			} */else {
				write_register(chip, sx9325_1_i2c_reg_setup[i].reg, sx9325_1_i2c_reg_setup[i].val);
				read_register(chip, sx9325_1_i2c_reg_setup[i].reg, &tempval);
				i++;
			}
		}
	} else {
	dev_err(chip->pdev, "ERROR!\n");
	/*Force to touched if error */
	ForcetoTouched(chip);
	dev_info(chip->pdev, "Hardware_init-ForcetoTouched()\n");
	}
}

static int initialize(psx93XX_t chip)
{
	if (chip) {
	/* prepare reset by disabling any irq handling */
	chip->irq_disabled = 1;
	disable_irq(chip->irq);
	/* perform a reset */
	/*write_register(chip,SX9325_1_SOFTRESET_REG,SX9325_1_SOFTRESET);*/
	/* wait until the reset has finished by monitoring NIRQ */
	dev_dbg(chip->pdev, "Sent Software Reset. Waiting until device is back from reset to continue.\n");
	/* just sleep for awhile instead of using a loop with reading irq status */
	/*msleep(300); */
	msleep(20);
	hw_init(chip);
	/*msleep(100); // make sure everything is running */
	msleep(50); /* make sure everything is running */
	manual_offset_calibration(chip);

	/* re-enable interrupt handling */
	enable_irq(chip->irq);
	chip->irq_disabled = 0;

	/* make sure no interrupts are pending since enabling irq will only
	 * work on next falling edge */
	read_regStat(chip);
	return 0;
	}
	return -ENOMEM;
}

static void sx9325_1_touchProcess(psx93XX_t chip)
{
	int counter = 0;
	u8 touchStatus = 0;
	int numberOfButtons = 0;
	struct _buttonInfo *buttons = NULL;
	struct input_dev *input = NULL;

	struct _buttonInfo *pCurrentButton	= NULL;

	if (chip) {
	dev_dbg(chip->pdev, "Inside touchProcess()\n");
	read_register(chip, SX9325_STAT0_REG, &touchStatus);

	buttons = chip->buttons;
	input = chip->pinput;
	numberOfButtons = chip->buttonSize;

	if (unlikely((buttons == NULL) || (input == NULL))) {
		dev_err(chip->pdev, "ERROR!! buttons or input NULL!!!\n");
		return;
	}

	for (counter = 0; counter < numberOfButtons; counter++) {
		pCurrentButton = &buttons[counter];
		if (pCurrentButton == NULL) {
		dev_err(chip->pdev, "ERROR!! current button at index: %d NULL!!!\n", counter);
		return; /* ERRORR!!!! */
		}
	switch (pCurrentButton->state) {
	case IDLE: /* Button is not being touched! */
			if (((touchStatus & pCurrentButton->mask) == pCurrentButton->mask)) {
			/* User pressed button */
				dev_info(chip->pdev, "stone IDLE cap button %d touched\n", counter);
				if (sx9235_1_disable_sar_in_call == 0) {
					input_report_key(input, pCurrentButton->keycode, 1);
				}
				sx9325_1_need_input_index = counter;
				dev_dbg(chip->pdev, "stone sx9325_1_index = %d.\n", sx9325_1_need_input_index);

				pCurrentButton->state = ACTIVE;

			} else {
				dev_dbg(chip->pdev, "Button %d already released.\n", counter);
			}

			break;
	case ACTIVE: /* Button is being touched! */
			if (((touchStatus & pCurrentButton->mask) != pCurrentButton->mask)) {
			/* User released button */
			dev_info(chip->pdev, "stone  ACTIVE cap button %d released\n", counter);
				if (sx9235_1_disable_sar_in_call == 0) {
					input_report_key(input, pCurrentButton->keycode, 0);
				}
				pCurrentButton->state = IDLE;
				sx9325_1_need_input_index = -1;
				dev_dbg(chip->pdev, "stone  set sx9325_1_need_input_index -1.\n");
			} else {
				dev_dbg(chip->pdev, "stone  Button %d still touched.\n", counter);
			}
			break;
	default: /* Shouldn't be here, device only allowed ACTIVE or IDLE */
			break;
		};
	}
	input_sync(input);

		dev_dbg(chip->pdev, "Leaving touchProcess()\n");
	}
}

static void sx93XX_schedule_work(psx93XX_t chip, unsigned long delay)
{
	unsigned long flags;

	if (chip) {
	 dev_dbg(chip->pdev, "sx93XX_schedule_work()\n");
	 spin_lock_irqsave(&chip->lock, flags);
	 /* Stop any pending penup queues */
	 cancel_delayed_work(&chip->dworker);
	 schedule_delayed_work(&chip->dworker, delay);
	 spin_unlock_irqrestore(&chip->lock, flags);
	} else
	pr_err("sx93XX_schedule_work, NULL psx93XX_t\n");
}

static irqreturn_t sx93XX_irq(int irq, void *pvoid)
{
	psx93XX_t chip = 0;

	if (pvoid) {
	chip = (psx93XX_t)pvoid;
		dev_dbg(chip->pdev, "sx93XX_irq\n");
		sx93XX_schedule_work(chip, 0);
	} else
	pr_err("sx93XX_irq, NULL pvoid\n");
	return IRQ_HANDLED;
}

static void sx93XX_worker_func(struct work_struct *work)
{
	psx93XX_t chip = 0;
	int status = 0;

	unsigned char debug_reg[] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x7f};
	int debug_index = 0;
	unsigned char debug_reg_val;
	int debug_size = 0;

	if (work) {
		chip = container_of(work, sx93XX_t, dworker.work);

		if (!chip) {
			pr_err("%s, NULL psx9500_chip_t\n", __func__);
			return;
		}

		status = read_regStat(chip);

		if (sx9235_1_g_enable_log)	{
			debug_size = sizeof(debug_reg);
			for (debug_index = 0; debug_index < debug_size; debug_index++) {
				read_register(chip, debug_reg[debug_index], &debug_reg_val);
				pr_err("%s, sx9502 reg 0x%x value = 0x%x\n",
				__func__, debug_reg[debug_index], debug_reg_val);
			}
		}

		if (BITGET(status, SX9325_TOUCH_BIT) || BITGET(status, SX9325_REASE_BIT))
		sx9325_1_touchProcess(chip);
	} else {
		/*pr_err("sx93XX_worker_func, NULL work_struct\n"); */
	}
}

int sx93XX_1_init(psx93XX_t chip)
{
	int err = 0;

	if (chip) {
		if (chip->sx9325_1_vdd)
		err = regulator_enable(chip->sx9325_1_vdd);

		if (chip->sx9325_1_svdd)
		err = regulator_enable(chip->sx9325_1_svdd);

		if (chip->pinctrl) {
			err = pinctrl_select_state(chip->pinctrl, chip->sx9325_1_sleep);
			if (err) {
				pr_err("select sx9500_sleep failed with %d\n", err);
				return err;
			}

			msleep(20);

			err = pinctrl_select_state(chip->pinctrl, chip->sx9325_1_active);
			if (err) {
				pr_err("select sx9500_active failed with %d\n", err);
				return err;
			}
			msleep(20);
		}

	/* initialize spin lock */
	spin_lock_init(&chip->lock);

	/* initialize worker function */
	INIT_DELAYED_WORK(&chip->dworker, sx93XX_worker_func);

	/* initailize interrupt reporting */
	chip->irq_disabled = 0;
	err = request_irq(chip->irq, sx93XX_irq, IRQF_TRIGGER_FALLING,
			chip->pdev->driver->name, chip);
		if (err) {
			dev_err(chip->pdev, "irq %d busy?\n", chip->irq);
			return err;
	}
	dev_info(chip->pdev, "registered with irq (%d)\n", chip->irq);
	/* call init function pointer (chip should initialize all registers */
	err = initialize(chip);
	dev_err(chip->pdev, "No init function!!!!\n");
	}
	return -ENOMEM;
}
#ifdef BOARD_USES_SAR_CHIP
static int sx9325_1_fb_callback(struct notifier_block *nfb,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)	{
			if (g_sx9325_1_chip)
				manual_offset_calibration(g_sx9325_1_chip);
		}
	}

	return 0;
}

static struct notifier_block __refdata sx9325_1_fb_notifier = {
	.notifier_call = sx9325_1_fb_callback,
};
#endif
static const struct file_operations  proc_ops_sx9235_1_disable_sar_in_call = {
	.owner = THIS_MODULE,
	.read = sx9235_1_disable_sar_in_call_show,
	.write = sx9235_1_disable_sar_in_call_store,
};
#ifdef BOARD_USES_SAR_CHIP
/*@zte board id, begin*/
int sx9325_1_load_board_id(void)
{
	int board_id = 0;

	board_id = request_board_id();

	pr_info("%s:board_id %d\n", __func__, board_id);

	return board_id;
}
/*@zte board id, end*/
#endif
static int sx9325_1_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
#ifndef BOARD_USES_SAR_CHIP
	pr_err("%s, NODEV return E\n", __func__);
	return -ENODEV;
#else
	int i = 0;
	int ret = 0;
	int err = 0;
	psx93XX_t chip = 0;
	int board_id = 0;
#ifdef CONFIG_SAR_CHIPID9325_1
	u8 whoami = 0;
#endif
	/* psx9325_1_t pDevice = 0;*/
	/* psx9325_1_platform_data_t pplatData = 0; */
	struct input_dev *input = NULL;
	struct proc_dir_entry *dir;
	struct proc_dir_entry *refresh;

	pr_err("%s, E\n", __func__);

	board_id = sx9325_1_load_board_id();

	if (board_id > MSM_BOARD_ID_T3 && board_id < MSM_BOARD_ID_POAD_T3) {
		pr_err("%s not support board id\n", __func__);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_WORD_DATA)) {
	SX9325_1_DBG("%s i2c not support read word data\n", __func__);
	return -EIO;
	}

	chip = kzalloc(sizeof(sx93XX_t), GFP_KERNEL); /* create memory for main struct */
	dev_dbg(&client->dev, "\t Initialized Main Memory: 0x%p\n", chip);

	if (chip) {
	chip->bus = client;
	chip->pdev = &client->dev;
	/*sx9325_1_parse_dt(chip);*/
	chip->buttons = psmtcButtons;
	chip->buttonSize = ARRAY_SIZE(psmtcButtons);

	chip->irq = client->irq;
	chip->board_id = board_id;

	i2c_set_clientdata(client, chip);

#ifdef CONFIG_SAR_CHIPID9325_1
	read_register(chip, SX9325_1_WHOAMI_REG, &whoami);
	if (whoami != 0x22) {
		kfree(chip);
		return -ENOMEM;
	}
#endif

	device_create_file(chip->pdev, &dev_attr_calibrate);
	device_create_file(chip->pdev, &dev_attr_regval);
	device_create_file(chip->pdev, &dev_attr_reg);
	device_create_file(chip->pdev, &dev_attr_status);
	device_create_file(chip->pdev, &dev_attr_sx9325_1_enable_log);
	device_create_file(chip->pdev, &dev_attr_sx9325_1_enable);/*added by chenhui for enable/disable sar irq*/
	/*device_create_file(chip->pdev, &dev_attr_sx9235_1_disable_sar_in_call);*/

	device_create_file(chip->pdev, &dev_attr_diff);

	sx9325_1_dev_zte = chip->pdev;
	dir = proc_mkdir("sar", NULL);
	refresh = proc_create("sx9235_1_disable_sar_in_call", 0664, dir, &proc_ops_sx9235_1_disable_sar_in_call);
	if (refresh == NULL)
		pr_err("proc_create sx9235_1_disable_sar_in_call failed!\n");

	err = sysfs_create_group(&client->dev.kobj, &sx9325_1_attr_group);
	if (err) {
		kfree(chip);
		return -ENOMEM;
	}
	/* Create the input device */
	input = input_allocate_device();
	if (!input) {
		kfree(chip);
		return -ENOMEM;
	}

	/* Set all the keycodes */
	__set_bit(EV_KEY, input->evbit);
	for (i = 0; i < chip->buttonSize; i++) {
		__set_bit(chip->buttons[i].keycode, input->keybit);
		chip->buttons[i].state = IDLE;
	}
	/* save the input pointer and finish initialization */
	chip->pinput = input;
	input->name = "SX9325_1 Cap Touch";
	input->id.bustype = BUS_I2C;
	if (input_register_device(input)) {
		kfree(chip);
		return -ENOMEM;
	}

	chip->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(chip->pinctrl)) {
		pr_err("%s, error devm_pinctrl_get(), chip->pinctrl 0x%x\n", __func__, (unsigned int)chip->pinctrl);
		goto probe_error;
	} else {
		chip->sx9325_1_active = pinctrl_lookup_state(chip->pinctrl, "sx9325_1_active");
		if (IS_ERR_OR_NULL(chip->sx9325_1_active))	{
		pr_err("%s, error pinctrl_lookup_state() for SX9325_1_ACTIVE\n", __func__);
		goto probe_error;
		}
	chip->sx9325_1_sleep = pinctrl_lookup_state(chip->pinctrl,	"sx9325_1_sleep");
	if (IS_ERR_OR_NULL(chip->sx9325_1_sleep))	{
		pr_err("%s, error pinctrl_lookup_state() for SX9325_1_SLEEP\n", __func__);
		goto probe_error;
		}
	}

	chip->sx9325_1_vdd = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR(chip->sx9325_1_vdd)) {
		pr_err("unable to get sx9325_1 vdd\n");
		ret = PTR_ERR(chip->sx9325_1_vdd);
		chip->sx9325_1_vdd = NULL;
		goto probe_error;
	}
	chip->sx9325_1_svdd = devm_regulator_get(&client->dev, "svdd");
	if (IS_ERR(chip->sx9325_1_vdd)) {
		pr_err("unable to get sx9325_1 svdd\n");
		ret = PTR_ERR(chip->sx9325_1_svdd);
		chip->sx9325_1_svdd = NULL;
		goto probe_error;
	}

	g_sx9325_1_chip = chip;
	fb_register_client(&sx9325_1_fb_notifier);

	sx93XX_1_init(chip);
	return	0;
	}

probe_error:
	kfree(chip);
	i2c_set_clientdata(client, NULL);
	return -EPERM;
#endif
}

static int sx9325_1_remove(struct i2c_client *client)
{
	psx93XX_t chip = i2c_get_clientdata(client);

	if (chip) {
	input_unregister_device(chip->pinput);
	device_remove_file(chip->pdev, &dev_attr_calibrate);
	device_remove_file(chip->pdev, &dev_attr_regval);
	device_remove_file(chip->pdev, &dev_attr_reg);
	device_remove_file(chip->pdev, &dev_attr_status);
	device_remove_file(chip->pdev, &dev_attr_sx9325_1_enable_log);
	device_remove_file(chip->pdev, &dev_attr_sx9325_1_enable);/*added by chenhui for enable/disable sar irq*/
	/*device_remove_file(chip->pdev, &dev_attr_sx9235_1_disable_sar_in_call);*/
#ifdef BOARD_USES_SAR_CHIP
	device_remove_file(chip->pdev, &dev_attr_diff);
#endif
	}

	if (chip) {
	cancel_delayed_work_sync(&chip->dworker); /* Cancel the Worker Func */
	/*destroy_workqueue(chip->workq); */
	free_irq(chip->irq, chip);
	kfree(chip);
	return 0;
	}
	return -ENOMEM;

}

#if 0
static int sx9325_1_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	psx93XX_t chip = i2c_get_clientdata(client);
	/*struct input_dev *input = NULL;

	input = chip->pinput;*/
	SX9325_1_DBG("%s enter\n", __func__);

	if (chip) {
	/*SX9325_1_DBG("%s report far when diabling sar\n", __func__);
	input_report_key(input, KEY_F20, 0);
	input_sync(input);
	disable_irq(chip->irq);
	chip->irq_disabled = 1;*/
	}
return 0;
}

static int sx9325_1_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	psx93XX_t chip = i2c_get_clientdata(client);

	SX9325_1_DBG("%s enter\n", __func__);
	return 0;
}

static const struct dev_pm_ops sx9325_1_pm_ops = {
	.suspend	= sx9325_1_suspend,
	/*.suspend_noirq	= sx9500_suspend_noirq,*/
	.resume		= sx9325_1_resume,
};
#endif

static struct of_device_id sx9325_1_match_table[] = {
	{ .compatible = "zte, sx9325_1-input", },
	{ },
};

static const struct i2c_device_id sx9325_1_input_id[] = {
	{"sx9325_1-input", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, sx9325_1_driver_id);

static struct i2c_driver sx9325_1_input_driver = {
	.driver		= {
		.name		= "sx9325_1-input",
		.owner		= THIS_MODULE,
		.of_match_table	= sx9325_1_match_table,
		/*.pm		= &sx9325_1_pm_ops,*/
	},
	.probe		= sx9325_1_probe,
	.remove		= sx9325_1_remove,
	.id_table	= sx9325_1_input_id,
};


module_i2c_driver(sx9325_1_input_driver);

MODULE_DESCRIPTION("sx9325_1 input");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:sx9325_1-input");


