#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/pinctrl/consumer.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/alarmtimer.h>
#include <linux/proc_fs.h>
#include "idt_p922x_wls_power.h"

static int p922x_debug_mask = 0xff;
static unsigned char *idtp9220_rx_fod_5v = NULL;
static unsigned char *idtp9220_rx_fod_9v = NULL;
static unsigned char *idtp9220_rx_fod_12v = NULL;

#define STR1(R)  #R
#define STR2(R)  STR1(R)

#define p922x_err(chip, fmt, ...)		\
	pr_err("%s: %s: " fmt, chip->name,	\
		__func__, ##__VA_ARGS__)	\

#define p922x_dbg(chip, reason, fmt, ...)			\
	do {							\
		if (p922x_debug_mask & (reason))		\
			pr_err("%s: %s: " fmt, chip->name,	\
				__func__, ##__VA_ARGS__);	\
		else						\
			pr_err("%s: %s: " fmt, chip->name,	\
				__func__, ##__VA_ARGS__);	\
	} while (0)


struct p922x_dev *_chip;

struct idtp9220_access_func {
	int (*read)(struct p922x_dev *di, u16 reg, u8 *val);
	int (*write)(struct p922x_dev *di, u16 reg, u8 val);
	int (*read_buf)(struct p922x_dev *di,
					u16 reg, u8 *buf, u32 size);
	int (*write_buf)(struct p922x_dev *di,
					 u16 reg, u8 *buf, u32 size);
};

#define VENDOR_NAME_SIZE 12
struct p922x_dev {
	char				*name;
	struct i2c_client	 *client;
	struct device		*dev;
	struct regmap		*regmap;
	struct idtp9220_access_func bus;
	struct mutex		irq_complete;
	struct mutex		write_lock;
	struct mutex		send_pkg_lock;
	bool				resume_completed;
	bool				irq_waiting;
	int int_pin;
	int power_good_pin;
	bool power_good;
	bool status_good;
	bool id_auth_success;
	bool device_auth_success;
	bool device_auth_recheck_done;
	bool fod_5v_enabled;
	bool fod_9v_enabled;
	struct pinctrl		 *p922x_gpio_pinctrl;
	struct pinctrl_state *p922x_gpio_state;
	struct pinctrl		 *p922x_en_pin_pinctrl;
	struct pinctrl_state *p922x_en_pin_active;
	struct pinctrl_state *p922x_en_pin_suspend;
	struct dentry		 *debug_root;
	struct delayed_work  fod_voltage_check_work;
	struct delayed_work  device_key_auth_work;
	struct delayed_work  get_tx_fw_work;
	struct delayed_work  get_tx_adapter_work;
	struct delayed_work  e_trans_show_work;
	struct delayed_work  fop_check_work;
	struct power_supply *idtp922x_psy;
	struct notifier_block	nb;
	int tx_fw_version;
	int idt_adapter_type;
	int idt_tx_iin;
	int idt_tx_vin;
	int idt_rx_iout;
	int idt_rx_vout;
	int idt_tx_freq;
	int idt_fast_charging_voltage;
	int idt_package_send_success;
	int idt_tx_data_recive;
	int trans_efficiency;
	int out_voltage;
	int per_voltage;
	int out_voltage_max;
	int out_current_max;
	int signal_strength_good_threshold;
	u8 signal_strength;
	int board_id;
	char coil_vendor_name[VENDOR_NAME_SIZE];
};

static int p922x_irq_clr(struct p922x_dev *chip);
static int p922x_irq_disable(struct p922x_dev *chip);
static int p922x_irq_enable(struct p922x_dev *chip);
static int idtp922x_set_enable(struct p922x_dev *chip, bool enable);

int idtp9220_read(struct p922x_dev *chip, u16 reg, u8 *val)
{
	unsigned int temp = 0;
	int rc = 0;

	mutex_lock(&chip->write_lock);

	rc = regmap_read(chip->regmap, reg, &temp);
	if (rc >= 0)
		*val = (u8)temp;
	else
		dev_err(chip->dev, "idtp9220 read error: %d\n", rc);
	mutex_unlock(&chip->write_lock);

	return rc;
}

int idtp9220_write(struct p922x_dev *chip, u16 reg, u8 val)
{
	int rc = 0;

	mutex_lock(&chip->write_lock);
	rc = regmap_write(chip->regmap, reg, val);
	if (rc < 0)
		dev_err(chip->dev, "idtp9220 write error: %d\n", rc);
	mutex_unlock(&chip->write_lock);

	return rc;
}

static int idtp9220_masked_write(struct p922x_dev *chip, u16 addr, u8 mask, u8 val)
{
	int rc = 0;

	mutex_lock(&chip->write_lock);
	rc = regmap_update_bits(chip->regmap, addr, mask, val);
	mutex_unlock(&chip->write_lock);
	return rc;
}

int idtp9220_read_buffer(struct p922x_dev *chip, u16 reg, u8 *buf, u32 size)
{
	int ret;

	mutex_lock(&chip->write_lock);
	ret = regmap_bulk_read(chip->regmap, reg, buf, size);
	mutex_unlock(&chip->write_lock);
	return ret;
}

int idtp9220_write_buffer(struct p922x_dev *chip, u16 reg, u8 *buf, u32 size)
{
	int rc = 0;

	while (size--) {
		rc = chip->bus.write(chip, reg++, *buf++);
		if (rc < 0) {
			dev_err(chip->dev, "write error: %d\n", rc);
			return rc;
		}
	}
	return rc;
}

int ExtractPacketSize(u8 hdr)
{
	if (hdr < 0x20)
		return 1;
	if (hdr < 0x80)
		return (2 + ((hdr - 0x20) >> 4));
	if (hdr < 0xe0)
		return (8 + ((hdr - 0x80) >> 3));
	return (20 + ((hdr - 0xe0) >> 2));
}

void clritr(struct p922x_dev *chip, u16 s)
{
	chip->bus.write_buf(chip, REG_INT_CLEAR, (u8 *)&s, 2);
	chip->bus.write(chip, REG_COMMAND, CLRINT);
}

int checkitr(struct p922x_dev *chip, u16 s)
{
	u8 buf[2];
	u16 itr;
	int i = 0;

	for (i = 0; i < 10; i++) {
		chip->bus.read_buf(chip, REG_INTR, buf, 2);
		itr = buf[0]|(buf[1]<<8);
		if (itr & s) {
			p922x_dbg(chip, PR_DEBUG, "return true\n");
			return true;
		}
		p922x_dbg(chip, PR_DEBUG, "not true, retry.\n");
		msleep(200);
	}
	return false;
}

#define TIMEOUT_COUNT 100
bool sendPkt(struct p922x_dev *chip, ProPkt_Type *pkt)
{
	int length = ExtractPacketSize(pkt->header)+1;
	int i = 0;
	bool ret = true;

	mutex_lock(&chip->send_pkg_lock);
	p922x_dbg(chip, PR_DEBUG, "length:%d, keep send_pkg_lock\n", length);
	/* write data into proprietary packet buffer*/
	chip->bus.write_buf(chip, REG_PROPPKT_ADDR, (u8 *)pkt, length);
	/* send proprietary packet*/
	chip->bus.write(chip, REG_COMMAND, SENDPROPP);
	chip->idt_tx_data_recive = DATA_RCV_SEND;

	for (i = 0; i < TIMEOUT_COUNT; i++) {
		if (chip->idt_tx_data_recive != DATA_RCV_SEND) {
			break;
		}
		msleep(20);
	}
	if (i == TIMEOUT_COUNT) {
		p922x_irq_clr(chip);
		p922x_dbg(chip, PR_DEBUG, "no int recived, try again\n");
		chip->bus.write_buf(chip, REG_PROPPKT_ADDR, (u8 *)pkt, length);
		chip->bus.write(chip, REG_COMMAND, SENDPROPP);
		for (i = 0; i < TIMEOUT_COUNT; i++) {
			if (chip->idt_tx_data_recive != DATA_RCV_SEND) {
				break;
			}
			msleep(20);
		}
		if (i == TIMEOUT_COUNT) {
			chip->idt_tx_data_recive = DATA_RCV_NO_INT;
		}
	}
	p922x_dbg(chip, PR_DEBUG, "chip->idt_tx_data_recive:%d\n", chip->idt_tx_data_recive);
	if (chip->idt_tx_data_recive == DATA_RCV_WAIT_SUCCESS) {
		p922x_dbg(chip, PR_DEBUG, "send package successful\n");
		ret = true;
		goto out;
	} else {
		p922x_dbg(chip, PR_DEBUG, "send package failed\n");
		ret = false;
		goto out;
	}

out:
	p922x_dbg(chip, PR_DEBUG, "release send_pkg_lock\n");
	mutex_unlock(&chip->send_pkg_lock);
	return ret;
}

int receivePkt(struct p922x_dev *chip, u8 *data)
{
	u8 header;
	u8 length;

	if (checkitr(chip, TXDATARCVD)) {
		chip->bus.read(chip, REG_BCHEADER_ADDR, &header);
		length = ExtractPacketSize(header);
		chip->bus.read_buf(chip, REG_BCDATA_ADDR, data, length);
		clritr(chip, TXDATARCVD);
		return true;
	}
	return false;
}

int p922x_get_received_data(struct p922x_dev *chip, u8 *data)
{
	u8 header;
	u8 length;
	int rc = 0;

	rc = chip->bus.read(chip, REG_BCHEADER_ADDR, &header);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read %d rc = %d\n",
				REG_BCHEADER_ADDR, rc);
		return rc;
	}
	length = ExtractPacketSize(header);
	rc = chip->bus.read_buf(chip, REG_BCDATA_ADDR, data, length);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read %d rc = %d\n",
				REG_BCDATA_ADDR, rc);
	}
	return rc;
}

