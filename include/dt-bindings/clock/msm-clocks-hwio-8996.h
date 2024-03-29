/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define CLKFLAG_NO_RATE_CACHE					0x00004000

#define FMAX_LOWER						0
#define FMAX_LOW						1
#define FMAX_NOM						2
#define FMAX_TURBO						3

#define HALT_CHECK_DELAY					5

#define RPM_MISC_CLK_TYPE					0x306b6c63
#define RPM_BUS_CLK_TYPE					0x316b6c63
#define RPM_MEM_CLK_TYPE					0x326b6c63
#define RPM_IPA_CLK_TYPE					0x617069
#define RPM_CE_CLK_TYPE						0x6563
#define RPM_AGGR_CLK_TYPE					0x72676761
#define RPM_SMD_KEY_ENABLE					0x62616E45

#define CXO_CLK_SRC_ID						0x0
#define QDSS_CLK_ID						0x1

#define PNOC_CLK_ID						0x0
#define SNOC_CLK_ID						0x1
#define CNOC_CLK_ID						0x2
#define BIMC_CLK_ID						0x0
#define IPA_CLK_ID						0x0
#define CE1_CLK_ID						0x0
#define BB_CLK1_ID						0x1
#define BB_CLK2_ID						0x2
#define RF_CLK1_ID						0x4
#define RF_CLK2_ID						0x5
#define LN_BB_CLK_ID						0x8
#define DIV_CLK1_ID						0xb
#define DIV_CLK2_ID						0xc
#define DIV_CLK3_ID						0xd
#define BB_CLK1_PIN_ID						0x1
#define BB_CLK2_PIN_ID						0x2
#define RF_CLK1_PIN_ID						0x4
#define RF_CLK2_PIN_ID						0x5
#define AGGR1_NOC_ID						0x1
#define AGGR2_NOC_ID						0x2

