/*
 * sound\soc\sunxi\sunxi_ahub.c
 * (C) Copyright 2015-2017
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Wolfgang huang <huangjinhui@allwinnertechtech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "sunxi_ahub.h"

#define DRV_NAME	"sunxi-ahub"

struct sunxi_ahub_priv {
	struct device *dev;
	struct regmap *regmap;
	struct clk *pllclk;
	struct clk *moduleclk;
} sunxi_ahub_dev;

struct sunxi_ahub_priv *sunxi_ahub = &sunxi_ahub_dev;

static const struct regmap_config sunxi_ahub_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = SUNXI_AHUB_DAM_GAIN_CTL7(1),
	.cache_type = REGCACHE_RBTREE,
};

struct regmap *sunxi_ahub_regmap_init(struct platform_device *pdev)
{
	struct resource res, *memregion;
	void __iomem *sunxi_ahub_membase;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	if (!sunxi_ahub->regmap) {
		ret = of_address_to_resource(np, 0, &res);
		if (ret)
			return NULL;

		memregion = devm_request_mem_region(&pdev->dev, res.start,
						resource_size(&res), DRV_NAME);
		if (!memregion)
			return NULL;

		sunxi_ahub_membase = ioremap(res.start, resource_size(&res));
		if (!sunxi_ahub_membase)
			return NULL;

		sunxi_ahub->regmap = devm_regmap_init_mmio(&pdev->dev,
						sunxi_ahub_membase,
						&sunxi_ahub_regmap_config);
		if (IS_ERR_OR_NULL(sunxi_ahub->regmap)) {
			ret = PTR_ERR(sunxi_ahub->regmap);
			return NULL;
		}
	}
	return sunxi_ahub->regmap;
}
EXPORT_SYMBOL_GPL(sunxi_ahub_regmap_init);
static const char * const apbif_mux_text[] = {
	"NONE",
	"APBIF_TXDIF0",
	"APBIF_TXDIF1",
	"APBIF_TXDIF2",
	"I2S0_TXDIF",
	"I2S1_TXDIF",
	"I2S2_TXDIF",
	"I2S3_TXDIF",
	"DAM0_TXDIF",
	"DAM1_TXDIF",
};
static const unsigned int apbif_mux_values[] = {
	0,
	1<<I2S_RX_APBIF_TXDIF0,
	1<<I2S_RX_APBIF_TXDIF1,
	1<<I2S_RX_APBIF_TXDIF2,
	1<<I2S_RX_I2S0_TXDIF,
	1<<I2S_RX_I2S1_TXDIF,
	1<<I2S_RX_I2S2_TXDIF,
	1<<I2S_RX_I2S3_TXDIF,
	1<<I2S_RX_DAM0_TXDIF,
	1<<I2S_RX_DAM1_TXDIF,
};

#define AHUB_MUX_ENUM_DECL(name, reg)	\
	SOC_VALUE_ENUM_SINGLE_DECL(name, reg, 0, 0xffffffff,	\
			apbif_mux_text, apbif_mux_values)

static AHUB_MUX_ENUM_DECL(apbif0, SUNXI_AHUB_APBIF_RXFIFO_CONT(0));
static AHUB_MUX_ENUM_DECL(apbif1, SUNXI_AHUB_APBIF_RXFIFO_CONT(1));
static AHUB_MUX_ENUM_DECL(apbif2, SUNXI_AHUB_APBIF_RXFIFO_CONT(2));
static AHUB_MUX_ENUM_DECL(i2s0, SUNXI_AHUB_I2S_RXCONT(0));
static AHUB_MUX_ENUM_DECL(i2s1, SUNXI_AHUB_I2S_RXCONT(1));
static AHUB_MUX_ENUM_DECL(i2s2, SUNXI_AHUB_I2S_RXCONT(2));
static AHUB_MUX_ENUM_DECL(i2s3, SUNXI_AHUB_I2S_RXCONT(3));
static AHUB_MUX_ENUM_DECL(dam0chan0, SUNXI_AHUB_DAM_RX0_SRC(0));
static AHUB_MUX_ENUM_DECL(dam0chan1, SUNXI_AHUB_DAM_RX1_SRC(0));
static AHUB_MUX_ENUM_DECL(dam0chan2, SUNXI_AHUB_DAM_RX2_SRC(0));
static AHUB_MUX_ENUM_DECL(dam1chan0, SUNXI_AHUB_DAM_RX0_SRC(1));
static AHUB_MUX_ENUM_DECL(dam1chan1, SUNXI_AHUB_DAM_RX1_SRC(1));
static AHUB_MUX_ENUM_DECL(dam1chan2, SUNXI_AHUB_DAM_RX2_SRC(1));


#define APBIF_RX_MUX_CONTROLS(num)					\
static const struct snd_kcontrol_new apbif##num##_rx_mux =		\
	SOC_DAPM_VALUE_ENUM("APBIF##num Rx Mux", apbif##num);

#define I2S_RX_MUX_CONTROLS(num)					\
static const struct snd_kcontrol_new i2s##num##_rx_mux =		\
	SOC_DAPM_VALUE_ENUM("I2S##num RX Mux", i2s##num);

#define DAM0_RX_MUX_CONTROLS(chan)					\
static const struct snd_kcontrol_new  dam0chan##chan##_rx_mux =		\
	SOC_DAPM_VALUE_ENUM("DAM0 Chan##chan Rx Mux", dam0chan##chan);
#define DAM1_RX_MUX_CONTROLS(chan)                                       \
static const struct snd_kcontrol_new dam1chan##chan##_rx_mux =           \
	SOC_DAPM_VALUE_ENUM("DAM1 Chan##chan Rx Mux", dam1chan##chan);

#define AHUB_MUX(name, ctrl) \
	SND_SOC_DAPM_VALUE_MUX(name, SND_SOC_NOPM, 0, 0, &ctrl)

#define AHUB_MUX_DAM0(name, ctrl, ch) \
	SND_SOC_DAPM_VALUE_MUX(name, \
	SUNXI_AHUB_DAM_CTL(0), DAM_CTL_RX0EN+ch, 0, &ctrl)

#define AHUB_MUX_DAM1(name, ctrl, ch) \
	SND_SOC_DAPM_VALUE_MUX(name, \
	SUNXI_AHUB_DAM_CTL(1), DAM_CTL_RX0EN+ch, 0, &ctrl)

/*three apbif dev group */
APBIF_RX_MUX_CONTROLS(0)
APBIF_RX_MUX_CONTROLS(1)
APBIF_RX_MUX_CONTROLS(2)
/* four i2s dev group */
I2S_RX_MUX_CONTROLS(0)
I2S_RX_MUX_CONTROLS(1)
I2S_RX_MUX_CONTROLS(2)
I2S_RX_MUX_CONTROLS(3)
/* two digital audio mux & three chan dev group */
DAM0_RX_MUX_CONTROLS(0)
DAM0_RX_MUX_CONTROLS(1)
DAM0_RX_MUX_CONTROLS(2)
DAM1_RX_MUX_CONTROLS(0)
DAM1_RX_MUX_CONTROLS(1)
DAM1_RX_MUX_CONTROLS(2)

