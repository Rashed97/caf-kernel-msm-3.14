obj-$(CONFIG_COMMON_CLK_QCOM) += clk-qcom.o

clk-qcom-y += common.o
clk-qcom-y += clk-regmap.o
clk-qcom-y += clk-pll.o
clk-qcom-y += clk-rcg.o
clk-qcom-y += clk-rcg2.o
clk-qcom-y += clk-branch.o
clk-qcom-y += reset.o

obj-$(CONFIG_APQ_GCC_8084) += gcc-apq8084.o
obj-$(CONFIG_APQ_MMCC_8084) += mmcc-apq8084.o
obj-$(CONFIG_IPQ_GCC_806X) += gcc-ipq806x.o
obj-$(CONFIG_MSM_GCC_8660) += gcc-msm8660.o
obj-$(CONFIG_MSM_GCC_8960) += gcc-msm8960.o
obj-$(CONFIG_MSM_GCC_8974) += gcc-msm8974.o
obj-$(CONFIG_MSM_MMCC_8960) += mmcc-msm8960.o
obj-$(CONFIG_MSM_MMCC_8974) += mmcc-msm8974.o
obj-y				+= clock-pm.o

# MSM8992
obj-$(CONFIG_ARCH_MSM8992)	+= clock-rpm-8992.o
obj-$(CONFIG_ARCH_MSM8992)	+= clock-gcc-8992.o
obj-$(CONFIG_ARCH_MSM8992)	+= clock-mmss-8992.o

# MSMTELLURIUM
obj-$(CONFIG_ARCH_MSM8916)	+=clock-gcc-tellurium.o
