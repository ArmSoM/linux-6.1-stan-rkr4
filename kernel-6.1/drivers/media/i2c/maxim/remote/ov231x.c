// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim Remote Sensor OmniVision OV2311/OV2312 driver
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/compat.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>

#include "maxim_remote.h"

#define DRIVER_VERSION			KERNEL_VERSION(1, 0x00, 0x00)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define OV231X_NAME			"ov231x"

#define OV231X_XVCLK_FREQ		24000000
#define OV231X_MIPI_FREQ_148M		148000000

#define OV231X_I2C_ADDR_DEF		0x30

#define REG_NULL			0xFFFF
#define OV231X_REG_VALUE_08BIT		1

struct i2c_regval {
	u16 reg_addr;
	u8 reg_val;
};

struct ov231x_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 link_freq_idx;
	u32 bpp;
	const struct i2c_regval *reg_list;
};

struct ov231x {
	struct i2c_client *client;
	struct regulator *poc_regulator;

	struct mutex mutex;

	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *anal_gain;
	struct v4l2_ctrl *digi_gain;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *test_pattern;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *h_flip;
	struct v4l2_ctrl *v_flip;
	struct v4l2_fwnode_endpoint bus_cfg;

	bool streaming;
	bool power_on;
	bool hot_plug;
	u8 is_reset;

	const struct ov231x_mode *supported_modes;
	const struct ov231x_mode *cur_mode;
	u32 cfg_modes_num;

	u32 module_index;
	const char *module_facing;
	const char *module_name;
	const char *len_name;

	u8 cam_i2c_addr_def;
	u8 cam_i2c_addr_map;

	struct maxim_remote_ser *remote_ser;
};

static const struct i2c_regval ov231x_1600x1300_regs[] = {
	{ REG_NULL, 0x00 },
};

/*
 * The width and height must be configured to be
 * the same as the current output resolution of the sensor.
 * The input width of the isp needs to be 16 aligned.
 * The input height of the isp needs to be 8 aligned.
 * If the width or height does not meet the alignment rules,
 * you can configure the cropping parameters with the following function to
 * crop out the appropriate resolution.
 * struct v4l2_subdev_pad_ops {
 *	.get_selection
 * }
 */
static const struct ov231x_mode supported_modes[] = {
	{
		.width = 1600,
		.height = 1300,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.link_freq_idx = 0,
		.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
		.bpp = 16,
		.reg_list = ov231x_1600x1300_regs,
	}
};

static const s64 link_freq_menu_items[] = {
	OV231X_MIPI_FREQ_148M,
};

/* Write registers up to 4 at a time */
static int __maybe_unused ov231x_i2c_write_reg(struct i2c_client *client,
					u16 reg_addr, u32 val_len, u32 reg_val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	dev_info(&client->dev, "i2c addr(0x%02x) write: 0x%04x = 0x%08x (%d)\n",
			client->addr, reg_addr, reg_val, val_len);

	if (val_len > 4)
		return -EINVAL;

	buf[0] = reg_addr >> 8;
	buf[1] = reg_addr & 0xff;
	buf_i = 2;

	val_be = cpu_to_be32(reg_val);
	val_p = (u8 *)&val_be;
	val_i = 4 - val_len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, (val_len + 2)) != (val_len + 2)) {
		dev_err(&client->dev,
			"%s: writing register 0x%04x from 0x%02x failed\n",
			__func__, reg_addr, client->addr);
		return -EIO;
	}

	return 0;
}

/* Read registers up to 4 at a time */
static int __maybe_unused ov231x_i2c_read_reg(struct i2c_client *client,
					u16 reg_addr, u32 val_len, u32 *reg_val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg_addr);
	u8 *reg_be_p;
	int ret;

	if (val_len > 4 || !val_len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	reg_be_p = (u8 *)&reg_addr_be;

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = reg_be_p;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = val_len;
	msgs[1].buf = &data_be_p[4 - val_len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev,
			"%s: reading register 0x%04x from 0x%02x failed\n",
			__func__, reg_addr, client->addr);
		return -EIO;
	}

	*reg_val = be32_to_cpu(data_be);

