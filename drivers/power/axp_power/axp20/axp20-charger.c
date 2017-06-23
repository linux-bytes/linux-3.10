#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include "../axp-core.h"
#include "../axp-charger.h"
#include "axp20-charger.h"

static int axp20_get_ac_voltage(struct axp_charger_dev *cdev)
{
	return 0;
}

static int axp20_get_ac_current(struct axp_charger_dev *cdev)
{
	return 0;
}

static int axp20_set_ac_vhold(struct axp_charger_dev *cdev, int vol)
{
	return 0;
}

static int axp20_get_ac_vhold(struct axp_charger_dev *cdev)
{
	return 0;
}

static int axp20_set_ac_ihold(struct axp_charger_dev *cdev, int cur)
{
	return 0;
}

static int axp20_get_ac_ihold(struct axp_charger_dev *cdev)
{
	return 0;
}

static struct axp_ac_info axp20_ac_info = {
	.det_bit         = 7,
	.det_offset      = AXP20_CHARGE_STATUS,
	.valid_bit       = 6,
	.valid_offset    = AXP20_CHARGE_STATUS,
	.in_short_bit    = 1,
	.in_short_offset = AXP20_CHARGE_STATUS,
	.get_ac_voltage  = axp20_get_ac_voltage,
	.get_ac_current  = axp20_get_ac_current,
	.set_ac_vhold    = axp20_set_ac_vhold,
	.get_ac_vhold    = axp20_get_ac_vhold,
	.set_ac_ihold    = axp20_set_ac_ihold,
	.get_ac_ihold    = axp20_get_ac_ihold,
};

static int axp20_get_usb_voltage(struct axp_charger_dev *cdev)
{
	return 0;
}

static int axp20_get_usb_current(struct axp_charger_dev *cdev)
{
	return 0;
}

static int axp20_set_usb_vhold(struct axp_charger_dev *cdev, int vol)
{
	u8 tmp;
	struct axp_regmap *map = cdev->chip->regmap;

	if (vol) {
		axp_regmap_set_bits(map, AXP20_CHARGE_VBUS, 0x40);
		if (vol >= 4000 && vol <= 4700) {
			tmp = (vol - 4000) / 100;
			axp_regmap_update(map,
				AXP20_CHARGE_VBUS, tmp<<3, 0x7<<3);
		} else {
			pr_err("set usb limit voltage error, %d mV\n",
				axp20_config.pmu_usbpc_vol);
		}
	} else {
		axp_regmap_clr_bits(map, AXP20_CHARGE_VBUS, 0x40);
	}

	return 0;

}

static int axp20_get_usb_vhold(struct axp_charger_dev *cdev)
{
	u8 tmp;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_read(map, AXP20_CHARGE_VBUS, &tmp);
	tmp = (tmp >> 3) & 0x7;

	return 4000 + tmp * 100;
}

static int axp20_set_usb_ihold(struct axp_charger_dev *cdev, int cur)
{
	struct axp_regmap *map = cdev->chip->regmap;

	if (cur) {
		if (cur >= 900)
			axp_regmap_clr_bits(map, AXP20_CHARGE_VBUS, 0x3);
		else if (cur < 900)
			axp_regmap_update(map, AXP20_CHARGE_VBUS, 0x1, 0x3);
	} else {
		axp_regmap_set_bits(map, AXP20_CHARGE_VBUS, 0x3);
	}

	return 0;
}

static int axp20_get_usb_ihold(struct axp_charger_dev *cdev)
{
	u8 tmp;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_read(map, AXP20_CHARGE_VBUS, &tmp);

	tmp = tmp & 0x3;
	if (tmp == 0x1)
		return 500;
	else if (tmp == 0)
		return 900;
	else
		return 0;
}

static struct axp_usb_info axp20_usb_info = {
	.det_bit         = 5,
	.det_offset      = AXP20_CHARGE_STATUS,
	.valid_bit       = 4,
	.valid_offset    = AXP20_CHARGE_STATUS,
	.get_usb_voltage = axp20_get_usb_voltage,
	.get_usb_current = axp20_get_usb_current,
	.set_usb_vhold   = axp20_set_usb_vhold,
	.get_usb_vhold   = axp20_get_usb_vhold,
	.set_usb_ihold   = axp20_set_usb_ihold,
	.get_usb_ihold   = axp20_get_usb_ihold,
};