struct str_conv {
	char *str;
	int regbit;
};

/* I2S module usage control by cpu_dai */
static struct str_conv mod_str_conv[] = {
	{"APBIF0 DAC", 31},
	{"APBIF1 DAC", 30},
	{"APBIF2 DAC", 29},
	{"APBIF0 ADC", 27},
	{"APBIF1 ADC", 26},
	{"APBIF2 ADC", 25},
	{"DAM0 Mixer", 15},
	{"DAM1 Mixer", 14},
};

static int sunxi_ahub_mod_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	struct sunxi_ahub_priv *sunxi_ahub =
			snd_soc_codec_get_drvdata(w->codec);
	int reg_bit = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(mod_str_conv); i++)
		if (!strncmp(mod_str_conv[i].str, w->name,
					strlen(mod_str_conv[i].str))) {
			reg_bit = mod_str_conv[i].regbit;
			break;
		}

	switch (event) {
	case	SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_RST,
				(0x1<<reg_bit), (0x1<<reg_bit));
		regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_GAT,
				(0x1<<reg_bit), (0x1<<reg_bit));
		break;
	case	SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_RST,
				(0x1<<reg_bit), (0x0<<reg_bit));
		regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_GAT,
				(0x1<<reg_bit), (0x0<<reg_bit));
		break;
	default:
		break;
	}

	return 0;
}



