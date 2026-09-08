// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 LONTIUM
 *
 */
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/string.h>
#include <drm/drm_drv.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_edid.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_of.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>

#include <sound/hdmi-codec.h>

#define FW_SIZE (64 * 1024)
#define LT9611UXD_CMD_READY_TRIES	20
#define LT9611UXD_CMD_READY_INTERVAL_MS	100
#define LT9611UXD_EDID_TRIES		20
#define LT9611UXD_EDID_INTERVAL_MS	100
#define LT9611UXD_EDID_BLOCKS		2
#define LT9611UXD_EDID_BLOCK_LEN	128
#define LT_PAGE_SIZE 256
#define FW_FILE  "Lontium/LT9611UXD.bin"

struct lt9611uxd {
	struct device *dev;
	struct i2c_client *client;
	struct drm_bridge bridge;
	struct drm_connector connector;
	struct regmap *regmap;
	/* Protects all accesses to registers by stopping the on-chip MCU */
	struct mutex ocm_lock;
	struct work_struct work;
	struct device_node *dsi0_node;
	struct device_node *dsi2_node;
	struct mipi_dsi_device *dsi0;
	struct mipi_dsi_device *dsi2;
	struct platform_device *audio_pdev;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[2];
	struct device *codec_dev;
	struct task_struct *kthread;
	hdmi_codec_plugged_cb plugged_cb;
	const struct firmware *fw;
	int fw_version;
	u8 fw_crc;
	u8 edid[LT9611UXD_EDID_BLOCKS * LT9611UXD_EDID_BLOCK_LEN];
	unsigned long edid_valid;

	bool hdmi_connected;
	enum drm_connector_status audio_status;
};

#define LT9611C_PAGE_CONTROL	0xff

static const struct regmap_range_cfg lt9611uxd_ranges[] = {
	{
		.name = "register_range",
		.range_min =  0,
		.range_max = 0xffff,
		.selector_reg = LT9611C_PAGE_CONTROL,
		.selector_mask = 0xff,
		.selector_shift = 0,
		.window_start = 0,
		.window_len = 0x100,
	},
};

static const struct regmap_config lt9611uxd_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xffff,
	.ranges = lt9611uxd_ranges,
	.num_ranges = ARRAY_SIZE(lt9611uxd_ranges),
};

struct crc_info {
	u8 width;
	u32 poly;
	u32 crc_init;
	u32 xor_out;
	bool refin;
	bool refout;
};

static void lt9611uxd_audio_update_connector_status(struct lt9611uxd *lt9611uxd);

static unsigned int bits_reverse(u32 in_val, u8 bits)
{
	u32 out_val = 0;
	u8 i;

	for (i = 0; i < bits; i++) {
		if (in_val & (1 << i))
			out_val |= 1 << (bits - 1 - i);
	}

	return out_val;
}

static unsigned int get_crc(struct crc_info type, const u8 *buf, u64 buf_len)
{
	u8 width = type.width;
	u32 poly = type.poly;
	u32 crc = type.crc_init;
	u32 xorout = type.xor_out;
	bool refin = type.refin;
	bool refout = type.refout;
	u8 n;
	u32 bits;
	u32 data;
	u8 i;

	n = (width < 8) ? 0 : (width - 8);
	crc = (width < 8) ? (crc << (8 - width)) : crc;
	bits = (width < 8) ? 0x80 : (1 << (width - 1));
	poly = (width < 8) ? (poly << (8 - width)) : poly;
	while (buf_len--) {
		data = *(buf++);
		if (refin)
			data = bits_reverse(data, 8);
		crc ^= (data << n);
		for (i = 0; i < 8; i++) {
			if (crc & bits)
				crc = (crc << 1) ^ poly;
			else
				crc = crc << 1;
		}
	}
	crc = (width < 8) ? (crc >> (8 - width)) : crc;
	if (refout)
		crc = bits_reverse(crc, width);
	crc ^= xorout;

	return (crc & ((2 << (width - 1)) - 1));
}

static u8 calculate_crc(struct lt9611uxd *lt9611uxd)
{
	struct crc_info type = {
		.width = 8,
		.poly = 0x31,
		.crc_init = 0,
		.xor_out = 0,
		.refout = false,
		.refin = false,
	};
	const u8 *upgrade_data;
	u64 len;
	u64 crc_size = FW_SIZE - 1;
	u8 default_val = 0xff;

	upgrade_data = lt9611uxd->fw->data;
	len = lt9611uxd->fw->size;

	type.crc_init = get_crc(type, upgrade_data, len);

	crc_size -= len;
	while (crc_size--)
		type.crc_init = get_crc(type, &default_val, 1);

	return type.crc_init;
}

static int i2c_read_write_flow(struct lt9611uxd *lt9611uxd, u8 *params,
			       unsigned int param_count, u8 *return_buffer,
			       unsigned int return_count)
{
	int count, i;
	unsigned int temp;

	regmap_write(lt9611uxd->regmap, 0xe0de, 0x01);

	count = 0;
	do {
		regmap_read(lt9611uxd->regmap, 0xe0ae, &temp);
		usleep_range(1000, 2000);
		count++;
	} while (count < 100 && temp != 0x01);

	if (temp != 0x01)
		return -1;

	for (i = 0; i < param_count; i++) {
		if (i > 0xdd - 0xb0)
			break;

		regmap_write(lt9611uxd->regmap, 0xe0b0 + i, params[i]);
	}

	regmap_write(lt9611uxd->regmap, 0xe0de, 0x02);

	count = 0;
	do {
		regmap_read(lt9611uxd->regmap, 0xe0ae, &temp);
		usleep_range(1000, 2000);
		count++;
	} while (count < 100 && temp != 0x02);

	if (temp != 0x02)
		return -2;

	regmap_bulk_read(lt9611uxd->regmap, 0xe085, return_buffer, return_count);

	return 0;
}