ssize_t p922x_get_firmware_ver(struct p922x_dev *chip)
{
	int i = 0;

	u8 id[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, ver[4] = {0xff, 0xff, 0xff, 0xff};

	chip->bus.read_buf(chip, REG_CHIP_ID, id, 8);
	chip->bus.read_buf(chip, REG_CHIP_REV, ver, 4);

	for (i = 0; i < 8 ; i++) {
		p922x_dbg(chip, PR_DEBUG, "id[%d]=0x%02x\n", i, id[i]);
	}
	p922x_dbg(chip, PR_DEBUG, "IDT ChipID:%04x\nFWVer:%02x.%02x.%02x.%02x\n",
	id[4]|(id[0]<<8), ver[3], ver[2], ver[1], ver[0]);

	return 0;
}

int p922x_get_system_mode(struct p922x_dev *chip)
{
	u8 mode;

	chip->bus.read(chip, REG_MODE_ADDR, &mode);
	p922x_dbg(chip, PR_DEBUG, "system_mode:0x%x\n", mode);
	return mode;
}

void p922x_get_rx_firmware_version(struct p922x_dev *chip)
{
	u8 ver_data[4] = { 0, };
	int mode = 0;

	chip->bus.read_buf(chip, REG_OTPFWVER_ADDR, ver_data, 4);
	p922x_dbg(chip, PR_DEBUG, "FWVer:%02x.%02x.%02x.%02x\n",
		ver_data[3], ver_data[2], ver_data[1], ver_data[0]);

	chip->bus.read_buf(chip, REG_CHIP_REV, ver_data, 4);
	mode = p922x_get_system_mode(chip);
	if (mode & EEPROMSUPPORT) {
		p922x_dbg(chip, PR_DEBUG, "EEPROM:%02x.%02x.%02x.%02x\n",
			ver_data[3], ver_data[2], ver_data[1], ver_data[0]);
	} else if (mode & RAMPROGRAM) {
		p922x_dbg(chip, PR_DEBUG, "SRAM:%02x.%02x.%02x.%02x\n",
			ver_data[3], ver_data[2], ver_data[1], ver_data[0]);
	} else {
		p922x_dbg(chip, PR_DEBUG, "APP:%02x.%02x.%02x.%02x\n",
			ver_data[3], ver_data[2], ver_data[1], ver_data[0]);
	}
}

int p922x_get_rx_frequency(struct p922x_dev *chip)
{
	u8 ver_data[2];

	chip->bus.read_buf(chip, REG_FREQ_ADDR, ver_data, 2);
	p922x_dbg(chip, PR_DEBUG, "rx_frequency:%d\n", ver_data[0] | ver_data[1]<<8);
	return (ver_data[0] | ver_data[1]<<8);
}

int p922x_get_rx_vrect(struct p922x_dev *chip)
{
	u8 ver_data[2];

	chip->bus.read_buf(chip, REG_VRECT_ADDR, ver_data, 2);
	p922x_dbg(chip, PR_DEBUG, "RX Vrect:%dmV\n", (ver_data[0] | ver_data[1] << 8) * 21 * 1000 / 4095);
	return ((ver_data[0] | ver_data[1] << 8) * 21 * 1000 / 4095);
}

int p922x_get_rx_iout(struct p922x_dev *chip)
{
	u8 ver_data[2];

	chip->bus.read_buf(chip, REG_RX_LOUT, ver_data, 2);
	p922x_dbg(chip, PR_DEBUG, "Iout:%dmA\n", (ver_data[0] | ver_data[1]<<8));
	return (ver_data[0] | ver_data[1]<<8);
}

int p922x_get_rx_vout(struct p922x_dev *chip)
{
	u8 ver_data[2];
	int vout = 0;

	chip->bus.read_buf(chip, REG_ADC_VOUT, ver_data, 2);
	vout = ((ver_data[0] | ver_data[1]<<8)*6*21*1000/40950);
	p922x_dbg(chip, PR_DEBUG, "Vout:%dmV\n", vout);
	return vout;
}

void p922x_set_rx_vout(struct p922x_dev *chip, ushort vol)
{
	int val = 0;

	val = vol * 10 - 35;
	chip->bus.write(chip, REG_VOUT_SET, val);
}

int p922x_get_rx_ilimit(struct p922x_dev *chip)
{
	u8 data;
	int ilimit = 0;

	chip->bus.read(chip, REG_ILIM_SET, &data);
	ilimit = (data + 1) / 10;
	p922x_dbg(chip, PR_DEBUG, "ilimt:%d\n", ilimit);
	return ilimit;
}

int p922x_get_rx_fop(struct p922x_dev *chip)
{
	u8 data[2] = {0,};
	int fop = 0;

	chip->bus.read_buf(chip, REG_FREQ_ADDR, data, sizeof(data));

	fop = (data[1] << 8) | data[0];

	if (!fop)
		return 0;

	fop = 64 * 6000 / fop + 1;

	return fop;
}


void p922x_get_tx_atapter_type(struct p922x_dev *chip)
{
	ProPkt_Type proPkt;

	proPkt.header = PROPRIETARY18;
	proPkt.cmd = BC_ADAPTER_TYPE;
	sendPkt(chip, &proPkt);
}

#define V5P 5000
#define V9P 9000
#define V12P 12000
#define WLS_5V 5000000
#define WLS_9V 9000000
#define WLS_12V 12000000
#define WLS_DEF_VOL WLS_5V
#define WLS_0P25A 250000
#define WLS_0P50A 500000
#define WLS_0P75A 750000
#define WLS_1A	   1000000
#define WLS_1P1A   1100000
#define WLS_1P25A 1000000
#define WLS_2P0A 2000000

#define WLS_DEF_CUR WLS_1A

int p922x_get_rx_output_voltage_max(struct p922x_dev *chip)
{
	int wls_voltage = WLS_DEF_VOL;

	if (chip->idt_adapter_type <= ADAPTER_DCP) {
		wls_voltage = WLS_5V;
	} else if (chip->idt_adapter_type == ADAPTER_QC20) {
		wls_voltage = WLS_9V;
	} else if (chip->idt_adapter_type == ADAPTER_QC30
		|| chip->idt_adapter_type == ADAPTER_PD) {
		if (chip->tx_fw_version == TX_FW_VERSION_A10) {
			wls_voltage = WLS_12V;
		} else {
			wls_voltage = WLS_9V;
		}
	}

	p922x_dbg(chip, PR_DEBUG, "wls_voltage = %d, adapter_type:%d, tx_fw_version: %d\n",
		wls_voltage, chip->idt_adapter_type, chip->tx_fw_version);
	chip->out_voltage_max = wls_voltage;

	return wls_voltage;
}

int p922x_get_rx_output_current_max(struct p922x_dev *chip)
{
	int wls_current = WLS_DEF_CUR;

	if (chip->idt_adapter_type == ADAPTER_UNKNOWN)
		wls_current = WLS_DEF_CUR;
	else if (chip->idt_adapter_type == ADAPTER_SDP)
		wls_current = WLS_0P25A;
	else if (chip->idt_adapter_type == ADAPTER_CDP
		|| chip->idt_adapter_type == ADAPTER_DCP)
		wls_current = WLS_1A;
	else if (chip->idt_adapter_type == ADAPTER_QC20)
		wls_current = WLS_1P1A;
	else if (chip->idt_adapter_type == ADAPTER_QC30
		|| chip->idt_adapter_type == ADAPTER_PD)
		wls_current = WLS_1P25A;

	p922x_dbg(chip, PR_DEBUG, "wls_current = %d, adapter_type:%d\n",
		wls_current, chip->idt_adapter_type);
	chip->out_current_max = wls_current;

	return wls_current;
}

#define FOD_REGSTERS_NUM 12
#define TRY_MAX 5
static int p922x_fod_parameters_check(struct p922x_dev *chip, ushort mv)
{
	int rc = 0;
	int i = 0;
	u8 val[FOD_REGSTERS_NUM] = {0, };

	chip->bus.read_buf(chip, REG_FOD_START_ADDR, val, FOD_REGSTERS_NUM);
	for (i = 0; i < FOD_REGSTERS_NUM; i++) {
		if (mv == V5P) {
			if (val[i] != idtp9220_rx_fod_5v[i]) {
				p922x_dbg(chip, PR_INTERRUPT, "fod 5v parameters is wrong\n");
				rc = -EINVAL;
				break;
			}
		} else if (mv == V9P) {
			if (val[i] != idtp9220_rx_fod_9v[i]) {
				p922x_dbg(chip, PR_INTERRUPT, "fod 9v parameters is wrong\n");
				rc = -EINVAL;
				break;
			}
		} else if (mv == V12P) {
			if (val[i] != idtp9220_rx_fod_12v[i]) {
				p922x_dbg(chip, PR_INTERRUPT, "fod 12v parameters is wrong\n");
				rc = -EINVAL;
				break;
			}
		}
	}

	return rc;
}

static int p922x_update_fod(struct p922x_dev *chip, ushort mv)
{
	int rc = 0;
	int tries = 0;

	for (tries = 0; tries < TRY_MAX; tries++) {
		if (mv == V5P) {
			chip->bus.write_buf(chip, REG_FOD_START_ADDR,
				(u8 *)idtp9220_rx_fod_5v, FOD_REGSTERS_NUM);
		} else if (mv == V9P) {
			chip->bus.write_buf(chip, REG_FOD_START_ADDR,
				(u8 *)idtp9220_rx_fod_9v, FOD_REGSTERS_NUM);
		} else if (mv == V12P) {
			chip->bus.write_buf(chip, REG_FOD_START_ADDR,
				(u8 *)idtp9220_rx_fod_12v, FOD_REGSTERS_NUM);
		}
		rc = p922x_fod_parameters_check(chip, mv);
		if (!rc)
			break;
	}
	if (tries == TRY_MAX) {
		p922x_dbg(chip, PR_DEBUG, "fod update failed\n");
	}

	return rc;
}

static int p922x_set_pmic_dcin_current(int current_ua)
{
	struct power_supply *psy = NULL;
	union power_supply_propval val = {0, };
	int rc = 0;

	psy = power_supply_get_by_name("dc");
	if (psy == NULL) {
		pr_err("Get %s psy failed!!\n", "dc");
		return 0;
	}

	val.intval = current_ua;

	rc = power_supply_set_property(psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	if (rc < 0) {
		pr_err("Failed to set %s property:%d rc=%d\n",
					"dc", POWER_SUPPLY_PROP_CURRENT_MAX, rc);
	}

	power_supply_put(psy);

	return 0;

}


static int p922x_switch_voltage_handler(int switch_begin)
{
	if (switch_begin)
		p922x_set_pmic_dcin_current(WLS_0P25A);
	else
		p922x_set_pmic_dcin_current(WLS_2P0A);

	return 0;
}


void p922x_set_fast_charging_voltage(struct p922x_dev *chip, ushort mv)
{
	ushort val = 0;
	int vout_now = p922x_get_rx_vout(chip);

	if (mv <= 0)
		return;

	p922x_dbg(chip, PR_DEBUG, "vout_now:%dmV, new vol:%dmV\n", vout_now, mv);
	if (vout_now > 8000 && vout_now < 10000 && mv == V9P)
		return;
	if (vout_now > 11000 && mv == V12P)
		return;

	if ((chip->per_voltage != 0) && (mv != chip->per_voltage)) {
		p922x_dbg(chip, PR_DEBUG, "%dmV --->>> %dmV\n", chip->per_voltage, mv);
		p922x_switch_voltage_handler(true);
		msleep(300);
	}

	chip->per_voltage = mv;

	if (vout_now < 11000 && mv == V12P) {
		/* check wls power vout, if below 11V, set to 12V again, otherwise need not change */
		val = V9P;
		chip->bus.write_buf(chip, REG_FC_VOLTAGE, (u8 *)&val, 2);
		chip->bus.write(chip, REG_COMMAND, VSWITCH);
		msleep(3000);
		chip->bus.write_buf(chip, REG_FC_VOLTAGE, (u8 *)&mv, 2);
		chip->bus.write(chip, REG_COMMAND, VSWITCH);
		/* set 12V twice */
		msleep(20);
		chip->bus.write_buf(chip, REG_FC_VOLTAGE, (u8 *)&mv, 2);
		chip->bus.write(chip, REG_COMMAND, VSWITCH);
	} else if (vout_now > 11000 && mv == V5P) {
		val = V9P;
		chip->bus.write_buf(chip, REG_FC_VOLTAGE, (u8 *)&val, 2);
		chip->bus.write(chip, REG_COMMAND, VSWITCH);
		msleep(3000);
		chip->bus.write_buf(chip, REG_FC_VOLTAGE, (u8 *)&mv, 2);
		chip->bus.write(chip, REG_COMMAND, VSWITCH);

	} else {
		chip->bus.write_buf(chip, REG_FC_VOLTAGE, (u8 *)&mv, 2);
		chip->bus.write(chip, REG_COMMAND, VSWITCH);
	}

	msleep(300);

	p922x_update_fod(chip, mv);

	p922x_switch_voltage_handler(false);
}

void p922x_toggle_ldo(struct p922x_dev *chip)
{
	chip->bus.write(chip, REG_COMMAND, LDOTGL);
}

void p922x_get_tx_firmware_version(struct p922x_dev *chip)
{
	ProPkt_Type proPkt;

	proPkt.header = PROPRIETARY18;
	proPkt.cmd = BC_READ_FW_VER;
	sendPkt(chip, &proPkt);
}

int p922x_get_tx_iin(struct p922x_dev *chip)
{
	ProPkt_Type proPkt;

	proPkt.header = PROPRIETARY18;
	proPkt.cmd = BC_READ_IIN;
	sendPkt(chip, &proPkt);
	return chip->idt_tx_iin;
}

int p922x_get_tx_vin(struct p922x_dev *chip)
{
	ProPkt_Type proPkt;

	proPkt.header = PROPRIETARY18;
	proPkt.cmd = BC_READ_VIN;
	sendPkt(chip, &proPkt);
	return chip->idt_tx_vin;
}

void p922x_set_tx_vin(struct p922x_dev *chip, int mv)
{
	ProPkt_Type proPkt;

	proPkt.header  = PROPRIETARY38;
	proPkt.cmd = BC_SET_VIN;
	proPkt.data[0] = mv & 0xff;
	proPkt.data[1] = (mv >> 8) & 0xff;
	sendPkt(chip, &proPkt);
	chip->idt_tx_vin = mv;
	p922x_dbg(chip, PR_DEBUG, "set vin:%d\n", mv);
}

void p922x_set_tx_frequency(struct p922x_dev *chip, int freq)
{
	ProPkt_Type proPkt;

	proPkt.header  = PROPRIETARY38;
	proPkt.cmd = BC_SET_FREQ;
	proPkt.data[0] = freq & 0xff;
	proPkt.data[1] = (freq >> 8) & 0xff;
	sendPkt(chip, &proPkt);
	chip->idt_tx_freq = freq;
	p922x_dbg(chip, PR_DEBUG, "set freq:%d\n", freq);
}

int p922x_get_tx_frequency(struct p922x_dev *chip)
{
	ProPkt_Type proPkt;

	proPkt.header  = PROPRIETARY18;
	proPkt.cmd = BC_GET_FREQ;
	sendPkt(chip, &proPkt);
	return chip->idt_tx_freq;
}

void p922x_toggle_loopmode(struct p922x_dev *chip)
{
	ProPkt_Type proPkt;

	proPkt.header  = PROPRIETARY18;
	proPkt.cmd = BC_TOGGLE_LOOPMODE;
	sendPkt(chip, &proPkt);
}

void p922x_system_reset(struct p922x_dev *chip)
{
	ProPkt_Type proPkt;

	proPkt.header  = PROPRIETARY18;
	proPkt.cmd = BC_RESET;
	sendPkt(chip, &proPkt);

	/* Reset RX*/
	chip->bus.write(chip, 0x3000, 0x5a);
	chip->bus.write(chip, 0x3040, 0x10);
	msleep(2);

	chip->bus.write(chip, 0x3000, 0x5a);
	chip->bus.write(chip, 0x3048, 0x00);
	chip->bus.write(chip, 0x3040, 0x80);

	p922x_dbg(chip, PR_DEBUG, "reset system\n");
}

static int program_bootloader(struct p922x_dev *chip)
{
	int i, rc = 0;
	int len = 0;

	len = sizeof(bootloader);

	for (i = 0; i < len; i++) {
		rc = chip->bus.write(chip, 0x1c00+i, bootloader[i]);
		if (rc)
			return rc;
	}

	return 0;
}

int program_fw(struct p922x_dev *chip, u16 destAddr, u8 *src, u32 size)
{
	int i = 0, j = 0;
	u8 data = 0;

	/*=== Step-1 ===
	 Transfer 9220 boot loader code "OTPBootloader" to 9220 SRAM
	 - Setup 9220 registers before transferring the boot loader code
	 - Transfer the boot loader code to 9220 SRAM
	 - Reset 9220 => 9220 M0 runs the boot loader
	*/
	chip->bus.read(chip, 0x5870, &data);
	p922x_dbg(chip, PR_DEBUG, "0x5870 :%02x\n", data);
	chip->bus.read(chip, 0x5874, &data);
	p922x_dbg(chip, PR_DEBUG, "0x5874 :%02x\n", data);
	/*configure the system*/
	if (chip->bus.write(chip, 0x3000, 0x5a))
		return false;		 /*write key*/
	if (chip->bus.write(chip, 0x3040, 0x10))
		return false;		 /* halt M0 execution*/
	if (program_bootloader(chip))
		return false;
	if (chip->bus.write(chip, 0x3048, 0x80))
		return false;		 /* map RAM to OTP*/

	/* ignoreNAK */
	chip->bus.write(chip, 0x3040, 0x80);		/* reset chip and run the bootloader*/
	mdelay(100);

	/* === Step-2 ===
	 Program OTP image data to 9220 OTP memory
	*/
	for (i = destAddr; i < destAddr+size; i += 128) {		 /* program pages of 128 bytes*/
		/* Build a packet*/
		char sBuf[136]; /* 136=8+128 --- 8-byte header plus 128-byte data*/
		u16 StartAddr = (u16)i;
		u16 CheckSum = StartAddr;
		u16 CodeLength = 128;
		int retry_cnt = 0;

		memset(sBuf, 0, 136);

		/*(1) Copy the 128 bytes of the OTP image data to the packet data buffer
		  Array.Copy(srcData, i + srcOffs, sBuf, 8, 128);// Copy 128 bytes from srcData (starting at i+srcOffs)
		Copy 128 bytes from srcData (starting at i+srcOffs)*/
		memcpy(sBuf+8, src, 128);
		src += 128;
		/*(2) Calculate the packet checksum of the 128-byte data, StartAddr, and CodeLength*/
		/* find the 1st non zero value byte from the end of the sBuf[] buffer*/
		for (j = 127; j >= 0; j--) {
			if (sBuf[j + 8] != 0)
				break;
			CodeLength--;
		}
		if (CodeLength == 0)
			continue;			 /* skip programming if nothing to program*/

		for (; j >= 0; j--)
			CheckSum += sBuf[j + 8];	/* add the nonzero values*/
		CheckSum += CodeLength; /* finish calculation of the check sum*/

		/*(3) Fill up StartAddr, CodeLength, CheckSum of the current packet.*/
		memcpy(sBuf+2, &StartAddr, 2);
		memcpy(sBuf+4, &CodeLength, 2);
		memcpy(sBuf+6, &CheckSum, 2);

		/* Send the current packet to 9220 SRAM via I2C*/
		/* read status is guaranteed to be != 1 at this point*/
		for (j = 0; j < CodeLength+8; j++) {
			if (chip->bus.write(chip, 0x400+j, sBuf[j])) {
				p922x_dbg(chip, PR_DEBUG, "ERROR: on writing to OTP buffer");
				return false;
			}
		}

		/*Write 1 to the Status in the SRAM. This informs the 9220 to start programming the new packet
		 from SRAM to OTP memory*/
		if (chip->bus.write(chip, 0x400, 1))	{
			p922x_dbg(chip, PR_DEBUG, "ERROR: on OTP buffer validation");
			return false;
		}

		/*
		 Wait for 9220 bootloader to complete programming the current packet image data from SRAM to the OTP.
		 The boot loader will update the Status in the SRAM as follows:
			 Status:
			 "0" - reset value (from AP)
			 "1" - buffer validated / busy (from AP)
			 "2" - finish "OK" (from the boot loader)
			 "4" - programming error (from the boot loader)
			 "8" - wrong check sum (from the boot loader)
			 "16"- programming not possible (try to write "0" to bit location already programmed to "1")
				 (from the boot loader)*/

		/*		  DateTime startT = DateTime.Now;*/
		do {
			mdelay(100);
			chip->bus.read(chip, 0x400, sBuf);
			if (sBuf[0] == 1) {
				p922x_dbg(chip, PR_DEBUG, "Programming OTP buffer status sBuf:%02x i:%d\n",
					sBuf[0], i);
			}
			if (retry_cnt++ > 5)
				break;
		} while (sBuf[0] == 1); /*check if OTP programming finishes "OK"*/

		if (sBuf[0] != 2) { /* not OK*/
			p922x_dbg(chip, PR_DEBUG, "ERROR: buffer write to OTP returned status:%d :%s\n",
				sBuf[0], "X4");
			return false;
		}
		p922x_dbg(chip, PR_DEBUG, "Program OTP 0x%04x\n", i);
	}

	/* === Step-3 ===
	 Restore system (Need to reset or power cycle 9220 to run the OTP code)
	*/
	if (chip->bus.write(chip, 0x3000, 0x5a))
		return false;/* write key*/
	if (chip->bus.write(chip, 0x3048, 0x00))
		return false;/* remove code remapping*/
	return true;
}

static int p922x_remove(struct i2c_client *client)
{
	return 0;
}

/* first step: define regmap_config*/
static const struct regmap_config p922x_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
};

/*debug interface start*/
static int addr = -1;
static int p922x_register_set_addr(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}

	p922x_dbg(_chip, PR_DEBUG, "p922x_reg_addr:0x%x\n", addr);
	return 0;
}