#if 0
	dev_info(&client->dev, "i2c addr(0x%02x) read: 0x%04x = 0x%08x (%d)\n",
		client->addr, reg_addr, *reg_val, val_len);
#endif

	return 0;
}

static int __maybe_unused ov231x_i2c_write_array(struct i2c_client *client,
						const struct i2c_regval *regs)
{
	u32 i = 0;
	int ret = 0;

	for (i = 0; (ret == 0) && (regs[i].reg_addr != REG_NULL); i++) {
		ret = ov231x_i2c_write_reg(client, regs[i].reg_addr,
				OV231X_REG_VALUE_08BIT, regs[i].reg_val);
	}

	return ret;
}

static int __ov231x_power_on(struct ov231x *ov231x)
{
	struct device *dev = &ov231x->client->dev;
	int ret = 0;

	dev_info(dev, "ov231x device power on\n");

	ret = regulator_enable(ov231x->poc_regulator);
	if (ret < 0) {
		dev_err(dev, "Unable to turn PoC regulator on\n");
		return ret;
	}

	return 0;
}

static void __ov231x_power_off(struct ov231x *ov231x)
{
	struct device *dev = &ov231x->client->dev;
	int ret = 0;

	dev_info(dev, "ov231x device power off\n");

	ret = regulator_disable(ov231x->poc_regulator);
	if (ret < 0)
		dev_warn(dev, "Unable to turn PoC regulator off\n");
}

static int ov231x_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);
	int ret = 0;

	ret = __ov231x_power_on(ov231x);

	return ret;
}

static int ov231x_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);

	__ov231x_power_off(ov231x);

	return 0;
}

static const struct dev_pm_ops ov231x_pm_ops = {
	SET_RUNTIME_PM_OPS(
		ov231x_runtime_suspend, ov231x_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int ov231x_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
#else
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->pad, 0);
#endif
	const struct ov231x_mode *def_mode = &ov231x->supported_modes[0];

	mutex_lock(&ov231x->mutex);

	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&ov231x->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int ov231x_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);
	struct i2c_client *client = ov231x->client;
	int ret = 0;

	mutex_lock(&ov231x->mutex);

	/* If the power state is not modified - no work to do. */
	if (ov231x->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ov231x->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		ov231x->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&ov231x->mutex);

	return ret;
}

static void ov231x_get_module_inf(struct ov231x *ov231x,
					struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, OV231X_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, ov231x->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, ov231x->len_name, sizeof(inf->base.lens));
}

static void ov231x_get_vicap_rst_inf(struct ov231x *ov231x,
			struct rkmodule_vicap_reset_info *rst_info)
{
	struct i2c_client *client = ov231x->client;

	rst_info->is_reset = ov231x->hot_plug;
	ov231x->hot_plug = false;
	rst_info->src = RKCIF_RESET_SRC_ERR_HOTPLUG;

	dev_info(&client->dev, "%s: rst_info->is_reset:%d.\n",
		__func__, rst_info->is_reset);
}

static void ov231x_set_vicap_rst_inf(struct ov231x *ov231x,
				struct rkmodule_vicap_reset_info rst_info)
{
	ov231x->is_reset = rst_info.is_reset;
}

