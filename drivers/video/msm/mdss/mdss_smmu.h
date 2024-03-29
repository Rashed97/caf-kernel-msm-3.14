/* Copyright (c) 2007-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MDSS_SMMU_H
#define MDSS_SMMU_H

#include <linux/msm_ion.h>
#include <linux/msm_mdp.h>
#include <linux/mdss_io_util.h>
#include <linux/msm_iommu_domains.h>

#include "mdss.h"
#include "mdss_mdp.h"
#include "mdss_debug.h"

#define MDSS_SMMU_COMPATIBLE "qcom,smmu"

enum mdss_smmu_version {
	MDSS_SMMU_V1,
	MDSS_SMMU_V2
};

void mdss_smmu_register(struct device *dev);
int mdss_smmu_init(struct mdss_data_type *mdata, struct device *dev);

static inline bool is_mdss_smmu_compatible_device(const char *str)
{
	/* check the prefix */
	return (!strncmp(str, MDSS_SMMU_COMPATIBLE,
			strlen(MDSS_SMMU_COMPATIBLE))) ? true : false;
}

static inline struct mdss_smmu_client *mdss_smmu_get_cb(u32 domain)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	return (domain >= MDSS_IOMMU_MAX_DOMAIN) ? NULL :
			&mdata->mdss_smmu[domain];
}

static inline struct ion_client *mdss_get_ionclient(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	return mdata ? mdata->iclient : NULL;
}

static inline int is_mdss_iommu_attached(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	return mdata ? mdata->iommu_attached : false;
}

/*
 * mdss_smmu_is_valid_domain_type()
 *
 * Used to check if rotator smmu domain is defined or not by checking if
 * vbif base is defined. As those are associated.
 */
static inline bool mdss_smmu_is_valid_domain_type(struct mdss_data_type *mdata,
		int domain_type)
{
	if ((domain_type == MDSS_IOMMU_DOMAIN_ROT_UNSECURE ||
			domain_type == MDSS_IOMMU_DOMAIN_ROT_SECURE) &&
			!mdss_mdp_is_nrt_vbif_base_defined(mdata))
		return false;
	return true;
}

static inline int mdss_smmu_get_domain_type(u32 flags, bool rotator)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int type;

	if (flags & MDP_SECURE_OVERLAY_SESSION) {
		type = (rotator &&
			mdata->mdss_smmu[MDSS_IOMMU_DOMAIN_ROT_SECURE].dev) ?
			MDSS_IOMMU_DOMAIN_ROT_SECURE : MDSS_IOMMU_DOMAIN_SECURE;
	} else {
		type = (rotator &&
			mdata->mdss_smmu[MDSS_IOMMU_DOMAIN_ROT_UNSECURE].dev) ?
			MDSS_IOMMU_DOMAIN_ROT_UNSECURE :
			MDSS_IOMMU_DOMAIN_UNSECURE;
	}
	return type;
}

static inline int mdss_smmu_attach(struct mdss_data_type *mdata)
{
	int rc;

	MDSS_XLOG(mdata->iommu_attached);
	if (mdata->iommu_attached) {
		pr_debug("mdp iommu already attached\n");
		return 0;
	}

	if (!mdata->smmu_ops.smmu_attach)
		return -ENOSYS;

	rc =  mdata->smmu_ops.smmu_attach(mdata);
	if (!rc)
		mdata->iommu_attached = true;
	return rc;
}

static inline int mdss_smmu_detach(struct mdss_data_type *mdata)
{
	int rc;

	MDSS_XLOG(mdata->iommu_attached);

	if (!mdata->iommu_attached) {
		pr_debug("mdp iommu already dettached\n");
		return 0;
	}

	if (!mdata->smmu_ops.smmu_detach)
		return -ENOSYS;

	rc = mdata->smmu_ops.smmu_detach(mdata);
	if (!rc)
		mdata->iommu_attached = false;
	return rc;
}