static int axp20_get_rest_cap(struct axp_charger_dev *cdev)
{
	u8 val;
	int rest_vol;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_read(map, AXP20_CAP, &val);
	rest_vol = (int) (val & 0x7F);

	if (rest_vol == 127)
		rest_vol = 100;

	return rest_vol;
}

static int axp20_get_bat_health(struct axp_charger_dev *cdev)
{
	u8 val;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_read(map, AXP20_FAULT_LOG1, &val);
	if (val & AXP20_FAULT_LOG_BATINACT)
		return POWER_SUPPLY_HEALTH_DEAD;
	else if (val & AXP20_FAULT_LOG_OVER_TEMP)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (val & AXP20_FAULT_LOG_COLD)
		return POWER_SUPPLY_HEALTH_COLD;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
}

static inline int axp20_vbat_to_mV(u32 reg)
{
	return ((int)(((reg >> 8) << 4) | (reg & 0x000F))) * 1100 / 1000;
}

static int axp20_get_vbat(struct axp_charger_dev *cdev)
{
	u8 tmp[2];
	u32 res;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_reads(map, AXP20_VBATH_RES, 2, tmp);
	res = (tmp[0] << 8) | tmp[1];

	return axp20_vbat_to_mV(res);
}

static inline int axp20_ibat_to_mA(u32 reg)
{
	return ((int)(((reg >> 8) << 5) | (reg & 0x001F))) * 500 / 1000;
}

static inline int axp20_icharge_to_mA(u32 reg)
{
	return ((int)(((reg >> 8) << 4) | (reg & 0x000F))) * 500 / 1000;
}

static int axp20_get_ibat(struct axp_charger_dev *cdev)
{
	u8 tmp[2];
	u32 res;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_reads(map, AXP20_IBATH_REG, 2, tmp);
	res = (tmp[0] << 8) | tmp[1];

	return axp20_icharge_to_mA(res);
}

static int axp20_get_disibat(struct axp_charger_dev *cdev)
{
	u8 tmp[2];
	u32 dis_res;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_reads(map, AXP20_DISIBATH_REG, 2, tmp);
	dis_res = (tmp[0] << 8) | tmp[1];

	return axp20_ibat_to_mA(dis_res);
}

static int axp20_set_chg_cur(struct axp_charger_dev *cdev, int cur)
{
	uint8_t tmp = 0;
	struct axp_regmap *map = cdev->chip->regmap;

	if (cur == 0)
		axp_regmap_clr_bits(map, AXP20_CHARGE_CONTROL1, 0x80);
	else
		axp_regmap_set_bits(map, AXP20_CHARGE_CONTROL1, 0x80);

	pr_info("current_limit = %d\n", cur);

	if (cur >= 300 && cur <= 1800) {
		tmp = (cur - 300) / 100;
		axp_regmap_update(map, AXP20_CHARGE_CONTROL1, tmp, 0x0F);
	} else if (cur < 300) {
		axp_regmap_clr_bits(map, AXP20_CHARGE_CONTROL1, 0x0F);
	} else {
		axp_regmap_set_bits(map, AXP20_CHARGE_CONTROL1, 0x0F);
	}

	return 0;
}

static int axp20_set_chg_vol(struct axp_charger_dev *cdev, int vol)
{
	uint8_t tmp = 0;
	struct axp_regmap *map = cdev->chip->regmap;

	if (vol < 4150) {
		tmp &= ~(3 << 5);
	} else if (vol < 4200) {
		tmp &= ~(3 << 5);
		tmp |= 1 << 5;
	} else if (vol < 4360) {
		tmp &= ~(3 << 5);
		tmp |= 1 << 6;
	} else {
		tmp |= 3 << 5;
	}

	axp_regmap_update(map, AXP20_CHARGE_CONTROL1, tmp, 0x3<<5);

	return 0;
}