#define MMSS_MMPLL0_MODE					(0x0000)
#define MMSS_MMPLL1_MODE					(0x0030)
#define MMSS_MMPLL2_MODE					(0x4100)
#define MMSS_MMPLL3_MODE					(0x0060)
#define MMSS_MMPLL4_MODE					(0x0090)
#define MMSS_MMPLL5_MODE					(0x00C0)
#define MMSS_MMPLL8_MODE					(0x4130)
#define MMSS_MMPLL9_MODE					(0x4200)
#define MMSS_MMSS_PLL_VOTE_APCS					(0x0100)
#define MMSS_PCLK0_CMD_RCGR					(0x2000)
#define MMSS_PCLK1_CMD_RCGR					(0x2020)
#define MMSS_MDP_CMD_RCGR					(0x2040)
#define MMSS_EXTPCLK_CMD_RCGR					(0x2060)
#define MMSS_VSYNC_CMD_RCGR					(0x2080)
#define MMSS_HDMI_CMD_RCGR					(0x2100)
#define MMSS_BYTE0_CMD_RCGR					(0x2120)
#define MMSS_BYTE1_CMD_RCGR					(0x2140)
#define MMSS_ESC0_CMD_RCGR					(0x2160)
#define MMSS_ESC1_CMD_RCGR					(0x2180)
#define MMSS_MDSS_AHB_CBCR					(0x2308)
#define MMSS_MDSS_HDMI_AHB_CBCR					(0x230C)
#define MMSS_MDSS_AXI_CBCR					(0x2310)
#define MMSS_MDSS_PCLK0_CBCR					(0x2314)
#define MMSS_MDSS_PCLK1_CBCR					(0x2318)
#define MMSS_MDSS_MDP_CBCR					(0x231C)
#define MMSS_MDSS_EXTPCLK_CBCR					(0x2324)
#define MMSS_MDSS_VSYNC_CBCR					(0x2328)
#define MMSS_MDSS_HDMI_CBCR					(0x2338)
#define MMSS_MDSS_BYTE0_CBCR					(0x233C)
#define MMSS_MDSS_BYTE1_CBCR					(0x2340)
#define MMSS_MDSS_ESC0_CBCR					(0x2344)
#define MMSS_MDSS_ESC1_CBCR					(0x2348)
#define MMSS_CSI0PHYTIMER_CMD_RCGR				(0x3000)
#define MMSS_CAMSS_PHY0_CSI0PHYTIMER_CBCR			(0x3024)
#define MMSS_CSI1PHYTIMER_CMD_RCGR				(0x3030)
#define MMSS_CAMSS_PHY1_CSI1PHYTIMER_CBCR			(0x3054)
#define MMSS_CSI2PHYTIMER_CMD_RCGR				(0x3060)
#define MMSS_CAMSS_PHY2_CSI2PHYTIMER_CBCR			(0x3084)
#define MMSS_CSI0_CMD_RCGR					(0x3090)
#define MMSS_CAMSS_CSI0_CBCR					(0x30B4)
#define MMSS_CAMSS_CSI0_AHB_CBCR				(0x30BC)
#define MMSS_CAMSS_CSI0PHY_CBCR					(0x30C4)
#define MMSS_CAMSS_CSI0RDI_CBCR					(0x30D4)
#define MMSS_CAMSS_CSI0PIX_CBCR					(0x30E4)
#define MMSS_CSI1_CMD_RCGR					(0x3100)
#define MMSS_CAMSS_CSI1_CBCR					(0x3124)
#define MMSS_CAMSS_CSI1_AHB_CBCR				(0x3128)
#define MMSS_CAMSS_CSI1PHY_CBCR					(0x3134)
#define MMSS_CAMSS_CSI1RDI_CBCR					(0x3144)
#define MMSS_CAMSS_CSI1PIX_CBCR					(0x3154)
#define MMSS_CSI2_CMD_RCGR					(0x3160)
#define MMSS_CAMSS_CSI2_CBCR					(0x3184)
#define MMSS_CAMSS_CSI2_AHB_CBCR				(0x3188)
#define MMSS_CAMSS_CSI2PHY_CBCR					(0x3194)
#define MMSS_CAMSS_CSI2RDI_CBCR					(0x31A4)
#define MMSS_CAMSS_CSI2PIX_CBCR					(0x31B4)
#define MMSS_CSI3_CMD_RCGR					(0x31C0)
#define MMSS_CAMSS_CSI3_CBCR					(0x31E4)
#define MMSS_CAMSS_CSI3_AHB_CBCR				(0x31E8)
#define MMSS_CAMSS_CSI3PHY_CBCR					(0x31F4)
#define MMSS_CAMSS_CSI3RDI_CBCR					(0x3204)
#define MMSS_CAMSS_CSI3PIX_CBCR					(0x3214)
#define MMSS_CAMSS_ISPIF_AHB_CBCR				(0x3224)
#define MMSS_CCI_CMD_RCGR					(0x3300)
#define MMSS_CAMSS_CCI_CCI_CBCR					(0x3344)
#define MMSS_CAMSS_CCI_CCI_AHB_CBCR				(0x3348)
#define MMSS_MCLK0_CMD_RCGR					(0x3360)
#define MMSS_CAMSS_MCLK0_CBCR					(0x3384)
#define MMSS_MCLK1_CMD_RCGR					(0x3390)
#define MMSS_CAMSS_MCLK1_CBCR					(0x33B4)
#define MMSS_MCLK2_CMD_RCGR					(0x33C0)
#define MMSS_CAMSS_MCLK2_CBCR					(0x33E4)
#define MMSS_MCLK3_CMD_RCGR					(0x33F0)
#define MMSS_CAMSS_MCLK3_CBCR					(0x3414)
#define MMSS_CAMSS_GP0_CBCR					(0x3444)
#define MMSS_CAMSS_GP1_CBCR					(0x3474)
#define MMSS_CAMSS_TOP_AHB_CBCR					(0x3484)
#define MMSS_CAMSS_AHB_CBCR					(0x348C)
#define MMSS_CAMSS_MICRO_BCR					(0x3490)
#define MMSS_CAMSS_MICRO_AHB_CBCR				(0x3494)
#define MMSS_JPEG0_CMD_RCGR					(0x3500)
#define MMSS_JPEG2_CMD_RCGR					(0x3540)
#define MMSS_JPEG_DMA_CMD_RCGR					(0x3560)
#define MMSS_CAMSS_JPEG0_CBCR					(0x35A8)
#define MMSS_CAMSS_JPEG2_CBCR					(0x35B0)
#define MMSS_CAMSS_JPEG_DMA_CBCR				(0x35C0)
#define MMSS_CAMSS_JPEG_AHB_CBCR				(0x35B4)
#define MMSS_CAMSS_JPEG_AXI_CBCR				(0x35B8)
#define MMSS_VFE0_CMD_RCGR					(0x3600)
#define MMSS_VFE1_CMD_RCGR					(0x3620)
#define MMSS_CPP_CMD_RCGR					(0x3640)
#define MMSS_CAMSS_VFE_VFE0_CBCR				(0x36A8)
#define MMSS_CAMSS_VFE_VFE1_CBCR				(0x36AC)
#define MMSS_CAMSS_VFE_CPP_CBCR					(0x36B0)
#define MMSS_CAMSS_VFE_CPP_AHB_CBCR				(0x36B4)
#define MMSS_CAMSS_VFE_AHB_CBCR					(0x36B8)
#define MMSS_CAMSS_VFE_AXI_CBCR					(0x36BC)
#define MMSS_CAMSS_VFE_CPP_AXI_CBCR				(0x36C4)
#define MMSS_CAMSS_CSI_VFE0_CBCR				(0x3704)
#define MMSS_CAMSS_CSI_VFE1_CBCR				(0x3714)
#define MMSS_FD_CORE_CMD_RCGR					(0x3B00)
#define MMSS_FD_CORE_CBCR					(0x3B68)
#define MMSS_FD_CORE_UAR_CBCR					(0x3B6C)
#define MMSS_FD_AHB_CBCR					(0x3B74)
#define MMSS_OXILI_GFX3D_CBCR					(0x4028)
#define MMSS_OXILICX_AHB_CBCR					(0x403C)
#define MMSS_MMSS_MISC_AHB_CBCR					(0x5018)
#define MMSS_AXI_CMD_RCGR					(0x5040)
#define MMSS_MMSS_S0_AXI_CBCR					(0x5064)
#define MMSS_MMSS_DEBUG_CLK_CTL					(0x0900)
#define MMSS_EDPLINK_CMD_RCGR					(0x20C0)
#define MMSS_MAXI_CMD_RCGR					(0x5090)
#define MMSS_VIDEO_CORE_CMD_RCGR				(0x1000)
#define MMSS_CSIPHY0_3P_CMD_RCGR				(0x3240)
#define MMSS_CSIPHY1_3P_CMD_RCGR				(0x3260)
#define MMSS_CSIPHY2_3P_CMD_RCGR				(0x3280)
#define MMSS_CAMSS_GP0_CMD_RCGR					(0x3420)
#define MMSS_CAMSS_GP1_CMD_RCGR					(0x3450)
#define MMSS_ISENSE_CMD_RCGR					(0x4010)
#define MMSS_RBBMTIMER_CMD_RCGR					(0x4090)
#define MMSS_EDPAUX_CMD_RCGR					(0x20E0)
#define MMSS_EDPGTC_CMD_RCGR					(0x2220)
#define MMSS_EDPPIXEL_CMD_RCGR					(0x20A0)
#define MMSS_RBCPR_CMD_RCGR					(0x4060)
#define MMSS_VIDEO_SUBCORE0_CMD_RCGR				(0x1060)
#define MMSS_VIDEO_SUBCORE1_CMD_RCGR				(0x1080)
#define MMSS_BTO_AHB_CBCR					(0x5028)
#define MMSS_CAMSS_CCI_AHB_CBCR					(0x3348)
#define MMSS_AHB_CMD_RCGR					(0x5000)
#define MMSS_CAMSS_CCI_CBCR					(0x3344)
#define MMSS_CAMSS_CPP_AHB_CBCR					(0x36B4)
#define MMSS_CAMSS_CPP_CBCR					(0x36B0)
#define MMSS_CAMSS_CPP_AXI_CBCR					(0x36C4)
#define MMSS_CAMSS_CPP_VBIF_AHB_CBCR				(0x36C8)
#define MMSS_CAMSS_CSIPHY0_3P_CBCR				(0x3234)
#define MMSS_CAMSS_CSIPHY1_3P_CBCR				(0x3254)
#define MMSS_CAMSS_CSIPHY2_3P_CBCR				(0x3274)
#define MMSS_CAMSS_JPEG0_CBCR					(0x35A8)
#define MMSS_CAMSS_JPEG2_CBCR					(0x35B0)
#define MMSS_CAMSS_JPEG_AHB_CBCR				(0x35B4)
#define MMSS_CAMSS_JPEG_AXI_CBCR				(0x35B8)
#define MMSS_CAMSS_CSI0PHYTIMER_CBCR				(0x3024)
#define MMSS_CAMSS_CSI1PHYTIMER_CBCR				(0x3054)
#define MMSS_CAMSS_CSI2PHYTIMER_CBCR				(0x3084)
#define MMSS_CAMSS_VFE0_AHB_CBCR				(0x3668)
#define MMSS_CAMSS_VFE0_CBCR					(0x36A8)
#define MMSS_CAMSS_VFE0_STREAM_CBCR				(0x3720)
#define MMSS_CAMSS_VFE1_AHB_CBCR				(0x3678)
#define MMSS_CAMSS_VFE1_CBCR					(0x36AC)
#define MMSS_CAMSS_VFE1_STREAM_CBCR				(0x3724)
#define MMSS_GPU_AHB_CBCR					(0x403C)
#define MMSS_GPU_AON_ISENSE_CBCR				(0x4044)
#define MMSS_GPU_GX_GFX3D_CBCR					(0x4028)
#define MMSS_GPU_GX_RBBMTIMER_CBCR				(0x40B0)
#define MMSS_MDSS_EDPAUX_CBCR					(0x2334)
#define MMSS_MDSS_EDPGTC_CBCR					(0x2364)
#define MMSS_MDSS_EDPLINK_CBCR					(0x2330)
#define MMSS_MDSS_EDPPIXEL_CBCR					(0x232C)
#define MMSS_MMSS_MISC_CXO_CBCR					(0x5014)
#define MMSS_MMAGIC_BIMC_AXI_CBCR				(0x5294)
#define MMSS_MMAGIC_BIMC_NOC_CFG_AHB_CBCR			(0x5298)
#define MMSS_MMAGIC_CAMSS_AXI_CBCR				(0x3C44)
#define MMSS_MMAGIC_CAMSS_NOC_CFG_AHB_CBCR			(0x3C48)
#define MMSS_MMSS_MMAGIC_CFG_AHB_CBCR				(0x5054)
#define MMSS_MMAGIC_MDSS_AXI_CBCR				(0x2474)
#define MMSS_MMAGIC_MDSS_NOC_CFG_AHB_CBCR			(0x2478)
#define MMSS_MMAGIC_VIDEO_AXI_CBCR				(0x1194)
#define MMSS_MMAGIC_VIDEO_NOC_CFG_AHB_CBCR			(0x1198)
#define MMSS_MMSS_MMAGIC_AHB_CBCR				(0x5024)
#define MMSS_MMSS_MMAGIC_AXI_CBCR				(0x506C)
#define MMSS_MMSS_MMAGIC_MAXI_CBCR				(0x5074)
#define MMSS_MMSS_RBCPR_AHB_CBCR				(0x4088)
#define MMSS_MMSS_RBCPR_CBCR					(0x4084)
#define MMSS_MMSS_SPDM_CPP_CBCR					(0x0220)
#define MMSS_MMSS_SPDM_JPEG_DMA_CBCR				(0x0208)
#define MMSS_SMMU_CPP_AHB_CBCR					(0x3C14)
#define MMSS_SMMU_CPP_AXI_CBCR					(0x3C18)
#define MMSS_SMMU_JPEG_AHB_CBCR					(0x3C24)
#define MMSS_SMMU_JPEG_AXI_CBCR					(0x3C28)
#define MMSS_SMMU_MDP_AHB_CBCR					(0x2454)
#define MMSS_SMMU_MDP_AXI_CBCR					(0x2458)
#define MMSS_SMMU_ROT_AHB_CBCR					(0x2444)
#define MMSS_SMMU_ROT_AXI_CBCR					(0x2448)
#define MMSS_SMMU_VFE_AHB_CBCR					(0x3C04)
#define MMSS_SMMU_VFE_AXI_CBCR					(0x3C08)
#define MMSS_SMMU_VIDEO_AHB_CBCR				(0x1174)
#define MMSS_SMMU_VIDEO_AXI_CBCR				(0x1178)
#define MMSS_VIDEO_AHB_CBCR					(0x1030)
#define MMSS_VIDEO_AXI_CBCR					(0x1034)
#define MMSS_VIDEO_CXO_CBCR					(0x1028)
#define MMSS_VIDEO_MAXI_CBCR					(0x1038)
#define MMSS_VIDEO_SUBCORE0_CBCR				(0x1048)
#define MMSS_VIDEO_SUBCORE1_CBCR				(0x104C)
#define MMSS_VMEM_AHB_CBCR					(0x1208)
#define MMSS_VMEM_MAXI_CBCR					(0x1204)
#define MMSS_VIDEO_CORE_CBCR					(0x1028)
#define MMSS_GFX3D_CMD_RCGR					(0x4000)
#define MMSS_MDSS_BCR						(0x2300)
#define MMSS_MMAGICAHB_BCR					(0x5020)
#define MMSS_MMAGICAXI_BCR					(0x5060)
#define MMSS_MMSS_SPDM_BCR					(0x0200)
#define MMSS_VIDEO_BCR						(0x1020)
#define MMSS_MNOC_DCD_CONFIG_AHB				(0x50D8)