static int p922x_register_get_addr(char *val, const struct kernel_param *kp)
{
	p922x_dbg(_chip, PR_DEBUG, "p922x_reg_addr:0x%x\n", addr);

	return snprintf(val, sizeof(int), "%x", addr);
}

static struct kernel_param_ops p922x_register_addr_ops = {
	.set = p922x_register_set_addr,
	.get = p922x_register_get_addr,
};
module_param_cb(addr, &p922x_register_addr_ops, &addr, 0644);

static int data = -1;
static int p922x_register_set_val(const char *val, const struct kernel_param *kp)
{
	int rc = 0;
	u8 value;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}
	value = data;
	_chip->bus.write(_chip, addr, value);

	p922x_dbg(_chip, PR_DEBUG, "p922x_reg_val:0x%x\n", data);
	return 0;
}

static int p922x_register_get_val(char *val, const struct kernel_param *kp)
{
	u8 value;

	_chip->bus.read(_chip, addr, &value);
	data = value;
	p922x_dbg(_chip, PR_DEBUG, "read addr:0x%x[0x%x]\n", addr, data);

	return snprintf(val, sizeof(int), "%x", data);
}

static struct kernel_param_ops p922x_register_data_ops = {
	.set = p922x_register_set_val,
	.get = p922x_register_get_val,
};
module_param_cb(data, &p922x_register_data_ops, &data, 0644);

static int count = 0;
static int p922x_register_set_count(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting count %d\n", rc);
		return rc;
	}
	if (count == 0x5a) {
		p922x_system_reset(_chip);
	}
	p922x_dbg(_chip, PR_DEBUG, "count:0x%x\n", count);
	return 0;
}

static int p922x_register_get_count(char *val, const struct kernel_param *kp)
{
	p922x_dbg(_chip, PR_DEBUG, "count:0x%x\n", count);
	p922x_get_rx_firmware_version(_chip);
	p922x_get_tx_firmware_version(_chip);

	return snprintf(val, sizeof(int), "%x", count);
}

static struct kernel_param_ops p922x_register_count_ops = {
	.set = p922x_register_set_count,
	.get = p922x_register_get_count,
};
module_param_cb(count, &p922x_register_count_ops, &count, 0644);

static int rx_fw_otp_write = 0;
static int p922x_rx_fw_otp_write(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error %d\n", rc);
		return rc;
	}
	if (rx_fw_otp_write) {
		p922x_irq_clr(_chip);
		p922x_irq_disable(_chip);
		cancel_delayed_work(&_chip->fod_voltage_check_work);
		cancel_delayed_work_sync(&_chip->device_key_auth_work);
		cancel_delayed_work_sync(&_chip->get_tx_fw_work);
		cancel_delayed_work_sync(&_chip->get_tx_adapter_work);
		cancel_delayed_work_sync(&_chip->e_trans_show_work);
		if (!program_fw(_chip, 0x0000, idtp9220_rx_fw_a129, sizeof(idtp9220_rx_fw_a129))) {
			p922x_dbg(_chip, PR_DEBUG, "download_wireless_charger_firmware failed.\n");
		} else {
			p922x_dbg(_chip, PR_DEBUG, "download_wireless_charger_firmware success.\n");
		}
		p922x_irq_enable(_chip);
	}

	rx_fw_otp_write = 0;

	return 0;
}

static struct kernel_param_ops p922x_rx_fw_otp_ops = {
	.set = p922x_rx_fw_otp_write,
	.get = param_get_int,
};
module_param_cb(rx_fw_otp_write, &p922x_rx_fw_otp_ops, &rx_fw_otp_write, 0644);

static int data_buf = -1;
static int p922x_register_set_buf(const char *val, const struct kernel_param *kp)
{
	int rc = 0;
	int value;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}
	value = data_buf;
	_chip->bus.write_buf(_chip, addr, (u8 *)&value, 2);

	p922x_dbg(_chip, PR_DEBUG, "p922x_reg_val:0x%x\n", data_buf);
	return 0;
}

static int p922x_register_get_buf(char *val, const struct kernel_param *kp)
{
	u8 value[2] = { 0. };
	int i = 0;

	if (count) {
		for (i = 0; i < count; i++) {
			addr = addr + i*2;
			_chip->bus.read_buf(_chip, addr, value, 2);
			data_buf = value[0] | (value[1]<<8);
			p922x_dbg(_chip, PR_DEBUG, "read addr:0x%x = 0x%x ,addr:0x%x = 0x%x\n",
				addr+1, value[1], addr, value[0]);
		}
	} else {
		_chip->bus.read_buf(_chip, addr, value, 2);
		data_buf = value[0] | (value[1]<<8);
		p922x_dbg(_chip, PR_DEBUG, "read addr:0x%x = 0x%x ,addr:0x%x = 0x%x\n",
			addr+1, value[1], addr, value[0]);
	}
	return snprintf(val, sizeof(int), "%x", data_buf);
}

static struct kernel_param_ops p922x_register_buf_ops = {
	.set = p922x_register_set_buf,
	.get = p922x_register_get_buf,
};
module_param_cb(data_buf, &p922x_register_buf_ops, &data_buf, 0644);

static int system_mode = -1;
static int p922x_get_system_mode_node(char *val, const struct kernel_param *kp)
{
	system_mode = p922x_get_system_mode(_chip);
	return snprintf(val, sizeof(int), "%d", system_mode);
}

static struct kernel_param_ops p922x_system_mode_node_ops = {
	.set = param_set_int,
	.get = p922x_get_system_mode_node,
};
module_param_cb(system_mode, &p922x_system_mode_node_ops, &system_mode, 0644);

static int adapter_type = ADAPTER_UNKNOWN;
static int p922x_get_tx_adapter_type_node(char *val, const struct kernel_param *kp)
{
	return snprintf(val, sizeof(int), "%d", adapter_type);
}

static struct kernel_param_ops p922x_tx_adapter_type_node_ops = {
	.set = param_set_int,
	.get = p922x_get_tx_adapter_type_node,
};
module_param_cb(adapter_type, &p922x_tx_adapter_type_node_ops, &adapter_type, 0644);

static int rx_vrect = -1;
static int p922x_get_rx_vrect_node(char *val, const struct kernel_param *kp)
{
	rx_vrect = p922x_get_rx_vrect(_chip);
	return snprintf(val, sizeof(int), "%d", rx_vrect);
}

static struct kernel_param_ops p922x_rx_vrect_node_ops = {
	.set = param_set_int,
	.get = p922x_get_rx_vrect_node,
};
module_param_cb(rx_vrect, &p922x_rx_vrect_node_ops, &rx_vrect, 0644);

static int rx_iout = -1;
static int p922x_get_rx_iout_node(char *val, const struct kernel_param *kp)
{
	rx_iout = p922x_get_rx_iout(_chip);
	return snprintf(val, sizeof(int), "%d", rx_iout);
}

static struct kernel_param_ops p922x_rx_iout_node_ops = {
	.set = param_set_int,
	.get = p922x_get_rx_iout_node,
};
module_param_cb(rx_iout, &p922x_rx_iout_node_ops, &rx_iout, 0644);

static int rx_vout = -1;
static int p922x_set_rx_vout_node(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}
	p922x_set_rx_vout(_chip, rx_vout);

	p922x_dbg(_chip, PR_DEBUG, "rx_vout:%d\n", rx_vout);
	return 0;
}

static int p922x_get_rx_vout_node(char *val, const struct kernel_param *kp)
{
	rx_vout = p922x_get_rx_vout(_chip);
	return snprintf(val, sizeof(int), "%d", rx_vout);
}

static struct kernel_param_ops p922x_rx_vout_node_ops = {
	.set = p922x_set_rx_vout_node,
	.get = p922x_get_rx_vout_node,
};
module_param_cb(rx_vout, &p922x_rx_vout_node_ops, &rx_vout, 0644);

static int rx_ilimit = -1;
static int p922x_get_rx_ilimit_node(char *val, const struct kernel_param *kp)
{
	rx_ilimit = p922x_get_rx_ilimit(_chip);
	return snprintf(val, sizeof(int), "%d", rx_ilimit);
}