static struct axp_battery_info axp20_batt_info = {
	.chgstat_bit          = 6,
	.chgstat_offset       = AXP20_MODE_CHGSTATUS,
	.det_bit              = 5,
	.det_offset           = AXP20_MODE_CHGSTATUS,
	.cur_direction_bit    = 2,
	.cur_direction_offset = AXP20_CHARGE_STATUS,
	.get_rest_cap         = axp20_get_rest_cap,
	.get_bat_health       = axp20_get_bat_health,
	.get_vbat             = axp20_get_vbat,
	.get_ibat             = axp20_get_ibat,
	.get_disibat          = axp20_get_disibat,
	.set_chg_cur          = axp20_set_chg_cur,
	.set_chg_vol          = axp20_set_chg_vol,
};

static struct power_supply_info battery_data = {
	.name = "PTI PL336078",
	.technology = POWER_SUPPLY_TECHNOLOGY_LiFe,
	.voltage_max_design = 4200000,
	.voltage_min_design = 3500000,
	.use_for_apm = 1,
};

static struct axp_supply_info axp20_spy_info = {
	.ac   = &axp20_ac_info,
	.usb  = &axp20_usb_info,
	.batt = &axp20_batt_info,
};

static int axp20_charger_init(struct axp_dev *axp_dev)
{
	u8 ocv_cap[32];
	u8 val;
	int rdc;
	struct axp_regmap *map = axp_dev->regmap;

	if (axp20_config.pmu_init_chgend_rate == 10)
		val &= ~(1 << 4);
	else
		val |= 1 << 4;

	val &= 0x7F;
	axp_regmap_write(map, AXP20_CHARGE_CONTROL1, val);

	if (axp20_config.pmu_init_chg_pretime < 40)
		axp20_config.pmu_init_chg_pretime = 40;

	if (axp20_config.pmu_init_chg_csttime < 360)
		axp20_config.pmu_init_chg_csttime = 360;

	val = ((((axp20_config.pmu_init_chg_pretime - 40) / 10) << 6)
			| ((axp20_config.pmu_init_chg_csttime - 360) / 120));
	axp_regmap_update(map, AXP20_CHARGE_CONTROL2, val, 0xC2);

	/* adc set */
	val = AXP20_ADC_BATVOL_ENABLE | AXP20_ADC_BATCUR_ENABLE;
	if (0 != axp20_config.pmu_bat_temp_enable)
		val = val | AXP20_ADC_TSVOL_ENABLE;
	axp_regmap_update(map, AXP20_ADC_CONTROL1, val,
						AXP20_ADC_BATVOL_ENABLE
						| AXP20_ADC_BATCUR_ENABLE
						| AXP20_ADC_TSVOL_ENABLE);

	axp_regmap_read(map, AXP20_ADC_CONTROL3, &val);
	switch (axp20_config.pmu_init_adc_freq / 25) {
	case 1:
		val &= ~(3 << 6);
		break;
	case 2:
		val &= ~(3 << 6);
		val |= 1 << 6;
		break;
	case 4:
		val &= ~(3 << 6);
		val |= 2 << 6;
		break;
	case 8:
		val |= 3 << 6;
		break;
	default:
		break;
	}

	if (0 != axp20_config.pmu_bat_temp_enable)
		val &= (~(1 << 2));
	axp_regmap_write(map, AXP20_ADC_CONTROL3, val);

	/* bat para */
	ocv_cap[0]  = axp20_config.pmu_bat_para1;
	ocv_cap[1]  = axp20_config.pmu_bat_para2;
	ocv_cap[2]  = axp20_config.pmu_bat_para3;
	ocv_cap[3]  = axp20_config.pmu_bat_para4;
	ocv_cap[4]  = axp20_config.pmu_bat_para5;
	ocv_cap[5]  = axp20_config.pmu_bat_para6;
	ocv_cap[6]  = axp20_config.pmu_bat_para7;
	ocv_cap[7]  = axp20_config.pmu_bat_para8;
	ocv_cap[8]  = axp20_config.pmu_bat_para9;
	ocv_cap[9]  = axp20_config.pmu_bat_para10;
	ocv_cap[10] = axp20_config.pmu_bat_para11;
	ocv_cap[11] = axp20_config.pmu_bat_para12;
	ocv_cap[12] = axp20_config.pmu_bat_para13;
	ocv_cap[13] = axp20_config.pmu_bat_para14;
	ocv_cap[14] = axp20_config.pmu_bat_para15;
	ocv_cap[15] = axp20_config.pmu_bat_para16;
	axp_regmap_writes(map, 0xC0, 16, ocv_cap);

	/*Init CHGLED function*/
	if (axp20_config.pmu_chgled_func)
		axp_regmap_set_bits(map, 0x32, 0x08); /* control by pmu */
	else
		axp_regmap_clr_bits(map, 0x32, 0x08); /* drive charge */

	/*set CHGLED Indication Type*/
	if (axp20_config.pmu_chgled_type)
		axp_regmap_set_bits(map, 0x34, 0x10); /* Type B */
	else
		axp_regmap_clr_bits(map, 0x34, 0x10); /* Type A */

	/*Init battery capacity correct function*/
	if (axp20_config.pmu_batt_cap_correct)
		axp_regmap_set_bits(map, 0xb8, 0x20); /* enable */
	else
		axp_regmap_clr_bits(map, 0xb8, 0x20); /* disable */

	if (!axp20_config.pmu_batdeten)
		axp_regmap_clr_bits(map, AXP20_OFF_CTL, 0x40);
	else
		axp_regmap_set_bits(map, AXP20_OFF_CTL, 0x40);

	/* RDC initial */
	axp_regmap_read(map, AXP20_RDC1, &val);
	if ((axp20_config.pmu_battery_rdc) && (!(val & 0x40))) {
		rdc = (axp20_config.pmu_battery_rdc * 10000 + 5371) / 10742;
		axp_regmap_write(map, AXP20_RDC1, ((rdc >> 8) & 0x1F)|0x80);
		axp_regmap_write(map, AXP20_RDC0, rdc & 0x00FF);
	}

	return 0;
}

