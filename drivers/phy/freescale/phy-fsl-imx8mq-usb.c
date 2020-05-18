// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2017 NXP. */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#define PHY_CTRL0			0x0
#define PHY_CTRL0_REF_CLKDIV2		BIT(1)
#define PHY_CTRL0_REF_SSP_EN		BIT(2)
#define PHY_CTRL0_FSEL_MASK		GENMASK(10, 5)
#define PHY_CTRL0_FSEL_24M		0x2a
#define PHY_CTRL0_FSEL_100M		0x27
#define PHY_CTRL0_SSC_RANGE_MASK	GENMASK(23, 21)
#define PHY_CTRL0_SSC_RANGE_4003PPM	(0x2 << 21)

#define PHY_CTRL1			0x4
#define PHY_CTRL1_RESET			BIT(0)
#define PHY_CTRL1_COMMONONN		BIT(1)
#define PHY_CTRL1_ATERESET		BIT(3)
#define PHY_CTRL1_VDATSRCENB0		BIT(19)
#define PHY_CTRL1_VDATDETENB0		BIT(20)

#define PHY_CTRL2			0x8
#define PHY_CTRL2_TXENABLEN0		BIT(8)
#define PHY_CTRL2_OTG_DISABLE		BIT(9)

#define PHY_CTRL3			0xc
#define PHY_CTRL3_COMPDISTUNE_MASK	GENMASK(2, 0)
#define PHY_CTRL3_TXPREEMP_TUNE_MASK	GENMASK(16, 15)
#define PHY_CTRL3_TXPREEMP_TUNE_SHIFT	15
#define PHY_CTRL3_TXRISE_TUNE_MASK	GENMASK(21, 20)
#define PHY_CTRL3_TXRISE_TUNE_SHIFT	20
/* 1111: +24% ... 0000: -6% step: 2% */
#define PHY_CTRL3_TXVREF_TUNE_MASK	GENMASK(25, 22)
#define PHY_CTRL3_TXVREF_TUNE_SHIFT	22

#define PHY_CTRL4			0x10
#define PHY_CTRL4_PCS_TX_DEEMPH_3P5DB_MASK	GENMASK(20, 15)
#define PHY_CTRL4_PCS_TX_DEEMPH_3P5DB_SHIFT	15

#define PHY_CTRL5			0x14
#define PHY_CTRL5_PCS_TX_SWING_FULL_MASK	GENMASK(6, 0)

#define PHY_CTRL6			0x18
#define PHY_CTRL6_RXTERM_OVERRIDE_SEL	BIT(29)
#define PHY_CTRL6_ALT_CLK_EN		BIT(1)
#define PHY_CTRL6_ALT_CLK_SEL		BIT(0)

#define PHY_TUNE_DEFAULT		0xffffffff

struct imx8mq_usb_phy {
	struct phy *phy;
	struct clk *clk;
	void __iomem *base;
	struct regulator *vbus;
	u32	pcs_tx_swing_full;
	u32	pcs_tx_deemph_3p5db;
	u32	tx_vref_tune;
	u32	tx_rise_tune;
	u32	tx_preemp_amp_tune;
	u32	comp_dis_tune;
};

static int imx8mq_usb_phy_init(struct phy *phy)
{
	struct imx8mq_usb_phy *imx_phy = phy_get_drvdata(phy);
	u32 value;

	value = readl(imx_phy->base + PHY_CTRL1);
	value &= ~(PHY_CTRL1_VDATSRCENB0 | PHY_CTRL1_VDATDETENB0 |
		   PHY_CTRL1_COMMONONN);
	value |= PHY_CTRL1_RESET | PHY_CTRL1_ATERESET;
	writel(value, imx_phy->base + PHY_CTRL1);

	value = readl(imx_phy->base + PHY_CTRL0);
	value |= PHY_CTRL0_REF_SSP_EN;
	value &= ~PHY_CTRL0_SSC_RANGE_MASK;
	value |= PHY_CTRL0_SSC_RANGE_4003PPM;
	writel(value, imx_phy->base + PHY_CTRL0);

	value = readl(imx_phy->base + PHY_CTRL2);
	value |= PHY_CTRL2_TXENABLEN0;
	writel(value, imx_phy->base + PHY_CTRL2);

	value = readl(imx_phy->base + PHY_CTRL1);
	value &= ~(PHY_CTRL1_RESET | PHY_CTRL1_ATERESET);
	writel(value, imx_phy->base + PHY_CTRL1);

	return 0;
}

