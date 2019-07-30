/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */
#ifndef SX9325_H
#define SX9325_H

/*
 *  I2C Registers
 */
#define SX9325_IRQSTAT_REG    0x00
#define SX9325_TOUCH_BIT 0x6
#define SX9325_REASE_BIT 0x5
#define SX9325_STAT0_REG    0x01
#define SX9325_STAT1_REG    0x02
#define SX9325_IRQ_ENABLE_REG 0x05
#define SX9325_IRQFUNC_REG 0x07

#define SX9325_CPS_CTRL0_REG    0x10
#define SX9325_CPS_CTRL1_REG    0x11
#define SX9325_CPS_CTRL2_REG    0x12
#define SX9325_CPS_CTRL3_REG    0x13
#define SX9325_CPS_CTRL4_REG    0x14
#define SX9325_CPS_CTRL5_REG    0x15
#define SX9325_CPS_CTRL6_REG    0x16
#define SX9325_CPS_CTRL7_REG    0x17
#define SX9325_CPS_CTRL8_REG    0x18
#define SX9325_CPS_CTRL9_REG   0x19
#define SX9325_CPS_CTRL10_REG   0x1A
#define SX9325_CPS_CTRL11_REG   0x1B
#define SX9325_CPS_CTRL12_REG   0x1C
#define SX9325_CPS_CTRL13_REG   0x1D
#define SX9325_CPS_CTRL14_REG   0x1E
#define SX9325_CPS_CTRL15_REG   0x1F
#define SX9325_CPS_CTRL16_REG   0x20
#define SX9325_CPS_CTRL17_REG   0x21
#define SX9325_CPS_CTRL18_REG   0x22
#define SX9325_CPS_CTRL19_REG   0x23
#define SX9325_SAR_CTRL0_REG   0x2A
#define SX9325_SAR_CTRL1_REG   0x2B
#define SX9325_SAR_CTRL2_REG   0x2C

#define SX9325_SOFTRESET_REG  0x9F

/*
 *  I2C Registers
 */
 /*-Interrupt and status*/
#define SX9325_IRQSTAT_REG	0x00
#define SX9325_STAT0_REG	0x01
#define SX9325_STAT1_REG	0x02
#define SX9325_STAT2_REG	0x03
#define SX9325_STAT3_REG	0x04
#define SX9325_IRQ_ENABLE_REG	0x05
#define SX9325_IRQCFG0_REG	0x06
#define SX9325_IRQCFG1_REG	0x07
#define SX9325_IRQCFG2_REG	0x08
/*-General control*/
#define SX9325_CTRL0_REG	0x10
#define SX9325_CTRL1_REG	0x11
#define SX9325_I2CADDR_REG	0x14
#define SX9325_CLKSPRD		0x15
/*-AFE Control*/
#define SX9325_AFE_CTRL0_REG    0x20
#define SX9325_AFE_CTRL1_REG    0x21
#define SX9325_AFE_CTRL2_REG    0x22
#define SX9325_AFE_CTRL3_REG    0x23
#define SX9325_AFE_CTRL4_REG	0x24
#define SX9325_AFE_CTRL5_REG	0x25
#define SX9325_AFE_CTRL6_REG	0x26
#define SX9325_AFE_CTRL7_REG	0x27
#define SX9325_AFE_PH0_REG		0x28
#define SX9325_AFE_PH1_REG		0x29
#define SX9325_AFE_PH2_REG		0x2A
#define SX9325_AFE_PH3_REG		0x2B
#define SX9325_AFE_CTRL8		0x2C
#define SX9325_AFE_CTRL9		0x2D
/*-Main Digital Processing (Prox) control*/
#define SX9325_PROX_CTRL0_REG	0x30
#define SX9325_PROX_CTRL1_REG	0x31
#define SX9325_PROX_CTRL2_REG	0x32
#define SX9325_PROX_CTRL3_REG	0x33
#define SX9325_PROX_CTRL4_REG	0x34
#define SX9325_PROX_CTRL5_REG	0x35
#define SX9325_PROX_CTRL6_REG	0x36
#define SX9325_PROX_CTRL7_REG	0x37
/*-Advanced Digital Processing control*/
#define SX9325_ADV_CTRL0_REG	0x40
#define SX9325_ADV_CTRL1_REG	0x41
#define SX9325_ADV_CTRL2_REG	0x42
#define SX9325_ADV_CTRL3_REG	0x43
#define SX9325_ADV_CTRL4_REG	0x44
#define SX9325_ADV_CTRL5_REG	0x45
#define SX9325_ADV_CTRL6_REG	0x46
#define SX9325_ADV_CTRL7_REG	0x47
#define SX9325_ADV_CTRL8_REG	0x48
#define SX9325_ADV_CTRL9_REG	0x49
#define SX9325_ADV_CTRL10_REG	0x4A
#define SX9325_ADV_CTRL11_REG	0x4B
#define SX9325_ADV_CTRL12_REG	0x4C
#define SX9325_ADV_CTRL13_REG	0x4D
#define SX9325_ADV_CTRL14_REG	0x4E
#define SX9325_ADV_CTRL15_REG	0x4F
#define SX9325_ADV_CTRL16_REG	0x50
#define SX9325_ADV_CTRL17_REG	0x51
#define SX9325_ADV_CTRL18_REG	0x52
#define SX9325_ADV_CTRL19_REG	0x53
#define SX9325_ADV_CTRL20_REG	0x54
/*      Sensor Readback */
#define SX9325_CPSRD	0x60
#define SX9325_USEMSB	0x61
#define SX9325_USELSB	0x62
#define SX9325_AVGMSB	0x63
#define SX9325_AVGLSB	0x64
#define SX9325_DIFFMSB	0x65
#define SX9325_DIFFLSB	0x66
#define SX9325_OFFSETMSB	0x67
#define SX9325_OFFSETLSB	0x68
#define SX9325_SARMSB	0x69
#define SX9325_SARLSB		0x6A

#define SX9325_SOFTRESET_REG	0x9F
#define SX9325_WHOAMI_REG	0xFA
#define SX9325_REV_REG	0xFB

/*      IrqStat 0:Inactive 1:Active     */
#define SX9325_IRQSTAT_RESET_FLAG      0x80
#define SX9325_IRQSTAT_TOUCH_FLAG      0x40
#define SX9325_IRQSTAT_RELEASE_FLAG    0x20
#define SX9325_IRQSTAT_COMPDONE_FLAG   0x10
#define SX9325_IRQSTAT_CONV_FLAG       0x08
#define SX9325_IRQSTAT_CLOSEALL_FLAG   0x04
#define SX9325_IRQSTAT_FARALL_FLAG     0x02
#define SX9325_IRQSTAT_SMARTSAR_FLAG   0x01


/* CpsStat  */
#define SX9325_TCHCMPSTAT_TCHCOMB_FLAG    0x08
#define SX9325_TCHCMPSTAT_TCHSTAT2_FLAG   0x04
#define SX9325_TCHCMPSTAT_TCHSTAT1_FLAG   0x02
#define SX9325_TCHCMPSTAT_TCHSTAT0_FLAG   0x01


/*      SoftReset */
#define SX9325_SOFTRESET  0xDE

#define BITGET(byte, bit) (byte & (1 << bit))

struct smtc_reg_data {
unsigned char reg;
unsigned char val;
};

struct _buttonInfo {
  /*! The Key to send to the input */
int keycode;
  /*! Mask to look for on Touch Status */
int mask;
  /*! Current state of button. */
int state;
};

#endif