static long ov231x_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);
	long ret = 0;

	dev_dbg(&ov231x->client->dev, "ioctl cmd = 0x%08x\n", cmd);

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		ov231x_get_module_inf(ov231x, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_VICAP_RST_INFO:
		ov231x_get_vicap_rst_inf(ov231x,
			(struct rkmodule_vicap_reset_info *)arg);
		break;
	case RKMODULE_SET_VICAP_RST_INFO:
		ov231x_set_vicap_rst_inf(ov231x,
			*(struct rkmodule_vicap_reset_info *)arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ov231x_compat_ioctl32(struct v4l2_subdev *sd, unsigned int cmd,
					unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_vicap_reset_info *vicap_rst_inf;
	long ret = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = ov231x_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_GET_VICAP_RST_INFO:
		vicap_rst_inf = kzalloc(sizeof(*vicap_rst_inf), GFP_KERNEL);
		if (!vicap_rst_inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = ov231x_ioctl(sd, cmd, vicap_rst_inf);
		if (!ret) {
			ret = copy_to_user(up, vicap_rst_inf, sizeof(*vicap_rst_inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(vicap_rst_inf);
		break;
	case RKMODULE_SET_VICAP_RST_INFO:
		vicap_rst_inf = kzalloc(sizeof(*vicap_rst_inf), GFP_KERNEL);
		if (!vicap_rst_inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(vicap_rst_inf, up, sizeof(*vicap_rst_inf));
		if (!ret)
			ret = ov231x_ioctl(sd, cmd, vicap_rst_inf);
		else
			ret = -EFAULT;
		kfree(vicap_rst_inf);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif /* CONFIG_COMPAT */

static int __ov231x_start_stream(struct ov231x *ov231x)
{
	maxim_remote_ser_t *remote_ser = ov231x->remote_ser;
	struct i2c_client *client = ov231x->client;
	struct device *dev = &client->dev;
	int ret = 0;

	if (remote_ser == NULL) {
		dev_err(dev, "%s: remote_ser error\n", __func__);
		return -EINVAL;
	}

	if (remote_ser->ser_ops == NULL) {
		dev_err(dev, "%s: remote_ser ser_ops error\n", __func__);
		return -EINVAL;
	}

	ret = remote_ser->ser_ops->ser_module_init(remote_ser);
	if (ret) {
		dev_err(dev, "%s: remote_ser module_init error\n", __func__);
		return ret;
	}

	ret = ov231x_i2c_write_array(client, ov231x->cur_mode->reg_list);
	if (ret) {
		dev_err(dev, "%s: ov231x i2c write array error\n", __func__);
		return ret;
	}

	/* In case these controls are set before streaming */
	mutex_unlock(&ov231x->mutex);
	ret = v4l2_ctrl_handler_setup(&ov231x->ctrl_handler);
	mutex_lock(&ov231x->mutex);
	if (ret)
		return ret;

	ret = remote_ser->ser_ops->ser_pclk_detect(remote_ser);
	if (ret) {
		dev_err(dev, "%s: remote_ser pclk_detect error\n", __func__);
		return ret;
	}

	return 0;
}

static int __ov231x_stop_stream(struct ov231x *ov231x)
{
	maxim_remote_ser_t *remote_ser = ov231x->remote_ser;
	struct device *dev = &ov231x->client->dev;
	int ret = 0;

	if (remote_ser == NULL) {
		dev_err(dev, "%s: remote_ser error\n", __func__);
		return -EINVAL;
	}

	if (remote_ser->ser_ops == NULL) {
		dev_err(dev, "%s: remote_ser ser_ops error\n", __func__);
		return -EINVAL;
	}

	ret = remote_ser->ser_ops->ser_module_deinit(remote_ser);
	if (ret) {
		dev_err(dev, "%s: remote_ser module_deinit error\n", __func__);
		return ret;
	}

	return 0;
}

static int ov231x_s_stream(struct v4l2_subdev *sd, int on)
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);
	struct i2c_client *client = ov231x->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on = %d\n", __func__, on);

	mutex_lock(&ov231x->mutex);
	on = !!on;
	if (on == ov231x->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __ov231x_start_stream(ov231x);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__ov231x_stop_stream(ov231x);
		pm_runtime_put(&client->dev);
	}

	ov231x->streaming = on;

unlock_and_return:
	mutex_unlock(&ov231x->mutex);

	return ret;
}

static int ov231x_g_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *fi)
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);
	const struct ov231x_mode *mode = ov231x->cur_mode;

	mutex_lock(&ov231x->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&ov231x->mutex);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int ov231x_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_mbus_code_enum *code)
#else
static int ov231x_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = ov231x->cur_mode->bus_fmt;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int ov231x_enum_frame_sizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_size_enum *fse)
#else
static int ov231x_enum_frame_sizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);

	if (fse->index >= ov231x->cfg_modes_num)
		return -EINVAL;

	if (fse->code != ov231x->supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = ov231x->supported_modes[fse->index].width;
	fse->max_width  = ov231x->supported_modes[fse->index].width;
	fse->max_height = ov231x->supported_modes[fse->index].height;
	fse->min_height = ov231x->supported_modes[fse->index].height;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int ov231x_enum_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_frame_interval_enum *fie)
#else
static int ov231x_enum_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_frame_interval_enum *fie)
#endif
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);

	if (fie->index >= ov231x->cfg_modes_num)
		return -EINVAL;

	fie->code = ov231x->supported_modes[fie->index].bus_fmt;
	fie->width = ov231x->supported_modes[fie->index].width;
	fie->height = ov231x->supported_modes[fie->index].height;
	fie->interval = ov231x->supported_modes[fie->index].max_fps;

	return 0;
}