static struct axp_interrupts axp_charger_irq[] = {
	{"usb in",        axp_usb_in_isr},
	{"usb out",       axp_usb_out_isr},
	{"ac in",         axp_ac_in_isr},
	{"ac out",        axp_ac_out_isr},
	{"bat in",        axp_capchange_isr},
	{"bat out",       axp_capchange_isr},
	{"bat temp low",  axp_change_isr},
	{"bat temp over", axp_change_isr},
	{"charging",      axp_change_isr},
	{"charge over",   axp_change_isr},
};

static int axp22_charger_probe(struct platform_device *pdev)
{
	int ret, i, irq;
	struct axp_charger_dev *chg_dev;
	struct axp_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);

	if (pdev->dev.of_node) {
		/* get dt and sysconfig */
		ret = axp_charger_dt_parse(pdev->dev.of_node, &axp20_config);
		if (ret) {
			pr_err("%s parse device tree err\n", __func__);
			return -EINVAL;
		}
	} else {
		pr_err("axp22 charger device tree err!\n");
		return -EBUSY;
	}

	axp20_ac_info.ac_vol = axp20_config.pmu_ac_vol;
	axp20_ac_info.ac_cur = axp20_config.pmu_ac_cur;
	axp20_usb_info.usb_pc_vol = axp20_config.pmu_usbpc_vol;
	axp20_usb_info.usb_pc_cur = axp20_config.pmu_usbpc_cur;
	axp20_usb_info.usb_ad_vol = axp20_config.pmu_ac_vol;
	axp20_usb_info.usb_ad_cur = axp20_config.pmu_ac_cur;
	axp20_batt_info.runtime_chgcur = axp20_config.pmu_runtime_chgcur;
	axp20_batt_info.suspend_chgcur = axp20_config.pmu_suspend_chgcur;
	axp20_batt_info.shutdown_chgcur = axp20_config.pmu_shutdown_chgcur;
	battery_data.voltage_max_design = axp20_config.pmu_init_chgvol
								* 1000;
	battery_data.voltage_min_design = axp20_config.pmu_pwroff_vol
								* 1000;
	battery_data.energy_full_design = axp20_config.pmu_battery_cap;

	axp20_charger_init(axp_dev);

	chg_dev = axp_power_supply_register(&pdev->dev, axp_dev,
					&battery_data, &axp20_spy_info);
	if (IS_ERR_OR_NULL(chg_dev))
		goto fail;

	for (i = 0; i < ARRAY_SIZE(axp_charger_irq); i++) {
		irq = platform_get_irq_byname(pdev, axp_charger_irq[i].name);
		if (irq < 0)
			continue;

		ret = axp_request_irq(axp_dev, irq,
				axp_charger_irq[i].isr, chg_dev);
		if (ret != 0) {
			dev_err(&pdev->dev, "failed to request %s IRQ %d: %d\n",
					axp_charger_irq[i].name, irq, ret);
			goto out_irq;
		}

		dev_dbg(&pdev->dev, "Requested %s IRQ %d: %d\n",
			axp_charger_irq[i].name, irq, ret);
	}

	platform_set_drvdata(pdev, chg_dev);

	return 0;

