#ifndef AXP22_REGULATOR_HH
#define AXP22_REGULATOR_HH

enum {
	AXP22_ID_LDO1,   /* RTCLDO */
	AXP22_ID_LDO2,   /* ALDO1 */
	AXP22_ID_LDO3,   /* ALDO2 */
	AXP22_ID_LDO4,   /* ALDO3 */
	AXP22_ID_LDO5,   /* DLDO1 */
	AXP22_ID_LDO6,   /* DLDO2 */
	AXP22_ID_LDO7,   /* DLDO3 */
	AXP22_ID_LDO8,   /* DLDO4 */
	AXP22_ID_LDO9,   /* ELDO1 */
	AXP22_ID_LDO10,  /* ELDO2 */
	AXP22_ID_LDO11,  /* ELDO3 */
	AXP22_ID_LDO12,  /* DC5LDO */
	AXP22_ID_LDOIO0 = AXP_LDOIO_ID_START,
	AXP22_ID_LDOIO1,
	AXP22_ID_SW1,
	AXP22_ID_DCDC1 = AXP_DCDC_ID_START,
	AXP22_ID_DCDC2,
	AXP22_ID_DCDC3,
	AXP22_ID_DCDC4,
	AXP22_ID_DCDC5,
};

/* AXP22 Regulator Registers */
#define AXP22_LDO1          AXP22_STATUS
#define AXP22_LDO5          AXP22_DLDO1OUT_VOL
#define AXP22_LDO6          AXP22_DLDO2OUT_VOL
#define AXP22_LDO7          AXP22_DLDO3OUT_VOL
#define AXP22_LDO8          AXP22_DLDO4OUT_VOL
#define AXP22_LDO9          AXP22_ELDO1OUT_VOL
#define AXP22_LDO10         AXP22_ELDO2OUT_VOL
#define AXP22_LDO11         AXP22_ELDO3OUT_VOL
#define AXP22_LDO12         AXP22_DC5LDOOUT_VOL
#define AXP22_DCDC1         AXP22_DC1OUT_VOL
#define AXP22_DCDC2         AXP22_DC2OUT_VOL
#define AXP22_DCDC3         AXP22_DC3OUT_VOL
#define AXP22_DCDC4         AXP22_DC4OUT_VOL
#define AXP22_DCDC5         AXP22_DC5OUT_VOL

#define AXP22_LDOIO0        AXP22_GPIO0LDOOUT_VOL
#define AXP22_LDOIO1        AXP22_GPIO1LDOOUT_VOL
#define AXP22_LDO2          AXP22_ALDO1OUT_VOL
#define AXP22_LDO3          AXP22_ALDO2OUT_VOL
#define AXP22_LDO4          AXP22_ALDO3OUT_VOL
#define AXP22_SW0           AXP22_STATUS
#define AXP22_DC1SW         AXP22_DC1OUT_VOL

#define AXP22_LDO1EN        AXP22_STATUS
#define AXP22_LDO2EN        AXP22_LDO_DC_EN1
#define AXP22_LDO3EN        AXP22_LDO_DC_EN1

#define AXP22_LDO4EN        AXP22_LDO_DC_EN3
#define AXP22_LDO5EN        AXP22_LDO_DC_EN2
#define AXP22_LDO6EN        AXP22_LDO_DC_EN2
#define AXP22_LDO7EN        AXP22_LDO_DC_EN2
#define AXP22_LDO8EN        AXP22_LDO_DC_EN2
#define AXP22_LDO9EN        AXP22_LDO_DC_EN2
#define AXP22_LDO10EN       AXP22_LDO_DC_EN2
#define AXP22_LDO11EN       AXP22_LDO_DC_EN2
#define AXP22_LDO12EN       AXP22_LDO_DC_EN1
#define AXP22_DCDC1EN       AXP22_LDO_DC_EN1
#define AXP22_DCDC2EN       AXP22_LDO_DC_EN1
#define AXP22_DCDC3EN       AXP22_LDO_DC_EN1
#define AXP22_DCDC4EN       AXP22_LDO_DC_EN1
#define AXP22_DCDC5EN       AXP22_LDO_DC_EN1
#define AXP22_LDOIO0EN      AXP22_GPIO0_CTL
#define AXP22_LDOIO1EN      AXP22_GPIO1_CTL
#define AXP22_DC1SW1EN      AXP22_LDO_DC_EN2
#define AXP22_SW0EN         AXP22_LDO_DC_EN2
#define AXP22_DC1SWEN       AXP22_LDO_DC_EN2
#define AXP22_BUCKMODE      AXP22_DCDC_MODESET
#define AXP22_BUCKFREQ      AXP22_DCDC_FREQSET

extern int axp22_need_save_regulator;
extern int axp22_regulator_save(void);
extern void axp22_regulator_restore(void);

#endif /* AXP22_REGULATROR_HH */