static int imx8mp_usb_phy_init(struct phy *phy)
{
	struct imx8mq_usb_phy *imx_phy = phy_get_drvdata(phy);
	u32 value;

	/* USB3.0 PHY signal fsel for 24M ref */
	value = readl(imx_phy->base + PHY_CTRL0);
	value &= ~PHY_CTRL0_FSEL_MASK;
	value |= (PHY_CTRL0_FSEL_24M << 5);
	writel(value, imx_phy->base + PHY_CTRL0);

	/* Disable alt_clk_en and use internal MPLL clocks */
	value = readl(imx_phy->base + PHY_CTRL6);
	value &= ~(PHY_CTRL6_ALT_CLK_SEL | PHY_CTRL6_ALT_CLK_EN);
	writel(value, imx_phy->base + PHY_CTRL6);

	value = readl(imx_phy->base + PHY_CTRL1);
	value &= ~(PHY_CTRL1_VDATSRCENB0 | PHY_CTRL1_VDATDETENB0);
	value |= PHY_CTRL1_RESET | PHY_CTRL1_ATERESET;
	writel(value, imx_phy->base + PHY_CTRL1);

	value = readl(imx_phy->base + PHY_CTRL0);
	value |= PHY_CTRL0_REF_SSP_EN;
	writel(value, imx_phy->base + PHY_CTRL0);

	/* Disable OTG block */
	value = readl(imx_phy->base + PHY_CTRL2);
	value |= PHY_CTRL2_TXENABLEN0 | PHY_CTRL2_OTG_DISABLE;
	writel(value, imx_phy->base + PHY_CTRL2);

	udelay(10);

	value = readl(imx_phy->base + PHY_CTRL1);
	value &= ~(PHY_CTRL1_RESET | PHY_CTRL1_ATERESET);
	writel(value, imx_phy->base + PHY_CTRL1);


	/* PHY tuning */
	if (imx_phy->pcs_tx_deemph_3p5db != PHY_TUNE_DEFAULT) {
		value = readl(imx_phy->base + PHY_CTRL4);
		value &= ~PHY_CTRL4_PCS_TX_DEEMPH_3P5DB_MASK;
		value |= imx_phy->pcs_tx_deemph_3p5db <<
			 PHY_CTRL4_PCS_TX_DEEMPH_3P5DB_SHIFT;
		writel(value, imx_phy->base + PHY_CTRL4);
	}

	if (imx_phy->pcs_tx_swing_full != PHY_TUNE_DEFAULT) {
		value = readl(imx_phy->base + PHY_CTRL5);
		value &= ~PHY_CTRL5_PCS_TX_SWING_FULL_MASK;
		value |= imx_phy->pcs_tx_swing_full;
		writel(value, imx_phy->base + PHY_CTRL5);
	}

	if ((imx_phy->tx_vref_tune & imx_phy->tx_rise_tune &
	    imx_phy->tx_preemp_amp_tune & imx_phy->comp_dis_tune) ==
	    PHY_TUNE_DEFAULT)
		/* If all are the default values, no need update. */
		return 0;

	value = readl(imx_phy->base + PHY_CTRL3);
	if (imx_phy->tx_vref_tune == PHY_TUNE_DEFAULT)
		imx_phy->tx_vref_tune = (value & PHY_CTRL3_TXVREF_TUNE_MASK) >>
					PHY_CTRL3_TXVREF_TUNE_SHIFT;

	if (imx_phy->tx_rise_tune == PHY_TUNE_DEFAULT)
		imx_phy->tx_rise_tune = (value & PHY_CTRL3_TXRISE_TUNE_MASK) >>
					PHY_CTRL3_TXRISE_TUNE_SHIFT;

	if (imx_phy->tx_preemp_amp_tune == PHY_TUNE_DEFAULT)
		imx_phy->tx_preemp_amp_tune = (value &
					       PHY_CTRL3_TXPREEMP_TUNE_MASK) >>
					      PHY_CTRL3_TXPREEMP_TUNE_SHIFT;

	if (imx_phy->comp_dis_tune == PHY_TUNE_DEFAULT)
		imx_phy->comp_dis_tune = value & PHY_CTRL3_COMPDISTUNE_MASK;

	value &= ~(PHY_CTRL3_TXVREF_TUNE_MASK |
		   PHY_CTRL3_TXRISE_TUNE_MASK |
		   PHY_CTRL3_TXPREEMP_TUNE_MASK |
		   PHY_CTRL3_COMPDISTUNE_MASK);
	value |= imx_phy->tx_vref_tune << PHY_CTRL3_TXVREF_TUNE_SHIFT |
		 imx_phy->tx_rise_tune << PHY_CTRL3_TXRISE_TUNE_SHIFT |
		 imx_phy->tx_preemp_amp_tune <<
		 PHY_CTRL3_TXPREEMP_TUNE_SHIFT |
		 imx_phy->comp_dis_tune;

	writel(value, imx_phy->base + PHY_CTRL3);

	return 0;
}

static int imx8m_usb_phy_init(struct phy *phy)
{
	if (of_device_is_compatible(phy->dev.parent->of_node,
				    "fsl,imx8mp-usb-phy"))
		return imx8mp_usb_phy_init(phy);
	else
		return imx8mq_usb_phy_init(phy);
}