static int ov231x_get_reso_dist(const struct ov231x_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct ov231x_mode *
ov231x_find_best_fit(struct ov231x *ov231x, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ov231x->cfg_modes_num; i++) {
		dist = ov231x_get_reso_dist(&ov231x->supported_modes[i], framefmt);
		if ((cur_best_fit_dist == -1 || dist < cur_best_fit_dist) &&
			(ov231x->supported_modes[i].bus_fmt == framefmt->code)) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &ov231x->supported_modes[cur_best_fit];
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int ov231x_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *fmt)
#else
static int ov231x_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);
	struct device *dev = &ov231x->client->dev;
	const struct ov231x_mode *mode;
	u64 link_freq = 0, pixel_rate = 0;
	u8 data_lanes = ov231x->bus_cfg.bus.mipi_csi2.num_data_lanes;

	mutex_lock(&ov231x->mutex);

	mode = ov231x_find_best_fit(ov231x, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
	#else
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
	#endif
#else
		mutex_unlock(&ov231x->mutex);
		return -ENOTTY;
#endif
	} else {
		ov231x->cur_mode = mode;

		__v4l2_ctrl_s_ctrl(ov231x->link_freq, mode->link_freq_idx);

		/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		link_freq = link_freq_menu_items[mode->link_freq_idx];
		pixel_rate = (u32)link_freq / mode->bpp * 2 * data_lanes;
		__v4l2_ctrl_s_ctrl_int64(ov231x->pixel_rate, pixel_rate);

		dev_info(dev, "mipi_freq_idx = %d, mipi_link_freq = %lld\n",
					mode->link_freq_idx, link_freq);
		dev_info(dev, "pixel_rate = %lld, bpp = %d\n",
					pixel_rate, mode->bpp);
	}

	mutex_unlock(&ov231x->mutex);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int ov231x_get_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *fmt)
#else
static int ov231x_get_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);
	const struct ov231x_mode *mode = ov231x->cur_mode;

	mutex_lock(&ov231x->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
	#else
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
	#endif
#else
		mutex_unlock(&ov231x->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&ov231x->mutex);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int ov231x_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
#else
static int ov231x_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
#endif
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = 0;
		sel->r.width = ov231x->cur_mode->width;
		sel->r.top = 0;
		sel->r.height = ov231x->cur_mode->height;
		return 0;
	}

	return -EINVAL;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int ov231x_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_config *config)
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2 = ov231x->bus_cfg.bus.mipi_csi2;

	return 0;
}
#elif KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static int ov231x_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_config *config)
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);
	u32 val = 0;
	u8 data_lanes = ov231x->bus_cfg.bus.mipi_csi2.num_data_lanes;

	val |= V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	val |= (1 << (data_lanes - 1));

	val |= V4L2_MBUS_CSI2_CHANNEL_3 | V4L2_MBUS_CSI2_CHANNEL_2 |
	       V4L2_MBUS_CSI2_CHANNEL_1 | V4L2_MBUS_CSI2_CHANNEL_0;

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->flags = val;

	return 0;
}
#else
static int ov231x_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *config)
{
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);
	u32 val = 0;
	u8 data_lanes = ov231x->bus_cfg.bus.mipi_csi2.num_data_lanes;

	val |= V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	val |= (1 << (data_lanes - 1));

	val |= V4L2_MBUS_CSI2_CHANNEL_3 | V4L2_MBUS_CSI2_CHANNEL_2 |
	       V4L2_MBUS_CSI2_CHANNEL_1 | V4L2_MBUS_CSI2_CHANNEL_0;

	config->type = V4L2_MBUS_CSI2;
	config->flags = val;

	return 0;
}
#endif /* LINUX_VERSION_CODE */

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops ov231x_internal_ops = {
	.open = ov231x_open,
};
#endif

