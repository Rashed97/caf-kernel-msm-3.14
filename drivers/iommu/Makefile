obj-$(CONFIG_IOMMU_API) += iommu.o
obj-$(CONFIG_IOMMU_API) += iommu-traces.o
obj-$(CONFIG_OF_IOMMU)	+= of_iommu.o
obj-$(CONFIG_MSM_IOMMU) += msm_iommu.o msm_iommu_domains.o msm_iommu_mapping.o
obj-$(CONFIG_MSM_IOMMU_V1) += msm_iommu-v1.o msm_iommu_dev-v1.o msm_iommu_sec.o
obj-$(CONFIG_MSM_IOMMU_PMON) += msm_iommu_perfmon.o
ifdef CONFIG_MSM_IOMMU_V1
obj-$(CONFIG_MSM_IOMMU_PMON) += msm_iommu_perfmon-v1.o
endif
ifdef CONFIG_IOMMU_LPAE
obj-$(CONFIG_MSM_IOMMU_V1) += msm_iommu_pagetable_lpae.o
else
obj-$(CONFIG_MSM_IOMMU_V1) += msm_iommu_pagetable.o
endif
obj-$(CONFIG_AMD_IOMMU) += amd_iommu.o amd_iommu_init.o
obj-$(CONFIG_AMD_IOMMU_V2) += amd_iommu_v2.o
obj-$(CONFIG_ARM_SMMU) += arm-smmu.o
obj-$(CONFIG_DMAR_TABLE) += dmar.o
obj-$(CONFIG_INTEL_IOMMU) += iova.o intel-iommu.o
obj-$(CONFIG_IRQ_REMAP) += intel_irq_remapping.o irq_remapping.o
obj-$(CONFIG_OMAP_IOMMU) += omap-iommu.o
obj-$(CONFIG_OMAP_IOMMU) += omap-iommu2.o
obj-$(CONFIG_OMAP_IOVMM) += omap-iovmm.o
obj-$(CONFIG_OMAP_IOMMU_DEBUG) += omap-iommu-debug.o
obj-$(CONFIG_TEGRA_IOMMU_GART) += tegra-gart.o
obj-$(CONFIG_TEGRA_IOMMU_SMMU) += tegra-smmu.o
obj-$(CONFIG_EXYNOS_IOMMU) += exynos-iommu.o
obj-$(CONFIG_SHMOBILE_IOMMU) += shmobile-iommu.o
obj-$(CONFIG_SHMOBILE_IPMMU) += shmobile-ipmmu.o
obj-$(CONFIG_FSL_PAMU) += fsl_pamu.o fsl_pamu_domain.o