static struct kernel_param_ops p922x_rx_ilimit_node_ops = {
	.set = param_set_int,
	.get = p922x_get_rx_ilimit_node,
};
module_param_cb(rx_ilimit, &p922x_rx_ilimit_node_ops, &rx_ilimit, 0644);

static int fast_charging_voltage_mv = -1;
static int p922x_set_fast_charging_voltage_node(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}
	p922x_set_fast_charging_voltage(_chip, fast_charging_voltage_mv);

	p922x_dbg(_chip, PR_DEBUG, "fast_charging_voltage:%d\n", fast_charging_voltage_mv);
	return 0;
}

static struct kernel_param_ops p922x_fast_charging_voltage_node_ops = {
	.set = p922x_set_fast_charging_voltage_node,
	.get = param_get_int,
};
module_param_cb(fast_charging_voltage_mv, &p922x_fast_charging_voltage_node_ops,
				&fast_charging_voltage_mv, 0644);

static int tx_iin = -1;
static int p922x_get_tx_iin_node(char *val, const struct kernel_param *kp)
{
	tx_iin = p922x_get_tx_iin(_chip);
	return snprintf(val, sizeof(int), "%d", tx_iin);
}

static struct kernel_param_ops p922x_tx_iin_node_ops = {
	.set = param_set_int,
	.get = p922x_get_tx_iin_node,
};
module_param_cb(tx_iin, &p922x_tx_iin_node_ops, &tx_iin, 0644);

static int tx_vin = -1;
static int p922x_set_tx_vin_node(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}
	p922x_set_tx_vin(_chip, tx_vin);

	p922x_dbg(_chip, PR_DEBUG, "tx_vin:%d\n", tx_vin);
	return 0;
}

static int p922x_get_tx_vin_node(char *val, const struct kernel_param *kp)
{
	tx_vin = p922x_get_tx_vin(_chip);
	return snprintf(val, sizeof(int), "%d", tx_vin);
}

static struct kernel_param_ops p922x_tx_vin_node_ops = {
	.set = p922x_set_tx_vin_node,
	.get = p922x_get_tx_vin_node,
};
module_param_cb(tx_vin, &p922x_tx_vin_node_ops, &tx_vin, 0644);

static int tx_freq = -1;
static int p922x_set_tx_frequency_node(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}
	p922x_set_tx_frequency(_chip, tx_freq);

	p922x_dbg(_chip, PR_DEBUG, "tx_freq:%d\n", tx_freq);
	return 0;
}

static int p922x_get_tx_frequency_node(char *val, const struct kernel_param *kp)
{
	tx_freq = p922x_get_tx_frequency(_chip);
	return snprintf(val, sizeof(int), "%d", tx_freq);
}

static struct kernel_param_ops p922x_tx_frequency_node_ops = {
	.set = p922x_set_tx_frequency_node,
	.get = p922x_get_tx_frequency_node,
};
module_param_cb(tx_freq, &p922x_tx_frequency_node_ops, &tx_freq, 0644);

static int rx_disable = 0;
static int p922x_set_rx_disable_node(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}
	if (rx_disable == 0)
		idtp922x_set_enable(_chip, true);
	else
		idtp922x_set_enable(_chip, false);

	p922x_dbg(_chip, PR_DEBUG, "rx_disable:%d\n", rx_disable);
	return 0;
}

static int p922x_get_rx_disable_node(char *val, const struct kernel_param *kp)
{
	return snprintf(val, sizeof(int), "%d", rx_disable);
}

static struct kernel_param_ops p922x_rx_disable_node_ops = {
	.set = p922x_set_rx_disable_node,
	.get = p922x_get_rx_disable_node,
};
module_param_cb(rx_disable, &p922x_rx_disable_node_ops, &rx_disable, 0644);

static bool use_fod_5v_dbg_enable = false;
static bool use_fod_9v_dbg_enable = false;
static unsigned int fod_5v_dbg = 0;
static unsigned int fod_9v_dbg = 0;
static unsigned char idtp9220_rx_fod_5v_dbg[12] = {0, };
static unsigned char idtp9220_rx_fod_9v_dbg[12] = {0, };
#define FOD_DBG_ARR_MAX 11
static int p922x_init_fod_dbg_node(const char *val, const struct kernel_param *kp)
{
	int rc = 0, i = 0;
	static int position_5v = 0;
	static int position_9v = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}

	p922x_dbg(_chip, PR_DEBUG, "p922x_set_fod_5v_dbg_node: %d\n", fod_5v_dbg);
	if (fod_5v_dbg == 0) {
		for (i = 0; i <= FOD_DBG_ARR_MAX; i++) {
			idtp9220_rx_fod_5v_dbg[i] = 0;
		}
		use_fod_5v_dbg_enable = false;
		position_5v = 0;
	} else {
		switch (position_5v) {
		case 0:
		case 3:
		case 6:
		case 9:
			for (i = position_5v + 2; i >= position_5v; i--) {
				idtp9220_rx_fod_5v_dbg[i] = fod_5v_dbg % 1000;
				fod_5v_dbg = fod_5v_dbg / 1000;
				if (i == 9) {
					use_fod_5v_dbg_enable = true;
					for (i = 0; i <= FOD_DBG_ARR_MAX; i++) {
						p922x_dbg(_chip, PR_DEBUG, "%d--0x%x\n",
							idtp9220_rx_fod_5v_dbg[i], idtp9220_rx_fod_5v_dbg[i]);
					}
					position_5v = 0;
					break;
				}
			}
			position_5v += 3;
			break;
		}
	}

	p922x_dbg(_chip, PR_DEBUG, "p922x_set_fod_9v_dbg_node: %d\n", fod_9v_dbg);
	if (fod_9v_dbg == 0) {
		for (i = 0; i <= FOD_DBG_ARR_MAX; i++) {
			idtp9220_rx_fod_9v_dbg[i] = 0;
		}
		use_fod_9v_dbg_enable = false;
		position_9v = 0;
	} else {
		switch (position_9v) {
		case 0:
		case 3:
		case 6:
		case 9:
			for (i = position_9v + 2; i >= position_9v; i--) {
				idtp9220_rx_fod_9v_dbg[i] = fod_9v_dbg % 1000;
				fod_9v_dbg = fod_9v_dbg / 1000;
				if (i == 9) {
					use_fod_9v_dbg_enable = true;
					for (i = 0; i <= FOD_DBG_ARR_MAX; i++) {
						p922x_dbg(_chip, PR_DEBUG, "%d--0x%x\n",
							idtp9220_rx_fod_9v_dbg[i], idtp9220_rx_fod_9v_dbg[i]);
					}
					position_9v = 0;
					break;
				}
			}
			position_9v += 3;
			break;
		}
	}

	return 0;
}

static struct kernel_param_ops p922x_init_5vfod_dbg_node_ops = {
	.set = p922x_init_fod_dbg_node,
	.get = param_get_int,
};
module_param_cb(fod_5v_dbg, &p922x_init_5vfod_dbg_node_ops, &fod_5v_dbg, 0644);

static struct kernel_param_ops p922x_init_9vfod_dbg_node_ops = {
	.set = p922x_init_fod_dbg_node,
	.get = param_get_int,
};
module_param_cb(fod_9v_dbg, &p922x_init_9vfod_dbg_node_ops, &fod_9v_dbg, 0644);

static int lab_debug_vol_mv = 0;
static int p922x_set_lab_debug_node(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}

	schedule_delayed_work(&_chip->fod_voltage_check_work, msecs_to_jiffies(0));

	p922x_dbg(_chip, PR_DEBUG, "lab_debug_vol_mv:%d\n", lab_debug_vol_mv);
	return 0;
}

static struct kernel_param_ops p922x_lab_debug_node_ops = {
	.set = p922x_set_lab_debug_node,
	.get = param_get_int,
};
module_param_cb(lab_debug_vol_mv, &p922x_lab_debug_node_ops, &lab_debug_vol_mv, 0644);

static int skip_auth = 0;
static int p922x_set_skip_auth_node(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_int(val, kp);
	if (rc) {
		p922x_dbg(_chip, PR_DEBUG, "error setting value %d\n", rc);
		return rc;
	}

	p922x_dbg(_chip, PR_DEBUG, "skip_auth:%d\n", skip_auth);

	return 0;
}

static struct kernel_param_ops p922x_skip_auth_node_ops = {
	.set = p922x_set_skip_auth_node,
	.get = param_get_int,
};
module_param_cb(skip_auth, &p922x_skip_auth_node_ops, &skip_auth, 0644);

#define WIRELESS_CHARGING_SIGNAL_GOOD_THRESHOLD_DEFAULT 115
extern int wireless_charging_signal_good;
static void p922x_get_signal_strength(struct p922x_dev *chip)
{
	if (chip->power_good) {
		chip->bus.read(chip, REG_SIGNAL_STRENGTH, &chip->signal_strength);
		if (chip->signal_strength >= chip->signal_strength_good_threshold) {
			wireless_charging_signal_good = 1;
		} else {
			wireless_charging_signal_good = 0;
		}
	} else {
		chip->signal_strength = 0;
		wireless_charging_signal_good = 0;
	}

	p922x_dbg(chip, PR_DEBUG, "signal_strength:%d, signal_good:%d\n",
		chip->signal_strength, wireless_charging_signal_good);
}

/*debug interface end*/