static const struct snd_soc_dapm_widget sunxi_ahub_codec_dapm_widgets[] = {
	/* APBIF module output & input widgets */
	SND_SOC_DAPM_AIF_IN_E("APBIF0 DAC", "AIF1 Playback", 0,
			SND_SOC_NOPM, 0, 0,
			sunxi_ahub_mod_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("APBIF0 ADC", "AIF1 Capture", 0,
			SND_SOC_NOPM, 0, 0,
			sunxi_ahub_mod_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("APBIF1 DAC", "AIF2 Playback", 0,
			SND_SOC_NOPM, 0, 0,
			sunxi_ahub_mod_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("APBIF1 ADC", "AIF2 Capture", 0,
			SND_SOC_NOPM, 0, 0,
			sunxi_ahub_mod_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("APBIF2 DAC", "AIF3 Playback", 0,
			SND_SOC_NOPM, 0, 0,
			sunxi_ahub_mod_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("APBIF2 ADC", "AIF3 Capture", 0,
			SND_SOC_NOPM, 0, 0,
			sunxi_ahub_mod_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	AHUB_MUX("APBIF0 Src Select", apbif0_rx_mux),
	AHUB_MUX("APBIF1 Src Select", apbif1_rx_mux),
	AHUB_MUX("APBIF2 Src Select", apbif2_rx_mux),
	AHUB_MUX("I2S0 Src Select", i2s0_rx_mux),
	AHUB_MUX("I2S1 Src Select", i2s1_rx_mux),
	AHUB_MUX("I2S2 Src Select", i2s2_rx_mux),
	AHUB_MUX("I2S3 Src Select", i2s3_rx_mux),
	AHUB_MUX_DAM0("DAM0Chan0 Src Select", dam0chan0_rx_mux, 0),
	AHUB_MUX_DAM0("DAM0Chan1 Src Select", dam0chan1_rx_mux, 1),
	AHUB_MUX_DAM0("DAM0Chan2 Src Select", dam0chan2_rx_mux, 2),
	AHUB_MUX_DAM1("DAM1Chan0 Src Select", dam1chan0_rx_mux, 0),
	AHUB_MUX_DAM1("DAM1Chan1 Src Select", dam1chan1_rx_mux, 1),
	AHUB_MUX_DAM1("DAM1Chan2 Src Select", dam1chan2_rx_mux, 2),


	SND_SOC_DAPM_OUT_DRV_E("DAM0 Mixer", SUNXI_AHUB_DAM_CTL(0),
			DAM_CTL_TXEN, 0, NULL, 0, sunxi_ahub_mod_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("DAM1 Mixer", SUNXI_AHUB_DAM_CTL(1),
			DAM_CTL_TXEN, 0, NULL, 0, sunxi_ahub_mod_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("I2S0 DAC"),
	SND_SOC_DAPM_INPUT("I2S1 DAC"),
	SND_SOC_DAPM_INPUT("I2S2 DAC"),
	SND_SOC_DAPM_INPUT("I2S3 DAC"),
	SND_SOC_DAPM_OUTPUT("I2S0 ADC"),
	SND_SOC_DAPM_OUTPUT("I2S1 ADC"),
	SND_SOC_DAPM_OUTPUT("I2S2 ADC"),
	SND_SOC_DAPM_OUTPUT("I2S3 ADC"),
};

static const struct snd_soc_dapm_route sunxi_ahub_codec_dapm_routes[] = {
	/* APBIF0 from DMA to RX Mux routes */
	{"APBIF0 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"APBIF1 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"APBIF2 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"I2S0 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"I2S1 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"I2S2 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"I2S3 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"DAM0Chan0 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"DAM0Chan1 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"DAM0Chan2 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"DAM1Chan0 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"DAM1Chan1 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	{"DAM1Chan2 Src Select", "APBIF_TXDIF0", "APBIF0 DAC"},
	/* APBIF1 from DMA to RX Mux routes */
	{"APBIF0 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"APBIF1 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"APBIF2 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"I2S0 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"I2S1 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"I2S2 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"I2S3 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"DAM0Chan0 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"DAM0Chan1 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"DAM0Chan2 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"DAM1Chan0 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"DAM1Chan1 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	{"DAM1Chan2 Src Select", "APBIF_TXDIF1", "APBIF1 DAC"},
	/* APBIF2 from DMA to RX Mux routes */
	{"APBIF0 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"APBIF1 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"APBIF2 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"I2S0 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"I2S1 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"I2S2 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"I2S3 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"DAM0Chan0 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"DAM0Chan1 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"DAM0Chan2 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"DAM1Chan0 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"DAM1Chan1 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	{"DAM1Chan2 Src Select", "APBIF_TXDIF2", "APBIF2 DAC"},
	/* I2S0 to RX Mux routes */
	{"APBIF0 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"APBIF1 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"APBIF2 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"I2S0 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"I2S1 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"I2S2 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"I2S3 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"DAM0Chan0 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"DAM0Chan1 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"DAM0Chan2 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"DAM1Chan0 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"DAM1Chan1 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	{"DAM1Chan2 Src Select", "I2S0_TXDIF", "I2S0 DAC"},
	/* I2S1 to RX Mux routes */
	{"APBIF0 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"APBIF1 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"APBIF2 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"I2S0 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"I2S1 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"I2S2 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"I2S3 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"DAM0Chan0 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"DAM0Chan1 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"DAM0Chan2 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"DAM1Chan0 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"DAM1Chan1 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	{"DAM1Chan2 Src Select", "I2S1_TXDIF", "I2S1 DAC"},
	/* I2S2 to RX Mux routes */
	{"APBIF0 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"APBIF1 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"APBIF2 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"I2S0 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"I2S1 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"I2S2 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"I2S3 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"DAM0Chan0 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"DAM0Chan1 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"DAM0Chan2 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"DAM1Chan0 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"DAM1Chan1 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	{"DAM1Chan2 Src Select", "I2S2_TXDIF", "I2S2 DAC"},
	/* I2S3 to RX Mux routes */
	{"APBIF0 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"APBIF1 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"APBIF2 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"I2S0 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"I2S1 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"I2S2 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"I2S3 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"DAM0Chan0 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"DAM0Chan1 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"DAM0Chan2 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"DAM1Chan0 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"DAM1Chan1 Src Select", "I2S3_TXDIF", "I2S3 DAC"},
	{"DAM1Chan2 Src Select", "I2S3_TXDIF", "I2S3 DAC"},

	/* DAM0 Audio Mixer output route */
	{"DAM0 Mixer", "NULL", "DAM0Chan0 Src Select"},
	{"DAM0 Mixer", "NULL", "DAM0Chan1 Src Select"},
	{"DAM0 Mixer", "NULL", "DAM0Chan2 Src Select"},
	{"DAM1 Mixer", "NULL", "DAM1Chan0 Src Select"},
	{"DAM1 Mixer", "NULL", "DAM1Chan1 Src Select"},
	{"DAM1 Mixer", "NULL", "DAM1Chan2 Src Select"},
	/* DAM0 Audio Mixer output route */
	{"APBIF0 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"APBIF1 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"APBIF2 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"I2S0 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"I2S1 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"I2S2 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"I2S3 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"DAM0Chan0 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"DAM0Chan1 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"DAM0Chan2 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"DAM1Chan0 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"DAM1Chan1 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	{"DAM1Chan2 Src Select", "DAM0_TXDIF", "DAM0 Mixer"},
	/* DAM1 Audio Mixer output route */
	{"APBIF0 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"APBIF1 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"APBIF2 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"I2S0 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"I2S1 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"I2S2 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"I2S3 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"DAM0Chan0 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"DAM0Chan1 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"DAM0Chan2 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"DAM1Chan0 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"DAM1Chan1 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},
	{"DAM1Chan2 Src Select", "DAM1_TXDIF", "DAM1 Mixer"},

	/* Mixer to APBIF Capture */
	{"APBIF0 ADC", "NULL", "APBIF0 Src Select"},
	{"APBIF1 ADC", "NULL", "APBIF1 Src Select"},
	{"APBIF2 ADC", "NULL", "APBIF2 Src Select"},
	/* Mixer to I2S OUT(as ahub side says) */
	{"I2S0 ADC", "NULL", "I2S0 Src Select"},
	{"I2S1 ADC", "NULL", "I2S1 Src Select"},
	{"I2S2 ADC", "NULL", "I2S2 Src Select"},
	{"I2S3 ADC", "NULL", "I2S3 Src Select"},
};

static void sunxi_ahub_codec_init(struct snd_soc_codec *codec)
{
	struct sunxi_ahub_priv *sunxi_ahub = snd_soc_codec_get_drvdata(codec);
	int i;

	/* if we used the audio hub, so we default setting HDMI clk from ahub */
	regmap_write(sunxi_ahub->regmap, SUNXI_AHUB_CTL, 1<<HDMI_SRC_SEL);

	for (i = 0; i < 2; i++) {
		/* setting audio hub default channel line configure */
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_MIX_CTL0(i), 0x01110000);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_MIX_CTL1(i), 0x03330222);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_MIX_CTL2(i), 0x05550444);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_MIX_CTL3(i), 0x07770666);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_MIX_CTL4(i), 0x09990888);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_MIX_CTL5(i), 0x0bbb0aaa);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_MIX_CTL6(i), 0x0ddd0ccc);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_MIX_CTL7(i), 0x0fff0eee);
		/* setting default audio hub volume */
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_GAIN_CTL0(i), 0x01110111);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_GAIN_CTL1(i), 0x01110111);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_GAIN_CTL2(i), 0x01110111);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_GAIN_CTL3(i), 0x01110111);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_GAIN_CTL4(i), 0x01110111);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_GAIN_CTL5(i), 0x01110111);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_GAIN_CTL6(i), 0x01110111);
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_DAM_GAIN_CTL7(i), 0x01110111);
	}
}

