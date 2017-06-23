#ifndef AXP_REGULATOR_H
#define AXP_REGULATOR_H

#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

struct  axp_reg_init {
	struct regulator_init_data axp_reg_init_data;
	struct axp_regulator_info *info;
};

/* The values of the various regulator constraints are obviously dependent
 * on exactly what is wired to each ldo.  Unfortunately this information is
 * not generally available.  More information has been requested from Xbow
 * but as of yet they haven't been forthcoming.
 *
 * Some of these are clearly Stargate 2 related (no way of plugging
 * in an lcd on the IM2 for example!).
 */

struct axp_consumer_supply {
	char supply[20];    /* consumer supply - e.g. "vcc" */
};

#define AXP_LDOIO_ID_START      30
#define AXP_DCDC_ID_START       40

#define AXP_LDO(_pmic, _id, min, max, step1, vreg, shift, nbits, ereg,\
	emask, enval, disval, switch_vol, step2, new_level,\
	mode_addr, freq_addr, dvm_ereg, dvm_ebit)          \
{                                                      \
	.desc   = {                                        \
		.name   = #_pmic"_LDO" #_id,                   \
		.type   = REGULATOR_VOLTAGE,                   \
		.id = _pmic##_ID_LDO##_id,                     \
		.n_voltages = (step1) ? ((switch_vol) ?        \
				((new_level) ? ((switch_vol - min) / step1 + \
				(max - new_level) / step2 + 2)         \
				: ((switch_vol - min) / step1 +        \
				(max - switch_vol) / step2 + 1)) :     \
				((max - min) / step1 + 1)) : 1,        \
		.owner  = THIS_MODULE,                         \
		.enable_reg  = _pmic##_##ereg,                 \
		.enable_mask = emask,                          \
	},                                                 \
	.min_uv     = (min) * 1000,                        \
	.max_uv     = (max) * 1000,                        \
	.enable_val  = enval,                              \
	.disable_val = disval,                             \
	.step1_uv   = (step1) * 1000,                      \
	.vol_reg    = _pmic##_##vreg,                      \
	.vol_shift  = (shift),                             \
	.vol_nbits  = (nbits),                             \
	.switch_uv  = (switch_vol)*1000,                   \
	.step2_uv   = (step2)*1000,                        \
	.new_level_uv   = (new_level)*1000,                \
	.mode_reg   = mode_addr,                           \
	.freq_reg   = freq_addr,                           \
	.dvm_enable_reg = dvm_ereg,                        \
	.dvm_enable_bit = dvm_ebit,                        \
}

#define AXP_DCDC(_pmic, _id, min, max, step1, vreg, shift, nbits, ereg,\
	emask, enval, disval, switch_vol, step2, new_level,\
	mode_addr, mode_bit, freq_addr, dvm_ereg, dvm_ebit)\
{                                                      \
	.desc   = {                                        \
		.name   = #_pmic"_DCDC" #_id,                  \
		.type   = REGULATOR_VOLTAGE,                   \
		.id = _pmic##_ID_DCDC##_id,                    \
		.n_voltages = (step1) ? ((switch_vol) ?        \
				((new_level) ? ((switch_vol - min) / step1 + \
				(max - new_level) / step2 + 2)         \
				: ((switch_vol - min) / step1 +        \
				(max - switch_vol) / step2 + 1)) :     \
				((max - min) / step1 + 1)) : 1,        \
		.owner  = THIS_MODULE,                         \
		.enable_reg  = _pmic##_##ereg,                 \
		.enable_mask = emask,                          \
	},                                                 \
	.min_uv     = (min) * 1000,                        \
	.max_uv     = (max) * 1000,                        \
	.enable_val  = enval,                              \
	.disable_val = disval,                             \
	.step1_uv   = (step1) * 1000,                      \
	.vol_reg    = _pmic##_##vreg,                      \
	.vol_shift  = (shift),                             \
	.vol_nbits  = (nbits),                             \
	.switch_uv  = (switch_vol)*1000,                   \
	.step2_uv   = (step2)*1000,                        \
	.new_level_uv   = (new_level)*1000,                \
	.mode_reg   = mode_addr,                           \
	.mode_mask  = mode_bit,                            \
	.freq_reg   = freq_addr,                           \
	.dvm_enable_reg = dvm_ereg,                        \
	.dvm_enable_bit = dvm_ebit,                        \
}


#define AXP_SW(_pmic, _id, min, max, step1, vreg, shift, nbits, ereg,\
	emask, enval, disval, switch_vol, step2, new_level,\
	mode_addr, freq_addr, dvm_ereg, dvm_ebit)          \
{                                                      \
	.desc   = {                                        \
		.name   = #_pmic"_SW" #_id,                    \
		.type   = REGULATOR_VOLTAGE,                   \
		.id = _pmic##_ID_SW##_id,                      \
		.n_voltages = (step1) ? ((switch_vol) ?        \
				((new_level) ? ((switch_vol - min) / step1 + \
				(max - new_level) / step2 + 2)         \
				: ((switch_vol - min) / step1 +        \
				(max - switch_vol) / step2 + 1)) :     \
				((max - min) / step1 + 1)) : 1,        \
		.owner  = THIS_MODULE,                         \
		.enable_reg  = _pmic##_##ereg,                 \
		.enable_mask = emask,                          \
	},                                                 \
	.min_uv     = (min) * 1000,                        \
	.max_uv     = (max) * 1000,                        \
	.step1_uv   = (step1) * 1000,                      \
	.enable_val  = enval,                              \
	.disable_val = disval,                             \
	.vol_reg    = _pmic##_##vreg,                      \
	.vol_shift  = (shift),                             \
	.vol_nbits  = (nbits),                             \
	.switch_uv  = (switch_vol)*1000,                   \
	.step2_uv   = (step2)*1000,                        \
	.new_level_uv   = (new_level)*1000,                \
	.mode_reg   = mode_addr,                           \
	.freq_reg   = freq_addr,                           \
	.dvm_enable_reg = dvm_ereg,                        \
	.dvm_enable_bit = dvm_ebit,                        \
}