static int over_curr_handler(struct p922x_dev *chip, u8 rt_stat)
{
	p922x_dbg(chip, PR_DEBUG, "rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int over_volt_handler(struct p922x_dev *chip, u8 rt_stat)
{
	p922x_dbg(chip, PR_DEBUG, "rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int over_temp_handler(struct p922x_dev *chip, u8 rt_stat)
{
	p922x_dbg(chip, PR_DEBUG, "rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int rx_ready_handler(struct p922x_dev *chip, u8 rt_stat)
{
	p922x_dbg(chip, PR_DEBUG, "rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int tx_data_rcvd_handler(struct p922x_dev *chip, u8 rt_stat)
{
#if 0
	ProPkt_Type proPkt;

	chip->bus.read(chip, REG_BCCMD_ADDR, &proPkt.cmd);
	p922x_get_received_data(chip, proPkt.data);
	p922x_dbg(chip, PR_DEBUG, "proPkt.cmd:0x%x,data_list:0x%x,%x,%x,%x\n",
		proPkt.cmd, proPkt.data[0], proPkt.data[1], proPkt.data[2], proPkt.data[3]);
	switch (proPkt.cmd) {
	case BC_NONE:
		break;
	case BC_SET_FREQ:
		p922x_dbg(chip, PR_DEBUG, "set idt_tx_freq%d success\n", chip->idt_tx_freq);
		break;
	case BC_GET_FREQ:
		chip->idt_tx_freq = (proPkt.data[0] | proPkt.data[1]<<8);
		p922x_dbg(chip, PR_DEBUG, "get idt_tx_freq%d\n", chip->idt_tx_freq);
		break;
	case BC_READ_FW_VER:
		p922x_dbg(chip, PR_DEBUG, "TX Version is: %d.%d.%d.%d\n",
			proPkt.data[3], proPkt.data[2], proPkt.data[1], proPkt.data[0]);
		break;
	case BC_READ_IIN:
		chip->idt_tx_iin = (proPkt.data[0] | proPkt.data[1]<<8);
		p922x_dbg(chip, PR_DEBUG, "chip->idt_tx_iin%d\n", chip->idt_tx_iin);
		break;
	case BC_READ_VIN:
		chip->idt_tx_vin = (proPkt.data[0] | proPkt.data[1]<<8);
		p922x_dbg(chip, PR_DEBUG, "In voltage:%d\n", chip->idt_tx_vin);
		break;
	case BC_SET_VIN:
		p922x_dbg(chip, PR_DEBUG, "set in voltage:%d success\n", chip->idt_tx_vin);
		break;
	case BC_ADAPTER_TYPE:
		chip->idt_adapter_type = proPkt.data[0];
		p922x_dbg(chip, PR_DEBUG, "adapter type:%d\n", chip->idt_adapter_type);
		break;
	case BC_RESET:
		break;
	case BC_READ_I2C:
		break;
	case BC_WRITE_I2C:
		break;
	case BC_VI2C_INIT:
		break;
	case BC_TOGGLE_LOOPMODE:
		p922x_dbg(chip, PR_DEBUG, "LOOPMODE success\n");
		break;
	default:
		p922x_dbg(chip, PR_DEBUG, "error type\n");
	}
	chip->idt_tx_data_recive = DATA_RCV_WAIT_SUCCESS;
#endif
	return 0;
}

static int mode_changed_handler(struct p922x_dev *chip, u8 rt_stat)
{
	p922x_dbg(chip, PR_DEBUG, "rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int ldo_on_handler(struct p922x_dev *chip, u8 rt_stat)
{
	p922x_dbg(chip, PR_DEBUG, "rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int ldo_off_handler(struct p922x_dev *chip, u8 rt_stat)
{
	p922x_dbg(chip, PR_INTERRUPT, "rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int device_auth_failed_handler(struct p922x_dev *chip, u8 rt_stat)
{
	chip->power_good = gpio_get_value(chip->power_good_pin);
	p922x_dbg(chip, PR_INTERRUPT, "rt_stat = 0x%02x, device_auth_recheck_done = %d\n",
		rt_stat, chip->device_auth_recheck_done);

	if (skip_auth) {
		chip->device_auth_success = true;
		if (chip->power_good) {
			/* it is for debug, allow 12V */
			chip->tx_fw_version = TX_FW_VERSION_A10;
			schedule_delayed_work(&chip->get_tx_adapter_work, msecs_to_jiffies(0));
			return 0;
		}
	}

	if (chip->power_good && (!chip->device_auth_recheck_done)) {
		p922x_dbg(chip, PR_INTERRUPT, "rerun device_key_auth_work\n");
		cancel_delayed_work_sync(&chip->device_key_auth_work);
		schedule_delayed_work(&chip->device_key_auth_work, msecs_to_jiffies(500));
		chip->device_auth_recheck_done = true;
	} else if (chip->power_good && chip->device_auth_recheck_done) {
		schedule_delayed_work(&chip->get_tx_fw_work, msecs_to_jiffies(0));
	}

	return 0;
}

static int device_auth_success_handler(struct p922x_dev *chip, u8 rt_stat)
{
	chip->power_good = gpio_get_value(chip->power_good_pin);
	p922x_dbg(chip, PR_INTERRUPT, "rt_stat = 0x%02x\n", rt_stat);

	chip->device_auth_success = true;
	if (chip->power_good) {
		if (chip->tx_fw_version == TX_FW_VERSION_UNKNOWN)
			schedule_delayed_work(&chip->get_tx_fw_work, msecs_to_jiffies(0));
		schedule_delayed_work(&chip->get_tx_adapter_work, msecs_to_jiffies(1000));
	}

	return 0;
}

static int p922x_match_tx_without_key(struct p922x_dev *chip)
{
	/* if zte tx firmware version is 0xa.0x0.0x2.0xb, allow rx communicate with tx */
	if (chip->tx_fw_version == TX_FW_VERSION_A9) {
		p922x_dbg(chip, PR_INTERRUPT, "force call device_auth_success_handler\n");
		device_auth_success_handler(chip, 0);
	}

	return 0;
}

static int send_pkt_timeout_handler(struct p922x_dev *chip, u8 rt_stat)
{
	ProPkt_Type proPkt;

	p922x_dbg(chip, PR_INTERRUPT, "rt_stat = 0x%02x\n", rt_stat);

	chip->bus.read(chip, REG_BCCMD_ADDR, &proPkt.cmd);
	p922x_get_received_data(chip, proPkt.data);
	p922x_dbg(chip, PR_INTERRUPT, "proPkt.cmd:0x%x,data_list:0x%x,%x,%x,%x\n",
		proPkt.cmd, proPkt.data[0], proPkt.data[1], proPkt.data[2], proPkt.data[3]);
	switch (proPkt.cmd) {
	case BC_NONE:
		break;
	case BC_SET_FREQ:
		p922x_dbg(chip, PR_INTERRUPT, "set idt_tx_freq%d success\n", chip->idt_tx_freq);
		break;
	case BC_GET_FREQ:
		p922x_dbg(chip, PR_INTERRUPT, "get idt_tx_freq%d\n", chip->idt_tx_freq);
		break;
	case BC_READ_FW_VER:
		p922x_dbg(chip, PR_INTERRUPT, "TX firmware Version: 0x%x.0x%x.0x%x.0x%x\n",
			proPkt.data[3], proPkt.data[2], proPkt.data[1], proPkt.data[0]);
		if (proPkt.data[3] == 0xa && proPkt.data[2] == 0x0 && proPkt.data[1] == 0x2) {
			if (proPkt.data[0] == 0xb) {
				chip->tx_fw_version = TX_FW_VERSION_A9;
				if (!chip->device_auth_success)
					p922x_match_tx_without_key(chip);
			} else if (proPkt.data[0] == 0xd) {
				chip->tx_fw_version = TX_FW_VERSION_A10;
			} else {
				/* default as A9 to use 9V */
				chip->tx_fw_version = TX_FW_VERSION_A9;
			}
		}
		break;
	case BC_READ_IIN:
		p922x_dbg(chip, PR_INTERRUPT, "chip->idt_tx_iin%d\n", chip->idt_tx_iin);
		break;
	case BC_READ_VIN:
		p922x_dbg(chip, PR_INTERRUPT, "In voltage:%d\n", chip->idt_tx_vin);
		break;
	case BC_SET_VIN:
		p922x_dbg(chip, PR_INTERRUPT, "set in voltage:%d success\n", chip->idt_tx_vin);
		break;
	case BC_ADAPTER_TYPE:
		p922x_get_tx_atapter_type(chip);
		p922x_dbg(chip, PR_INTERRUPT, "send adapter type pkg failed, try again\n");
		break;
	case BC_RESET:
		break;
	case BC_READ_I2C:
		break;
	case BC_WRITE_I2C:
		break;
	case BC_VI2C_INIT:
		break;
	case BC_TOGGLE_LOOPMODE:
		p922x_dbg(chip, PR_INTERRUPT, "LOOPMODE success\n");
		break;
	default:
		p922x_dbg(chip, PR_INTERRUPT, "error type\n");
	}
	chip->idt_tx_data_recive = DATA_RCV_WAIT_TIMEOUT;
	return 0;
}

static int send_pkt_success_handler(struct p922x_dev *chip, u8 rt_stat)
{
	ProPkt_Type proPkt;

	chip->bus.read(chip, REG_BCCMD_ADDR, &proPkt.cmd);
	p922x_get_received_data(chip, proPkt.data);
	p922x_dbg(chip, PR_INTERRUPT, "proPkt.cmd:0x%x,data_list:0x%x,%x,%x,%x\n",
		proPkt.cmd, proPkt.data[0], proPkt.data[1], proPkt.data[2], proPkt.data[3]);
	switch (proPkt.cmd) {
	case BC_NONE:
		break;
	case BC_SET_FREQ:
		break;
	case BC_GET_FREQ:
		chip->idt_tx_freq = (proPkt.data[0] | proPkt.data[1]<<8);
		p922x_dbg(chip, PR_INTERRUPT, "get idt_tx_freq%d\n", chip->idt_tx_freq);
		break;
	case BC_READ_FW_VER:
		p922x_dbg(chip, PR_INTERRUPT, "TX firmware Version: 0x%x.0x%x.0x%x.0x%x\n",
			proPkt.data[3], proPkt.data[2], proPkt.data[1], proPkt.data[0]);
		if (proPkt.data[3] == 0xa && proPkt.data[2] == 0x0 && proPkt.data[1] == 0x2) {
			if (proPkt.data[0] == 0xb) {
				chip->tx_fw_version = TX_FW_VERSION_A9;
				if (!chip->device_auth_success)
					p922x_match_tx_without_key(chip);
			} else if (proPkt.data[0] == 0xd) {
				chip->tx_fw_version = TX_FW_VERSION_A10;
			} else {
				/* default as A9 to use 9V */
				chip->tx_fw_version = TX_FW_VERSION_A9;
			}
		}
		break;
	case BC_READ_IIN:
		chip->idt_tx_iin = (proPkt.data[0] | proPkt.data[1]<<8);
		p922x_dbg(chip, PR_INTERRUPT, "Iin:%dmA\n", chip->idt_tx_iin);
		break;
	case BC_READ_VIN:
		chip->idt_tx_vin = (proPkt.data[0] | proPkt.data[1]<<8);
		p922x_dbg(chip, PR_INTERRUPT, "Vin:%dmV\n", chip->idt_tx_vin);
		break;
	case BC_SET_VIN:
		break;
	case BC_ADAPTER_TYPE:
		chip->idt_adapter_type = proPkt.data[0];
		adapter_type = chip->idt_adapter_type;
		p922x_dbg(chip, PR_INTERRUPT, "adapter type:%d\n", chip->idt_adapter_type);
		break;
	case BC_RESET:
		break;
	case BC_READ_I2C:
		break;
	case BC_WRITE_I2C:
		break;
	case BC_VI2C_INIT:
		break;
	case BC_TOGGLE_LOOPMODE:
		break;
	default:
		p922x_dbg(chip, PR_INTERRUPT, "error type\n");
	}
	chip->idt_tx_data_recive = DATA_RCV_WAIT_SUCCESS;

	return 0;
}

static int id_auth_failed_handler(struct p922x_dev *chip, u8 rt_stat)
{
	p922x_dbg(chip, PR_INTERRUPT, "rt_stat = 0x%02x\n", rt_stat);
	if (skip_auth) {
		chip->id_auth_success = true;
		if (chip->power_good) {
			cancel_delayed_work_sync(&chip->device_key_auth_work);
			schedule_delayed_work(&chip->device_key_auth_work, msecs_to_jiffies(500));
		}
	}
	return 0;
}

static int id_auth_success_handler(struct p922x_dev *chip, u8 rt_stat)
{
	chip->power_good = gpio_get_value(chip->power_good_pin);
	p922x_dbg(chip, PR_INTERRUPT, "rt_stat = 0x%02x\n", rt_stat);
	p922x_dbg(chip, PR_INTERRUPT, "power %s, old id auth %s\n",
		chip->power_good ? "good" : "not good",
		chip->id_auth_success ? "success" : "fail");

	chip->id_auth_success = true;
	if (chip->power_good) {
		cancel_delayed_work_sync(&chip->device_key_auth_work);
		schedule_delayed_work(&chip->device_key_auth_work, msecs_to_jiffies(500));
	}
	p922x_dbg(chip, PR_INTERRUPT, "power %s, now id auth %s\n",
		chip->power_good ? "good" : "not good",
		chip->id_auth_success ? "success" : "fail");
	return 0;
}

static int sleep_mode_handler(struct p922x_dev *chip, u8 rt_stat)
{
	p922x_dbg(chip, PR_INTERRUPT, "rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int power_on_handler(struct p922x_dev *chip, u8 rt_stat)
{
	p922x_dbg(chip, PR_INTERRUPT, "rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

struct p922x_irq_info {
	const char		*name;
	int (*p922x_irq)(struct p922x_dev *chip,
							u8 rt_stat);
	int	high;
	int	low;
};

struct irq_handler_info {
	u8			stat_reg;
	u8			val;
	u8			prev_val;
	struct p922x_irq_info	irq_info[8];
};

static struct irq_handler_info handlers[] = {
	{REG_INTR, 0, 0,
		{
			{
				.name		= "over_curr",
				.p922x_irq	= over_curr_handler,
			},
			{
				.name		= "over_volt",
				.p922x_irq	= over_volt_handler,
			},
			{
				.name		= "over_temp",
				.p922x_irq	= over_temp_handler,
			},
			{
				.name		= "rx_ready",
				.p922x_irq	= rx_ready_handler,
			},
			{
				.name		= "tx_data_rcvd",
				.p922x_irq	= tx_data_rcvd_handler,
			},
			{
				.name		= "mode_changed",
				.p922x_irq	= mode_changed_handler,
			},
			{
				.name		= "ldo_on",
				.p922x_irq	= ldo_on_handler,
			},
			{
				.name		= "ldo_off",
				.p922x_irq	= ldo_off_handler,
			},
		},
	},
	{REG_INTR + 1, 0, 0,
		{
			{
				.name		= "device_auth_failed",
				.p922x_irq	= device_auth_failed_handler,
			},
			{
				.name		= "device_auth_success",
				.p922x_irq	= device_auth_success_handler,
			},
			{
				.name		= "send_pkt_timeout",
				.p922x_irq	= send_pkt_timeout_handler,
			},
			{
				.name		= "send_pkt_success",
				.p922x_irq	= send_pkt_success_handler,
			},
			{
				.name		= "id_auth_failed",
				.p922x_irq	= id_auth_failed_handler,
			},
			{
				.name		= "id_auth_success",
				.p922x_irq	= id_auth_success_handler,
			},
			{
				.name		= "sleep_mode",
				.p922x_irq	= sleep_mode_handler,
			},
			{
				.name		= "power_on",
				.p922x_irq	= power_on_handler,
			},
		},
	},
};

static int p922x_irq_disable(struct p922x_dev *chip)
{
	int rc = 0;
	int value;

	value = 0x0;
	rc = chip->bus.write_buf(chip, REG_INTR_EN, (u8 *)&value, 2);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't write %d rc = %d\n",
					REG_INT_CLEAR, rc);
	}
	return rc;
}

static int p922x_irq_enable(struct p922x_dev *chip)
{
	int rc = 0;
	int value = 0;

	value = 0xffff;
	rc = chip->bus.write_buf(chip, REG_INTR_EN, (u8 *)&value, 2);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't write %d rc = %d\n",
					REG_INT_CLEAR, rc);
	}
	return rc;
}

static int p922x_irq_clr(struct p922x_dev *chip)
{
	int rc = 0;
	int value;

	value = 0xffff;
	rc = chip->bus.write_buf(chip, REG_INT_CLEAR, (u8 *)&value, 2);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't write %d rc = %d\n",
					REG_INT_CLEAR, rc);
		goto clr_int_end;
	}
	rc = idtp9220_masked_write(chip, REG_COMMAND, CLRINT, CLRINT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't write %d rc = %d\n",
					REG_COMMAND, rc);
		goto clr_int_end;
	}

	mdelay(5);
clr_int_end:
	return rc;
}

static irqreturn_t p922x_power_good_handler(int irq, void *dev_id)
{
	struct p922x_dev *chip = dev_id;

	chip->power_good = gpio_get_value(chip->power_good_pin);
	p922x_dbg(chip, PR_INTERRUPT, "power %s\n",
		chip->power_good ? "good" : "not good");

	if (!chip->power_good) {
		cancel_delayed_work_sync(&chip->device_key_auth_work);
		cancel_delayed_work_sync(&chip->get_tx_fw_work);
		cancel_delayed_work_sync(&chip->get_tx_adapter_work);
		cancel_delayed_work_sync(&chip->e_trans_show_work);
		chip->id_auth_success = false;
		chip->device_auth_success = false;
		chip->device_auth_recheck_done = false;
		chip->fod_5v_enabled = false;
		chip->fod_9v_enabled = false;
		chip->idt_tx_vin = 0;
		chip->idt_tx_iin = 0;
		chip->idt_rx_vout = 0;
		chip->idt_rx_iout = 0;
		chip->trans_efficiency = 0;
		chip->signal_strength = 0;
		chip->out_voltage = 0;
		chip->per_voltage = 0;
		chip->out_voltage_max = WLS_DEF_VOL;
		chip->out_current_max = WLS_DEF_CUR;
		chip->idt_adapter_type = ADAPTER_UNKNOWN;
		chip->tx_fw_version = TX_FW_VERSION_UNKNOWN;
		chip->status_good = false;
		wireless_charging_signal_good = 0;
	} else {
		if (p922x_update_fod(chip, V5P)) {
			p922x_dbg(chip, PR_INTERRUPT, "set fod 5v fail.\n");
		}
		schedule_delayed_work(&chip->e_trans_show_work, msecs_to_jiffies(1000));
	}
	return IRQ_HANDLED;
}

#define IRQ_STATUS_MASK	0x01
static irqreturn_t p922x_stat_handler(int irq, void *dev_id)
{
	struct p922x_dev *chip = dev_id;
	int i = 0, j = 0;
	u8 changed = 0;
	u8 rt_stat = 0, prev_rt_stat = 0;
	int rc = 0;
	int handler_count = 0;

	p922x_dbg(chip, PR_INTERRUPT, "p922x_stat_handler start\n");
	if (rx_fw_otp_write) {
		return IRQ_HANDLED;
	}
	/* pr_info("enter..\n"); */
	mutex_lock(&chip->irq_complete);
	msleep(2);

	p922x_get_signal_strength(chip);

	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		rc = idtp9220_read(chip, handlers[i].stat_reg,
					&handlers[i].val);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't read %d rc = %d\n",
					handlers[i].stat_reg, rc);
			continue;
		}
	}

	p922x_irq_clr(chip);

	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		p922x_dbg(chip, PR_INTERRUPT, "[%d]reg=0x%x val=0x%x prev_val=0x%x\n",
				i, handlers[i].stat_reg, handlers[i].val, handlers[i].prev_val);

		for (j = 0; j < ARRAY_SIZE(handlers[i].irq_info); j++) {
			rt_stat = handlers[i].val
				& (IRQ_STATUS_MASK << j);
			prev_rt_stat = handlers[i].prev_val
				& (IRQ_STATUS_MASK << j);
			changed = prev_rt_stat ^ rt_stat;
			changed = rt_stat;
			if (changed)
				rt_stat ? handlers[i].irq_info[j].high++ :
						handlers[i].irq_info[j].low++;

			if (changed && handlers[i].irq_info[j].p922x_irq != NULL) {
				handler_count++;
				p922x_dbg(chip, PR_INTERRUPT, "call %pf, handler_count=%d\n",
					handlers[i].irq_info[j].p922x_irq, handler_count);
				rc = handlers[i].irq_info[j].p922x_irq(chip,
								rt_stat);
				if (rc < 0)
					dev_err(chip->dev,
						"Couldn't handle %d irq for reg 0x%02x rc = %d\n",
						j, handlers[i].stat_reg, rc);
			}
		}
		handlers[i].prev_val = handlers[i].val;
	}

	p922x_dbg(chip, PR_INTERRUPT, "handler count = %d\n", handler_count);

#if 0
	if (handler_count) {
		cancel_delayed_work(&chip->update_heartbeat_work);
		schedule_delayed_work(&chip->update_heartbeat_work, 0);

		cancel_delayed_work(&chip->charger_eoc_work);
		schedule_delayed_work(&chip->charger_eoc_work, 0);
	}
#endif

	mutex_unlock(&chip->irq_complete);
	p922x_dbg(chip, PR_INTERRUPT, "p922x_stat_handler end\n");

	return IRQ_HANDLED;
}


/* set hall gpio input and no pull*/
static int p922x_set_gpio_state(struct p922x_dev *chip)
{
	int error = 0;

	chip->p922x_gpio_pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->p922x_gpio_pinctrl)) {
		p922x_dbg(chip, PR_DEBUG, "Can not get p922x_gpio_pinctrl\n");
		error = PTR_ERR(chip->p922x_gpio_pinctrl);
		return error;
	}
	chip->p922x_gpio_state = pinctrl_lookup_state(chip->p922x_gpio_pinctrl, "wls_int_default");
	if (IS_ERR_OR_NULL(chip->p922x_gpio_state)) {
		p922x_dbg(chip, PR_DEBUG, "Can not get p922x_gpio_state\n");
		error = PTR_ERR(chip->p922x_gpio_state);
		return error;
	}

	error = pinctrl_select_state(chip->p922x_gpio_pinctrl, chip->p922x_gpio_state);
	if (error) {
		p922x_dbg(chip, PR_DEBUG, "can not set hall_gpio pins to zte_hall_gpio_active states\n");
	} else {
		p922x_dbg(chip, PR_DEBUG, "set_p922x_gpio_state success.\n");
	}
	return error;
}

/*
static int p922x_en_pin_pinctrl_deinit(struct p922x_dev *chip)
{
	int rc = 0;

	devm_pinctrl_put(chip->p922x_en_pin_pinctrl);

	return rc;
}
*/

static int p922x_en_pin_pinctrl_init(struct p922x_dev *chip)
{
	int rc = 0;

	chip->p922x_en_pin_pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->p922x_en_pin_pinctrl)) {
		rc = PTR_ERR(chip->p922x_en_pin_pinctrl);
		pr_err("failed to get pinctrl, rc=%d\n", rc);
		return rc;
	}

	chip->p922x_en_pin_active = pinctrl_lookup_state(chip->p922x_en_pin_pinctrl, "wls_power_enable");
	if (IS_ERR_OR_NULL(chip->p922x_en_pin_active)) {
		rc = PTR_ERR(chip->p922x_en_pin_active);
		pr_err("failed to get pinctrl active state, rc=%d\n", rc);
		return rc;
	}

	chip->p922x_en_pin_suspend = pinctrl_lookup_state(chip->p922x_en_pin_pinctrl, "wls_power_disable");
	if (IS_ERR_OR_NULL(chip->p922x_en_pin_suspend)) {
		rc = PTR_ERR(chip->p922x_en_pin_suspend);
		pr_err("failed to get pinctrl suspend state, rc=%d\n", rc);
		return rc;
	}

	p922x_dbg(chip, PR_DEBUG, "done.\n");
	return rc;
}

static int p922x_set_en_pin_pinctrl_state(struct p922x_dev *chip, bool enable)
{
	int rc = 0;
	struct pinctrl_state *state;

#ifndef ZTE_WIRELESS_CHARGER
	enable = false;
#endif
	p922x_dbg(chip, PR_INTERRUPT, "enable = %d\n", enable);

	if (enable) {
		state = chip->p922x_en_pin_active;
	} else {
		state = chip->p922x_en_pin_suspend;
	}

	rc = pinctrl_select_state(chip->p922x_en_pin_pinctrl, state);
	if (rc) {
		pr_err("failed to set pin state, rc=%d\n", rc);
	}

	return rc;
}

static int idtp922x_set_enable(struct p922x_dev *chip, bool enable)
{
	int rc = 0;

	rc = p922x_set_en_pin_pinctrl_state(chip, enable);

	return rc;
}

static int show_irq_count(struct seq_file *m, void *data)
{
	int i = 0, j = 0, total = 0;

	for (i = 0; i < ARRAY_SIZE(handlers); i++)
		for (j = 0; j < 8; j++) {
			seq_printf(m, "%s=%d\t(high=%d low=%d)\n",
						handlers[i].irq_info[j].name,
						handlers[i].irq_info[j].high
						+ handlers[i].irq_info[j].low,
						handlers[i].irq_info[j].high,
						handlers[i].irq_info[j].low);
			total += (handlers[i].irq_info[j].high
					+ handlers[i].irq_info[j].low);
		}

	seq_printf(m, "\n\tTotal = %d\n", total);

	return 0;
}

static int irq_count_debugfs_open(struct inode *inode, struct file *file)
{
	struct p922x_dev *chip = inode->i_private;

	return single_open(file, show_irq_count, chip);
}

static const struct file_operations irq_count_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= irq_count_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void fod_voltage_check_work_cb(struct work_struct *work)
{
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
						fod_voltage_check_work.work);
	int i = 0, timeout = 100, delay_ms = 20;

	if (!chip->device_auth_success) {
		p922x_dbg(chip, PR_INTERRUPT, "device auth failed return.\n");
		p922x_set_fast_charging_voltage(chip, (chip->out_voltage / 1000));
		if (!skip_auth)
			return;
	}

	/* check adapter type again if it's necessary */
	if (chip->idt_adapter_type == ADAPTER_UNKNOWN) {
		schedule_delayed_work(&chip->get_tx_adapter_work, msecs_to_jiffies(0));
		for (i = 0; i < timeout; i++) {
			if (chip->idt_adapter_type != ADAPTER_UNKNOWN) {
				break;
			}
			msleep(delay_ms);
		}
	}

	/* check tx fw again if it's necessary */
	if (chip->tx_fw_version == TX_FW_VERSION_UNKNOWN) {
		schedule_delayed_work(&chip->get_tx_fw_work, msecs_to_jiffies(0));
		for (i = 0; i < timeout; i++) {
			if (chip->tx_fw_version == TX_FW_VERSION_UNKNOWN) {
				break;
			}
			msleep(delay_ms);
		}
	}

	p922x_dbg(chip, PR_INTERRUPT, "adapter type:%d, out_voltage:%d, power %s,"
				"tx_fw_version:%d, lab_debug_vol_mv:%d\n",
				chip->idt_adapter_type, chip->out_voltage, gpio_get_value(chip->power_good_pin),
				chip->tx_fw_version, lab_debug_vol_mv);

	if (chip->out_voltage && gpio_get_value(chip->power_good_pin)) {
		if (lab_debug_vol_mv)
			chip->out_voltage = lab_debug_vol_mv * 1000;
		p922x_set_fast_charging_voltage(chip, (chip->out_voltage / 1000));
	}
}

enum {
	CUR_LEGACY = 0,
	VOL_LEGACY,
	EFF_LEGACY,
};

#define CURRENT_MIN 0
#define CURRENT_MAX 4000
#define VOLTAGE_MIN 0
#define VOLTAGE_MAX 12500
#define EFFICIENCY_MIN 0
#define EFFICIENCY_MAX 100

static int in_range(int val, int min, int max)
{
	int ret = 0;

	if (val >= min && val <= max) {
		ret = val;
	} else if (val < min) {
		ret = min;
	} else if (val > max) {
		ret = max;
	}

	return ret;
}

static int get_legal_val(int val, int type)
{
	int ret = 0;

	if (type == CUR_LEGACY) {
		ret = in_range(val, CURRENT_MIN, CURRENT_MAX);
	} else if (type == VOL_LEGACY) {
		ret = in_range(val, VOLTAGE_MIN, VOLTAGE_MAX);
	} else if (type == EFF_LEGACY) {
		ret = in_range(val, EFFICIENCY_MIN, EFFICIENCY_MAX);
	}

	return ret;
}

static void tx_fw_work(struct work_struct *work)
{
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
						get_tx_fw_work.work);

	p922x_dbg(chip, PR_DEBUG, "tx_fw_work\n");
	p922x_get_tx_firmware_version(chip);
}

static void tx_adapter_work(struct work_struct *work)
{
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
						get_tx_adapter_work.work);

	p922x_dbg(chip, PR_DEBUG, "tx_adapter_work\n");
	p922x_get_tx_atapter_type(chip);
}