static int sunxi_ahub_codec_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct sunxi_ahub_priv *sunxi_ahub =
				snd_soc_codec_get_drvdata(dai->codec);

	switch (params_format(params)) {
	case	SNDRV_PCM_FORMAT_S16_LE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_TX_CTL(dai->id),
					(7<<APBIF_TX_WS), (3<<APBIF_TX_WS));
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_TXFIFO_CTL(dai->id),
				(1<<APBIF_TX_TXIM), (1<<APBIF_TX_TXIM));
		} else {
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_RX_CTL(dai->id),
					(7<<APBIF_RX_WS), (3<<APBIF_RX_WS));
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_RXFIFO_CTL(dai->id),
				(3<<APBIF_RX_RXOM), (1<<APBIF_RX_RXOM));
		}
		break;
	case	SNDRV_PCM_FORMAT_S24_LE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_TX_CTL(dai->id),
					(7<<APBIF_TX_WS), (5<<APBIF_TX_WS));
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_TXFIFO_CTL(dai->id),
				(1<<APBIF_TX_TXIM), (1<<APBIF_TX_TXIM));
		} else {
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_RX_CTL(dai->id),
					(7<<APBIF_RX_WS), (5<<APBIF_RX_WS));
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_RXFIFO_CTL(dai->id),
				(3<<APBIF_RX_RXOM), (1<<APBIF_RX_RXOM));
		}
		break;
	case	SNDRV_PCM_FORMAT_S32_LE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_TX_CTL(dai->id),
					(7<<APBIF_TX_WS), (7<<APBIF_TX_WS));
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_TXFIFO_CTL(dai->id),
				(1<<APBIF_TX_TXIM), (1<<APBIF_TX_TXIM));
		} else {
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_RX_CTL(dai->id),
					(7<<APBIF_RX_WS), (7<<APBIF_RX_WS));
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_RXFIFO_CTL(dai->id),
				(3<<APBIF_RX_RXOM), (1<<APBIF_RX_RXOM));
		}
		break;
	default:
		dev_info(sunxi_ahub->dev, "unsupport format");
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		regmap_update_bits(sunxi_ahub->regmap,
			SUNXI_AHUB_APBIF_TX_CTL(dai->id),
			(0xf<<APBIF_TX_CHAN_NUM),
			((params_channels(params)-1)<<APBIF_TX_CHAN_NUM));
	else
		regmap_update_bits(sunxi_ahub->regmap,
			SUNXI_AHUB_APBIF_RX_CTL(dai->id),
			(0xf<<APBIF_RX_CHAN_NUM),
			((params_channels(params)-1)<<APBIF_RX_CHAN_NUM));

	/*
	 * we should using this as the demand chans, but we can't distinguish
	 * stream type(playback or capture). so we can't make it done on demand,
	 * so we just make all dam rx channel number as the sunxi_ahub->channel.
	 */
	regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_DAM_CTL(0),
			(0xf<<DAM_CTL_RX0_NUM),
			((params_channels(params)-1)<<DAM_CTL_RX0_NUM));
	regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_DAM_CTL(0),
			(0xf<<DAM_CTL_RX1_NUM),
			((params_channels(params)-1)<<DAM_CTL_RX1_NUM));
	regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_DAM_CTL(0),
			(0xf<<DAM_CTL_RX2_NUM),
			((params_channels(params)-1)<<DAM_CTL_RX2_NUM));
	regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_DAM_CTL(0),
			(0xf<<DAM_CTL_TX_NUM),
			((params_channels(params)-1)<<DAM_CTL_TX_NUM));
	regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_DAM_CTL(1),
			(0xf<<DAM_CTL_RX0_NUM),
			((params_channels(params)-1)<<DAM_CTL_RX0_NUM));
	regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_DAM_CTL(1),
			(0xf<<DAM_CTL_RX1_NUM),
			((params_channels(params)-1)<<DAM_CTL_RX1_NUM));
	regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_DAM_CTL(1),
			(0xf<<DAM_CTL_RX2_NUM),
			((params_channels(params)-1)<<DAM_CTL_RX2_NUM));
	regmap_update_bits(sunxi_ahub->regmap, SUNXI_AHUB_DAM_CTL(1),
			(0xf<<DAM_CTL_TX_NUM),
			((params_channels(params)-1)<<DAM_CTL_TX_NUM));

	return 0;
}