static int lt9611uxd_prepare_firmware_data(struct lt9611uxd *lt9611uxd)
{
	struct device *dev = lt9611uxd->dev;
	int ret;

	/* ensure filesystem ready */
	msleep(3000);
	ret = request_firmware(&lt9611uxd->fw, FW_FILE, dev);
	if (ret) {
		dev_err(dev, "failed load file '%s', error type %d\n", FW_FILE, ret);
		return ret;
	}

	if (lt9611uxd->fw->size > FW_SIZE - 1) {
		dev_err(dev, "firmware too large (%zu > %d)\n", lt9611uxd->fw->size, FW_SIZE - 1);
		lt9611uxd->fw = NULL;
		return -EINVAL;
	}

	dev_info(dev, "firmware size: %zu bytes\n", lt9611uxd->fw->size);

	lt9611uxd->fw_crc = calculate_crc(lt9611uxd);

	dev_info(dev, "LT9611C.bin crc: 0x%02x\n", lt9611uxd->fw_crc);

	return 0;
}

static void lt9611uxd_config_parameters(struct lt9611uxd *lt9611uxd)
{
	struct reg_sequence seq_write_paras[] = {
		REG_SEQ0(0xe0ee, 0x01),
		//fifo rst
		REG_SEQ0(0xe103, 0x3f),
		REG_SEQ0(0xe103, 0xff),

		REG_SEQ0(0xe05e, 0xc1),
		REG_SEQ0(0xe058, 0x00),
		REG_SEQ0(0xe059, 0x50),
		REG_SEQ0(0xe05a, 0x10),
		REG_SEQ0(0xe05a, 0x00),
		REG_SEQ0(0xe058, 0x21),
	};

	regmap_multi_reg_write(lt9611uxd->regmap, seq_write_paras, ARRAY_SIZE(seq_write_paras));
}

static void lt9611uxd_wren(struct lt9611uxd *lt9611uxd)
{
	regmap_write(lt9611uxd->regmap, 0xe05a, 0x04);
	regmap_write(lt9611uxd->regmap, 0xe05a, 0x00);
}

static void lt9611uxd_wrdi(struct lt9611uxd *lt9611uxd)
{
	regmap_write(lt9611uxd->regmap, 0xe05a, 0x08);
	regmap_write(lt9611uxd->regmap, 0xe05a, 0x00);
}

static void lt9611uxd_erase_op(struct lt9611uxd *lt9611uxd, u32 addr)
{
	struct reg_sequence seq_write[] = {
		REG_SEQ0(0xe0ee, 0x01),
		REG_SEQ0(0xe05a, 0x04),
		REG_SEQ0(0xe05a, 0x00),
		REG_SEQ0(0xe05b, (addr >> 16) & 0xff),
		REG_SEQ0(0xe05c, (addr >> 8) & 0xff),
		REG_SEQ0(0xe05d, addr & 0xff),
		REG_SEQ0(0xe05a, 0x01),
		REG_SEQ0(0xe05a, 0x00),
	};

	regmap_multi_reg_write(lt9611uxd->regmap, seq_write, ARRAY_SIZE(seq_write));
}

static void read_flash_reg_status(struct lt9611uxd *lt9611uxd, unsigned int *status)
{
	struct reg_sequence seq_write[] = {
		REG_SEQ0(0xe103, 0x3f),
		REG_SEQ0(0xe103, 0xff), //fifo rst

		REG_SEQ0(0xe05e, 0x40),
		REG_SEQ0(0xe056, 0x05),
		REG_SEQ0(0xe055, 0x25),
		REG_SEQ0(0xe055, 0x01),
		REG_SEQ0(0xe058, 0x21),
	};

	regmap_multi_reg_write(lt9611uxd->regmap, seq_write, ARRAY_SIZE(seq_write));

	regmap_read(lt9611uxd->regmap, 0xe05f, status);
}

static void lt9611uxd_crc_to_sram(struct lt9611uxd *lt9611uxd)
{
	struct reg_sequence seq_write[] = {
		REG_SEQ0(0xe051, 0x00),
		REG_SEQ0(0xe055, 0xc0),
		REG_SEQ0(0xe055, 0x80),
		REG_SEQ0(0xe05e, 0xc0),
		REG_SEQ0(0xe058, 0x21),
	};

	regmap_multi_reg_write(lt9611uxd->regmap, seq_write, ARRAY_SIZE(seq_write));
}

static void lt9611uxd_data_to_sram(struct lt9611uxd *lt9611uxd)
{
	struct reg_sequence seq_write[] = {
		REG_SEQ0(0xe051, 0xff),
		REG_SEQ0(0xe055, 0x80),
		REG_SEQ0(0xe05e, 0xc0),
		REG_SEQ0(0xe058, 0x21),
	};

	regmap_multi_reg_write(lt9611uxd->regmap, seq_write, ARRAY_SIZE(seq_write));
}

static void lt9611uxd_sram_to_flash(struct lt9611uxd *lt9611uxd, u64 addr)
{
	struct reg_sequence seq_write[] = {
		REG_SEQ0(0xe05b, (addr >> 16) & 0xff),
		REG_SEQ0(0xe05c, (addr >> 8) & 0xff),
		REG_SEQ0(0xe05d, addr & 0xff),
		REG_SEQ0(0xe05a, 0x30),
		REG_SEQ0(0xe05a, 0x00),
	};

	regmap_multi_reg_write(lt9611uxd->regmap, seq_write, ARRAY_SIZE(seq_write));
}

static void lt9611uxd_block_erase(struct lt9611uxd *lt9611uxd)
{
	struct device *dev = lt9611uxd->dev;
	u32 i = 0;
	unsigned int flash_status = 0;
	unsigned int block_num = 0x00;
	u32 flash_addr = 0x00;

	for (block_num = 0; block_num < 2; block_num++) {
		flash_addr = (block_num * 0x008000);
		lt9611uxd_erase_op(lt9611uxd, flash_addr);
		msleep(100);
		i = 0;
		while (1) {
			read_flash_reg_status(lt9611uxd, &flash_status);
			if ((flash_status & 0x01) == 0)
				break;

			if (i > 50)
				break;

			i++;
			msleep(50);
		}
	}

	dev_info(dev, "erase flash done.\n");
}