static const struct v4l2_subdev_core_ops ov231x_core_ops = {
	.s_power = ov231x_s_power,
	.ioctl = ov231x_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = ov231x_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops ov231x_video_ops = {
	.s_stream = ov231x_s_stream,
	.g_frame_interval = ov231x_g_frame_interval,
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
	.g_mbus_config = ov231x_g_mbus_config,
#endif
};

static const struct v4l2_subdev_pad_ops ov231x_pad_ops = {
	.enum_mbus_code = ov231x_enum_mbus_code,
	.enum_frame_size = ov231x_enum_frame_sizes,
	.enum_frame_interval = ov231x_enum_frame_interval,
	.get_fmt = ov231x_get_fmt,
	.set_fmt = ov231x_set_fmt,
	.get_selection = ov231x_get_selection,
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	.get_mbus_config = ov231x_g_mbus_config,
#endif
};

static const struct v4l2_subdev_ops ov231x_subdev_ops = {
	.core	= &ov231x_core_ops,
	.video	= &ov231x_video_ops,
	.pad	= &ov231x_pad_ops,
};

static int ov231x_initialize_controls(struct ov231x *ov231x)
{
	struct device *dev = &ov231x->client->dev;
	const struct ov231x_mode *mode;
	struct v4l2_ctrl_handler *handler;
	u64 link_freq = 0, pixel_rate = 0;
	u8 data_lanes;

	int ret = 0;

	handler = &ov231x->ctrl_handler;
	mode = ov231x->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 2);
	if (ret)
		return ret;

	handler->lock = &ov231x->mutex;

	/* ctrl handler: link freq */
	ov231x->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_menu_items) - 1, 0,
				link_freq_menu_items);
	__v4l2_ctrl_s_ctrl(ov231x->link_freq, mode->link_freq_idx);
	link_freq = link_freq_menu_items[mode->link_freq_idx];
	dev_info(dev, "mipi_freq_idx = %d, mipi_link_freq = %lld\n",
				mode->link_freq_idx, link_freq);

	/* ctrl handler: pixel rate */
	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	data_lanes = ov231x->bus_cfg.bus.mipi_csi2.num_data_lanes;
	pixel_rate = (u32)link_freq / mode->bpp * 2 * data_lanes;

	ov231x->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
					V4L2_CID_PIXEL_RATE,
					0, pixel_rate, 1, pixel_rate);
	dev_info(dev, "pixel_rate = %lld, bpp = %d\n", pixel_rate, mode->bpp);

	if (handler->error) {
		ret = handler->error;
		dev_err(&ov231x->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	ov231x->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int ov231x_parse_dt(struct ov231x *ov231x)
{
	struct device *dev = &ov231x->client->dev;
	struct device_node *of_node = dev->of_node;
	u32 value = 0;
	int ret = 0;

	dev_info(dev, "=== ov231x parse dt ===\n");

	ret = of_property_read_u32(of_node, "cam-i2c-addr-def", &value);
	if (ret == 0) {
		dev_info(dev, "cam-i2c-addr-def property: 0x%x", value);
		ov231x->cam_i2c_addr_def = value;
	} else {
		ov231x->cam_i2c_addr_def = OV231X_I2C_ADDR_DEF;
	}

	return 0;
}

static int ov231x_mipi_data_lanes_parse(struct ov231x *ov231x)
{
	struct device *dev = &ov231x->client->dev;
	struct device_node *endpoint = NULL;
	u8 mipi_data_lanes;
	int ret = 0;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
		&ov231x->bus_cfg);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}
	mipi_data_lanes = ov231x->bus_cfg.bus.mipi_csi2.num_data_lanes;
	dev_info(dev, "mipi csi2 phy data lanes = %d\n", mipi_data_lanes);

	return 0;
}