static int sunxi_ahub_codec_dai_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct sunxi_ahub_priv *sunxi_ahub =
				snd_soc_codec_get_drvdata(dai->codec);

	switch (cmd) {
	case	SNDRV_PCM_TRIGGER_START:
	case	SNDRV_PCM_TRIGGER_RESUME:
	case	SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_TXFIFO_CTL(dai->id),
					(1<<APBIF_TX_FTX), (1<<APBIF_TX_FTX));
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_TX_IRQ_CTL(dai->id),
					(1<<APBIF_TX_DRQ), (1<<APBIF_TX_DRQ));
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_TX_CTL(dai->id),
				(1<<APBIF_TX_START), (1<<APBIF_TX_START));
		} else {
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_RXFIFO_CTL(dai->id),
					(1<<APBIF_RX_FRX), (1<<APBIF_RX_FRX));
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_RX_IRQ_CTL(dai->id),
					(1<<APBIF_RX_DRQ), (1<<APBIF_RX_DRQ));
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_RX_CTL(dai->id),
				(1<<APBIF_RX_START), (1<<APBIF_RX_START));
		}
		break;
	case	SNDRV_PCM_TRIGGER_STOP:
	case	SNDRV_PCM_TRIGGER_SUSPEND:
	case	SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			regmap_update_bits(sunxi_ahub->regmap,
					SUNXI_AHUB_APBIF_TX_IRQ_CTL(dai->id),
					(1<<APBIF_TX_DRQ), (0<<APBIF_TX_DRQ));
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_TX_CTL(dai->id),
				(1<<APBIF_TX_START), (0<<APBIF_TX_START));
		} else {
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_RX_IRQ_CTL(dai->id),
				(1<<APBIF_RX_DRQ), (0<<APBIF_RX_DRQ));
			regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_RX_CTL(dai->id),
				(1<<APBIF_RX_START), (0<<APBIF_RX_START));
		}
		/*hw requires at least 1ms delay when doing 3if dam*/
		mdelay(2);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sunxi_ahub_codec_dai_set_sysclk(struct snd_soc_dai *dai,
			int clk_id, unsigned int freq, int dir)
{
	struct sunxi_ahub_priv *sunxi_ahub = snd_soc_dai_get_drvdata(dai);

	if (clk_set_rate(sunxi_ahub->pllclk, freq)) {
		dev_err(sunxi_ahub->dev, "set pllclk rate failed\n");
		return -EINVAL;
	}
	return 0;
}