static int lt9611uxd_write_data(struct lt9611uxd *lt9611uxd, u64 addr)
{
	struct device *dev = lt9611uxd->dev;
	int ret;
	int page = 0, num = 0, i = 0;
	u64 size, index;
	const u8 *data;
	unsigned int value;

	data = lt9611uxd->fw->data;
	size = lt9611uxd->fw->size;
	page = (size + LT_PAGE_SIZE - 1) / LT_PAGE_SIZE;
	if (page * LT_PAGE_SIZE > FW_SIZE) {
		dev_err(dev, "firmware size out of range\n");
		return -EINVAL;
	}

	dev_info(dev, "%u pages, total size %llu byte\n", page, size);

	for (num = 0; num < page; num++) {
		lt9611uxd_data_to_sram(lt9611uxd);

		for (i = 0; i < LT_PAGE_SIZE; i++) {
			index = num * LT_PAGE_SIZE + i;
			value = (index < size) ? data[index] : 0xff;

			ret = regmap_write(lt9611uxd->regmap, 0xe059, value);
			if (ret < 0) {
				dev_err(dev, "write error at page %u, index %u\n", num, i);
				return ret;
			}
		}

		lt9611uxd_wren(lt9611uxd);
		lt9611uxd_sram_to_flash(lt9611uxd, addr);

		addr += LT_PAGE_SIZE;
	}

	lt9611uxd_wrdi(lt9611uxd);

	return 0;
}

static int lt9611uxd_write_crc(struct lt9611uxd *lt9611uxd, u64 addr)
{
	struct device *dev = lt9611uxd->dev;
	int ret;
	u8 crc;

	crc = lt9611uxd->fw_crc;
	lt9611uxd_crc_to_sram(lt9611uxd);
	ret = regmap_write(lt9611uxd->regmap, 0xe059, crc);
	if (ret < 0) {
		dev_err(dev, "failed to write CRC\n");
		return -1;
	}

	lt9611uxd_wren(lt9611uxd);
	lt9611uxd_sram_to_flash(lt9611uxd, addr);
	lt9611uxd_wrdi(lt9611uxd);

	dev_info(dev, "CRC 0x%02x written to flash at addr 0x%llx\n", crc, addr);

	return 0;
}

static int lt9611uxd_firmware_upgrade(struct lt9611uxd *lt9611uxd)
{
	struct device *dev = lt9611uxd->dev;
	int ret;

	dev_info(dev, "starting firmware upgrade, size: %zu bytes\n", lt9611uxd->fw->size);

	lt9611uxd_config_parameters(lt9611uxd);
	lt9611uxd_block_erase(lt9611uxd);

	ret = lt9611uxd_write_data(lt9611uxd, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to write firmware data\n");
		return ret;
	}

	ret = lt9611uxd_write_crc(lt9611uxd, FW_SIZE - 1);
	if (ret < 0) {
		dev_err(dev, "Failed to write firmware CRC\n");
		return ret;
	}

	return 0;
}

static int lt9611uxd_upgrade_result(struct lt9611uxd *lt9611uxd)
{
	struct device *dev = lt9611uxd->dev;
	unsigned int crc_result;

	regmap_write(lt9611uxd->regmap, 0xe0ee, 0x01);
	regmap_read(lt9611uxd->regmap, 0xe021, &crc_result);

	if (crc_result != lt9611uxd->fw_crc) {
		dev_err(dev, "LT9611C firmware upgrade failed, expected CRC=0x%02X, read CRC=0x%02X\n",
			lt9611uxd->fw_crc, crc_result);
		return -EIO;
	}

	dev_info(dev, "LT9611C firmware upgrade success, CRC=0x%02x\n", crc_result);
	return 0;
}

static struct lt9611uxd *connector_to_lt9611uxd(struct drm_connector *connector)
{
	return container_of(connector, struct lt9611uxd, connector);
}

static struct lt9611uxd *bridge_to_lt9611uxd(struct drm_bridge *bridge)
{
	return container_of(bridge, struct lt9611uxd, bridge);
}

static void lt9611uxd_lock(struct lt9611uxd *lt9611uxd)
{
	mutex_lock(&lt9611uxd->ocm_lock);
	regmap_write(lt9611uxd->regmap, 0xe0ee, 0x01);
}

static void lt9611uxd_unlock(struct lt9611uxd *lt9611uxd)
{
	regmap_write(lt9611uxd->regmap, 0xe0ee, 0x00);
	mutex_unlock(&lt9611uxd->ocm_lock);
}

static irqreturn_t lt9611uxd_irq_thread_handler(int irq, void *dev_id)
{
	struct lt9611uxd *lt9611uxd = dev_id;
	struct device *dev = lt9611uxd->dev;
	int ret;
	unsigned int irq_status;
	u8 cmd[5] = {0x52, 0x48, 0x31, 0x3a, 0x00};
	u8 data[5];

	regmap_read(lt9611uxd->regmap, 0xe084, &irq_status);
	if (!(irq_status & BIT(0)))
		return IRQ_HANDLED;

	mutex_lock(&lt9611uxd->ocm_lock);

	ret = i2c_read_write_flow(lt9611uxd, cmd, 5, data, 5);
	if (ret) {
		dev_err(dev, "failed to read HPD status\n");
	} else {
		lt9611uxd->hdmi_connected = (data[4] == 0x02);
		dev_info(dev, "HDMI %s\n", lt9611uxd->hdmi_connected ? "connected" : "disconnected");
	}

	lt9611uxd->audio_status = lt9611uxd->hdmi_connected ?
			connector_status_connected :
			connector_status_disconnected;

	schedule_work(&lt9611uxd->work);

	/*clear interrupt*/
	regmap_write(lt9611uxd->regmap, 0xe0df, irq_status & BIT(0));
	usleep_range(10000, 12000);
	regmap_write(lt9611uxd->regmap, 0xe0df, irq_status & (~BIT(0)));

	mutex_unlock(&lt9611uxd->ocm_lock);

	return IRQ_HANDLED;
}