#define GCC_GPLL0_MODE						(0x00000)
#define PLLTEST_PAD_CFG					        (0x6200C)
#define GCC_XO_DIV4_CBCR				        (0x43008)
#define CLOCK_FRQ_MEASURE_CTL				        (0x62004)
#define CLOCK_FRQ_MEASURE_STATUS			        (0x62008)
#define GCC_USB_30_BCR						(0x0F000)
#define GCC_USB30_MASTER_CBCR					(0x0F008)
#define GCC_USB30_SLEEP_CBCR					(0x0F00C)
#define GCC_USB30_MOCK_UTMI_CBCR				(0x0F010)
#define GCC_USB30_MASTER_CMD_RCGR				(0x0F014)
#define GCC_USB30_MOCK_UTMI_CMD_RCGR				(0x0F028)
#define GCC_USB3_PHY_BCR					(0x50020)
#define GCC_USB3PHY_PHY_BCR					(0x50024)
#define GCC_USB3_PHY_AUX_CBCR					(0x50000)
#define GCC_USB3_PHY_PIPE_CBCR					(0x50004)
#define GCC_USB3_PHY_AUX_CMD_RCGR				(0x5000C)
#define GCC_USB_PHY_CFG_AHB2PHY_CBCR				(0x6A004)
#define GCC_SDCC1_APPS_CMD_RCGR					(0x13010)
#define GCC_SDCC1_APPS_CBCR					(0x13004)
#define GCC_SDCC1_AHB_CBCR					(0x13008)
#define GCC_SDCC2_APPS_CMD_RCGR					(0x14010)
#define GCC_SDCC2_APPS_CBCR					(0x14004)
#define GCC_SDCC2_AHB_CBCR					(0x14008)
#define GCC_SDCC3_APPS_CMD_RCGR					(0x15010)
#define GCC_SDCC3_APPS_CBCR					(0x15004)
#define GCC_SDCC3_AHB_CBCR					(0x15008)
#define GCC_SDCC4_APPS_CMD_RCGR					(0x16010)
#define GCC_SDCC4_APPS_CBCR					(0x16004)
#define GCC_SDCC4_AHB_CBCR					(0x16008)
#define GCC_QUSB2PHY_PRIM_BCR					(0x12038)
#define GCC_QUSB2PHY_SEC_BCR					(0x1203C)
#define GCC_PERIPH_NOC_USB20_AHB_CBCR				(0x06010)
#define GCC_BLSP1_AHB_CBCR					(0x17004)
#define GCC_BLSP1_QUP1_SPI_APPS_CBCR				(0x19004)
#define GCC_BLSP1_QUP1_I2C_APPS_CBCR				(0x19008)
#define GCC_BLSP1_QUP1_I2C_APPS_CMD_RCGR			(0x19020)
#define GCC_BLSP1_QUP2_I2C_APPS_CMD_RCGR			(0x1B020)
#define GCC_BLSP1_QUP3_I2C_APPS_CMD_RCGR			(0x1D020)
#define GCC_BLSP1_QUP4_I2C_APPS_CMD_RCGR			(0x1F020)
#define GCC_BLSP1_QUP5_I2C_APPS_CMD_RCGR			(0x21020)
#define GCC_BLSP1_QUP6_I2C_APPS_CMD_RCGR			(0x23020)
#define GCC_BLSP2_QUP1_I2C_APPS_CMD_RCGR			(0x26020)
#define GCC_BLSP2_QUP2_I2C_APPS_CMD_RCGR			(0x28020)
#define GCC_BLSP2_QUP3_I2C_APPS_CMD_RCGR			(0x2A020)
#define GCC_BLSP2_QUP4_I2C_APPS_CMD_RCGR			(0x2C020)
#define GCC_BLSP2_QUP5_I2C_APPS_CMD_RCGR			(0x2E020)
#define GCC_BLSP2_QUP6_I2C_APPS_CMD_RCGR			(0x30020)
#define GCC_BLSP1_QUP1_SPI_APPS_CMD_RCGR			(0x1900C)
#define GCC_BLSP1_UART1_APPS_CBCR				(0x1A004)
#define GCC_BLSP1_UART1_APPS_CMD_RCGR				(0x1A00C)
#define GCC_BLSP1_QUP2_SPI_APPS_CBCR				(0x1B004)
#define GCC_BLSP1_QUP2_I2C_APPS_CBCR				(0x1B008)
#define GCC_BLSP1_QUP2_SPI_APPS_CMD_RCGR			(0x1B00C)
#define GCC_BLSP1_UART2_APPS_CBCR				(0x1C004)
#define GCC_BLSP1_UART2_APPS_CMD_RCGR				(0x1C00C)
#define GCC_BLSP1_QUP3_SPI_APPS_CBCR				(0x1D004)
#define GCC_BLSP1_QUP3_I2C_APPS_CBCR				(0x1D008)
#define GCC_BLSP1_QUP3_SPI_APPS_CMD_RCGR			(0x1D00C)
#define GCC_BLSP1_UART3_APPS_CBCR				(0x1E004)
#define GCC_BLSP1_UART3_APPS_CMD_RCGR				(0x1E00C)
#define GCC_BLSP1_QUP4_SPI_APPS_CBCR				(0x1F004)
#define GCC_BLSP1_QUP4_I2C_APPS_CBCR				(0x1F008)
#define GCC_BLSP1_QUP4_SPI_APPS_CMD_RCGR			(0x1F00C)
#define GCC_BLSP1_UART4_APPS_CBCR				(0x20004)
#define GCC_BLSP1_UART4_APPS_CMD_RCGR				(0x2000C)
#define GCC_BLSP1_QUP5_SPI_APPS_CBCR				(0x21004)
#define GCC_BLSP1_QUP5_I2C_APPS_CBCR				(0x21008)
#define GCC_BLSP1_QUP5_SPI_APPS_CMD_RCGR			(0x2100C)
#define GCC_BLSP1_UART5_APPS_CBCR				(0x22004)
#define GCC_BLSP1_UART5_APPS_CMD_RCGR				(0x2200C)
#define GCC_BLSP1_QUP6_SPI_APPS_CBCR				(0x23004)
#define GCC_BLSP1_QUP6_I2C_APPS_CBCR				(0x23008)
#define GCC_BLSP1_QUP6_SPI_APPS_CMD_RCGR			(0x2300C)
#define GCC_BLSP1_UART6_APPS_CBCR				(0x24004)
#define GCC_BLSP1_UART6_APPS_CMD_RCGR				(0x2400C)
#define GCC_BLSP2_AHB_CBCR					(0x25004)
#define GCC_BLSP2_QUP1_SPI_APPS_CBCR				(0x26004)
#define GCC_BLSP2_QUP1_I2C_APPS_CBCR				(0x26008)
#define GCC_BLSP2_QUP1_SPI_APPS_CMD_RCGR			(0x2600C)
#define GCC_BLSP2_UART1_APPS_CBCR				(0x27004)
#define GCC_BLSP2_UART1_APPS_CMD_RCGR				(0x2700C)
#define GCC_BLSP2_QUP2_SPI_APPS_CBCR				(0x28004)
#define GCC_BLSP2_QUP2_I2C_APPS_CBCR				(0x28008)
#define GCC_BLSP2_QUP2_SPI_APPS_CMD_RCGR			(0x2800C)
#define GCC_BLSP2_UART2_APPS_CBCR				(0x29004)
#define GCC_BLSP2_UART2_APPS_CMD_RCGR				(0x2900C)
#define GCC_BLSP2_QUP3_SPI_APPS_CBCR				(0x2A004)
#define GCC_BLSP2_QUP3_I2C_APPS_CBCR				(0x2A008)
#define GCC_BLSP2_QUP3_SPI_APPS_CMD_RCGR			(0x2A00C)
#define GCC_BLSP2_UART3_APPS_CBCR				(0x2B004)
#define GCC_BLSP2_UART3_APPS_CMD_RCGR				(0x2B00C)
#define GCC_BLSP2_QUP4_SPI_APPS_CBCR				(0x2C004)
#define GCC_BLSP2_QUP4_I2C_APPS_CBCR				(0x2C008)
#define GCC_BLSP2_QUP4_SPI_APPS_CMD_RCGR			(0x2C00C)
#define GCC_BLSP2_UART4_APPS_CBCR				(0x2D004)
#define GCC_BLSP2_UART4_APPS_CMD_RCGR				(0x2D00C)
#define GCC_BLSP2_QUP5_SPI_APPS_CBCR				(0x2E004)
#define GCC_BLSP2_QUP5_I2C_APPS_CBCR				(0x2E008)
#define GCC_BLSP2_QUP5_SPI_APPS_CMD_RCGR			(0x2E00C)
#define GCC_BLSP2_UART5_APPS_CBCR				(0x2F004)
#define GCC_BLSP2_UART5_APPS_CMD_RCGR				(0x2F00C)
#define GCC_BLSP2_QUP6_SPI_APPS_CBCR				(0x30004)
#define GCC_BLSP2_QUP6_I2C_APPS_CBCR				(0x30008)
#define GCC_BLSP2_QUP6_SPI_APPS_CMD_RCGR			(0x3000C)
#define GCC_BLSP2_UART6_APPS_CBCR				(0x31004)
#define GCC_BLSP2_UART6_APPS_CMD_RCGR				(0x3100C)
#define GCC_PDM_AHB_CBCR					(0x33004)
#define GCC_PDM2_CBCR						(0x3300C)
#define GCC_PDM2_CMD_RCGR					(0x33010)
#define GCC_PRNG_AHB_CBCR					(0x34004)
#define GCC_TSIF_AHB_CBCR					(0x36004)
#define GCC_TSIF_REF_CBCR					(0x36008)
#define GCC_TSIF_REF_CMD_RCGR					(0x36010)
#define GCC_BOOT_ROM_AHB_CBCR					(0x38004)
#define GCC_GCC_XO_DIV4_CBCR					(0x43008)
#define GCC_APCS_GPLL_ENA_VOTE					(0x52000)
#define GCC_APCS_CLOCK_BRANCH_ENA_VOTE				(0x52004)
#define GCC_APCS_CLOCK_SLEEP_ENA_VOTE				(0x52008)
#define GCC_GCC_DEBUG_CLK_CTL					(0x62000)
#define GCC_CLOCK_FRQ_MEASURE_CTL				(0x62004)
#define GCC_CLOCK_FRQ_MEASURE_STATUS				(0x62008)
#define GCC_PLLTEST_PAD_CFG					(0x6200C)
#define GCC_GP1_CBCR						(0x64000)
#define GCC_GP1_CMD_RCGR					(0x64004)
#define GCC_GP2_CBCR						(0x65000)
#define GCC_GP2_CMD_RCGR					(0x65004)
#define GCC_GP3_CBCR						(0x66000)
#define GCC_GP3_CMD_RCGR					(0x66004)
#define GCC_GPLL4_MODE						(0x77000)
#define GCC_UFS_AXI_CBCR					(0x75008)
#define GCC_UFS_AHB_CBCR					(0x7500C)
#define GCC_UFS_TX_CFG_CBCR					(0x75010)
#define GCC_UFS_RX_CFG_CBCR					(0x75014)
#define GCC_UFS_TX_SYMBOL_0_CBCR				(0x75018)
#define GCC_UFS_RX_SYMBOL_0_CBCR				(0x7501C)
#define GCC_UFS_RX_SYMBOL_1_CBCR				(0x75020)
#define GCC_UFS_AXI_CMD_RCGR					(0x75024)
#define GCC_PCIE_0_AUX_CBCR					(0x6B014)
#define GCC_PCIE_0_BCR						(0x6B000)
#define GCC_PCIE_0_CFG_AHB_CBCR					(0x6B010)
#define GCC_PCIE_0_GDSCR					(0x6B004)
#define GCC_PCIE_0_LINK_DOWN_BCR				(0x6C014)
#define GCC_PCIE_0_MISC_RESET					(0x6C018)
#define GCC_PCIE_0_MSTR_AXI_CBCR				(0x6B00C)
#define GCC_PCIE_0_NOCSR_COM_PHY_BCR				(0x6C020)
#define GCC_PCIE_0_PHY_BCR					(0x6C01C)
#define GCC_PCIE_0_PIPE_CBCR					(0x6B018)
#define GCC_PCIE_0_SLV_AXI_CBCR					(0x6B008)
#define GCC_PCIE_1_AUX_CBCR					(0x6D014)
#define GCC_PCIE_1_BCR						(0x6D000)
#define GCC_PCIE_1_CFG_AHB_CBCR					(0x6D010)
#define GCC_PCIE_1_GDSCR					(0x6D004)
#define GCC_PCIE_1_LINK_DOWN_BCR				(0x6D030)
#define GCC_PCIE_1_MISC						(0x6D01C)
#define GCC_PCIE_1_MISC_RESET					(0x6D034)
#define GCC_PCIE_1_MSTR_AXI_CBCR				(0x6D00C)
#define GCC_PCIE_1_NOCSR_COM_PHY_BCR				(0x6D03C)
#define GCC_PCIE_1_PHY_BCR					(0x6D038)
#define GCC_PCIE_1_PIPE_CBCR					(0x6D018)
#define GCC_PCIE_1_SLV_AXI_CBCR					(0x6D008)
#define GCC_PCIE_2_AUX_CBCR					(0x6E014)
#define GCC_PCIE_2_BCR						(0x6E000)
#define GCC_PCIE_2_CFG_AHB_CBCR					(0x6E010)
#define GCC_PCIE_2_GDSCR					(0x6E004)
#define GCC_PCIE_2_LINK_DOWN_BCR				(0x6E030)
#define GCC_PCIE_2_MISC						(0x6E01C)
#define GCC_PCIE_2_MISC_RESET					(0x6E034)
#define GCC_PCIE_2_MSTR_AXI_CBCR				(0x6E00C)
#define GCC_PCIE_2_NOCSR_COM_PHY_BCR				(0x6E03C)
#define GCC_PCIE_2_PHY_BCR					(0x6E038)
#define GCC_PCIE_2_PIPE_CBCR					(0x6E018)
#define GCC_PCIE_2_SLV_AXI_CBCR					(0x6E008)
#define GCC_PCIE_AUX_CFG_RCGR					(0x6C004)
#define GCC_PCIE_AUX_CMD_RCGR					(0x6C000)
#define GCC_PCIE_AUX_D						(0x6C010)
#define GCC_PCIE_AUX_M						(0x6C008)
#define GCC_PCIE_AUX_N						(0x6C00C)
#define GCC_PCIE_CLKREF_EN					(0x88010)
#define GCC_PCIE_PHY_AUX_CBCR					(0x6F008)
#define GCC_PCIE_PHY_BCR					(0x6F000)
#define GCC_PCIE_PHY_CFG_AHB_BCR				(0x6F010)
#define GCC_PCIE_PHY_CFG_AHB_CBCR				(0x6F004)
#define GCC_PCIE_PHY_COM_BCR					(0x6F014)
#define GCC_PCIE_PHY_NOCSR_COM_PHY_BCR				(0x6F00C)
#define GCC_SYS_NOC_HS_AXI_CMD_RCGR				(0x04044)
#define GCC_USB20_MASTER_CMD_RCGR				(0x12010)
#define GCC_UFS_ICE_CORE_CMD_RCGR				(0x76014)
#define GCC_PCIE_0_PIPE_CMD_RCGR				(0x6B01C)
#define GCC_PCIE_1_PIPE_CMD_RCGR				(0x6D01C)
#define GCC_PCIE_2_PIPE_CMD_RCGR				(0x6E01C)
#define GCC_USB20_MOCK_UTMI_CMD_RCGR				(0x12024)
#define GCC_SYS_NOC_HS_AXI_CBCR					(0x04034)
#define GCC_UFS_ICE_CORE_CBCR					(0x76010)
#define GCC_UFS_UNIPRO_CORE_CBCR				(0x7600C)
#define GCC_USB20_MASTER_CBCR					(0x12004)
#define GCC_USB20_MOCK_UTMI_CBCR				(0x1200C)
#define GCC_USB20_SLEEP_CBCR					(0x12008)
#define GCC_BIMC_BCR						(0x44000)
#define GCC_BLSP1_BCR						(0x17000)
#define GCC_BLSP2_BCR						(0x25000)
#define GCC_BOOT_ROM_BCR					(0x38000)
#define GCC_PRNG_BCR						(0x34000)
#define GCC_AGGRE0_SNOC_AXI_CBCR				(0x81008)
#define GCC_AGGRE0_CNOC_AHB_CBCR				(0x8100C)
#define GCC_AGGRE0_NOC_AT_CBCR					(0x81010)
#define GCC_AGGRE0_NOC_QOSGEN_EXTREF_CTL			(0x8101C)
#define GCC_HLOS1_VOTE_LPASS_CORE_SMMU_CBCR			(0x7D010)
#define GCC_HLOS1_VOTE_LPASS_ADSP_SMMU_CBCR			(0x7D014)
#define GCC_MMSS_SYS_NOC_AXI_CBCR				(0x09004)
#define GCC_MMSS_NOC_CFG_AHB_CBCR				(0x09008)
#define GCC_USB3_CLKREF_EN					(0x8800C)
#define GCC_HDMI_CLKREF_EN					(0x88000)
#define GCC_EDP_CLKREF_EN					(0x88004)
#define GCC_UFS_CLKREF_EN					(0x88008)
#define GCC_PCIE_CLKREF_EN					(0x88010)
#define GCC_RX2_USB2_CLKREF_EN					(0x88014)
#define GCC_RX1_USB2_CLKREF_EN					(0x88018)
#define GCC_SMMU_AGGRE0_AXI_CBCR				(0x81014)
#define GCC_SMMU_AGGRE0_AHB_CBCR				(0x81018)
#define GCC_SYS_NOC_USB3_AXI_CBCR				(0x0F03C)
#define GCC_SYS_NOC_UFS_AXI_CBCR				(0x75038)
#define GCC_UFS_SYS_CLK_CORE_CBCR				(0x76030)
#define GCC_UFS_TX_SYMBOL_CLK_CORE_CBCR				(0x76034)
#define GCC_AGGRE2_USB3_AXI_CBCR				(0x83018)
#define GCC_AGGRE2_UFS_AXI_CBCR					(0x83014)
#define GCC_APCS_CLOCK_BRANCH_ENA_VOTE_1			(0x5200C)
#define GCC_MMSS_BIMC_GFX_CBCR					(0x09010)
#define GCC_BIMC_GFX_CBCR					(0x46018)
#define GCC_HMSS_RBCPR_CBCR					(0x4800C)
#define GCC_HMSS_RBCPR_CMD_RCGR					(0x48040)