static int sunxi_ahub_codec_dai_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct sunxi_ahub_priv *sunxi_ahub = snd_soc_dai_get_drvdata(dai);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_TXFIFO_CTL(dai->id),
				(1<<APBIF_TX_FTX), (1<<APBIF_TX_FTX));
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_TX_IRQ_STA(dai->id),
				(1<<APBIF_TX_OV_PEND|1<<APBIF_TX_EM_PEND));
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_TXFIFO_CNT(dai->id), 0);
	} else {
		regmap_update_bits(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_RXFIFO_CTL(dai->id),
				(1<<APBIF_RX_FRX), (1<<APBIF_RX_FRX));
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_RX_IRQ_STA(dai->id),
				(1<<APBIF_RX_UV_PEND|1<<APBIF_RX_AV_PEND));
		regmap_write(sunxi_ahub->regmap,
				SUNXI_AHUB_APBIF_RXFIFO_CNT(dai->id), 0);
	}
	return 0;
}

static const struct snd_soc_dai_ops sunxi_ahub_codec_dai_ops = {
	.hw_params	= sunxi_ahub_codec_dai_hw_params,
	.set_sysclk	= sunxi_ahub_codec_dai_set_sysclk,
	.trigger	= sunxi_ahub_codec_dai_trigger,
	.prepare	= sunxi_ahub_codec_dai_prepare,
};