static int ov231x_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct ov231x *ov231x = NULL;
	struct v4l2_subdev *sd = NULL;
	maxim_remote_ser_t *remote_ser = NULL;
	char facing[2];
	int ret = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x", DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8, DRIVER_VERSION & 0x00ff);

	ov231x = devm_kzalloc(dev, sizeof(*ov231x), GFP_KERNEL);
	if (!ov231x) {
		dev_err(dev, "ov231x probe no memory error\n");
		return -ENOMEM;
	}

	ov231x->client = client;
	ov231x->cam_i2c_addr_map = client->addr;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &ov231x->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &ov231x->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &ov231x->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &ov231x->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	// poc regulator
	ov231x->poc_regulator = devm_regulator_get(dev, "poc");
	if (IS_ERR(ov231x->poc_regulator)) {
		if (PTR_ERR(ov231x->poc_regulator) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get PoC regulator (%ld)\n",
				PTR_ERR(ov231x->poc_regulator));
		else
			dev_err(dev, "Get PoC regulator deferred\n");

		ret = PTR_ERR(ov231x->poc_regulator);

		return ret;
	}

	ov231x_mipi_data_lanes_parse(ov231x);
	ov231x->supported_modes = supported_modes;
	ov231x->cfg_modes_num = ARRAY_SIZE(supported_modes);
	ov231x->cur_mode = &ov231x->supported_modes[0];

	mutex_init(&ov231x->mutex);

	ret = __ov231x_power_on(ov231x);
	if (ret)
		goto err_destroy_mutex;

	pm_runtime_set_active(dev);
	pm_runtime_get_noresume(dev);
	pm_runtime_enable(dev);

	sd = &ov231x->subdev;
	v4l2_i2c_subdev_init(sd, client, &ov231x_subdev_ops);
	ret = ov231x_initialize_controls(ov231x);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &ov231x_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

#if defined(CONFIG_MEDIA_CONTROLLER)
	ov231x->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &ov231x->pad);
	if (ret < 0)
		goto err_free_handler;
#endif

	v4l2_set_subdevdata(sd, ov231x);

	memset(facing, 0, sizeof(facing));
	if (strcmp(ov231x->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 ov231x->module_index, facing, OV231X_NAME,
		 dev_name(sd->dev));

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	ret = v4l2_async_register_subdev_sensor(sd);
#else
	ret = v4l2_async_register_subdev_sensor_common(sd);
#endif
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	ov231x_parse_dt(ov231x);

	/* remote serializer bind */
	ov231x->remote_ser = NULL;
	remote_ser = maxim_remote_cam_bind_ser(dev);
	if (remote_ser != NULL) {
		dev_info(dev, "remote serializer bind success\n");

		remote_ser->cam_i2c_addr_def = ov231x->cam_i2c_addr_def;
		remote_ser->cam_i2c_addr_map = ov231x->cam_i2c_addr_map;

		ov231x->remote_ser = remote_ser;
	} else {
		dev_err(dev, "remote serializer bind fail\n");
	}

	pm_runtime_set_autosuspend_delay(dev, 1000);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif

err_free_handler:
	v4l2_ctrl_handler_free(&ov231x->ctrl_handler);

err_power_off:
	pm_runtime_disable(dev);
	pm_runtime_put_noidle(dev);
	__ov231x_power_off(ov231x);

err_destroy_mutex:
	mutex_destroy(&ov231x->mutex);

	return ret;
}

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
static int ov231x_remove(struct i2c_client *client)
#else
static void ov231x_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov231x *ov231x = v4l2_get_subdevdata(sd);

	v4l2_async_unregister_subdev(sd);

#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&ov231x->ctrl_handler);

	mutex_destroy(&ov231x->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__ov231x_power_off(ov231x);
	pm_runtime_set_suspended(&client->dev);

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static const struct of_device_id ov231x_of_match[] = {
	{ .compatible = "maxim,ovti,ov2311" },
	{ .compatible = "maxim,ovti,ov2312" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, ov231x_of_match);

static struct i2c_driver ov231x_i2c_driver = {
	.driver = {
		.name = OV231X_NAME,
		.pm = &ov231x_pm_ops,
		.of_match_table = of_match_ptr(ov231x_of_match),
	},
	.probe		= &ov231x_probe,
	.remove		= &ov231x_remove,
};

module_i2c_driver(ov231x_i2c_driver);

MODULE_AUTHOR("Cai Wenzhong <cwz@rock-chips.com>");
MODULE_DESCRIPTION("Maxim Remote Sensor OmniVision ov2311/ov2312 Driver");
MODULE_LICENSE("GPL");