#define AXP_LDO_SEL(_pmic, _id, min, max, vreg, shift, nbits, ereg,\
	emask, enval, disval, table_name,\
	mode_addr, freq_addr, dvm_ereg, dvm_ebit)          \
{                                                      \
	.desc   = {                                        \
		.name   = #_pmic"_LDO" #_id,                   \
		.type   = REGULATOR_VOLTAGE,                   \
		.id = _pmic##_ID_LDO##_id,                     \
		.n_voltages = ARRAY_SIZE(table_name##_table),  \
		.owner  = THIS_MODULE,                         \
		.enable_reg  = _pmic##_##ereg,                 \
		.enable_mask = emask,                          \
	},                                                 \
	.min_uv     = (min) * 1000,                        \
	.max_uv     = (max) * 1000,                        \
	.enable_val  = enval,                              \
	.disable_val = disval,                             \
	.vol_reg    = _pmic##_##vreg,                      \
	.vol_shift  = (shift),                             \
	.vol_nbits  = (nbits),                             \
	.mode_reg   = mode_addr,                           \
	.freq_reg   = freq_addr,                           \
	.dvm_enable_reg = dvm_ereg,                        \
	.dvm_enable_bit = dvm_ebit,                        \
	.vtable         = (int *)&table_name##_table,      \
}

#define AXP_DCDC_SEL(_pmic, _id, min, max, vreg, shift, nbits, ereg,\
	emask, enval, disval, table_name,\
	mode_addr, freq_addr, dvm_ereg, dvm_ebit)          \
{                                                      \
	.desc   = {                                        \
		.name   = #_pmic"_DCDC" #_id,                  \
		.type   = REGULATOR_VOLTAGE,                   \
		.id = _pmic##_ID_DCDC##_id,                    \
		.n_voltages = ARRAY_SIZE(table_name##_table),  \
		.owner  = THIS_MODULE,                         \
		.enable_reg  = _pmic##_##ereg,                 \
		.enable_mask = emask,                          \
	},                                                 \
	.min_uv     = (min) * 1000,                        \
	.max_uv     = (max) * 1000,                        \
	.enable_val  = enval,                              \
	.disable_val = disval,                             \
	.vol_reg    = _pmic##_##vreg,                      \
	.vol_shift  = (shift),                             \
	.vol_nbits  = (nbits),                             \
	.mode_reg   = mode_addr,                           \
	.freq_reg   = freq_addr,                           \
	.dvm_enable_reg = dvm_ereg,                        \
	.dvm_enable_bit = dvm_ebit,                        \
	.vtable         = (int *)&table_name##_table,      \
}


#define AXP_SW_SEL(_pmic, _id, min, max, vreg, shift, nbits, ereg,\
	emask, enval, disval, table_name,\
	mode_addr, freq_addr, dvm_ereg, dvm_ebit)          \
{                                                      \
	.desc   = {                                        \
		.name   = #_pmic"_SW" #_id,                    \
		.type   = REGULATOR_VOLTAGE,                   \
		.id = _pmic##_ID_SW##_id,                      \
		.n_voltages = ARRAY_SIZE(table_name##_table),  \
		.owner  = THIS_MODULE,                         \
		.enable_reg  = _pmic##_##ereg,                 \
		.enable_mask = emask,                          \
	},                                                 \
	.min_uv     = (min) * 1000,                        \
	.max_uv     = (max) * 1000,                        \
	.enable_val  = enval,                              \
	.disable_val = disval,                             \
	.vol_reg    = _pmic##_##vreg,                      \
	.vol_shift  = (shift),                             \
	.vol_nbits  = (nbits),                             \
	.mode_reg   = mode_addr,                           \
	.freq_reg   = freq_addr,                           \
	.dvm_enable_reg = dvm_ereg,                        \
	.dvm_enable_bit = dvm_ebit,                        \
	.vtable          = (int *)&table_name##_table,     \
}

#define AXP_REGU_ATTR(_name)                           \
{                                                      \
	.attr = { .name = #_name, .mode = 0644 },          \
	.show =  _name##_show,                             \
	.store = _name##_store,                            \
}

struct axp_regulator_info {
	struct regulator_desc desc;
	struct axp_regmap *regmap;
	s32 min_uv;
	s32 max_uv;
	s32 enable_val;
	s32 disable_val;
	s32 step1_uv;
	s32 vol_reg;
	s32 vol_shift;
	s32 vol_nbits;
	s32 switch_uv;
	s32 step2_uv;
	s32 new_level_uv;
	s32 mode_reg;
	s32 mode_mask;
	s32 freq_reg;
	s32 dvm_enable_reg;
	s32 dvm_enable_bit;
	s32 *vtable;
};
struct regulator_dev *axp_regulator_register(struct device *dev,
				struct axp_regmap *regmap,
				struct regulator_init_data *init_data,
				struct axp_regulator_info *info);
struct regulator_dev *axp_regulator_sel_register(struct device *dev,
				struct axp_regmap *regmap,
				struct regulator_init_data *init_data,
				struct axp_regulator_info *info);
void axp_regulator_unregister(struct regulator_dev *rdev);
s32  axp_regu_device_tree_parse(struct regulator_init_data *axp_init_data);

extern s32 axp_debug;

#endif /* AXP_REGULATOR_H */