static int imx8mq_phy_power_on(struct phy *phy)
{
	struct imx8mq_usb_phy *imx_phy = phy_get_drvdata(phy);
	u32 value;
	int ret;

	ret = regulator_enable(imx_phy->vbus);
	if (ret)
		return ret;

	ret = clk_prepare_enable(imx_phy->clk);
	if (ret)
		return ret;

	/* Disable rx term override */
	value = readl(imx_phy->base + PHY_CTRL6);
	value &= ~PHY_CTRL6_RXTERM_OVERRIDE_SEL;
	writel(value, imx_phy->base + PHY_CTRL6);

	return 0;
}

static int imx8mq_phy_power_off(struct phy *phy)
{
	struct imx8mq_usb_phy *imx_phy = phy_get_drvdata(phy);
	u32 value;

	/* Override rx term to be 0 */
	value = readl(imx_phy->base + PHY_CTRL6);
	value |= PHY_CTRL6_RXTERM_OVERRIDE_SEL;
	writel(value, imx_phy->base + PHY_CTRL6);

	clk_disable_unprepare(imx_phy->clk);
	regulator_disable(imx_phy->vbus);

	return 0;
}

static struct phy_ops imx8mq_usb_phy_ops = {
	.init		= imx8m_usb_phy_init,
	.power_on	= imx8mq_phy_power_on,
	.power_off	= imx8mq_phy_power_off,
	.owner		= THIS_MODULE,
};

static void imx8mp_get_phy_tuning_data(struct imx8mq_usb_phy *imx_phy)
{
	struct device *dev = imx_phy->phy->dev.parent;

	if (device_property_read_u32(dev, "fsl,phy-tx-vref-tune",
				     &imx_phy->tx_vref_tune))
		imx_phy->tx_vref_tune = PHY_TUNE_DEFAULT;

	if (device_property_read_u32(dev, "fsl,phy-tx-rise-tune",
				     &imx_phy->tx_rise_tune))
		imx_phy->tx_rise_tune = PHY_TUNE_DEFAULT;

	if (device_property_read_u32(dev, "fsl,phy-tx-preemp-amp-tune",
				     &imx_phy->tx_preemp_amp_tune))
		imx_phy->tx_preemp_amp_tune = PHY_TUNE_DEFAULT;

	if (device_property_read_u32(dev, "fsl,phy-comp-dis-tune",
				     &imx_phy->comp_dis_tune))
		imx_phy->comp_dis_tune = PHY_TUNE_DEFAULT;

	if (device_property_read_u32(dev, "fsl,pcs-tx-deemph-3p5db",
				     &imx_phy->pcs_tx_deemph_3p5db))
		imx_phy->pcs_tx_deemph_3p5db = PHY_TUNE_DEFAULT;

	if (device_property_read_u32(dev, "fsl,phy-pcs-tx-swing-full",
				     &imx_phy->pcs_tx_swing_full))
		imx_phy->pcs_tx_swing_full = PHY_TUNE_DEFAULT;
}

static int imx8mq_usb_phy_probe(struct platform_device *pdev)
{
	struct phy_provider *phy_provider;
	struct device *dev = &pdev->dev;
	struct imx8mq_usb_phy *imx_phy;
	struct resource *res;

	imx_phy = devm_kzalloc(dev, sizeof(*imx_phy), GFP_KERNEL);
	if (!imx_phy)
		return -ENOMEM;

	imx_phy->clk = devm_clk_get(dev, "phy");
	if (IS_ERR(imx_phy->clk)) {
		dev_err(dev, "failed to get imx8mq usb phy clock\n");
		return PTR_ERR(imx_phy->clk);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	imx_phy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(imx_phy->base))
		return PTR_ERR(imx_phy->base);

	imx_phy->phy = devm_phy_create(dev, NULL, &imx8mq_usb_phy_ops);
	if (IS_ERR(imx_phy->phy))
		return PTR_ERR(imx_phy->phy);

	imx_phy->vbus = devm_regulator_get(dev, "vbus");
	if (IS_ERR(imx_phy->vbus))
		return PTR_ERR(imx_phy->vbus);

	phy_set_drvdata(imx_phy->phy, imx_phy);

	imx8mp_get_phy_tuning_data(imx_phy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);
}

static const struct of_device_id imx8mq_usb_phy_of_match[] = {
	{.compatible = "fsl,imx8mq-usb-phy",},
	{.compatible = "fsl,imx8mp-usb-phy",},
	{ },
};
MODULE_DEVICE_TABLE(of, imx8mq_usb_phy_of_match);

static struct platform_driver imx8mq_usb_phy_driver = {
	.probe	= imx8mq_usb_phy_probe,
	.driver = {
		.name	= "imx8mq-usb-phy",
		.of_match_table	= imx8mq_usb_phy_of_match,
	}
};
module_platform_driver(imx8mq_usb_phy_driver);

MODULE_DESCRIPTION("FSL IMX8MQ USB PHY driver");
MODULE_LICENSE("GPL");