/* ahub codec dai */
static struct snd_soc_dai_driver sunxi_ahub_codec_dais[] = {
	{
		.name = "sunxi-ahub-aif1",
		.id = 0,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
		 },
		.ops = &sunxi_ahub_codec_dai_ops,
	},
	{
		.name = "sunxi-ahub-aif2",
		.id = 1,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &sunxi_ahub_codec_dai_ops,
	},
	{
		.name = "sunxi-ahub-aif3",
		.id = 2,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "AIF3 Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
		 },
		.ops = &sunxi_ahub_codec_dai_ops,
	}
};

static int sunxi_ahub_codec_probe(struct snd_soc_codec *codec)
{
	sunxi_ahub_codec_init(codec);
	return 0;
}

static int sunxi_ahub_codec_suspend(struct snd_soc_codec *codec)
{
	struct sunxi_ahub_priv *sunxi_ahub = snd_soc_codec_get_drvdata(codec);
	pr_debug("Enter %s\n", __func__);

	clk_disable_unprepare(sunxi_ahub->moduleclk);
	clk_disable_unprepare(sunxi_ahub->pllclk);

	pr_debug("End %s\n", __func__);

	return 0;
}

static int sunxi_ahub_codec_resume(struct snd_soc_codec *codec)
{
	struct sunxi_ahub_priv *sunxi_ahub = snd_soc_codec_get_drvdata(codec);

	pr_debug("Enter %s\n", __func__);

	if (clk_prepare_enable(sunxi_ahub->pllclk)) {
		dev_err(sunxi_ahub->dev, "pllclk resume failed\n");
		return -EBUSY;
	}

	if (clk_prepare_enable(sunxi_ahub->moduleclk)) {
		dev_err(sunxi_ahub->dev, "moduleclk resume failed\n");
		return -EBUSY;
	}

	sunxi_ahub_codec_init(codec);

	pr_debug("End %s\n", __func__);

	return 0;
}