static void lt9611uxd_hpd_work(struct work_struct *work)
{
	struct lt9611uxd *lt9611uxd = container_of(work, struct lt9611uxd, work);
	bool connected;

	mutex_lock(&lt9611uxd->ocm_lock);
	connected = lt9611uxd->hdmi_connected;
	mutex_unlock(&lt9611uxd->ocm_lock);

	drm_bridge_hpd_notify(&lt9611uxd->bridge, connected ?
			      connector_status_connected :
			      connector_status_disconnected);

	usleep_range(300, 500);
	lt9611uxd_audio_update_connector_status(lt9611uxd);
}

static void lt9611uxd_reset(struct lt9611uxd *lt9611uxd)
{
	gpiod_set_value_cansleep(lt9611uxd->reset_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(lt9611uxd->reset_gpio, 0);
	msleep(20);
	gpiod_set_value_cansleep(lt9611uxd->reset_gpio, 1);
	msleep(400);
}

static int lt9611uxd_regulator_init(struct lt9611uxd *lt9611uxd)
{
	struct device *dev = lt9611uxd->dev;
	int ret;

	lt9611uxd->supplies[0].supply = "vcc";
	lt9611uxd->supplies[1].supply = "vdd";

	ret = devm_regulator_bulk_get(dev, 2, lt9611uxd->supplies);

	return ret;
}

static int lt9611uxd_regulator_enable(struct lt9611uxd *lt9611uxd)
{
	int ret;

	ret = regulator_enable(lt9611uxd->supplies[0].consumer);
	if (ret < 0)
		return ret;

	usleep_range(5000, 10000);

	ret = regulator_enable(lt9611uxd->supplies[1].consumer);
	if (ret < 0) {
		regulator_disable(lt9611uxd->supplies[0].consumer);
		return ret;
	}

	return ret;
}

static int lt9611uxd_regulator_disable(struct lt9611uxd *lt9611uxd)
{
	int ret;

	ret = regulator_disable(lt9611uxd->supplies[0].consumer);
	if (ret < 0)
		return ret;

	usleep_range(5000, 10000);

	ret = regulator_disable(lt9611uxd->supplies[1].consumer);
	if (ret < 0)
		return ret;

	return 0;
}

static struct mipi_dsi_device *lt9611uxd_attach_dsi(struct lt9611uxd *lt9611uxd,
						  struct device_node *dsi_node)
{
	const struct mipi_dsi_device_info info = { "lt9611uxd", 0, NULL };
	struct mipi_dsi_device *dsi;
	struct mipi_dsi_host *host;
	struct device *dev = lt9611uxd->dev;
	int ret;

	host = of_find_mipi_dsi_host_by_node(dsi_node);
	if (!host)
		return ERR_PTR(dev_err_probe(dev, -EPROBE_DEFER, "failed to find dsi host\n"));

	dsi = devm_mipi_dsi_device_register_full(dev, host, &info);
	if (IS_ERR(dsi)) {
		dev_err(dev, "failed to create dsi device\n");
		return dsi;
	}

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_HSE;

	ret = devm_mipi_dsi_attach(dev, dsi);
	if (ret < 0) {
		dev_err(dev, "failed to attach dsi to host\n");
		return ERR_PTR(ret);
	}

	return dsi;
}

static int lt9611uxd_connector_get_modes(struct drm_connector *connector)
{
	struct lt9611uxd *lt9611uxd = connector_to_lt9611uxd(connector);
	const struct drm_edid *drm_edid;
	int count;

	drm_edid = drm_bridge_edid_read(&lt9611uxd->bridge, connector);
	drm_edid_connector_update(connector, drm_edid);
	count = drm_edid_connector_add_modes(connector);
	drm_edid_free(drm_edid);

	return count;
}

static enum drm_mode_status lt9611uxd_connector_mode_valid(struct drm_connector *connector,
							 struct drm_display_mode *mode)
{
	u32 Pixclk;
	//struct lt9611uxd *lt9611uxd = connector_to_lt9611uxd(connector);

	Pixclk = (mode->htotal * mode->vtotal * drm_mode_vrefresh(mode))/1000000;

	if((Pixclk >= 25) && (Pixclk <= 340))
		return MODE_OK;
	else
		return MODE_BAD;
}

static enum drm_connector_status lt9611uxd_connector_detect(struct drm_connector *connector,
							 bool force)
{
	struct lt9611uxd *lt9611uxd = connector_to_lt9611uxd(connector);

	return lt9611uxd->bridge.funcs->detect(&lt9611uxd->bridge);
}

static const struct drm_connector_helper_funcs lt9611uxd_bridge_connector_helper_funcs = {
	.get_modes = lt9611uxd_connector_get_modes,
	.mode_valid = lt9611uxd_connector_mode_valid,
};

static const struct drm_connector_funcs lt9611uxd_bridge_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = lt9611uxd_connector_detect,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int lt9611uxd_connector_init(struct drm_bridge *bridge, struct lt9611uxd *lt9611uxd)
{
	int ret;

	if (!bridge->encoder) {
		DRM_ERROR("lt9611uxd Parent encoder object not found");
		return -ENODEV;
	}

	lt9611uxd->connector.polled = DRM_CONNECTOR_POLL_HPD;

	drm_connector_helper_add(&lt9611uxd->connector,
				 &lt9611uxd_bridge_connector_helper_funcs);
	ret = drm_connector_init(bridge->dev, &lt9611uxd->connector,
				 &lt9611uxd_bridge_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		DRM_ERROR("lt9611uxd failed to initialize connector with drm\n");
		return ret;
	}

	return drm_connector_attach_encoder(&lt9611uxd->connector, bridge->encoder);
}


static int lt9611uxd_bridge_attach(struct drm_bridge *bridge,
				 enum drm_bridge_attach_flags flags)
{
	struct lt9611uxd *lt9611uxd = bridge_to_lt9611uxd(bridge);
	int ret;

	if (!(flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)) {
		ret = lt9611uxd_connector_init(bridge, lt9611uxd);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static enum drm_mode_status lt9611uxd_bridge_mode_valid(struct drm_bridge *bridge,
						      const struct drm_display_info *info,
						      const struct drm_display_mode *mode)
{
	u32 pixclk;

	pixclk = (mode->htotal * mode->vtotal * drm_mode_vrefresh(mode)) / 1000000;

	if (pixclk >= 25 && pixclk <= 340)
		return MODE_OK;
	else
		return MODE_BAD;
}

static void lt9611uxd_bridge_mode_set(struct drm_bridge *bridge,
				    const struct drm_display_mode *mode,
				    const struct drm_display_mode *adj_mode)
{
	struct lt9611uxd *lt9611uxd = bridge_to_lt9611uxd(bridge);
	struct device *dev = lt9611uxd->dev;
	int ret;
	u32 h_total, hactive, hsync_len, hfront_porch, hback_porch;
	u32 v_total, vactive, vsync_len, vfront_porch, vback_porch;
	u8 video_timing_set_cmd[26] = {0x57, 0x4d, 0x33, 0x3a};
	u8 return_timing_set_param[3];
	u8 framerate;
	u8 vic = 0x00;

	h_total = mode->htotal;
	hactive = mode->hdisplay;
	hsync_len = mode->hsync_end - mode->hsync_start;
	hfront_porch = mode->hsync_start - mode->hdisplay;
	hback_porch = mode->htotal - mode->hsync_end;

	v_total = mode->vtotal;
	vactive = mode->vdisplay;
	vsync_len = mode->vsync_end - mode->vsync_start;
	vfront_porch = mode->vsync_start - mode->vdisplay;
	vback_porch = mode->vtotal - mode->vsync_end;
	framerate = drm_mode_vrefresh(mode);
	vic = drm_match_cea_mode(mode);

	dev_info(dev, "hactive=%d, vactive=%d\n", hactive, vactive);
	dev_info(dev, "framerate=%d\n", framerate);
	dev_info(dev, "vic = 0x%02x\n", vic);

	video_timing_set_cmd[4] = (h_total >> 8) & 0xff;
	video_timing_set_cmd[5] = h_total & 0xff;
	video_timing_set_cmd[6] = (hactive >> 8) & 0xff;
	video_timing_set_cmd[7] = hactive & 0xff;
	video_timing_set_cmd[8] = (hfront_porch >> 8) & 0xff;
	video_timing_set_cmd[9] = hfront_porch & 0xff;
	video_timing_set_cmd[10] = (hsync_len >> 8) & 0xff;
	video_timing_set_cmd[11] = hsync_len & 0xff;
	video_timing_set_cmd[12] = (hback_porch >> 8) & 0xff;
	video_timing_set_cmd[13] = hback_porch & 0xff;
	video_timing_set_cmd[14] = (v_total >> 8) & 0xff;
	video_timing_set_cmd[15] = v_total & 0xff;
	video_timing_set_cmd[16] = (vactive >> 8) & 0xff;
	video_timing_set_cmd[17] = vactive & 0xFF;
	video_timing_set_cmd[18] = (vfront_porch >> 8) & 0xff;
	video_timing_set_cmd[19] = vfront_porch & 0xff;
	video_timing_set_cmd[20] = (vsync_len >> 8) & 0xff;
	video_timing_set_cmd[21] = vsync_len & 0xff;
	video_timing_set_cmd[22] = (vback_porch >> 8) & 0xff;
	video_timing_set_cmd[23] = vback_porch & 0xff;
	video_timing_set_cmd[24] = framerate;
	video_timing_set_cmd[25] = vic;

	mutex_lock(&lt9611uxd->ocm_lock);
	ret = i2c_read_write_flow(lt9611uxd, video_timing_set_cmd, 26, return_timing_set_param, 3);
	if (ret)
		dev_err(dev, "video set failed\n");
	mutex_unlock(&lt9611uxd->ocm_lock);
}

static enum drm_connector_status lt9611uxd_bridge_detect(struct drm_bridge *bridge)
{
	struct lt9611uxd *lt9611uxd = bridge_to_lt9611uxd(bridge);
	struct device *dev = lt9611uxd->dev;
	int ret;
	bool connected = false;
	u8 cmd[5] = {0x52, 0x48, 0x31, 0x3a, 0x00};
	u8 data[5];

	mutex_lock(&lt9611uxd->ocm_lock);
	ret = i2c_read_write_flow(lt9611uxd, cmd, 5, data, 5);
	if (ret)
		dev_err(dev, "Failed to read HPD status (err=%d)\n", ret);
	else
		connected = (data[4] == 0x02);

	lt9611uxd->hdmi_connected = connected;
	if (connected) {
		lt9611uxd->audio_status = connector_status_connected;
	} else {
		lt9611uxd->audio_status = connector_status_disconnected;
		lt9611uxd->edid_valid = 0;
	}

	mutex_unlock(&lt9611uxd->ocm_lock);

	return connected ? connector_status_connected :
				connector_status_disconnected;
}

static int lt9611uxd_read_edid_block(struct lt9611uxd *lt9611uxd, u8 *buf,
				     unsigned int block)
{
	struct device *dev = lt9611uxd->dev;
	u8 cmd[5] = {0x52, 0x48, 0x33, 0x3a, 0x00};
	u8 packet[37];
	int ret, i, offset = 0;

	for (i = 0; i < 4; i++) {
		cmd[4] = block * 4 + i;
		ret = i2c_read_write_flow(lt9611uxd, cmd, sizeof(cmd),
					  packet, sizeof(packet));
		if (ret) {
			dev_err(dev, "Failed to read EDID block %u packet %d\n",
				block, i);
			return ret;
		}

		memcpy(buf + offset, &packet[5], 32);
		offset += 32;
	}

	return 0;
}

static int lt9611uxd_get_edid_block(void *data, u8 *buf,
				  unsigned int block, size_t len)
{
	struct lt9611uxd *lt9611uxd = data;
	struct device *dev = lt9611uxd->dev;
	unsigned int i;
	int ret;

	if (len != LT9611UXD_EDID_BLOCK_LEN)
		return -EINVAL;

	if (block < LT9611UXD_EDID_BLOCKS && test_bit(block, &lt9611uxd->edid_valid)) {
		memcpy(buf, lt9611uxd->edid + block * LT9611UXD_EDID_BLOCK_LEN, len);
		return 0;
	}

	for (i = 0; i < LT9611UXD_EDID_TRIES; i++) {
		ret = lt9611uxd_read_edid_block(lt9611uxd, buf, block);
		if (ret)
			return ret;

		if (memchr_inv(buf, 0, len)) {
			if (block < LT9611UXD_EDID_BLOCKS) {
				memcpy(lt9611uxd->edid + block * LT9611UXD_EDID_BLOCK_LEN,
				       buf, len);
				set_bit(block, &lt9611uxd->edid_valid);
			}
			return 0;
		}

		msleep(LT9611UXD_EDID_INTERVAL_MS);
	}

	dev_err(dev, "EDID block %u still empty after %ums\n", block,
		i * LT9611UXD_EDID_INTERVAL_MS);

	return -EIO;
}

static const struct drm_edid *lt9611uxd_bridge_edid_read(struct drm_bridge *bridge,
						       struct drm_connector *connector)
{
	struct lt9611uxd *lt9611uxd = bridge_to_lt9611uxd(bridge);

	return drm_edid_read_custom(connector, lt9611uxd_get_edid_block, lt9611uxd);
}

static const struct drm_bridge_funcs lt9611uxd_bridge_funcs = {
	.attach = lt9611uxd_bridge_attach,
	.mode_valid = lt9611uxd_bridge_mode_valid,
	.mode_set = lt9611uxd_bridge_mode_set,
	.detect = lt9611uxd_bridge_detect,
	.edid_read = lt9611uxd_bridge_edid_read,
};

static int lt9611uxd_parse_dt(struct device *dev,
			    struct lt9611uxd *lt9611uxd)
{
	lt9611uxd->dsi0_node = of_graph_get_remote_node(dev->of_node, 0, -1);
	if (!lt9611uxd->dsi0_node) {
		dev_err(dev, "failed to get remote node for primary dsi\n");
		return -ENODEV;
	}

	lt9611uxd->dsi2_node = of_graph_get_remote_node(dev->of_node, 1, -1);

	return 0;
}

static int lt9611uxd_gpio_init(struct lt9611uxd *lt9611uxd)
{
	struct device *dev = lt9611uxd->dev;

	lt9611uxd->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(lt9611uxd->reset_gpio)) {
		dev_err(dev, "failed to acquire reset gpio\n");
		return PTR_ERR(lt9611uxd->reset_gpio);
	}

	return 0;
}

static int lt9611uxd_read_version(struct lt9611uxd *lt9611uxd)
{
	u8 buf[2];
	int ret;

	ret = regmap_write(lt9611uxd->regmap, 0xe0ee, 0x01);
	if (ret)
		return ret;

	ret = regmap_bulk_read(lt9611uxd->regmap, 0xe080, buf, 2);
	if (ret)
		return ret;

	return (buf[0] << 8) | buf[1];
}

static int lt9611uxd_read_chipid(struct lt9611uxd *lt9611uxd)
{
	struct device *dev = lt9611uxd->dev;
	u8 chipid[2];
	int ret;

	ret = regmap_write(lt9611uxd->regmap, 0xe0ee, 0x01);
	if (ret) {
		dev_err(dev, "Failed to write unlock register: %d\n", ret);
		return ret;
	}

	ret = regmap_bulk_read(lt9611uxd->regmap, 0xe100, chipid, 2);
	if (ret) {
		dev_err(dev, "Failed to read chip ID: %d\n", ret);
		return ret;
	}

	if (chipid[0] != 0x23 || chipid[1] != 0x06) {
		dev_err(dev, "ChipID: 0x%02x 0x%02x\n", chipid[0], chipid[1]);
		return -ENODEV;
	}
	return 0;
}

static int lt9611uxd_hdmi_hw_params(struct device *dev, void *data,
				  struct hdmi_codec_daifmt *fmt,
				 struct hdmi_codec_params *hparms)
{
	struct lt9611uxd *lt9611uxd = dev_get_drvdata(dev);

	 dev_info(lt9611uxd->dev, "SOC sample_rate: %d, sample_width: %d, fmt: %d\n",
		  hparms->sample_rate, hparms->sample_width, fmt->fmt);

	switch (hparms->sample_rate) {
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
	case 176400:
	case 192000:
		break;
	default:
		return -EINVAL;
	}

	switch (hparms->sample_width) {
	case 16:
	case 18:
	case 20:
	case 24:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt->fmt) {
	case HDMI_I2S:
	case HDMI_SPDIF:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void lt9611uxd_audio_shutdown(struct device *dev, void *data)
{
}

static int lt9611uxd_audio_startup(struct device *dev, void *data)
{
	return 0;
}

static void lt9611uxd_audio_update_connector_status(struct lt9611uxd *lt9611uxd)
{
	enum drm_connector_status status;

	status = lt9611uxd->audio_status;
	if (lt9611uxd->plugged_cb && lt9611uxd->codec_dev)
		lt9611uxd->plugged_cb(lt9611uxd->codec_dev,
				 status == connector_status_connected);
}

static int lt9611uxd_hdmi_audio_hook_plugged_cb(struct device *dev,
					      void *data,
					 hdmi_codec_plugged_cb fn,
					 struct device *codec_dev)
{
	struct lt9611uxd *lt9611uxd = data;

	lt9611uxd->plugged_cb = fn;
	lt9611uxd->codec_dev = codec_dev;
	lt9611uxd_audio_update_connector_status(lt9611uxd);

	return 0;
}

static const struct hdmi_codec_ops lt9611uxd_codec_ops = {
	.hw_params	= lt9611uxd_hdmi_hw_params,
	.audio_shutdown = lt9611uxd_audio_shutdown,
	.audio_startup = lt9611uxd_audio_startup,
	.hook_plugged_cb = lt9611uxd_hdmi_audio_hook_plugged_cb,
};

static int lt9611uxd_audio_init(struct device *dev, struct lt9611uxd *lt9611uxd)
{
	struct hdmi_codec_pdata codec_data = {
		.ops = &lt9611uxd_codec_ops,
		.max_i2s_channels = 8,
		.i2s = 1,
		.data = lt9611uxd,
	};

	lt9611uxd->audio_pdev =
		platform_device_register_data(dev, HDMI_CODEC_DRV_NAME,
					      PLATFORM_DEVID_AUTO,
					 &codec_data, sizeof(codec_data));

	return PTR_ERR_OR_ZERO(lt9611uxd->audio_pdev);
}

static void lt9611uxd_audio_exit(struct lt9611uxd *lt9611uxd)
{
	if (lt9611uxd->audio_pdev) {
		platform_device_unregister(lt9611uxd->audio_pdev);
		lt9611uxd->audio_pdev = NULL;
	}
}

static ssize_t lt9611uxd_firmware_store(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t len)
{
	struct lt9611uxd *lt9611uxd = dev_get_drvdata(dev);
	int ret;

	lt9611uxd_lock(lt9611uxd);
	ret = lt9611uxd_prepare_firmware_data(lt9611uxd);
	if (ret < 0) {
		dev_err(dev, "Failed prepare firmware data: %d\n", ret);
		goto out;
	}

	ret = lt9611uxd_firmware_upgrade(lt9611uxd);
	if (ret < 0) {
		dev_err(dev, "upgrade failure\n");
		goto out;
	}
	lt9611uxd_reset(lt9611uxd);
	ret = lt9611uxd_upgrade_result(lt9611uxd);
	if (ret < 0)
		goto out;

out:
	lt9611uxd_unlock(lt9611uxd);
	lt9611uxd_reset(lt9611uxd);
	if (lt9611uxd->fw) {
		release_firmware(lt9611uxd->fw);
		lt9611uxd->fw = NULL;
	}

	return ret < 0 ? ret : len;
}

static ssize_t lt9611uxd_firmware_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct lt9611uxd *lt9611uxd = dev_get_drvdata(dev);

	return sysfs_emit(buf, "0x%04x\n", lt9611uxd->fw_version);
}

static DEVICE_ATTR_RW(lt9611uxd_firmware);

static struct attribute *lt9611uxd_attrs[] = {
	&dev_attr_lt9611uxd_firmware.attr,
	NULL,
};

static const struct attribute_group lt9611uxd_attr_group = {
	.attrs = lt9611uxd_attrs,
};

static const struct attribute_group *lt9611uxd_attr_groups[] = {
	&lt9611uxd_attr_group,
	NULL,
};

static int lt9611uxd_main(void *data)
{
	struct lt9611uxd *lt9611uxd = data;
	struct device *dev = lt9611uxd->dev;
	struct i2c_client *client = lt9611uxd->client;
	int ret;
	unsigned int i;
	bool fw_updated = false;

	ret = lt9611uxd_parse_dt(dev, lt9611uxd);
	if (ret) {
		dev_err(dev, "failed to parse device tree\n");
		return ret;
	}

	ret = lt9611uxd_gpio_init(lt9611uxd);
	if (ret < 0)
		goto err_of_put;

	ret = lt9611uxd_regulator_init(lt9611uxd);
	if (ret < 0)
		goto err_of_put;

	ret = lt9611uxd_regulator_enable(lt9611uxd);
	if (ret)
		goto err_of_put;

	lt9611uxd_reset(lt9611uxd);

	ret = lt9611uxd_read_chipid(lt9611uxd);
	if (ret < 0) {
		dev_err(dev, "failed to read chip id.\n");
		goto err_disable_regulators;
	}

	lt9611uxd_lock(lt9611uxd);
retry:
	ret = lt9611uxd_read_version(lt9611uxd);
	if (ret < 0) {
		dev_err(dev, "failed to read FW version\n");
		lt9611uxd_unlock(lt9611uxd);
		goto err_disable_regulators;

	} else if (ret == 0) { /*Upgrade conditions*/
		if (!fw_updated) {
			fw_updated = true;
			ret = lt9611uxd_prepare_firmware_data(lt9611uxd);
			if (ret < 0) {
				lt9611uxd_unlock(lt9611uxd);
				goto err_disable_regulators;
			}

			ret = lt9611uxd_firmware_upgrade(lt9611uxd);
			if (ret < 0) {
				lt9611uxd_unlock(lt9611uxd);
				goto err_disable_regulators;
			}

			lt9611uxd_reset(lt9611uxd);

			ret = lt9611uxd_upgrade_result(lt9611uxd);
			if (ret < 0) {
				lt9611uxd_unlock(lt9611uxd);
				goto err_disable_regulators;
			} else {
				goto retry;
			}
		} else {
			dev_err(dev, "FW version 0x%04x, update failed\n", ret);
			ret = -EOPNOTSUPP;
			lt9611uxd_unlock(lt9611uxd);
			goto err_disable_regulators;
		}
	}

	if (lt9611uxd->fw) {
		release_firmware(lt9611uxd->fw);
		lt9611uxd->fw = NULL;
	}
	lt9611uxd_unlock(lt9611uxd);
	lt9611uxd->fw_version = ret;

	dev_info(dev, "current version:0x%04x", lt9611uxd->fw_version);

	INIT_WORK(&lt9611uxd->work, lt9611uxd_hpd_work);

	ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
					lt9611uxd_irq_thread_handler,
					IRQF_TRIGGER_FALLING |
					IRQF_ONESHOT |
					IRQF_NO_AUTOEN,
					"lt9611uxd", lt9611uxd);

	if (ret) {
		dev_err(dev, "failed to request irq\n");
		goto err_disable_regulators;
	}

	lt9611uxd_reset(lt9611uxd);

	for (i = 0; i < LT9611UXD_CMD_READY_TRIES; i++) {
		u8 hpd_cmd[5] = {0x52, 0x48, 0x31, 0x3a, 0x00};
		u8 hpd_data[5];

		mutex_lock(&lt9611uxd->ocm_lock);
		ret = i2c_read_write_flow(lt9611uxd, hpd_cmd, sizeof(hpd_cmd),
					  hpd_data, sizeof(hpd_data));
		mutex_unlock(&lt9611uxd->ocm_lock);
		if (!ret)
			break;

		msleep(LT9611UXD_CMD_READY_INTERVAL_MS);
	}

	if (ret)
		dev_warn(dev, "command interface not ready, HPD and EDID will not work\n");
	else
		dev_info(dev, "command interface ready after %ums\n",
			 i * LT9611UXD_CMD_READY_INTERVAL_MS);

	lt9611uxd->bridge.funcs = &lt9611uxd_bridge_funcs;
	lt9611uxd->bridge.of_node = lt9611uxd->client->dev.of_node;
	lt9611uxd->bridge.ops = DRM_BRIDGE_OP_DETECT | DRM_BRIDGE_OP_EDID | DRM_BRIDGE_OP_HPD;
	lt9611uxd->bridge.type = DRM_MODE_CONNECTOR_HDMIA;

	drm_bridge_add(&lt9611uxd->bridge);

	/* Attach primary DSI */
	lt9611uxd->dsi0 = lt9611uxd_attach_dsi(lt9611uxd, lt9611uxd->dsi0_node);
	if (IS_ERR(lt9611uxd->dsi0)) {
		ret = PTR_ERR(lt9611uxd->dsi0);
		goto err_remove_bridge;
	}

	/* Attach secondary DSI, if specified */
	if (lt9611uxd->dsi2_node) {
		lt9611uxd->dsi2 = lt9611uxd_attach_dsi(lt9611uxd, lt9611uxd->dsi2_node);
		if (IS_ERR(lt9611uxd->dsi2)) {
			ret = PTR_ERR(lt9611uxd->dsi2);
			goto err_remove_bridge;
		}
	}

	lt9611uxd->audio_status = connector_status_disconnected;

	ret = lt9611uxd_audio_init(dev, lt9611uxd);
	if (ret < 0) {
		dev_err(dev, "audio init failed\n");
		goto err_remove_bridge;
	}

	enable_irq(client->irq);

	return 0;

err_remove_bridge:
	cancel_work_sync(&lt9611uxd->work);
	drm_bridge_remove(&lt9611uxd->bridge);

err_disable_regulators:
	regulator_bulk_disable(ARRAY_SIZE(lt9611uxd->supplies), lt9611uxd->supplies);

err_of_put:
	of_node_put(lt9611uxd->dsi2_node);
	of_node_put(lt9611uxd->dsi0_node);

	if (lt9611uxd->fw) {
		release_firmware(lt9611uxd->fw);
		lt9611uxd->fw = NULL;
	}

	return ret;
}

static int lt9611uxd_probe(struct i2c_client *client)
{
	struct lt9611uxd *lt9611uxd;
	struct device *dev = &client->dev;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "device doesn't support I2C\n");
		return -ENODEV;
	}

	lt9611uxd = devm_kzalloc(dev, sizeof(*lt9611uxd), GFP_KERNEL);
	if (!lt9611uxd)
		return -ENOMEM;

	lt9611uxd->dev = dev;
	lt9611uxd->client = client;
	mutex_init(&lt9611uxd->ocm_lock);

	lt9611uxd->regmap = devm_regmap_init_i2c(client, &lt9611uxd_regmap_config);
	if (IS_ERR(lt9611uxd->regmap)) {
		dev_err(dev, "regmap i2c init failed\n");
		return PTR_ERR(lt9611uxd->regmap);
	}

	i2c_set_clientdata(client, lt9611uxd);

	//Use kthread to ensure that the firmware upgrade does
	//not block the initialization of the file system.
	lt9611uxd->kthread = kthread_run(lt9611uxd_main, lt9611uxd, "lt9611uxd");
	if (IS_ERR(lt9611uxd->kthread)) {
		dev_err(dev, "Failed to create kernel thread\n");
		return PTR_ERR(lt9611uxd->kthread);
	}

	return 0;
}