#define WLS_RX_IOUT_MIN 100
#define WLS_RX_VOUT_MIN 4800
static void e_trans_work(struct work_struct *work)
{
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
						e_trans_show_work.work);

	if (chip->power_good && chip->id_auth_success && chip->device_auth_success) {
		chip->idt_tx_vin = get_legal_val(p922x_get_tx_vin(chip), VOL_LEGACY);
		chip->idt_tx_iin = get_legal_val(p922x_get_tx_iin(chip), CUR_LEGACY);
		chip->idt_rx_vout = get_legal_val(p922x_get_rx_vout(chip), VOL_LEGACY);
		chip->idt_rx_iout = get_legal_val(p922x_get_rx_iout(chip), CUR_LEGACY);
		if (chip->idt_tx_vin * chip->idt_tx_iin != 0) {
			chip->trans_efficiency =
				100 * (chip->idt_rx_vout * chip->idt_rx_iout) / (chip->idt_tx_vin * chip->idt_tx_iin);
			chip->trans_efficiency = get_legal_val(chip->trans_efficiency, EFF_LEGACY);
		}
		p922x_dbg(chip, PR_DEBUG, "power%s, ss:%d, efficiency:%d, RX:%dmV, %dmA, TX:%dmV, %dmA\n",
			chip->power_good ? "on" : "off", chip->signal_strength,
			chip->trans_efficiency, chip->idt_rx_vout,
			chip->idt_rx_iout, chip->idt_tx_vin, chip->idt_tx_iin);
	} else if (chip->power_good) {
		chip->idt_tx_vin = 0;
		chip->idt_tx_iin = 0;
		chip->idt_rx_vout = get_legal_val(p922x_get_rx_vout(chip), VOL_LEGACY);
		chip->idt_rx_iout = get_legal_val(p922x_get_rx_iout(chip), CUR_LEGACY);
		chip->trans_efficiency = 0;
		p922x_dbg(chip, PR_DEBUG, "power%s or id auth %s or device auth %s, ss:%d, RX:%dmV, %dmA\n",
			chip->power_good ? "on" : "off", chip->id_auth_success ? "success" : "fail",
			chip->device_auth_success ? "success" : "fail",
			chip->signal_strength, chip->idt_rx_vout, chip->idt_rx_iout);
	} else if (!chip->power_good) {
		chip->idt_tx_vin = 0;
		chip->idt_tx_iin = 0;
		chip->idt_rx_vout = 0;
		chip->idt_rx_iout = 0;
		chip->trans_efficiency = 0;
	}
	/* if power is good but rx iout is too small, it means not in correct state*/
	if (chip->power_good && chip->idt_rx_iout < WLS_RX_IOUT_MIN
						&& chip->idt_rx_vout > WLS_RX_VOUT_MIN) {
		p922x_dbg(chip, PR_DEBUG, "power good but rx iout is too small, reset\n");
		p922x_system_reset(chip);
	}

	schedule_delayed_work(&chip->e_trans_show_work, msecs_to_jiffies(2000));
}