static struct snd_soc_codec_driver soc_ahub_dev_sunxi = {
	.probe = sunxi_ahub_codec_probe,
	.suspend = sunxi_ahub_codec_suspend,
	.resume = sunxi_ahub_codec_resume,
	.ignore_pmdown_time = 1,
	.dapm_widgets = sunxi_ahub_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sunxi_ahub_codec_dapm_widgets),
	.dapm_routes = sunxi_ahub_codec_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(sunxi_ahub_codec_dapm_routes),
};

static int sunxi_ahub_dev_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;

	dev_set_drvdata(&pdev->dev, sunxi_ahub);
	sunxi_ahub->dev = &pdev->dev;

	sunxi_ahub->pllclk = of_clk_get(np, 0);
	if (IS_ERR_OR_NULL(sunxi_ahub->pllclk)) {
		dev_err(&pdev->dev, "pllclk get failed\n");
		ret = PTR_ERR(sunxi_ahub->pllclk);
		goto err_node_put;
	}

	sunxi_ahub->moduleclk = of_clk_get(np, 1);
	if (IS_ERR_OR_NULL(sunxi_ahub->moduleclk)) {
		dev_err(&pdev->dev, "moduleclk get failed\n");
		ret = PTR_ERR(sunxi_ahub->moduleclk);
		goto err_pllclk_put;
	}

	if (clk_set_parent(sunxi_ahub->moduleclk, sunxi_ahub->pllclk)) {
		dev_err(&pdev->dev, "set parent of moduleclk to pllclk fail\n");
		ret = -EBUSY;
		goto err_moduleclk_put;
	}
	clk_prepare_enable(sunxi_ahub->pllclk);
	clk_prepare_enable(sunxi_ahub->moduleclk);

	sunxi_ahub->regmap = sunxi_ahub_regmap_init(pdev);
	if (!sunxi_ahub->regmap) {
		dev_err(&pdev->dev, "regmap not init ok\n");
		ret = -ENOMEM;
		goto err_moduleclk_put;
	}

	ret = snd_soc_register_codec(&pdev->dev, &soc_ahub_dev_sunxi,
				sunxi_ahub_codec_dais,
				ARRAY_SIZE(sunxi_ahub_codec_dais));
	if (ret) {
		dev_err(&pdev->dev, "component register failed\n");
		ret = -ENOMEM;
		goto err_moduleclk_put;
	}

	return 0;

err_moduleclk_put:
	clk_put(sunxi_ahub->moduleclk);
err_pllclk_put:
	clk_put(sunxi_ahub->pllclk);
err_node_put:
	of_node_put(np);
	return ret;
}

static int __exit sunxi_ahub_dev_remove(struct platform_device *pdev)
{
	struct sunxi_ahub_priv *sunxi_ahub = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	clk_put(sunxi_ahub->moduleclk);
	clk_put(sunxi_ahub->pllclk);
	return 0;
}

static const struct of_device_id sunxi_ahub_of_match[] = {
	{ .compatible = "allwinner,sunxi-ahub", },
	{},
};

static struct platform_driver sunxi_ahub_driver = {
	.probe = sunxi_ahub_dev_probe,
	.remove = __exit_p(sunxi_ahub_dev_remove),
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = sunxi_ahub_of_match,
	},
};

module_platform_driver(sunxi_ahub_driver);

MODULE_AUTHOR("wolfgang huang <huangjinhui@allwinnertech.com>");
MODULE_DESCRIPTION("SUNXI Audio Hub Codec ASoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sunxi-ahub");