out_irq:
	for (i = i - 1; i >= 0; i--) {
		irq = platform_get_irq_byname(pdev, axp_charger_irq[i].name);
		if (irq < 0)
			continue;
		axp_free_irq(axp_dev, irq);
	}
fail:
	return -1;
}

static int axp20_charger_remove(struct platform_device *pdev)
{
	int i, irq;
	struct axp_charger_dev *chg_dev = platform_get_drvdata(pdev);
	struct axp_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);

	for (i = 0; i < ARRAY_SIZE(axp_charger_irq); i++) {
		irq = platform_get_irq_byname(pdev, axp_charger_irq[i].name);
		if (irq < 0)
			continue;
		axp_free_irq(axp_dev, irq);
	}

	axp_power_supply_unregister(chg_dev);

	return 0;
}

static int axp22_charger_suspend(struct platform_device *dev,
				pm_message_t state)
{
	struct axp_charger_dev *chg_dev = platform_get_drvdata(dev);

	axp_suspend_flag = AXP_WAS_SUSPEND;
	axp_charger_suspend(chg_dev);

	return 0;
}

static int axp22_charger_resume(struct platform_device *dev)
{
	struct axp_charger_dev *chg_dev = platform_get_drvdata(dev);
	struct axp_regmap *map = chg_dev->chip->regmap;
	int pre_rest_vol;

	if (axp_suspend_flag == AXP_SUSPEND_WITH_IRQ) {
		axp_suspend_flag = AXP_NOT_SUSPEND;
#ifdef CONFIG_AXP_NMI_USED
		enable_nmi();
#endif
	} else {
		axp_suspend_flag = AXP_NOT_SUSPEND;
	}

	pre_rest_vol = chg_dev->rest_vol;
	axp_charger_resume(chg_dev);

	if (chg_dev->rest_vol - pre_rest_vol) {
		pr_info("battery vol change: %d->%d\n",
				pre_rest_vol, chg_dev->rest_vol);
		axp_regmap_write(map, AXP20_DATA_BUFFER1,
				chg_dev->rest_vol | 0x80);
	}

	return 0;
}

static void axp20_charger_shutdown(struct platform_device *dev)
{
	struct axp_charger_dev *chg_dev = platform_get_drvdata(dev);
	axp_charger_shutdown(chg_dev);
}

static const struct of_device_id axp20_charger_dt_ids[] = {
	{ .compatible = "axp209-charger", },
	{},
};
MODULE_DEVICE_TABLE(of, axp20_charger_dt_ids);

static struct platform_driver axp20_charger_driver = {
	.driver     = {
		.name   = "axp20-charger",
		.of_match_table = axp20_charger_dt_ids,
	},
	.probe    = axp22_charger_probe,
	.remove   = axp20_charger_remove,
	.suspend  = axp22_charger_suspend,
	.resume   = axp22_charger_resume,
	.shutdown = axp20_charger_shutdown,
};

static int __init axp20_charger_initcall(void)
{
	int ret;

	ret = platform_driver_register(&axp20_charger_driver);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: failed, errno %d\n", __func__, ret);
		return -EINVAL;
	}

	return 0;
}
fs_initcall_sync(axp20_charger_initcall);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qin <qinyongshen@allwinnertech.com>");
MODULE_DESCRIPTION("Charger Driver for axp20x PMIC");
MODULE_ALIAS("platform:axp22-charger");