static void key_auth_work(struct work_struct *work)
{
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
						device_key_auth_work.work);

	if (chip->power_good && chip->id_auth_success) {
		p922x_dbg(chip, PR_INTERRUPT, "SENDDEVICEAUTH CMD\n");
		chip->bus.write(chip, REG_COMMAND, SENDDEVICEAUTH);
	}
}

#define EMODE_SHOW_SIZE 160
static int tx_rx_trans_efficiency = 0;
static int p922x_tx_rx_trans_efficiency_node(char *val, const struct kernel_param *kp)
{
	static bool rx_fw_gotten = false;
	static u8 ver_data[4];

	p922x_dbg(_chip, PR_DEBUG, "efficiency:%d\n",
		_chip->trans_efficiency);

	if (!rx_fw_gotten) {
		_chip->bus.read_buf(_chip, REG_CHIP_REV, ver_data, 4);
		if (ver_data[3] == 0x0a) {
			rx_fw_gotten = true;
		}
	}

	return snprintf(val, EMODE_SHOW_SIZE,
		"ID:%2d, Coil:%8s\tFW:%02x.%02x.%02x.%02x,SS:%2d,E:%2d,RX:%4dmV,%4dmA,TX:%4dmV,%4dmA",
		_chip->board_id, _chip->coil_vendor_name, ver_data[3], ver_data[2], ver_data[1], ver_data[0],
		_chip->signal_strength, _chip->trans_efficiency,
		_chip->idt_rx_vout, _chip->idt_rx_iout, _chip->idt_tx_vin, _chip->idt_tx_iin);
}

static struct kernel_param_ops p922x_tx_rx_trans_efficiency_node_ops = {
	.set = param_set_int,
	.get = p922x_tx_rx_trans_efficiency_node,
};
module_param_cb(tx_rx_trans_efficiency, &p922x_tx_rx_trans_efficiency_node_ops,
				&tx_rx_trans_efficiency, 0644);

static void __iomem *vendor_imem_info_addr;
static int vendor_imem_info_parse_dt(const char *compatible)
{
	struct device_node *np;
	int val = -EINVAL;

	np = of_find_compatible_node(NULL, NULL, compatible);
	if (!np) {
		pr_err("unable to find DT imem %s node\n", compatible);
	} else {
		vendor_imem_info_addr = of_iomap(np, 0);
		if (!vendor_imem_info_addr) {
			pr_err("unable to map imem %s offset\n", compatible);
		} else {
			val = __raw_readl(vendor_imem_info_addr);
			pr_info("%s: %d\n", compatible, val);
		}
	}
	return val;
}

static int read_board_id(void)
{
	int id = 0;

	id = vendor_imem_info_parse_dt("qcom,msm-imem-board-id");

	return id;
}

void copy_array(const char *p_src, char *p_dest, int size)
{
	int num = 0;

	for (num = 0; num <= size - 1; num++) {
		p_dest[num] = p_src[num];
	}
}

static enum power_supply_property idtp922x_properties[] = {
	POWER_SUPPLY_PROP_PIN_ENABLED,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
};

static int idtp922x_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct p922x_dev *chip = power_supply_get_drvdata(psy);

	if (chip) {
		switch (psp) {
		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			val->intval = p922x_get_rx_output_voltage_max(chip);
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
			val->intval = p922x_get_rx_output_current_max(chip);
			break;
		case POWER_SUPPLY_PROP_PIN_ENABLED:
			break;
		case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
			if (chip->power_good) {
				val->intval = p922x_get_rx_vout(chip);
			} else {
				val->intval = 0;
			}
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

static int idtp922x_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct p922x_dev *chip = power_supply_get_drvdata(psy);

	if (chip) {
		switch (psp) {
		case POWER_SUPPLY_PROP_PIN_ENABLED:
			idtp922x_set_enable(chip, val->intval);
			break;
		case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
			chip->out_voltage = val->intval;
			p922x_dbg(chip, PR_INTERRUPT, "set out_voltage %d.\n", chip->out_voltage);
			cancel_delayed_work(&chip->fod_voltage_check_work);
			schedule_delayed_work(&chip->fod_voltage_check_work, msecs_to_jiffies(20));
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

static int idtp922x_property_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PIN_ENABLED:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

#define MIN_INPUT_BTYE 64
#define INPUT_BUFFER_LEN 128
#define FOD_ARRAY_LEN 12

static int fod_5v_debug_show(struct seq_file *m, void *data)
{
	int i = 0;

	for (i = 0; i < FOD_REGSTERS_NUM; i++) {
		seq_printf(m, "0x%02X", idtp9220_rx_fod_5v[i]);
		if (i != FOD_REGSTERS_NUM - 1)
			seq_printf(m, ", ");
		else
			seq_printf(m, "\n");
	}

	return 0;
}

static int fod_5v_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, fod_5v_debug_show, NULL);
}

static ssize_t fod_5v_debug_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	char strings[INPUT_BUFFER_LEN] = {0, };
	int temp[FOD_ARRAY_LEN] = {0, }, i = 0;

	pr_info("IDT %s into, count %d\n", __func__, count);

	if ((count == 0) || (count < MIN_INPUT_BTYE) || (count >= INPUT_BUFFER_LEN)) {
		pr_info("IDT %s count is too short to store, count %d\n", __func__, count);
		return count;
	}

	memset(strings, 0, sizeof(strings));
	if (copy_from_user(strings, buf, count)) {
		pr_info("IDT %s copy_from_user failed, count %d\n", __func__, count);
		return count;
	}

	strings[INPUT_BUFFER_LEN - 1] = 0;

	pr_info("IDT %s read strings %s\n", __func__, strings);

	for (i = 0; i < sizeof(strings); i++) {
		if ((strings[i] == '\r') || (strings[i] == '\n'))
			strings[i] = '\0';
	}

	sscanf(strings, "0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, "
		"0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X",
		temp, temp + 1, temp + 2, temp + 3,
		temp + 4, temp + 5, temp + 6, temp + 7,
		temp + 8, temp + 9, temp + 10, temp + 11);

	for (i = 0; i < ARRAY_SIZE(temp); i++) {
		pr_info("IDT %s restore[%d]: %02X\n", __func__, i, temp[i]);
		idtp9220_rx_fod_5v[i] = temp[i];
	}

	return count;
}

static const struct file_operations fod_5v_debug_ops = {
	.owner		= THIS_MODULE,
	.open		= fod_5v_debug_open,
	.read		= seq_read,
	.write		= fod_5v_debug_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int fod_9v_debug_show(struct seq_file *m, void *data)
{
	int i = 0;

	for (i = 0; i < FOD_REGSTERS_NUM; i++) {
		seq_printf(m, "0x%02X", idtp9220_rx_fod_9v[i]);
		if (i != FOD_REGSTERS_NUM - 1)
			seq_printf(m, ", ");
		else
			seq_printf(m, "\n");
	}

	return 0;
}

static int fod_9v_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, fod_9v_debug_show, NULL);
}

static ssize_t fod_9v_debug_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	char strings[INPUT_BUFFER_LEN] = {0, };
	int temp[FOD_ARRAY_LEN] = {0, }, i = 0;

	pr_info("IDT %s into, count %d\n", __func__, count);

	if ((count == 0) || (count < MIN_INPUT_BTYE) || (count >= INPUT_BUFFER_LEN)) {
		pr_info("IDT %s count is too short to store, count %d\n", __func__, count);
		return count;
	}

	memset(strings, 0, sizeof(strings));
	if (copy_from_user(strings, buf, count)) {
		pr_info("IDT %s copy_from_user failed, count %d\n", __func__, count);
		return count;
	}

	strings[INPUT_BUFFER_LEN - 1] = 0;

	pr_info("IDT %s read strings %s\n", __func__, strings);

	for (i = 0; i < sizeof(strings); i++) {
		if ((strings[i] == '\r') || (strings[i] == '\n'))
			strings[i] = '\0';
	}

	sscanf(strings, "0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, "
		"0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X",
		temp, temp + 1, temp + 2, temp + 3,
		temp + 4, temp + 5, temp + 6, temp + 7,
		temp + 8, temp + 9, temp + 10, temp + 11);

	for (i = 0; i < ARRAY_SIZE(temp); i++) {
		pr_info("IDT %s restore[%d]: %02X\n", __func__, i, temp[i]);
		idtp9220_rx_fod_9v[i] = temp[i];
	}

	return count;
}

static const struct file_operations fod_9v_debug_ops = {
	.owner		= THIS_MODULE,
	.open		= fod_9v_debug_open,
	.read		= seq_read,
	.write		= fod_9v_debug_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int fod_12v_debug_show(struct seq_file *m, void *data)
{
	int i = 0;

	for (i = 0; i < FOD_REGSTERS_NUM; i++) {
		seq_printf(m, "0x%02X", idtp9220_rx_fod_12v[i]);
		if (i != FOD_REGSTERS_NUM - 1)
			seq_printf(m, ", ");
		else
			seq_printf(m, "\n");
	}

	return 0;
}

static int fod_12v_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, fod_12v_debug_show, NULL);
}

static ssize_t fod_12v_debug_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	char strings[INPUT_BUFFER_LEN] = {0, };
	int temp[FOD_ARRAY_LEN] = {0, }, i = 0;

	pr_info("IDT %s into, count %d\n", __func__, count);

	if ((count == 0) || (count < MIN_INPUT_BTYE) || (count >= INPUT_BUFFER_LEN)) {
		pr_info("IDT %s count is too short to store, count %d\n", __func__, count);
		return count;
	}

	memset(strings, 0, sizeof(strings));
	if (copy_from_user(strings, buf, count)) {
		pr_info("IDT %s copy_from_user failed, count %d\n", __func__, count);
		return count;
	}

	strings[INPUT_BUFFER_LEN - 1] = 0;

	pr_info("IDT %s read strings %s\n", __func__, strings);

	for (i = 0; i < sizeof(strings); i++) {
		if ((strings[i] == '\r') || (strings[i] == '\n'))
			strings[i] = '\0';
	}

	sscanf(strings, "0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, "
		"0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X",
		temp, temp + 1, temp + 2, temp + 3,
		temp + 4, temp + 5, temp + 6, temp + 7,
		temp + 8, temp + 9, temp + 10, temp + 11);

	for (i = 0; i < ARRAY_SIZE(temp); i++) {
		pr_info("IDT %s restore[%d]: %02X\n", __func__, i, temp[i]);
		idtp9220_rx_fod_12v[i] = temp[i];
	}

	return count;
}