static inline int mdss_smmu_get_domain_id(u32 type)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (!mdata || !mdata->smmu_ops.smmu_get_domain_id
			|| type >= MDSS_IOMMU_MAX_DOMAIN)
		return -ENODEV;

	return mdata->smmu_ops.smmu_get_domain_id(type);
}

static inline struct dma_buf_attachment *mdss_smmu_dma_buf_attach(
		struct dma_buf *dma_buf, struct device *dev, int domain)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (!mdata->smmu_ops.smmu_dma_buf_attach)
		return NULL;

	return mdata->smmu_ops.smmu_dma_buf_attach(dma_buf, dev, domain);
}

static inline int mdss_smmu_map_dma_buf(struct dma_buf *dma_buf,
		struct sg_table *table, int domain, dma_addr_t *iova,
		unsigned long *size, int dir)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (!mdata->smmu_ops.smmu_map_dma_buf)
		return -ENOSYS;

	return mdata->smmu_ops.smmu_map_dma_buf(dma_buf, table,
			domain, iova, size, dir);
}

static inline void mdss_smmu_unmap_dma_buf(struct sg_table *table, int domain,
		int dir)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata->smmu_ops.smmu_unmap_dma_buf)
		mdata->smmu_ops.smmu_unmap_dma_buf(table, domain, dir);
}

static inline int mdss_smmu_dma_alloc_coherent(struct device *dev, size_t size,
		dma_addr_t *phys, dma_addr_t *iova, void *cpu_addr,
		gfp_t gfp, int domain)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (!mdata->smmu_ops.smmu_dma_alloc_coherent)
		return -ENOSYS;

	return mdata->smmu_ops.smmu_dma_alloc_coherent(dev, size,
			phys, iova, cpu_addr, gfp, domain);
}

static inline void mdss_smmu_dma_free_coherent(struct device *dev, size_t size,
		void *cpu_addr, dma_addr_t phys, dma_addr_t iova, int domain)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata->smmu_ops.smmu_dma_free_coherent)
		mdata->smmu_ops.smmu_dma_free_coherent(dev, size, cpu_addr,
			phys, iova, domain);
}

static inline int mdss_smmu_map(int domain, phys_addr_t iova, phys_addr_t phys,
		int gfp_order, int prot)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (!mdata->smmu_ops.smmu_map)
		return -ENOSYS;

	return mdata->smmu_ops.smmu_map(domain, iova, phys, gfp_order, prot);
}

static inline void mdss_smmu_unmap(int domain, unsigned long iova,
		int gfp_order)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata->smmu_ops.smmu_unmap)
		mdata->smmu_ops.smmu_unmap(domain, iova, gfp_order);
}

static inline char *mdss_smmu_dsi_alloc_buf(struct device *dev, int size,
		dma_addr_t *dmap, gfp_t gfp)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (!mdata->smmu_ops.smmu_dsi_alloc_buf)
		return NULL;

	return mdata->smmu_ops.smmu_dsi_alloc_buf(dev, size, dmap, gfp);
}

static inline int mdss_smmu_dsi_map_buffer(phys_addr_t phys,
		unsigned int domain, unsigned long size, dma_addr_t *dma_addr,
		void *cpu_addr, int dir)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (!mdata->smmu_ops.smmu_dsi_map_buffer)
		return -ENOSYS;

	return mdata->smmu_ops.smmu_dsi_map_buffer(phys, domain, size,
			dma_addr, cpu_addr, dir);
}

static inline void mdss_smmu_dsi_unmap_buffer(dma_addr_t dma_addr, int domain,
		unsigned long size, int dir)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata->smmu_ops.smmu_dsi_unmap_buffer)
		mdata->smmu_ops.smmu_dsi_unmap_buffer(dma_addr, domain,
			size, dir);
}

static inline void mdss_smmu_deinit(struct mdss_data_type *mdata)
{
	if (mdata->smmu_ops.smmu_deinit)
		mdata->smmu_ops.smmu_deinit(mdata);
}

#endif /* MDSS_SMMU_H */
