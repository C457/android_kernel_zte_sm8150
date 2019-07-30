#ifndef __AW9523B_H_
#define __AW9523B_H_

struct aw9523b_platform_data {
	int shdn_gpio;
	int irq_gpio;
};

typedef enum{
	LED_SMART_FADE = 0,
	LED_BLINK,
	GPIO_MODE_OUTPUT,
	GPIO_MODE_INTPUT,
} aw9106b_mode;

typedef enum {
	AW9523_PIN_P0_0 = 0,
	AW9523_PIN_P0_1,
	AW9523_PIN_P0_2,
	AW9523_PIN_P0_3,
	AW9523_PIN_P0_4,
	AW9523_PIN_P0_5,
	AW9523_PIN_P0_6,
	AW9523_PIN_P0_7,
	AW9523_PIN_P1_0,
	AW9523_PIN_P1_1,
	AW9523_PIN_P1_2,
	AW9523_PIN_P1_3,
	AW9523_PIN_P1_4,
	AW9523_PIN_P1_5,
	AW9523_PIN_P1_6,
	AW9523_PIN_P1_7,
	AW9523_PIN_MAX,
} aw9523b_pin;

typedef enum {
	GROUP_P0 = 0,
	GROUP_P1 = 1,
} aw9523b_group;

typedef enum {
	ISEL_37MA = 0,
	ISEL_27MA = 1,
	ISEL_18MA = 2,
	ISEL_9MA = 3,
} aw9523b_isel_level;

typedef enum {
	AW9106B_LEVEL_OFF = 0,
	AW9106B_LEVEL_1 = 0x10,
	AW9106B_LEVEL_2 = 0x20,
	AW9106B_LEVEL_3 = 0x30,
	AW9106B_LEVEL_4 = 0x60,
	AW9106B_LEVEL_5 = 0x80,
	AW9106B_LEVEL_MAX = 0xFF,
} aw9106b_brightness_level;

struct aw9106b_config_data {
	unsigned char mode;
	unsigned char level;
	int blink;
	unsigned long delay_on;
	unsigned long delay_off;
};

struct aw9106b_led_data {
	unsigned int id;/* which aw9106b chip used */
	unsigned int pin;/* which pin of aw9106b used */
	unsigned int control_mode;/* 0: led mode; 1: gpio mode */
	unsigned int active;/* 0: low active; 1: high active */
	unsigned int level;
	struct aw9106b_config_data config;
};

#define AW9106B_LED_DATA(i, p, m, a, l) \
	{.id = i, .pin = p, .control_mode = m, .active = a, .level = l}
enum {
	LED_MODE = 0,
	GPIO_MODE = 1,
};
enum {
	LOW_ACTIVE = 0,
	HIGH_ACTIVE = 1,
};
enum {
	LEVEL_OFF = 0,
	LEVEL_1 = 1,
	LEVEL_2 = 2,
	LEVEL_3 = 3,
	LEVEL_4 = 4,
	LEVEL_5 = 5,
	LEVEL_MAX = 255,
};

#define GROUP_A_NUM			2
#define GROUP_B_NUM			4

/* REGS */
#define	GPIO_INPUT_P0		0x00
#define	GPIO_INPUT_P1		0x01
#define	GPIO_OUTPUT_P0		0x02
#define	GPIO_OUTPUT_P1		0x03
#define	GPIO_CFG_P0			0x04
#define	GPIO_CFG_P1			0x05
#define	GPIO_INTN_P0			0x06
#define	GPIO_INTN_P1			0x07
#define   OVERALL_CTL			0x11
#define	GPMD_P0				0x12
#define	GPMD_P1				0x13
#define	DIM0				0x20
#define	DIM1				0x21
#define	DIM2				0x22
#define	DIM3				0x23
#define	DIM4				0x24
#define	DIM5				0x25
#define AW9523_MAX_DIM_REG				0x2F
#define AW9523_SW_RESET_REG				0x7F

/* CTL REG BITS */
#define OBERALL_ISEL_VAL					7
#define GPOMD				4
#define ISEL				0

/* GPMD_A REG BITS */
#define GPMD_A1				1
#define GPMD_A0				0

/* GPMD_B REG BITS */
#define GPMD_B3				3
#define GPMD_B2				2
#define GPMD_B1				1
#define GPMD_B0				0

/* EN_BRE REG BITS */
#define EN_BRE5				5
#define EN_BRE4				4
#define EN_BRE3				3
#define EN_BRE2				2
#define EN_BRE1				1
#define EN_BRE0				0

/* FADE_TMR REG BITS */
#define FDOFF_TMR			3
#define FDON_TMR			0

/* FADE_TMR REG VALUE */
#define FD_TMR_0MS			0
#define FD_TMR_256MS		1
#define FD_TMR_512MS		2
#define FD_TMR_1024MS		3
#define FD_TMR_2048MS		4
#define FD_TMR_4096MS		5

/* FULL_TMR REG BITS */
#define FLOFF_TMR			3
#define FLON_TMR			0

/* FULL_TMR REG VALUE */
#define FL_TMR_0MS			0
#define FL_TMR_256MS		1
#define FL_TMR_512MS		2
#define FL_TMR_1024MS		3
#define FL_TMR_2048MS		4
#define FL_TMR_4096MS		5
#define FL_TMR_8192MS		6
#define FL_TMR_16384MS		7

/* GPIO_INPUT_A REG BITS */
#define GPIO_INPUT_A1		1
#define GPIO_INPUT_A0		0

/* GPIO_INPUT_B REG BITS */
#define GPIO_INPUT_B3		3
#define GPIO_INPUT_B2		2
#define GPIO_INPUT_B1		1
#define GPIO_INPUT_B0		0

/* GPIO_OUTPUT_A REG BITS */
#define GPIO_OUTPUT_A1		1
#define GPIO_OUTPUT_A0		0

/* GPIO_OUTPUT_B REG BITS */
#define GPIO_OUTPUT_B3		3
#define GPIO_OUTPUT_B2		2
#define GPIO_OUTPUT_B1		1
#define GPIO_OUTPUT_B0		0

/* GPIO_CFG_A REG BITS */
#define GPIO_CFG_A1			1
#define GPIO_CFG_A0			0

/* GPIO_CFG_B REG BITS */
#define GPIO_CFG_B3			3
#define GPIO_CFG_B2			2
#define GPIO_CFG_B1			1
#define GPIO_CFG_B0			0

/* GPIO_INTN_A REG BITS */
#define GPIO_INTN_A1		1
#define GPIO_INTN_A0		0

/* GPIO_INTN_B REG BITS */
#define GPIO_INTN_B3		3
#define GPIO_INTN_B2		2
#define GPIO_INTN_B1		1
#define GPIO_INTN_B0		0

#define ISEL_MASK			0x3
#define FDON_TMR_MASK		0x7
#define FDOFF_TMR_MASK		0x7
#define FLON_TMR_MASK		0x7
#define FLOFF_TMR_MASK		0x7

extern int aw9523b_config(int id, aw9523b_pin pin, struct aw9106b_config_data *config);
#endif