static const struct file_operations fod_12v_debug_ops = {
	.owner		= THIS_MODULE,
	.open		= fod_12v_debug_open,
	.read		= seq_read,
	.write		= fod_12v_debug_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct power_supply_desc idtp922x_psy_desc = {
	.name = "wireless",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.get_property = idtp922x_get_property,
	.set_property = idtp922x_set_property,
	.properties = idtp922x_properties,
	.property_is_writeable = idtp922x_property_is_writeable,
	.num_properties = ARRAY_SIZE(idtp922x_properties),
};
#define FOP_THRESHOLD 116
static void p922x_fop_check_work(struct work_struct *work)
{
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
						fop_check_work.work);
	struct power_supply *battery_psy = NULL;
	union power_supply_propval charger_status = {0, };
	int fop = 0, rc = 0;

	pr_info("p922x_fop_check_work enter..\n");

	/*chip->power_good = gpio_get_value(chip->power_good_pin);*/
	if (!chip->power_good) {
		p922x_dbg(chip, PR_DEBUG, "chip->power_good_pin is OFF\n");
		return;
	}


	fop = p922x_get_rx_fop(chip);
	if (!fop)
		return;

	battery_psy = power_supply_get_by_name("battery");
	if (!battery_psy) {
		p922x_dbg(chip, PR_DEBUG, "Get battery psy failed!!\n");
		return;
	}

	rc = power_supply_get_property(battery_psy,
					POWER_SUPPLY_PROP_STATUS, &charger_status);
	if (rc < 0) {
		p922x_dbg(chip, PR_DEBUG, "Failed to get battery status\n");
		return;
	}

	p922x_dbg(chip, PR_DEBUG, "fop:%d, status:%d\n", fop, charger_status.intval);

	if (charger_status.intval == POWER_SUPPLY_STATUS_CHARGING) {
		chip->status_good = true;
	}

	if (chip->power_good && chip->status_good) {
		if ((charger_status.intval != POWER_SUPPLY_STATUS_CHARGING)
			&& charger_status.intval != POWER_SUPPLY_STATUS_FULL
			&& (fop <= FOP_THRESHOLD)) {
			p922x_dbg(chip, PR_DEBUG, "fop check reset start\n");
			p922x_set_pmic_dcin_current(0);
			idtp922x_set_enable(chip, false);
			msleep(5000);
			idtp922x_set_enable(chip, true);
			p922x_set_pmic_dcin_current(WLS_2P0A);
			p922x_dbg(chip, PR_DEBUG, "fop check reset end\n");
		}
	}
}


static int p922x_power_notifier_handle(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct power_supply *notify_psy = data;
	struct p922x_dev *chip = container_of(nb, struct p922x_dev, nb);

	pr_info("p922x_power_notifier_handle enter..\n");

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if (strncmp(notify_psy->desc->name,
				"battery", strlen("battery")) != 0) {
		return NOTIFY_DONE;
	}
	p922x_dbg(chip, PR_DEBUG, "will schedule_delayed_work fop_check_work\n");
	schedule_delayed_work(&chip->fop_check_work, msecs_to_jiffies(10));

	return NOTIFY_DONE;
}


__attribute__((unused)) static int p922x_register_power_notifier(struct p922x_dev *chip)
{
	int rc = 0;

	pr_info("p922x_register_power_notifier enter..\n");
	chip->nb.notifier_call = p922x_power_notifier_handle;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		p922x_dbg(chip, PR_DEBUG, "Couldn't register psy notifier rc = %d\n", rc);
		return -EINVAL;
	}

	return 0;
}

static int p922x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct p922x_dev *chip;
	int ret = 0;
	struct power_supply_config idtp922x_cfg = {};
	struct proc_dir_entry *dir;
	struct proc_dir_entry *refresh;

	pr_info("IDTP922X probe\n");

	if (strcmp(STR2(ZTE_BOARD_NAME), "pine") == 0) {
		pr_err("IDTP922X probe board is pine\n");
		idtp9220_rx_fod_5v = idtp9220_rx_fod_5v_pine;
		idtp9220_rx_fod_9v = idtp9220_rx_fod_9v_pine;
		idtp9220_rx_fod_12v = idtp9220_rx_fod_12v_pine;
	} else if (strcmp(STR2(ZTE_BOARD_NAME), "poad") == 0) {
		pr_err("IDTP922X probe board is poad\n");
		idtp9220_rx_fod_5v = idtp9220_rx_fod_5v_poad;
		idtp9220_rx_fod_9v = idtp9220_rx_fod_9v_poad;
		idtp9220_rx_fod_12v = idtp9220_rx_fod_12v_poad;
	} else {
		pr_err("IDTP922X probe can't find correct board name, use default\n");
		idtp9220_rx_fod_5v = idtp9220_rx_fod_5v_pine;
		idtp9220_rx_fod_9v = idtp9220_rx_fod_9v_pine;
		idtp9220_rx_fod_12v = idtp9220_rx_fod_12v_pine;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->board_id = read_board_id();
	pr_info("board_id = %d\n", chip->board_id);
	if (chip->board_id != BOARD_ID_VERSION_DEFAULT) {
		/*
		pr_info("IDTP922X update fod parameters for old coil.\n");
		copy_array(idtp9220_rx_fod_5v_Amphenol_Coil, idtp9220_rx_fod_5v, FOD_REGSTERS_NUM);
		copy_array(idtp9220_rx_fod_9v_Amphenol_Coil, idtp9220_rx_fod_9v, FOD_REGSTERS_NUM);
		strncpy(chip->coil_vendor_name, "Amphenol", strlen("Amphenol"));
		*/
	} else {
		/*
		pr_info("IDTP922X update fod parameters for new coil.\n");
		copy_array(idtp9220_rx_fod_5v_Amotech_Coil, idtp9220_rx_fod_5v, FOD_REGSTERS_NUM);
		copy_array(idtp9220_rx_fod_9v_Amotech_Coil, idtp9220_rx_fod_9v, FOD_REGSTERS_NUM);
		*/
		strlcpy(chip->coil_vendor_name, "default", strlen("default"));
	}
	pr_info("coil vendor name is %s\n", chip->coil_vendor_name);

	_chip = chip;
	chip->dev = &client->dev;
	chip->regmap = devm_regmap_init_i2c(client, &p922x_regmap_config);
	if (!chip->regmap) {
		pr_err("parent regmap is missing\n");
		return -EINVAL;
	}
	i2c_set_clientdata(client, chip);
	chip->client = client;
	chip->dev = &client->dev;

	chip->bus.read = idtp9220_read;
	chip->bus.write = idtp9220_write;
	chip->bus.read_buf = idtp9220_read_buffer;
	chip->bus.write_buf = idtp9220_write_buffer;
	chip->name = "IDT";
	device_init_wakeup(chip->dev, true);

	mutex_init(&chip->write_lock);
	mutex_init(&chip->irq_complete);
	mutex_init(&chip->send_pkg_lock);

	INIT_DELAYED_WORK(&chip->fod_voltage_check_work, fod_voltage_check_work_cb);
	INIT_DELAYED_WORK(&chip->get_tx_fw_work, tx_fw_work);
	INIT_DELAYED_WORK(&chip->get_tx_adapter_work, tx_adapter_work);
	INIT_DELAYED_WORK(&chip->e_trans_show_work, e_trans_work);
	INIT_DELAYED_WORK(&chip->device_key_auth_work, key_auth_work);
	INIT_DELAYED_WORK(&chip->fop_check_work, p922x_fop_check_work);

	chip->debug_root = debugfs_create_dir("p922x", NULL);
	if (!chip->debug_root)
		dev_err(chip->dev, "Couldn't create debug dir\n");
	if (chip->debug_root) {
		struct dentry *ent;
#if 0
		ent = debugfs_create_file("registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &cnfg_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create cnfg debug file\n");
		ent = debugfs_create_u8("address", S_IRUSR | S_IWUSR,
					chip->debug_root,
					&chip->reg_addr);
		if (!ent) {
			dev_err(chip->dev,
				"Couldn't create address debug file\n");
		}

		ent = debugfs_create_file("data",  S_IRUSR | S_IWUSR,
					chip->debug_root, chip,
					&ti2419x_debug_data_fops);
		if (!ent) {
			dev_err(chip->dev,
				"Couldn't create data debug file\n");
		}

		ent = debugfs_create_x32("skip_writes",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->skip_writes));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file\n");

		ent = debugfs_create_x32("skip_reads",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->skip_reads));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file\n");
#endif
		ent = debugfs_create_file("irq_count", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &irq_count_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create count debug file\n");
	}

	chip->fod_5v_enabled = false;
	chip->fod_9v_enabled = false;
	chip->status_good = false;

	ret = of_property_read_u32(chip->dev->of_node, "qcom,wls-ss-good-thr", &chip->signal_strength_good_threshold);
	if (ret < 0) {
		pr_err("get signal_strength_good_threshold failed\n");
		chip->signal_strength_good_threshold = WIRELESS_CHARGING_SIGNAL_GOOD_THRESHOLD_DEFAULT;
	}

	ret = p922x_en_pin_pinctrl_init(chip);
	if (ret < 0) {
		p922x_dbg(chip, PR_DEBUG, "p922x_en_pin_pinctrl_init failed.\n");
		goto release;
	} else {
		ret = p922x_set_en_pin_pinctrl_state(chip, true);
		if (ret < 0) {
			p922x_dbg(chip, PR_DEBUG, "p922x_set_en_pin_pinctrl_state failed.\n");
			goto release;
		} else {
			p922x_dbg(chip, PR_DEBUG, "p922x_set_en_pin_pinctrl_state success.\n");
		}
	}

	chip->int_pin = of_get_named_gpio(chip->dev->of_node, "qcom,wls-int-pin", 0);
	if (!gpio_is_valid(chip->int_pin)) {
		pr_err("int pin is invalid.\n");
		ret = -EINVAL;
		goto release;
	}

	chip->power_good_pin = of_get_named_gpio(chip->dev->of_node, "qcom,wls-pwrgd-pin", 0);
	if (!gpio_is_valid(chip->power_good_pin)) {
		pr_err("power good pin is invalid.\n");
		ret = -EINVAL;
		goto release;
	}
	ret = p922x_set_gpio_state(chip);
	if (ret < 0) {
		p922x_dbg(chip, PR_DEBUG, "p922x_set_gpio_state failed.\n");
		goto release;
	}
	gpio_direction_input(chip->power_good_pin);
	if (gpio_get_value(chip->power_good_pin)) {
		p922x_dbg(chip, PR_DEBUG, "p922x_irq_disable.\n");
		p922x_irq_disable(chip);
	}
	ret = request_threaded_irq(gpio_to_irq(chip->int_pin),
			NULL, p922x_stat_handler,
			(IRQ_TYPE_EDGE_FALLING | IRQF_ONESHOT),
			"p922x-handler", chip);
	if (ret < 0) {
		p922x_dbg(chip, PR_DEBUG, "request irq failed\n");
		gpio_free(chip->int_pin);
		goto release;
	} else {
		p922x_dbg(chip, PR_DEBUG, "request irq success\n");
	}
	enable_irq_wake(gpio_to_irq(chip->int_pin));

	ret = request_threaded_irq(gpio_to_irq(chip->power_good_pin),
			NULL, p922x_power_good_handler,
			(IRQ_TYPE_EDGE_BOTH | IRQF_ONESHOT),
			"p922x-power_good_handler", chip);
	if (ret < 0) {
		p922x_dbg(chip, PR_DEBUG, "request power_good irq failed\n");
		gpio_free(chip->power_good_pin);
		goto release;
	} else {
		p922x_dbg(chip, PR_DEBUG, "request power_good irq success\n");
	}
	enable_irq_wake(gpio_to_irq(chip->power_good_pin));

	if (gpio_get_value(chip->power_good_pin)) {
		p922x_dbg(chip, PR_DEBUG, "p922x_stat_handler.\n");
		p922x_stat_handler(gpio_to_irq(chip->int_pin), chip);
		p922x_irq_enable(chip);
	}

	idtp922x_cfg.drv_data = chip;
	chip->idtp922x_psy = power_supply_register(chip->dev,
			&idtp922x_psy_desc,
			&idtp922x_cfg);

	chip->out_voltage_max = WLS_DEF_VOL;
	chip->out_current_max = WLS_DEF_CUR;
	chip->idt_adapter_type = ADAPTER_UNKNOWN;

	p922x_dbg(chip, PR_DEBUG, "IDTP922X probed successfully chip is power %s\n",
		(gpio_get_value(chip->power_good_pin)) ? "ON" : "OFF");

	dir = proc_mkdir("idt-debug", NULL);
	if (!dir)
		pr_info("IDT %s proc_mkdir failed\n", __func__);

	refresh = proc_create("fod-5v-debug", 0664, dir, &fod_5v_debug_ops);
	if (!refresh)
		pr_info("IDT %s proc_create failed\n", __func__);

	refresh = proc_create("fod-9v-debug", 0664, dir, &fod_9v_debug_ops);
	if (!refresh)
		pr_info("IDT %s proc_create failed\n", __func__);

	refresh = proc_create("fod-12v-debug", 0664, dir, &fod_12v_debug_ops);
	if (!refresh)
		pr_info("IDT %s proc_create failed\n", __func__);

	/* disbale fop check */
	/* p922x_register_power_notifier(chip);*/

	return 0;

release:
	p922x_dbg(chip, PR_DEBUG, "IDTP922X init is failed");
	i2c_set_clientdata(client, NULL);

	return ret;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "IDT,P922X-WLS-POWER", },
	{ },
};

static const struct i2c_device_id p922x_dev_id[] = {
	{ "P922X-WLS-POWER", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, p922x_dev_id);

static struct i2c_driver p922x_driver = {
	.driver   = {
		.name			= "P922X-WLS-POWER",
		.owner			= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe	= p922x_probe,
	.remove	= p922x_remove,
	.id_table	= p922x_dev_id,
};

static int __init p922x_driver_init(void)
{
	return i2c_add_driver(&p922x_driver);
}
module_init(p922x_driver_init);

static void __exit p922x_driver_exit(void)
{
	i2c_del_driver(&p922x_driver);
}
module_exit(p922x_driver_exit);

MODULE_AUTHOR("ztechargerteam@zte.com.cn");
MODULE_DESCRIPTION("P922X Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL v2");