static void lt9611uxd_remove(struct i2c_client *client)
{
	struct lt9611uxd *lt9611uxd = i2c_get_clientdata(client);

	disable_irq(client->irq);
	cancel_work_sync(&lt9611uxd->work);
	lt9611uxd_audio_exit(lt9611uxd);
	drm_bridge_remove(&lt9611uxd->bridge);
	mutex_destroy(&lt9611uxd->ocm_lock);
	regulator_bulk_disable(ARRAY_SIZE(lt9611uxd->supplies), lt9611uxd->supplies);
	of_node_put(lt9611uxd->dsi2_node);
	of_node_put(lt9611uxd->dsi0_node);
}

static int lt9611uxd_bridge_suspend(struct device *dev)
{
	struct lt9611uxd *lt9611uxd = dev_get_drvdata(dev);
	int ret;

	dev_info(lt9611uxd->dev, "suspend\n");
	disable_irq(lt9611uxd->client->irq);
	ret = lt9611uxd_regulator_disable(lt9611uxd);
	gpiod_set_value_cansleep(lt9611uxd->reset_gpio, 0);

	return ret;
}

static int lt9611uxd_bridge_resume(struct device *dev)
{
	struct lt9611uxd *lt9611uxd = dev_get_drvdata(dev);
	int ret;

	ret = lt9611uxd_regulator_enable(lt9611uxd);
	lt9611uxd_reset(lt9611uxd);
	enable_irq(lt9611uxd->client->irq);
	dev_info(lt9611uxd->dev, "resume\n");

	return ret;
}

static const struct dev_pm_ops lt9611uxd_bridge_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lt9611uxd_bridge_suspend,
				lt9611uxd_bridge_resume)
};

static struct i2c_device_id lt9611uxd_id[] = {
	{ "lontium,lt9611uxd", 0 },
	{ /* sentinel */ }
};

static const struct of_device_id lt9611uxd_match_table[] = {
	{ .compatible = "lontium,lt9611uxd" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lt9611uxd_match_table);

static struct i2c_driver lt9611uxd_driver = {
	.driver = {
		.name = "lt9611uxd",
		.of_match_table = lt9611uxd_match_table,
		.pm = &lt9611uxd_bridge_pm_ops,
		.dev_groups = lt9611uxd_attr_groups,
	},
	.probe = lt9611uxd_probe,
	.remove = lt9611uxd_remove,
	.id_table = lt9611uxd_id,
};
module_i2c_driver(lt9611uxd_driver);

MODULE_AUTHOR("sunyun yang <syyang@lontium.com>");
MODULE_LICENSE("GPL v2");

MODULE_FIRMWARE(FW_FILE);
