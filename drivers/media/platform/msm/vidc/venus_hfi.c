/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#include <asm/dma-iommu.h>
#include <asm/memory.h>
#include <linux/coresight-stm.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/qcom_iommu.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/subsystem_restart.h>
#include "hfi_packetization.h"
#include "msm_vidc_debug.h"
#include "venus_hfi.h"
#include "vidc_hfi_io.h"

#define FIRMWARE_SIZE			0X00A00000
#define REG_ADDR_OFFSET_BITMASK	0x000FFFFF
#define QDSS_IOVA_START 0x80001000

static struct hal_device_data hal_ctxt;

#define TZBSP_MEM_PROTECT_VIDEO_VAR 0x8
struct tzbsp_memprot {
	u32 cp_start;
	u32 cp_size;
	u32 cp_nonpixel_start;
	u32 cp_nonpixel_size;
};

struct tzbsp_resp {
	int ret;
};

#define TZBSP_VIDEO_SET_STATE 0xa

/* Poll interval in uS */
#define POLL_INTERVAL_US 50

enum tzbsp_video_state {
	TZBSP_VIDEO_STATE_SUSPEND = 0,
	TZBSP_VIDEO_STATE_RESUME
};

struct tzbsp_video_set_state_req {
	u32 state; /*shoud be tzbsp_video_state enum value*/
	u32 spare; /*reserved for future, should be zero*/
};

const struct msm_vidc_gov_data DEFAULT_BUS_VOTE = {
	.data = NULL,
	.data_count = 0,
	.imem_size = 0,
};

static void venus_hfi_pm_hndlr(struct work_struct *work);
static DECLARE_DELAYED_WORK(venus_hfi_pm_work, venus_hfi_pm_hndlr);
static int venus_hfi_power_enable(void *dev);
static inline int venus_hfi_power_on(
	struct venus_hfi_device *device);
static int venus_hfi_disable_regulators(struct venus_hfi_device *device);
static int venus_hfi_enable_regulators(struct venus_hfi_device *device);
static inline int venus_hfi_prepare_enable_clks(
	struct venus_hfi_device *device);
static inline void venus_hfi_disable_unprepare_clks(
	struct venus_hfi_device *device);
static void venus_hfi_flush_debug_queue(
	struct venus_hfi_device *device, u8 *packet);
static int venus_hfi_initialize_packetization(struct venus_hfi_device *device);

static inline void venus_hfi_set_state(struct venus_hfi_device *device,
		enum venus_hfi_state state)
{
	mutex_lock(&device->write_lock);
	mutex_lock(&device->read_lock);
	device->state = state;
	mutex_unlock(&device->write_lock);
	mutex_unlock(&device->read_lock);
}

static inline bool venus_hfi_core_in_valid_state(
		struct venus_hfi_device *device)
{
	return device->state != VENUS_STATE_DEINIT;
}

static void venus_hfi_dump_packet(u8 *packet)
{
	u32 c = 0, packet_size = *(u32 *)packet;
	const int row_size = 32;
	/* row must contain enough for 0xdeadbaad * 8 to be converted into
	 * "de ad ba ab " * 8 + '\0' */
	char row[3 * row_size];

	for (c = 0; c * row_size < packet_size; ++c) {
		int bytes_to_read = ((c + 1) * row_size > packet_size) ?
			packet_size % row_size : row_size;
		hex_dump_to_buffer(packet + c * row_size, bytes_to_read,
				row_size, 4, row, sizeof(row), false);
		dprintk(VIDC_PKT, "%s\n", row);
	}
}

static void venus_hfi_sim_modify_cmd_packet(u8 *packet,
				struct venus_hfi_device *device)
{
	struct hfi_cmd_sys_session_init_packet *sys_init;
	struct hal_session *session = NULL;
	u8 i;
	phys_addr_t fw_bias = 0;

	if (!device || !packet) {
		dprintk(VIDC_ERR, "Invalid Param\n");
		return;
	} else if (!device->hal_data->firmware_base
			|| is_iommu_present(device->res)) {
		return;
	}

	fw_bias = device->hal_data->firmware_base;
	sys_init = (struct hfi_cmd_sys_session_init_packet *)packet;

	/* Ideally we should acquire device->session_lock. If we acquire
	 * we may go to deadlock with inst->*_lock between two threads.
	 * Ex : in the forward path we acquire inst->internalbufs.lock and
	 * session_lock and in the reverse path, we acquire session_lock and
	 * internalbufs.lock. So this may introduce deadlock. So we are not
	 * doing that. On virtio it is less likely to run two sessions
	 * concurrently. So it should be fine */

	session = hfi_process_get_session(
			&device->sess_head, sys_init->session_id);
	if (!session) {
		dprintk(VIDC_DBG, "%s :Invalid session id: %x\n",
				__func__, sys_init->session_id);
		return;
	}
	switch (sys_init->packet_type) {
	case HFI_CMD_SESSION_EMPTY_BUFFER:
		if (session->is_decoder) {
			struct hfi_cmd_session_empty_buffer_compressed_packet
			*pkt = (struct
			hfi_cmd_session_empty_buffer_compressed_packet
			*) packet;
			pkt->packet_buffer -= fw_bias;
		} else {
			struct
			hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
			*pkt = (struct
			hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
			*) packet;
			pkt->packet_buffer -= fw_bias;
		}
		break;
	case HFI_CMD_SESSION_FILL_BUFFER:
	{
		struct hfi_cmd_session_fill_buffer_packet *pkt =
			(struct hfi_cmd_session_fill_buffer_packet *)packet;
		pkt->packet_buffer -= fw_bias;
		break;
	}
	case HFI_CMD_SESSION_SET_BUFFERS:
	{
		struct hfi_cmd_session_set_buffers_packet *pkt =
			(struct hfi_cmd_session_set_buffers_packet *)packet;
		if (pkt->buffer_type == HFI_BUFFER_OUTPUT ||
			pkt->buffer_type == HFI_BUFFER_OUTPUT2) {
			struct hfi_buffer_info *buff;
			buff = (struct hfi_buffer_info *) pkt->rg_buffer_info;
			buff->buffer_addr -= fw_bias;
			if (buff->extra_data_addr >= fw_bias)
				buff->extra_data_addr -= fw_bias;
		} else {
			for (i = 0; i < pkt->num_buffers; i++)
				pkt->rg_buffer_info[i] -= fw_bias;
		}
		break;
	}
	case HFI_CMD_SESSION_RELEASE_BUFFERS:
	{
		struct hfi_cmd_session_release_buffer_packet *pkt =
			(struct hfi_cmd_session_release_buffer_packet *)packet;
		if (pkt->buffer_type == HFI_BUFFER_OUTPUT ||
			pkt->buffer_type == HFI_BUFFER_OUTPUT2) {
			struct hfi_buffer_info *buff;
			buff = (struct hfi_buffer_info *) pkt->rg_buffer_info;
			buff->buffer_addr -= fw_bias;
			buff->extra_data_addr -= fw_bias;
		} else {
			for (i = 0; i < pkt->num_buffers; i++)
				pkt->rg_buffer_info[i] -= fw_bias;
		}
		break;
	}
	case HFI_CMD_SESSION_PARSE_SEQUENCE_HEADER:
	{
		struct hfi_cmd_session_parse_sequence_header_packet *pkt =
			(struct hfi_cmd_session_parse_sequence_header_packet *)
		packet;
		pkt->packet_buffer -= fw_bias;
		break;
	}
	case HFI_CMD_SESSION_GET_SEQUENCE_HEADER:
	{
		struct hfi_cmd_session_get_sequence_header_packet *pkt =
			(struct hfi_cmd_session_get_sequence_header_packet *)
		packet;
		pkt->packet_buffer -= fw_bias;
		break;
	}
	default:
		break;
	}
}

static int venus_hfi_acquire_regulator(struct regulator_info *rinfo)
{
	int rc = 0;

	if (rinfo->has_hw_power_collapse) {
		rc = regulator_set_mode(rinfo->regulator,
				REGULATOR_MODE_NORMAL);
		if (rc) {
			/*
			* This is somewhat fatal, but nothing we can do
			* about it. We can't disable the regulator w/o
			* getting it back under s/w control
			*/
			dprintk(VIDC_WARN,
				"Failed to acquire regulator control: %s\n",
					rinfo->name);
		} else {

			dprintk(VIDC_DBG,
					"Acquire regulator control from HW: %s\n",
					rinfo->name);

		}
	}
	WARN_ON(!regulator_is_enabled(rinfo->regulator));
	return rc;
}

static int venus_hfi_hand_off_regulator(struct regulator_info *rinfo)
{
	int rc = 0;

	if (rinfo->has_hw_power_collapse) {
		rc = regulator_set_mode(rinfo->regulator,
				REGULATOR_MODE_FAST);
		if (rc) {
			dprintk(VIDC_WARN,
				"Failed to hand off regulator control: %s\n",
					rinfo->name);
		} else {
			dprintk(VIDC_DBG,
					"Hand off regulator control to HW: %s\n",
					rinfo->name);
		}
	}

	return rc;
}

static int venus_hfi_hand_off_regulators(struct venus_hfi_device *device)
{
	struct regulator_info *rinfo;
	int rc = 0, c = 0;

	venus_hfi_for_each_regulator(device, rinfo) {
		rc = venus_hfi_hand_off_regulator(rinfo);
		/*
		* If one regulator hand off failed, driver should take
		* the control for other regulators back.
		*/
		if (rc)
			goto err_reg_handoff_failed;
		c++;
	}

	return rc;
err_reg_handoff_failed:
	venus_hfi_for_each_regulator_reverse_continue(device, rinfo, c)
		venus_hfi_acquire_regulator(rinfo);

	return rc;
}

static int venus_hfi_acquire_regulators(struct venus_hfi_device *device)
{
	int rc = 0;
	struct regulator_info *rinfo;

	dprintk(VIDC_DBG, "Enabling regulators\n");

	venus_hfi_for_each_regulator(device, rinfo) {
		if (rinfo->has_hw_power_collapse) {
			/*
			 * Once driver has the control, it restores the
			 * previous state of regulator. Hence driver no
			 * need to call regulator_enable for these.
			 */
			rc = venus_hfi_acquire_regulator(rinfo);
			if (rc) {
				dprintk(VIDC_WARN,
						"Failed: Aqcuire control: %s\n",
						rinfo->name);
				break;
			}
		}
	}
	return rc;
}

static int venus_hfi_write_queue(void *info, u8 *packet, u32 *rx_req_is_set)
{
	struct hfi_queue_header *queue;
	u32 packet_size_in_words, new_write_idx;
	struct vidc_iface_q_info *qinfo;
	u32 empty_space, read_idx;
	u32 *write_ptr;

	if (!info || !packet || !rx_req_is_set) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}

	qinfo =	(struct vidc_iface_q_info *) info;
	if (!qinfo || !qinfo->q_array.align_virtual_addr) {
		dprintk(VIDC_WARN, "Queues have already been freed\n");
		return -EINVAL;
	}

	queue = (struct hfi_queue_header *) qinfo->q_hdr;

	if (!queue) {
		dprintk(VIDC_ERR, "queue not present\n");
		return -ENOENT;
	}

	if (msm_vidc_debug & VIDC_PKT) {
		dprintk(VIDC_PKT, "%s: %p\n", __func__, qinfo);
		venus_hfi_dump_packet(packet);
	}

	packet_size_in_words = (*(u32 *)packet) >> 2;
	if (!packet_size_in_words) {
		dprintk(VIDC_ERR, "Zero packet size\n");
		return -ENODATA;
	}

	read_idx = queue->qhdr_read_idx;

	empty_space = (queue->qhdr_write_idx >=  read_idx) ?
		(queue->qhdr_q_size - (queue->qhdr_write_idx -  read_idx)) :
		(read_idx - queue->qhdr_write_idx);
	if (empty_space <= packet_size_in_words) {
		queue->qhdr_tx_req =  1;
		dprintk(VIDC_ERR, "Insufficient size (%d) to write (%d)\n",
					  empty_space, packet_size_in_words);
		return -ENOTEMPTY;
	}

	queue->qhdr_tx_req =  0;

	new_write_idx = (queue->qhdr_write_idx + packet_size_in_words);
	write_ptr = (u32 *)((qinfo->q_array.align_virtual_addr) +
		(queue->qhdr_write_idx << 2));
	if (new_write_idx < queue->qhdr_q_size) {
		memcpy(write_ptr, packet, packet_size_in_words << 2);
	} else {
		new_write_idx -= queue->qhdr_q_size;
		memcpy(write_ptr, packet, (packet_size_in_words -
			new_write_idx) << 2);
		memcpy((void *)qinfo->q_array.align_virtual_addr,
			packet + ((packet_size_in_words - new_write_idx) << 2),
			new_write_idx  << 2);
	}
	/* Memory barrier to make sure packet is written before updating the
	 * write index */
	mb();
	queue->qhdr_write_idx = new_write_idx;
	*rx_req_is_set = (1 == queue->qhdr_rx_req) ? 1 : 0;
	/*Memory barrier to make sure write index is updated before an
	 * interupt is raised on venus.*/
	mb();
	return 0;
}

static void venus_hfi_hal_sim_modify_msg_packet(u8 *packet,
					struct venus_hfi_device *device)
{
	struct hfi_msg_sys_session_init_done_packet *sys_idle;
	struct hal_session *session = NULL;
	phys_addr_t fw_bias = 0;

	if (!device || !packet) {
		dprintk(VIDC_ERR, "Invalid Param\n");
		return;
	} else if (!device->hal_data->firmware_base
			|| is_iommu_present(device->res)) {
		return;
	}

	fw_bias = device->hal_data->firmware_base;
	sys_idle = (struct hfi_msg_sys_session_init_done_packet *)packet;
	if (&device->session_lock) {
		mutex_lock(&device->session_lock);
		session = hfi_process_get_session(
				&device->sess_head, sys_idle->session_id);
		mutex_unlock(&device->session_lock);
	}
	if (!session) {
		dprintk(VIDC_DBG, "%s: Invalid session id: %x\n",
				__func__, sys_idle->session_id);
		return;
	}
	switch (sys_idle->packet_type) {
	case HFI_MSG_SESSION_FILL_BUFFER_DONE:
		if (session->is_decoder) {
			struct
			hfi_msg_session_fbd_uncompressed_plane0_packet
			*pkt_uc = (struct
			hfi_msg_session_fbd_uncompressed_plane0_packet
			*) packet;
			pkt_uc->packet_buffer += fw_bias;
		} else {
			struct
			hfi_msg_session_fill_buffer_done_compressed_packet
			*pkt = (struct
			hfi_msg_session_fill_buffer_done_compressed_packet
			*) packet;
			pkt->packet_buffer += fw_bias;
		}
		break;
	case HFI_MSG_SESSION_EMPTY_BUFFER_DONE:
	{
		struct hfi_msg_session_empty_buffer_done_packet *pkt =
		(struct hfi_msg_session_empty_buffer_done_packet *)packet;
		pkt->packet_buffer += fw_bias;
		break;
	}
	case HFI_MSG_SESSION_GET_SEQUENCE_HEADER_DONE:
	{
		struct
		hfi_msg_session_get_sequence_header_done_packet
		*pkt =
		(struct hfi_msg_session_get_sequence_header_done_packet *)
		packet;
		pkt->sequence_header += fw_bias;
		break;
	}
	default:
		break;
	}
}

static int venus_hfi_read_queue(void *info, u8 *packet, u32 *pb_tx_req_is_set)
{
	struct hfi_queue_header *queue;
	u32 packet_size_in_words, new_read_idx;
	u32 *read_ptr;
	u32 receive_request = 0;
	struct vidc_iface_q_info *qinfo;
	int rc = 0;

	if (!info || !packet || !pb_tx_req_is_set) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}

	qinfo = (struct vidc_iface_q_info *) info;
	if (!qinfo || !qinfo->q_array.align_virtual_addr) {
		dprintk(VIDC_WARN, "Queues have already been freed\n");
		return -EINVAL;
	}
	/*Memory barrier to make sure data is valid before
	 *reading it*/
	mb();
	queue = (struct hfi_queue_header *) qinfo->q_hdr;

	if (!queue) {
		dprintk(VIDC_ERR, "Queue memory is not allocated\n");
		return -ENOMEM;
	}

	/*
	 * Do not set receive request for debug queue, if set,
	 * Venus generates interrupt for debug messages even
	 * when there is no response message available.
	 * In general debug queue will not become full as it
	 * is being emptied out for every interrupt from Venus.
	 * Venus will anyway generates interrupt if it is full.
	 */
	if (queue->qhdr_type & HFI_Q_ID_CTRL_TO_HOST_MSG_Q)
		receive_request = 1;

	if (queue->qhdr_read_idx == queue->qhdr_write_idx) {
		queue->qhdr_rx_req = receive_request;
		*pb_tx_req_is_set = 0;
		dprintk(VIDC_DBG,
			"%s queue is empty, rx_req = %u, tx_req = %u, read_idx = %u\n",
			receive_request ? "message" : "debug",
			queue->qhdr_rx_req, queue->qhdr_tx_req,
			queue->qhdr_read_idx);
		return -ENODATA;
	}

	read_ptr = (u32 *)((qinfo->q_array.align_virtual_addr) +
				(queue->qhdr_read_idx << 2));
	packet_size_in_words = (*read_ptr) >> 2;
	if (!packet_size_in_words) {
		dprintk(VIDC_ERR, "Zero packet size\n");
		return -ENODATA;
	}

	new_read_idx = queue->qhdr_read_idx + packet_size_in_words;
	if (((packet_size_in_words << 2) <= VIDC_IFACEQ_VAR_HUGE_PKT_SIZE)
			&& queue->qhdr_read_idx <= queue->qhdr_q_size) {
		if (new_read_idx < queue->qhdr_q_size) {
			memcpy(packet, read_ptr,
					packet_size_in_words << 2);
		} else {
			new_read_idx -= queue->qhdr_q_size;
			memcpy(packet, read_ptr,
			(packet_size_in_words - new_read_idx) << 2);
			memcpy(packet + ((packet_size_in_words -
					new_read_idx) << 2),
					(u8 *)qinfo->q_array.align_virtual_addr,
					new_read_idx << 2);
		}
	} else {
		dprintk(VIDC_WARN,
			"BAD packet received, read_idx: %#x, pkt_size: %d\n",
			queue->qhdr_read_idx, packet_size_in_words << 2);
		dprintk(VIDC_WARN, "Dropping this packet\n");
		new_read_idx = queue->qhdr_write_idx;
		rc = -ENODATA;
	}

	queue->qhdr_read_idx = new_read_idx;

	if (queue->qhdr_read_idx != queue->qhdr_write_idx)
		queue->qhdr_rx_req = 0;
	else
		queue->qhdr_rx_req = receive_request;

	*pb_tx_req_is_set = (1 == queue->qhdr_tx_req) ? 1 : 0;

	if (msm_vidc_debug & VIDC_PKT) {
		dprintk(VIDC_PKT, "%s: %p\n", __func__, qinfo);
		venus_hfi_dump_packet(packet);
	}

	return rc;
}

static int venus_hfi_alloc(struct venus_hfi_device *dev, void *mem,
			u32 size, u32 align, u32 flags, u32 usage)
{
	struct vidc_mem_addr *vmem = NULL;
	struct msm_smem *alloc = NULL;
	int rc = 0;

	if (!dev || !dev->hal_client || !mem || !size) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}

	vmem = (struct vidc_mem_addr *)mem;
	dprintk(VIDC_INFO, "start to alloc size: %d, flags: %d\n", size, flags);

	venus_hfi_power_enable(dev);

	alloc = msm_smem_alloc(dev->hal_client, size, align, flags, usage, 1);
	if (!alloc) {
		dprintk(VIDC_ERR, "Alloc failed\n");
		rc = -ENOMEM;
		goto fail_smem_alloc;
	}
	dprintk(VIDC_DBG, "venus_hfi_alloc: ptr = %p, size = %d\n",
			alloc->kvaddr, size);
	rc = msm_smem_cache_operations(dev->hal_client, alloc,
		SMEM_CACHE_CLEAN);
	if (rc) {
		dprintk(VIDC_WARN, "Failed to clean cache\n");
		dprintk(VIDC_WARN, "This may result in undefined behavior\n");
	}
	vmem->mem_size = alloc->size;
	vmem->mem_data = alloc;
	vmem->align_virtual_addr = alloc->kvaddr;
	vmem->align_device_addr = alloc->device_addr;
	return rc;
fail_smem_alloc:
	return rc;
}

static void venus_hfi_free(struct venus_hfi_device *dev, struct msm_smem *mem)
{
	if (!dev || !mem) {
		dprintk(VIDC_ERR, "invalid param %p %p\n", dev, mem);
		return;
	}

	if (venus_hfi_power_on(dev))
		dprintk(VIDC_ERR, "%s: Power on failed\n", __func__);

	msm_smem_free(dev->hal_client, mem);
}

static void venus_hfi_write_register(
		struct venus_hfi_device *device, u32 reg, u32 value)
{
	u32 hwiosymaddr = reg;
	u8 *base_addr;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return;
	}
	if (device->clk_state != ENABLED_PREPARED) {
		dprintk(VIDC_WARN,
			"HFI Write register failed : Clocks are OFF\n");
		return;
	}

	base_addr = device->hal_data->register_base;
	dprintk(VIDC_DBG, "Base addr: %p, written to: %#x, Value: %#x...\n",
		base_addr, hwiosymaddr, value);
	base_addr += hwiosymaddr;
	writel_relaxed(value, base_addr);
	wmb();
}

static int venus_hfi_read_register(struct venus_hfi_device *device, u32 reg)
{
	int rc = 0;
	u8 *base_addr;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	if (device->clk_state != ENABLED_PREPARED) {
		dprintk(VIDC_WARN,
			"HFI Read register failed : Clocks are OFF\n");
		return -EINVAL;
	}
	base_addr = device->hal_data->register_base;

	rc = readl_relaxed(base_addr + reg);
	rmb();
	dprintk(VIDC_DBG, "Base addr: %p, read from: %#x, value: %#x...\n",
		base_addr, reg, rc);

	return rc;
}

static void venus_hfi_set_registers(struct venus_hfi_device *device)
{
	struct reg_set *reg_set;
	int i;

	if (!device->res) {
		dprintk(VIDC_ERR,
			"device resources null, cannot set registers\n");
		return;
	}

	reg_set = &device->res->reg_set;
	for (i = 0; i < reg_set->count; i++) {
		venus_hfi_write_register(device,
				reg_set->reg_tbl[i].reg,
				reg_set->reg_tbl[i].value);
	}
}

static int venus_hfi_core_start_cpu(struct venus_hfi_device *device)
{
	u32 ctrl_status = 0, count = 0, rc = 0;
	int max_tries = 100;
	venus_hfi_write_register(device,
			VIDC_WRAPPER_INTR_MASK,
			VIDC_WRAPPER_INTR_MASK_A2HVCODEC_BMSK);

	while (!ctrl_status && count < max_tries) {
		ctrl_status = venus_hfi_read_register(
				device,
				VIDC_CPU_CS_SCIACMDARG0);
		if ((ctrl_status & 0xFE) == 0x4) {
			dprintk(VIDC_ERR, "invalid setting for UC_REGION\n");
			break;
		}
		usleep_range(500, 1000);
		count++;
	}
	if (count >= max_tries)
		rc = -ETIME;
	return rc;
}

static void venus_hfi_iommu_detach(void *dev)
{
	struct context_bank_info *cb;
	struct venus_hfi_device *device = dev;

	if (!device || !device->res) {
		dprintk(VIDC_ERR, "Invalid paramter: %p\n", device);
		return;
	}

	list_for_each_entry(cb, &device->res->context_banks, list) {
		if (cb->dev)
			arm_iommu_detach_device(cb->dev);
		if (cb->mapping)
			arm_iommu_release_mapping(cb->mapping);
	}
}

static bool venus_hfi_is_session_supported(unsigned long sessions_supported,
		enum vidc_bus_vote_data_session session_type)
{
	bool same_codec, same_session_type;
	int codec_bit, session_type_bit;
	unsigned long session = session_type;

	if (!sessions_supported || !session)
		return false;

	/* ffs returns a 1 indexed, test_bit takes a 0 indexed...index */
	codec_bit = ffs(session) - 1;
	session_type_bit = codec_bit + 1;

	same_codec = test_bit(codec_bit, &sessions_supported) ==
		test_bit(codec_bit, &session);
	same_session_type = test_bit(session_type_bit, &sessions_supported) ==
		test_bit(session_type_bit, &session);

	return same_codec && same_session_type;
}

static int venus_hfi_devfreq_target(struct device *devfreq_dev,
		unsigned long *freq, u32 flags)
{
	int rc = 0;
	uint64_t ab = 0;
	struct bus_info *bus = NULL, *temp = NULL;
	struct venus_hfi_device *device = dev_get_drvdata(devfreq_dev);

	venus_hfi_for_each_bus(device, temp) {
		if (temp->dev == devfreq_dev) {
			bus = temp;
			break;
		}
	}

	if (!bus) {
		rc = -EBADHANDLE;
		goto err_unknown_device;
	}

	/* we expect governors to provide values in kBps form, convert to Bps */
	ab = *freq * 1000;
	rc = msm_bus_scale_update_bw(bus->client, ab, 0);
	if (rc) {
		dprintk(VIDC_ERR, "Failed voting bus %s to ab %llu\n: %d",
				bus->name, ab, rc);
		goto err_unknown_device;
	}

	dprintk(VIDC_PROF, "Voting bus %s to ab %llu\n", bus->name, ab);

	return 0;
err_unknown_device:
	return rc;
}

static int venus_hfi_devfreq_get_status(struct device *devfreq_dev,
		struct devfreq_dev_status *stat)
{
	int rc = 0;
	struct bus_info *bus = NULL, *temp = NULL;
	struct venus_hfi_device *device = dev_get_drvdata(devfreq_dev);

	venus_hfi_for_each_bus(device, temp) {
		if (temp->dev == devfreq_dev) {
			bus = temp;
			break;
		}
	}

	if (!bus) {
		rc = -EBADHANDLE;
		goto err_unknown_device;
	}

	*stat = (struct devfreq_dev_status) {
		.private_data = &device->bus_vote,
		/*
		 * Put in dummy place holder values for upstream govs, our
		 * custom gov only needs .private_data.  We should fill this in
		 * properly if we can actually measure busy_time accurately
		 * (which we can't at the moment)
		 */
		.total_time = 1,
		.busy_time = 1,
		.current_frequency = 0,
	};

err_unknown_device:
	return rc;
}

static int venus_hfi_unvote_buses(void *dev)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;
	struct bus_info *bus = NULL;

	if (!dev) {
		dprintk(VIDC_ERR, "Invalid device in %s\n", __func__);
		return -EINVAL;
	}

	kfree(device->bus_vote.data);
	device->bus_vote = DEFAULT_BUS_VOTE;

	venus_hfi_for_each_bus(device, bus) {
		int local_rc = 0;
		unsigned long zero = 0;

		devfreq_suspend_device(bus->devfreq);
		local_rc = venus_hfi_devfreq_target(bus->dev, &zero, 0);
		rc = rc ?: local_rc;
	}

	if (rc)
		dprintk(VIDC_WARN, "Failed to unvote some buses\n");

	return rc;
}

static int venus_hfi_vote_buses(void *dev,
		struct vidc_bus_vote_data *data, int num_data)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;
	struct bus_info *bus = NULL;
	struct vidc_bus_vote_data *new_data = NULL;

	if (!dev) {
		dprintk(VIDC_ERR, "Invalid device\n");
		return -EINVAL;
	} else if (!num_data) {
		/* Meh nothing to do */
		return 0;
	} else if (!data) {
		dprintk(VIDC_ERR, "Invalid voting data\n");
		return -EINVAL;
	}

	new_data = kmemdup(data, num_data * sizeof(*new_data), GFP_KERNEL);
	if (!new_data) {
		dprintk(VIDC_ERR, "Can't alloc memory to cache bus votes\n");
		rc = -ENOMEM;
		goto err_no_mem;
	}

	kfree(device->bus_vote.data);
	device->bus_vote.data = new_data;
	device->bus_vote.data_count = num_data;
	device->bus_vote.imem_size = device->res->imem_size;

	venus_hfi_for_each_bus(device, bus) {
		devfreq_resume_device(bus->devfreq); /* NOP if already resume */
		/* Kick devfreq awake incase _resume() didn't do it */
		bus->devfreq->nb.notifier_call(&bus->devfreq->nb, 0, NULL);
	}

err_no_mem:
	return rc;
}

static int venus_hfi_iface_cmdq_write_nolock(struct venus_hfi_device *device,
					void *pkt);

static int venus_hfi_iface_cmdq_write(struct venus_hfi_device *device,
					void *pkt)
{
	int result = -EPERM;
	if (!device || !pkt) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	mutex_lock(&device->write_lock);
	result = venus_hfi_iface_cmdq_write_nolock(device, pkt);
	mutex_unlock(&device->write_lock);
	return result;
}

static int venus_hfi_core_set_resource(void *device,
		struct vidc_resource_hdr *resource_hdr, void *resource_value,
		bool locked)
{
	struct hfi_cmd_sys_set_resource_packet *pkt;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;
	struct venus_hfi_device *dev;

	if (!device || !resource_hdr || !resource_value) {
		dprintk(VIDC_ERR, "set_res: Invalid Params\n");
		return -EINVAL;
	}

	dev = device;
	pkt = (struct hfi_cmd_sys_set_resource_packet *) packet;

	rc = call_hfi_pkt_op(dev, sys_set_resource,
			pkt, resource_hdr, resource_value);
	if (rc) {
		dprintk(VIDC_ERR, "set_res: failed to create packet\n");
		goto err_create_pkt;
	}

	rc = locked ? venus_hfi_iface_cmdq_write(dev, pkt) :
			venus_hfi_iface_cmdq_write_nolock(dev, pkt);
	if (rc)
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_core_release_resource(void *device,
			struct vidc_resource_hdr *resource_hdr)
{
	struct hfi_cmd_sys_release_resource_packet pkt;
	int rc = 0;
	struct venus_hfi_device *dev;

	if (!device || !resource_hdr) {
		dprintk(VIDC_ERR, "Inv-Params in rel_res\n");
		return -EINVAL;
	} else {
		dev = device;
	}

	rc = call_hfi_pkt_op(dev, sys_release_resource,
			&pkt, resource_hdr);
	if (rc) {
		dprintk(VIDC_ERR, "release_res: failed to create packet\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static DECLARE_COMPLETION(pc_prep_done);
static DECLARE_COMPLETION(release_resources_done);

static int venus_hfi_alloc_imem(void *dev, unsigned long size)
{
	struct imem *imem = NULL;
	struct venus_hfi_device *device = dev;
	int rc = 0;

	if (!device || !size)
		return -EINVAL;

	imem = &device->resources.imem;
	if (imem->type) {
		dprintk(VIDC_ERR, "IMEM of type %d already allocated\n",
				imem->type);
		return -ENOMEM;
	}

	switch (device->res->imem_type) {
	case IMEM_OCMEM:
	{
		struct ocmem_buf *ocmem_buffer =
			ocmem_allocate(OCMEM_VIDEO, size);
		if (IS_ERR_OR_NULL(ocmem_buffer)) {
			rc = PTR_ERR(ocmem_buffer) ?: -ENOMEM;
			goto imem_alloc_failed;
		}

		imem->ocmem.buf = ocmem_buffer;
		break;
	}
	case IMEM_VMEM:
	{
		phys_addr_t vmem_buffer = 0;

		rc = vmem_allocate(size, &vmem_buffer);
		if (rc) {
			goto imem_alloc_failed;
		} else if (!vmem_buffer) {
			rc = -ENOMEM;
			goto imem_alloc_failed;
		}

		imem->vmem = vmem_buffer;
		break;
	}
	default:
		rc = -ENOTSUPP;
		goto imem_alloc_failed;
	}

	imem->type = device->res->imem_type;
	dprintk(VIDC_DBG, "Allocated %ld bytes of IMEM of type %d\n", size,
			imem->type);
	return 0;
imem_alloc_failed:
	imem->type = IMEM_NONE;
	return rc;
}

static int venus_hfi_free_imem(struct venus_hfi_device *device)
{
	struct imem *imem = NULL;
	int rc = 0;

	if (!device)
		return -EINVAL;


	imem = &device->resources.imem;
	switch (imem->type) {
	case IMEM_NONE:
		/* Follow the semantics of free(NULL), which is a no-op. */
		break;
	case IMEM_OCMEM:
		rc = ocmem_free(OCMEM_VIDEO, imem->ocmem.buf);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to free ocmem\n");
			goto imem_free_failed;
		}

		break;
	case IMEM_VMEM:
		vmem_free(imem->vmem);
		break;
	default:
		rc = -ENOTSUPP;
		goto imem_free_failed;
	}

	imem->type = IMEM_NONE;
	return 0;

imem_free_failed:
	return rc;
}

static int venus_hfi_set_imem(struct venus_hfi_device *device,
		struct imem *imem, bool locked)
{
	struct vidc_resource_hdr rhdr;
	phys_addr_t addr = 0;
	int rc = 0;

	if (!device || !device->res || !imem) {
		dprintk(VIDC_ERR, "Invalid params, core: %p, imem: %p\n",
			device, imem);
		return -EINVAL;
	}

	rhdr.resource_handle = imem; /* cookie */
	rhdr.size = device->res->imem_size;
	rhdr.resource_id = VIDC_RESOURCE_NONE;

	switch (imem->type) {
	case IMEM_OCMEM:
		rhdr.resource_id = VIDC_RESOURCE_OCMEM;
		addr = imem->ocmem.buf->addr;
		/* Just for sanity */
		if (imem->ocmem.buf->len != rhdr.size) {
			dprintk(VIDC_ERR,
				"ocmem buffer size unexpectedly small (expected %d, have %lu)\n",
				rhdr.size, imem->ocmem.buf->len);
			rc = -EINVAL;
			goto imem_set_failed;
		}

		break;
	case IMEM_VMEM:
		rhdr.resource_id = VIDC_RESOURCE_VMEM;
		addr = imem->vmem;
		break;
	default:
		dprintk(VIDC_ERR, "IMEM of type %d unsupported\n", imem->type);
		rc = -ENOTSUPP;
		goto imem_set_failed;
	}

	BUG_ON(!addr);

	rc = venus_hfi_core_set_resource(device, &rhdr, (void *)addr, locked);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to set IMEM on driver\n");
		goto imem_set_failed;
	}

	dprintk(VIDC_DBG,
			"Managed to set IMEM buffer of type %d sized %d bytes at %pa\n",
			rhdr.resource_id, rhdr.size, &addr);

	rc = venus_hfi_vote_buses(device, device->bus_vote.data,
			device->bus_vote.data_count);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to vote for buses after setting imem: %d\n",
				rc);
	}

imem_set_failed:
	return rc;
}

static int venus_hfi_unset_imem(struct venus_hfi_device *device)
{
	struct vidc_resource_hdr rhdr;
	struct imem *imem = NULL;
	int rc = 0;
	phys_addr_t addr = 0;

	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid params, device: %p\n",
			__func__, device);
		rc = -EINVAL;
		goto imem_unset_failed;
	}

	mutex_lock(&device->write_lock);
	mutex_lock(&device->read_lock);
	rc = venus_hfi_core_in_valid_state(device);
	mutex_unlock(&device->read_lock);
	mutex_unlock(&device->write_lock);
	if (!rc) {
		dprintk(VIDC_WARN, "Core is in bad state, won't unset imem\n");
		rc = -EIO;
		goto imem_unset_failed;
	}

	imem = &device->resources.imem;
	switch (imem->type) {
	case IMEM_OCMEM:
		rhdr.resource_id = VIDC_RESOURCE_OCMEM;
		addr = imem->ocmem.buf->addr;
		break;
	case IMEM_VMEM:
		rhdr.resource_id = VIDC_RESOURCE_VMEM;
		addr = imem->vmem;
		break;
	default:
		dprintk(VIDC_ERR, "IMEM of type %d unsupported\n", imem->type);
		rc = -ENOTSUPP;
		goto imem_unset_failed;
	}

	if (!addr) {
		dprintk(VIDC_INFO, "Trying to unset IMEM which is not set\n");
		rc = -EINVAL;
		goto imem_unset_failed;
	}

	rhdr.resource_handle = imem; /* cookie */
	rhdr.size = device->res->imem_size;

	init_completion(&release_resources_done);

	rc = venus_hfi_core_release_resource(device, &rhdr);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to unset imem on driver\n");
		goto imem_unset_failed;
	}

	if (!wait_for_completion_timeout(&release_resources_done,
			msecs_to_jiffies(msm_vidc_hw_rsp_timeout))) {
		dprintk(VIDC_ERR,
				"Wait timedout in releasing IMEM\n");
		rc = -EIO;
		goto imem_unset_failed;
	}

imem_unset_failed:
	return rc;
}

static int venus_hfi_alloc_set_imem(struct venus_hfi_device *device,
		bool locked)
{
	int rc = 0;

	if (!device->res->imem_size)
		return 0;

	rc = venus_hfi_alloc_imem(device, device->res->imem_size);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to allocate imem: %d\n", rc);
		goto alloc_failed;
	}

	rc = venus_hfi_set_imem(device, &device->resources.imem, locked);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to set imem to core: %d\n", rc);
		goto set_failed;
	}

	return 0;
set_failed:
	venus_hfi_free_imem(device);
alloc_failed:
	return rc;
}

static int venus_hfi_unset_free_imem(struct venus_hfi_device *device)
{
	int rc = 0;

	if (!device->res->imem_size)
		return 0;

	rc = venus_hfi_unset_imem(device);
	if (rc) {
		dprintk(VIDC_WARN, "Failed to unset imem: %d\n", rc);
		goto unset_failed;
	}

	rc = venus_hfi_free_imem(device);
	if (rc) {
		dprintk(VIDC_WARN, "Failed to free imem: %d\n", rc);
		goto free_failed;
	}

unset_failed:
free_failed:
	return rc;
}

static inline int venus_hfi_tzbsp_set_video_state(enum tzbsp_video_state state)
{
	struct tzbsp_video_set_state_req cmd = {0};
	int tzbsp_rsp = 0;
	int rc = 0;
	struct scm_desc desc = {0};

	desc.args[0] = cmd.state = state;
	desc.args[1] = cmd.spare = 0;
	desc.arginfo = SCM_ARGS(2);

	if (!is_scm_armv8()) {
		rc = scm_call(SCM_SVC_BOOT, TZBSP_VIDEO_SET_STATE, &cmd,
				sizeof(cmd), &tzbsp_rsp, sizeof(tzbsp_rsp));
	} else {
		rc = scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT,
				TZBSP_VIDEO_SET_STATE), &desc);
		tzbsp_rsp = desc.ret[0];
	}
	if (rc) {
		dprintk(VIDC_ERR, "Failed scm_call %d\n", rc);
		return rc;
	}
	dprintk(VIDC_DBG, "Set state %d, resp %d\n", state, tzbsp_rsp);
	if (tzbsp_rsp) {
		dprintk(VIDC_ERR,
				"Failed to set video core state to suspend: %d\n",
				tzbsp_rsp);
		return -EINVAL;
	}
	return 0;
}

static inline int venus_hfi_reset_core(struct venus_hfi_device *device)
{
	int rc = 0;
	venus_hfi_write_register(device, VIDC_CTRL_INIT, 0x1);
	rc = venus_hfi_core_start_cpu(device);
	if (rc)
		dprintk(VIDC_ERR, "Failed to start core\n");
	return rc;
}

static struct clock_info *venus_hfi_get_clock(struct venus_hfi_device *device,
		char *name)
{
	struct clock_info *vc;

	venus_hfi_for_each_clock(device, vc) {
		if (!strcmp(vc->name, name))
			return vc;
	}
	dprintk(VIDC_WARN, "%s Clock %s not found\n", __func__, name);

	return NULL;
}

static unsigned long venus_hfi_get_clock_rate(struct clock_info *clock,
	int num_mbs_per_sec, int codecs_enabled)
{
	int num_rows = clock->count;
	struct load_freq_table *table = clock->load_freq_tbl;
	unsigned long freq = table[0].freq;
	int i;

	if (!num_mbs_per_sec && num_rows > 1)
		return table[num_rows - 1].freq;

	for (i = 0; i < num_rows; i++) {
		bool matches = venus_hfi_is_session_supported(
			table[i].supported_codecs, codecs_enabled);
		if (!matches)
			continue;

		if (num_mbs_per_sec > table[i].load)
			break;

		freq = table[i].freq;
	}

	return freq;
}

static unsigned long venus_hfi_get_core_clock_rate(void *dev)
{
	struct venus_hfi_device *device = (struct venus_hfi_device *) dev;
	struct clock_info *vc;

	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, device);
		return -EINVAL;
	}

	vc = venus_hfi_get_clock(device, "core_clk");
	if (vc)
		return clk_get_rate(vc->clk);
	else
		return 0;
}

static int venus_hfi_suspend(void *dev)
{
	int rc = 0;
	struct venus_hfi_device *device = (struct venus_hfi_device *) dev;

	if (!device) {
		dprintk(VIDC_ERR, "%s invalid device\n", __func__);
		return -EINVAL;
	} else if (!device->res->sw_power_collapsible) {
		return -ENOTSUPP;
	}

	if (device->power_enabled) {
		rc = flush_delayed_work(&venus_hfi_pm_work);
		dprintk(VIDC_INFO, "%s flush delayed work %d\n", __func__, rc);
	}
	return 0;
}

static enum hal_default_properties venus_hfi_get_default_properties(void *dev)
{
	enum hal_default_properties prop = 0;
	struct venus_hfi_device *device = (struct venus_hfi_device *) dev;

	if (!device) {
		dprintk(VIDC_ERR, "%s invalid device\n", __func__);
		return -EINVAL;
	}

	if (device->packetization_type == HFI_PACKETIZATION_3XX)
		prop = HAL_VIDEO_DYNAMIC_BUF_MODE;

	return prop;
}

static int venus_hfi_halt_axi(struct venus_hfi_device *device)
{
	u32 reg;
	int rc = 0;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid input: %p\n", device);
		return -EINVAL;
	}
	/*
	 * Driver needs to make sure that clocks are enabled to read Venus AXI
	 * registers. If not skip AXI HALT.
	 */
	if (device->clk_state != ENABLED_PREPARED) {
		dprintk(VIDC_WARN,
			"Clocks are OFF, skipping AXI HALT\n");
		return -EINVAL;
	}

	/* Halt AXI and AXI IMEM VBIF Access */
	reg = venus_hfi_read_register(device, VENUS_VBIF_AXI_HALT_CTRL0);
	reg |= VENUS_VBIF_AXI_HALT_CTRL0_HALT_REQ;
	venus_hfi_write_register(device, VENUS_VBIF_AXI_HALT_CTRL0, reg);

	/* Request for AXI bus port halt */
	rc = readl_poll_timeout(device->hal_data->register_base
			+ VENUS_VBIF_AXI_HALT_CTRL1,
			reg, reg & VENUS_VBIF_AXI_HALT_CTRL1_HALT_ACK,
			POLL_INTERVAL_US,
			VENUS_VBIF_AXI_HALT_ACK_TIMEOUT_US);
	if (rc)
		dprintk(VIDC_WARN, "AXI bus port halt timeout\n");

	return rc;
}

static inline int venus_hfi_power_off(struct venus_hfi_device *device)
{
	int rc = 0;

	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	if (!device->power_enabled) {
		dprintk(VIDC_DBG, "Power already disabled\n");
		return 0;
	}

	rc = venus_hfi_halt_axi(device);
	if (rc) {
		dprintk(VIDC_WARN, "Failed to halt AXI\n");
		return 0;
	}

	dprintk(VIDC_DBG, "Entering power collapse\n");
	rc = venus_hfi_tzbsp_set_video_state(TZBSP_VIDEO_STATE_SUSPEND);
	if (rc) {
		dprintk(VIDC_WARN, "Failed to suspend video core %d\n", rc);
		goto err_tzbsp_suspend;
	}
	/*
	* For some regulators, driver might have transfered the control to HW.
	* So before touching any clocks, driver should get the regulator
	* control back. Acquire regulators also makes sure that the regulators
	* are turned ON. So driver can touch the clocks safely.
	*/

	rc = venus_hfi_acquire_regulators(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to enable gdsc in %s Err code = %d\n",
			__func__, rc);
		goto err_acquire_regulators;
	}
	venus_hfi_disable_unprepare_clks(device);
	rc = venus_hfi_disable_regulators(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to disable gdsc\n");
		goto err_disable_regulators;
	}

	venus_hfi_unvote_buses(device);
	device->power_enabled = false;
	dprintk(VIDC_INFO, "Venus power collapsed\n");

	return rc;

err_disable_regulators:
	if (venus_hfi_prepare_enable_clks(device))
		dprintk(VIDC_ERR, "Failed prepare_enable_clks\n");
	if (venus_hfi_hand_off_regulators(device))
		dprintk(VIDC_ERR, "Failed hand_off_regulators\n");
err_acquire_regulators:
	if (venus_hfi_tzbsp_set_video_state(TZBSP_VIDEO_STATE_RESUME))
		dprintk(VIDC_ERR, "Failed TZBSP_RESUME\n");
err_tzbsp_suspend:
	return rc;
}

static inline int venus_hfi_power_on(struct venus_hfi_device *device)
{
	int rc = 0;

	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	if (device->power_enabled)
		return 0;

	dprintk(VIDC_DBG, "Resuming from power collapse\n");
	rc = venus_hfi_vote_buses(device, device->bus_vote.data,
			device->bus_vote.data_count);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to scale buses\n");
		goto err_vote_buses;
	}

	/* At this point driver has the control for all regulators */
	rc = venus_hfi_enable_regulators(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to enable GDSC in %s Err code = %d\n",
			__func__, rc);
		goto err_enable_gdsc;
	}

	rc = venus_hfi_prepare_enable_clks(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to enable clocks\n");
		goto err_enable_clk;
	}

	/* Reboot the firmware */
	rc = venus_hfi_tzbsp_set_video_state(TZBSP_VIDEO_STATE_RESUME);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to resume video core %d\n", rc);
		goto err_set_video_state;
	}

	rc = venus_hfi_hand_off_regulators(device);
	if (rc)
		dprintk(VIDC_WARN, "Failed to handoff control to HW %d\n", rc);

	/*
	 * Re-program all of the registers that get reset as a result of
	 * regulator_disable() and _enable()
	 */
	venus_hfi_set_registers(device);

	venus_hfi_write_register(device, VIDC_UC_REGION_ADDR,
			(u32)device->iface_q_table.align_device_addr);
	venus_hfi_write_register(device, VIDC_UC_REGION_SIZE, SHARED_QSIZE);
	venus_hfi_write_register(device, VIDC_CPU_CS_SCIACMDARG2,
		(u32)device->iface_q_table.align_device_addr);

	if (device->sfr.align_device_addr)
		venus_hfi_write_register(device, VIDC_SFR_ADDR,
				(u32)device->sfr.align_device_addr);
	if (device->qdss.align_device_addr)
		venus_hfi_write_register(device, VIDC_MMAP_ADDR,
				(u32)device->qdss.align_device_addr);

	/* Wait for boot completion */
	rc = venus_hfi_reset_core(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to reset venus core\n");
		goto err_reset_core;
	}

	/*
	 * Set the flag here to skip venus_hfi_power_on() which is
	 * being called again via *_alloc_set_imem() if imem is enabled
	 */
	device->power_enabled = true;

	/*
	 * write_lock is already acquired at this point, so to avoid
	 * recursive lock in cmdq_write function, call nolock version
	 * of alloc_icmem
	 */
	WARN_ON(!mutex_is_locked(&device->write_lock));
	rc = venus_hfi_alloc_set_imem(device, false);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to allocate IMEM");
		goto err_alloc_imem;
	}

	dprintk(VIDC_INFO, "Resumed from power collapse\n");
	return rc;

err_alloc_imem:
err_reset_core:
	venus_hfi_tzbsp_set_video_state(TZBSP_VIDEO_STATE_SUSPEND);
err_set_video_state:
	venus_hfi_disable_unprepare_clks(device);
err_enable_clk:
	venus_hfi_disable_regulators(device);
err_enable_gdsc:
	venus_hfi_unvote_buses(device);
err_vote_buses:
	device->power_enabled = false;
	dprintk(VIDC_ERR, "Failed to resume from power collapse\n");
	return rc;
}

static int venus_hfi_power_enable(void *dev)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	mutex_lock(&device->write_lock);
	rc = venus_hfi_power_on(device);
	if (rc)
		dprintk(VIDC_ERR, "%s: Failed to enable power\n", __func__);
	mutex_unlock(&device->write_lock);

	return rc;
}

static int venus_hfi_scale_clocks(void *dev, int load, int codecs_enabled)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;
	struct clock_info *cl;

	if (!device) {
		dprintk(VIDC_ERR, "Invalid args: %p\n", device);
		return -EINVAL;
	}
	device->clk_load = load;
	device->codecs_enabled = codecs_enabled;

	venus_hfi_for_each_clock(device, cl) {
		if (cl->count) {/* has_scaling */
			unsigned long rate = venus_hfi_get_clock_rate(cl, load,
				codecs_enabled);
			rc = clk_set_rate(cl->clk, rate);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to set clock rate %lu %s: %d\n",
					rate, cl->name, rc);
				break;
			}

			dprintk(VIDC_PROF, "Scaling clock %s to %lu\n",
					cl->name, rate);
		}
	}

	return rc;
}

static int venus_hfi_iface_cmdq_write_nolock(struct venus_hfi_device *device,
					void *pkt)
{
	u32 rx_req_is_set = 0;
	struct vidc_iface_q_info *q_info;
	struct vidc_hal_cmd_pkt_hdr *cmd_packet;
	int result = -EPERM;

	if (!device || !pkt) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	WARN(!mutex_is_locked(&device->write_lock),
			"Cmd queue write lock must be acquired");
	if (!venus_hfi_core_in_valid_state(device)) {
		dprintk(VIDC_DBG, "%s - fw not in init state\n", __func__);
		result = -EINVAL;
		goto err_q_null;
	}

	cmd_packet = (struct vidc_hal_cmd_pkt_hdr *)pkt;
	device->last_packet_type = cmd_packet->packet_type;

	q_info = &device->iface_queues[VIDC_IFACEQ_CMDQ_IDX];
	if (!q_info) {
		dprintk(VIDC_ERR, "cannot write to shared Q's\n");
		goto err_q_null;
	}

	if (!q_info->q_array.align_virtual_addr) {
		dprintk(VIDC_ERR, "cannot write to shared CMD Q's\n");
		result = -ENODATA;
		goto err_q_null;
	}

	venus_hfi_sim_modify_cmd_packet((u8 *)pkt, device);
	if (!venus_hfi_write_queue(q_info, (u8 *)pkt, &rx_req_is_set)) {

		if (venus_hfi_power_on(device)) {
			dprintk(VIDC_ERR, "%s: Power on failed\n", __func__);
			goto err_q_write;
		}
		if (venus_hfi_scale_clocks(device, device->clk_load,
			 device->codecs_enabled)) {
			dprintk(VIDC_ERR, "Clock scaling failed\n");
			goto err_q_write;
		}
		if (rx_req_is_set)
			venus_hfi_write_register(
				device, VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT);

		if (device->res->sw_power_collapsible) {
			dprintk(VIDC_DBG,
				"Cancel and queue delayed work again\n");
			cancel_delayed_work(&venus_hfi_pm_work);
			if (!queue_delayed_work(device->venus_pm_workq,
				&venus_hfi_pm_work,
				msecs_to_jiffies(
				msm_vidc_pwr_collapse_delay))) {
				dprintk(VIDC_DBG,
				"PM work already scheduled\n");
			}
		}
		result = 0;
	} else {
		dprintk(VIDC_ERR, "venus_hfi_iface_cmdq_write:queue_full\n");
	}
err_q_write:
err_q_null:
	return result;
}

static int venus_hfi_iface_msgq_read(struct venus_hfi_device *device, void *pkt)
{
	u32 tx_req_is_set = 0;
	int rc = 0;
	struct vidc_iface_q_info *q_info;

	if (!pkt) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	mutex_lock(&device->read_lock);
	if (!venus_hfi_core_in_valid_state(device)) {
		dprintk(VIDC_DBG, "%s - fw not in init state\n", __func__);
		rc = -EINVAL;
		goto read_error_null;
	}

	if (device->iface_queues[VIDC_IFACEQ_MSGQ_IDX].
		q_array.align_virtual_addr == 0) {
		dprintk(VIDC_ERR, "cannot read from shared MSG Q's\n");
		rc = -ENODATA;
		goto read_error_null;
	}

	q_info = &device->iface_queues[VIDC_IFACEQ_MSGQ_IDX];
	if (!venus_hfi_read_queue(q_info, (u8 *)pkt, &tx_req_is_set)) {
		venus_hfi_hal_sim_modify_msg_packet((u8 *)pkt, device);
		if (tx_req_is_set)
			venus_hfi_write_register(
				device, VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT);
		rc = 0;
	} else
		rc = -ENODATA;

read_error_null:
	mutex_unlock(&device->read_lock);
	return rc;
}

static int venus_hfi_iface_dbgq_read(struct venus_hfi_device *device, void *pkt)
{
	u32 tx_req_is_set = 0;
	int rc = 0;
	struct vidc_iface_q_info *q_info;

	if (!pkt) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	mutex_lock(&device->read_lock);
	if (!venus_hfi_core_in_valid_state(device)) {
		dprintk(VIDC_DBG, "%s - fw not in init state\n", __func__);
		rc = -EINVAL;
		goto dbg_error_null;
	}
	if (device->iface_queues[VIDC_IFACEQ_DBGQ_IDX].
		q_array.align_virtual_addr == 0) {
		dprintk(VIDC_ERR, "cannot read from shared DBG Q's\n");
		rc = -ENODATA;
		goto dbg_error_null;
	}
	q_info = &device->iface_queues[VIDC_IFACEQ_DBGQ_IDX];
	if (!venus_hfi_read_queue(q_info, (u8 *)pkt, &tx_req_is_set)) {
		if (tx_req_is_set)
			venus_hfi_write_register(
				device, VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT);
		rc = 0;
	} else
		rc = -ENODATA;

dbg_error_null:
	mutex_unlock(&device->read_lock);
	return rc;
}

static void venus_hfi_set_queue_hdr_defaults(struct hfi_queue_header *q_hdr)
{
	q_hdr->qhdr_status = 0x1;
	q_hdr->qhdr_type = VIDC_IFACEQ_DFLT_QHDR;
	q_hdr->qhdr_q_size = VIDC_IFACEQ_QUEUE_SIZE / 4;
	q_hdr->qhdr_pkt_size = 0;
	q_hdr->qhdr_rx_wm = 0x1;
	q_hdr->qhdr_tx_wm = 0x1;
	q_hdr->qhdr_rx_req = 0x1;
	q_hdr->qhdr_tx_req = 0x0;
	q_hdr->qhdr_rx_irq_status = 0x0;
	q_hdr->qhdr_tx_irq_status = 0x0;
	q_hdr->qhdr_read_idx = 0x0;
	q_hdr->qhdr_write_idx = 0x0;
}

static void venus_hfi_interface_queues_release(struct venus_hfi_device *device)
{
	int i;
	struct hfi_mem_map_table *qdss;
	struct hfi_mem_map *mem_map;
	int num_entries = device->res->qdss_addr_set.count;
	unsigned long mem_map_table_base_addr;
	struct context_bank_info *cb;

	mutex_lock(&device->write_lock);
	mutex_lock(&device->read_lock);
	if (device->qdss.mem_data) {
		qdss = (struct hfi_mem_map_table *)
			device->qdss.align_virtual_addr;
		qdss->mem_map_num_entries = num_entries;
		mem_map_table_base_addr =
			device->qdss.align_device_addr +
			sizeof(struct hfi_mem_map_table);
		qdss->mem_map_table_base_addr =
			(u32)mem_map_table_base_addr;
		if ((unsigned long)qdss->mem_map_table_base_addr !=
			mem_map_table_base_addr) {
			dprintk(VIDC_ERR,
				"Invalid mem_map_table_base_addr %#lx",
				mem_map_table_base_addr);
		}
		mem_map = (struct hfi_mem_map *)(qdss + 1);
		cb = msm_smem_get_context_bank(device->hal_client,
					false, HAL_BUFFER_INTERNAL_CMD_QUEUE);

		for (i = 0; cb && i < num_entries; i++) {
			iommu_unmap(cb->mapping->domain,
						mem_map[i].virtual_addr,
						mem_map[i].size);
		}

		venus_hfi_free(device, device->qdss.mem_data);
	}
	venus_hfi_free(device, device->iface_q_table.mem_data);
	venus_hfi_free(device, device->sfr.mem_data);

	for (i = 0; i < VIDC_IFACEQ_NUMQ; i++) {
		device->iface_queues[i].q_hdr = NULL;
		device->iface_queues[i].q_array.mem_data = NULL;
		device->iface_queues[i].q_array.align_virtual_addr = NULL;
		device->iface_queues[i].q_array.align_device_addr = 0;
	}
	device->iface_q_table.mem_data = NULL;
	device->iface_q_table.align_virtual_addr = NULL;
	device->iface_q_table.align_device_addr = 0;

	device->qdss.mem_data = NULL;
	device->qdss.align_virtual_addr = NULL;
	device->qdss.align_device_addr = 0;

	device->sfr.mem_data = NULL;
	device->sfr.align_virtual_addr = NULL;
	device->sfr.align_device_addr = 0;

	device->mem_addr.mem_data = NULL;
	device->mem_addr.align_virtual_addr = NULL;
	device->mem_addr.align_device_addr = 0;

	msm_smem_delete_client(device->hal_client);
	device->hal_client = NULL;
	mutex_unlock(&device->read_lock);
	mutex_unlock(&device->write_lock);
}

static int venus_hfi_get_qdss_iommu_virtual_addr(struct venus_hfi_device *dev,
		struct hfi_mem_map *mem_map, struct dma_iommu_mapping *mapping)
{
	int i;
	int rc = 0;
	dma_addr_t iova = QDSS_IOVA_START;
	int num_entries = dev->res->qdss_addr_set.count;
	struct addr_range *qdss_addr_tbl = dev->res->qdss_addr_set.addr_tbl;

	if (!num_entries)
		return -ENODATA;

	for (i = 0; i < num_entries; i++) {
		if (mapping) {
			rc = iommu_map(mapping->domain, iova,
					qdss_addr_tbl[i].start,
					qdss_addr_tbl[i].size,
					IOMMU_READ | IOMMU_WRITE);

			if (rc) {
				dprintk(VIDC_ERR,
						"IOMMU QDSS mapping failed for addr %#x\n",
						qdss_addr_tbl[i].start);
				rc = -ENOMEM;
				break;
			}
		} else {
			iova =  qdss_addr_tbl[i].start;
		}

		mem_map[i].virtual_addr = (u32)iova;
		mem_map[i].physical_addr = qdss_addr_tbl[i].start;
		mem_map[i].size = qdss_addr_tbl[i].size;
		mem_map[i].attr = 0x0;

		iova += mem_map[i].size;
	}
	if (i < num_entries) {
		dprintk(VIDC_ERR,
			"QDSS mapping failed, Freeing other entries %d\n", i);

		for (--i; mapping && i >= 0; i--) {
			iommu_unmap(mapping->domain,
				mem_map[i].virtual_addr,
				mem_map[i].size);
		}
	}
	return rc;
}

static int venus_hfi_interface_queues_init(struct venus_hfi_device *dev)
{
	struct hfi_queue_table_header *q_tbl_hdr;
	struct hfi_queue_header *q_hdr;
	u32 i;
	int rc = 0;
	struct hfi_mem_map_table *qdss;
	struct hfi_mem_map *mem_map;
	struct vidc_iface_q_info *iface_q;
	struct hfi_sfr_struct *vsfr;
	struct vidc_mem_addr *mem_addr;
	int offset = 0;
	int num_entries = dev->res->qdss_addr_set.count;
	u32 value = 0;
	phys_addr_t fw_bias = 0;
	size_t q_size;
	unsigned long mem_map_table_base_addr;
	struct context_bank_info *cb;

	q_size = SHARED_QSIZE - ALIGNED_SFR_SIZE - ALIGNED_QDSS_SIZE;
	mem_addr = &dev->mem_addr;
	if (!is_iommu_present(dev->res))
		fw_bias = dev->hal_data->firmware_base;
	rc = venus_hfi_alloc(dev, (void *) mem_addr, q_size, 1, 0,
			HAL_BUFFER_INTERNAL_CMD_QUEUE);
	if (rc) {
		dprintk(VIDC_ERR, "iface_q_table_alloc_fail\n");
		goto fail_alloc_queue;
	}
	dev->iface_q_table.align_virtual_addr = mem_addr->align_virtual_addr;
	dev->iface_q_table.align_device_addr = mem_addr->align_device_addr -
					fw_bias;
	dev->iface_q_table.mem_size = VIDC_IFACEQ_TABLE_SIZE;
	dev->iface_q_table.mem_data = mem_addr->mem_data;
	offset += dev->iface_q_table.mem_size;

	for (i = 0; i < VIDC_IFACEQ_NUMQ; i++) {
		iface_q = &dev->iface_queues[i];
		iface_q->q_array.align_device_addr = mem_addr->align_device_addr
			+ offset - fw_bias;
		iface_q->q_array.align_virtual_addr =
			mem_addr->align_virtual_addr + offset;
		iface_q->q_array.mem_size = VIDC_IFACEQ_QUEUE_SIZE;
		iface_q->q_array.mem_data = NULL;
		offset += iface_q->q_array.mem_size;
		iface_q->q_hdr = VIDC_IFACEQ_GET_QHDR_START_ADDR(
				dev->iface_q_table.align_virtual_addr, i);
		venus_hfi_set_queue_hdr_defaults(iface_q->q_hdr);
	}
	if ((msm_vidc_fw_debug_mode & HFI_DEBUG_MODE_QDSS) && num_entries) {
		rc = venus_hfi_alloc(dev, (void *) mem_addr,
				ALIGNED_QDSS_SIZE, 1, 0,
				HAL_BUFFER_INTERNAL_CMD_QUEUE);
		if (rc) {
			dprintk(VIDC_WARN,
				"qdss_alloc_fail: QDSS messages logging will not work\n");
			dev->qdss.align_device_addr = 0;
		} else {
			dev->qdss.align_device_addr =
				mem_addr->align_device_addr - fw_bias;
			dev->qdss.align_virtual_addr =
				mem_addr->align_virtual_addr;
			dev->qdss.mem_size = ALIGNED_QDSS_SIZE;
			dev->qdss.mem_data = mem_addr->mem_data;
		}
	}
	rc = venus_hfi_alloc(dev, (void *) mem_addr,
			ALIGNED_SFR_SIZE, 1, 0,
			HAL_BUFFER_INTERNAL_CMD_QUEUE);
	if (rc) {
		dprintk(VIDC_WARN, "sfr_alloc_fail: SFR not will work\n");
		dev->sfr.align_device_addr = 0;
	} else {
		dev->sfr.align_device_addr = mem_addr->align_device_addr -
					fw_bias;
		dev->sfr.align_virtual_addr = mem_addr->align_virtual_addr;
		dev->sfr.mem_size = ALIGNED_SFR_SIZE;
		dev->sfr.mem_data = mem_addr->mem_data;
	}
	q_tbl_hdr = (struct hfi_queue_table_header *)
			dev->iface_q_table.align_virtual_addr;
	q_tbl_hdr->qtbl_version = 0;
	q_tbl_hdr->qtbl_size = VIDC_IFACEQ_TABLE_SIZE;
	q_tbl_hdr->qtbl_qhdr0_offset = sizeof(
		struct hfi_queue_table_header);
	q_tbl_hdr->qtbl_qhdr_size = sizeof(
		struct hfi_queue_header);
	q_tbl_hdr->qtbl_num_q = VIDC_IFACEQ_NUMQ;
	q_tbl_hdr->qtbl_num_active_q = VIDC_IFACEQ_NUMQ;

	iface_q = &dev->iface_queues[VIDC_IFACEQ_CMDQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = (u32)iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_HOST_TO_CTRL_CMD_Q;
	if ((ion_phys_addr_t)q_hdr->qhdr_start_addr !=
		iface_q->q_array.align_device_addr) {
		dprintk(VIDC_ERR, "Invalid CMDQ device address (%pa)",
			&iface_q->q_array.align_device_addr);
	}

	iface_q = &dev->iface_queues[VIDC_IFACEQ_MSGQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = (u32)iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_CTRL_TO_HOST_MSG_Q;
	if ((ion_phys_addr_t)q_hdr->qhdr_start_addr !=
		iface_q->q_array.align_device_addr) {
		dprintk(VIDC_ERR, "Invalid MSGQ device address (%pa)",
			&iface_q->q_array.align_device_addr);
	}

	iface_q = &dev->iface_queues[VIDC_IFACEQ_DBGQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = (u32)iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_CTRL_TO_HOST_DEBUG_Q;
	/*
	 * Set receive request to zero on debug queue as there is no
	 * need of interrupt from video hardware for debug messages
	 */
	q_hdr->qhdr_rx_req = 0;
	if ((ion_phys_addr_t)q_hdr->qhdr_start_addr !=
		iface_q->q_array.align_device_addr) {
		dprintk(VIDC_ERR, "Invalid DBGQ device address (%pa)",
			&iface_q->q_array.align_device_addr);
	}

	value = (u32)dev->iface_q_table.align_device_addr;
	if ((ion_phys_addr_t)value !=
		dev->iface_q_table.align_device_addr) {
		dprintk(VIDC_ERR,
			"Invalid iface_q_table device address (%pa)",
			&dev->iface_q_table.align_device_addr);
	}
	venus_hfi_write_register(dev, VIDC_UC_REGION_ADDR, value);
	venus_hfi_write_register(dev, VIDC_UC_REGION_SIZE, SHARED_QSIZE);
	venus_hfi_write_register(dev, VIDC_CPU_CS_SCIACMDARG2, value);
	venus_hfi_write_register(dev, VIDC_CPU_CS_SCIACMDARG1, 0x01);
	if (dev->qdss.mem_data) {
		qdss = (struct hfi_mem_map_table *)dev->qdss.align_virtual_addr;
		qdss->mem_map_num_entries = num_entries;
		mem_map_table_base_addr = dev->qdss.align_device_addr +
			sizeof(struct hfi_mem_map_table);
		qdss->mem_map_table_base_addr =
			(u32)mem_map_table_base_addr;
		if ((ion_phys_addr_t)qdss->mem_map_table_base_addr !=
				mem_map_table_base_addr) {
			dprintk(VIDC_ERR,
					"Invalid mem_map_table_base_addr (%#lx)",
					mem_map_table_base_addr);
		}
		mem_map = (struct hfi_mem_map *)(qdss + 1);
		cb = msm_smem_get_context_bank(dev->hal_client, false,
				HAL_BUFFER_INTERNAL_CMD_QUEUE);

		if (!cb) {
			dprintk(VIDC_ERR,
				"%s: failed to get context bank\n", __func__);
			return -EINVAL;
		}

		rc = venus_hfi_get_qdss_iommu_virtual_addr(dev,
			mem_map, cb->mapping);
		if (rc) {
			dprintk(VIDC_ERR,
				"IOMMU mapping failed, Freeing qdss memdata\n");
			venus_hfi_free(dev, dev->qdss.mem_data);
			dev->qdss.mem_data = NULL;
			dev->qdss.align_virtual_addr = NULL;
			dev->qdss.align_device_addr = 0;
		}
		value = (u32)dev->qdss.align_device_addr;
		if ((ion_phys_addr_t)value !=
				dev->qdss.align_device_addr) {
			dprintk(VIDC_ERR, "Invalid qdss device address (%pa)",
					&dev->qdss.align_device_addr);
		}
		if (dev->qdss.align_device_addr)
			venus_hfi_write_register(dev, VIDC_MMAP_ADDR, value);
	}

	vsfr = (struct hfi_sfr_struct *) dev->sfr.align_virtual_addr;
	vsfr->bufSize = ALIGNED_SFR_SIZE;
	value = (u32)dev->sfr.align_device_addr;
	if ((ion_phys_addr_t)value !=
		dev->sfr.align_device_addr) {
		dprintk(VIDC_ERR, "Invalid sfr device address (%pa)",
			&dev->sfr.align_device_addr);
	}
	if (dev->sfr.align_device_addr)
		venus_hfi_write_register(dev, VIDC_SFR_ADDR, value);
	return 0;
fail_alloc_queue:
	return -ENOMEM;
}

static int venus_hfi_sys_set_debug(struct venus_hfi_device *device, u32 debug)
{
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;
	struct hfi_cmd_sys_set_property_packet *pkt =
		(struct hfi_cmd_sys_set_property_packet *) &packet;

	rc = call_hfi_pkt_op(device, sys_debug_config, pkt, debug);
	if (rc) {
		dprintk(VIDC_WARN,
			"Debug mode setting to FW failed\n");
		return -ENOTEMPTY;
	}
	if (venus_hfi_iface_cmdq_write(device, pkt))
		return -ENOTEMPTY;
	return 0;
}

static int venus_hfi_sys_set_coverage(struct venus_hfi_device *device, u32 mode)
{
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;
	struct hfi_cmd_sys_set_property_packet *pkt =
		(struct hfi_cmd_sys_set_property_packet *) &packet;

	rc = call_hfi_pkt_op(device, sys_coverage_config,
			pkt, mode);
	if (rc) {
		dprintk(VIDC_WARN,
			"Coverage mode setting to FW failed\n");
		return -ENOTEMPTY;
	}
	if (venus_hfi_iface_cmdq_write(device, pkt)) {
		dprintk(VIDC_WARN, "Failed to send coverage pkt to f/w\n");
		return -ENOTEMPTY;
	}
	return 0;
}

static int venus_hfi_sys_set_idle_message(struct venus_hfi_device *device,
	bool enable)
{
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct hfi_cmd_sys_set_property_packet *pkt =
		(struct hfi_cmd_sys_set_property_packet *) &packet;
	if (!enable) {
		dprintk(VIDC_DBG, "sys_idle_indicator is not enabled\n");
		return 0;
	}
	call_hfi_pkt_op(device, sys_idle_indicator, pkt, enable);
	if (venus_hfi_iface_cmdq_write(device, pkt))
		return -ENOTEMPTY;
	return 0;
}

static int venus_hfi_sys_set_power_control(struct venus_hfi_device *device,
	bool enable)
{
	struct regulator_info *rinfo;
	bool supported = false;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct hfi_cmd_sys_set_property_packet *pkt =
		(struct hfi_cmd_sys_set_property_packet *) &packet;

	venus_hfi_for_each_regulator(device, rinfo) {
		if (rinfo->has_hw_power_collapse) {
			supported = true;
			break;
		}
	}

	if (!supported)
		return 0;

	call_hfi_pkt_op(device, sys_power_control, pkt, enable);
	if (venus_hfi_iface_cmdq_write(device, pkt))
		return -ENOTEMPTY;
	return 0;
}

static int venus_hfi_core_init(void *device)
{
	struct hfi_cmd_sys_init_packet pkt;
	struct hfi_cmd_sys_get_property_packet version_pkt;
	int rc = 0;
	struct list_head *ptr, *next;
	struct hal_session *session = NULL;
	struct venus_hfi_device *dev;

	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "Invalid device\n");
		return -ENODEV;
	}

	venus_hfi_set_state(dev, VENUS_STATE_INIT);

	dev->intr_status = 0;

	mutex_lock(&dev->session_lock);
	list_for_each_safe(ptr, next, &dev->sess_head) {
		/* This means that session list is not empty. Kick stale
		 * sessions out of our valid instance list, but keep the
		 * list_head inited so that list_del (in the future, called
		 * by session_clean()) will be valid. When client doesn't close
		 * them, then it is a genuine leak which driver can't fix. */
		session = list_entry(ptr, struct hal_session, list);
		list_del_init(&session->list);
	}
	INIT_LIST_HEAD(&dev->sess_head);
	mutex_unlock(&dev->session_lock);

	venus_hfi_set_registers(dev);

	if (!dev->hal_client) {
		dev->hal_client = msm_smem_new_client(
				SMEM_ION, dev->res, MSM_VIDC_UNKNOWN);
		if (dev->hal_client == NULL) {
			dprintk(VIDC_ERR, "Failed to alloc ION_Client\n");
			rc = -ENODEV;
			goto err_core_init;
		}

		dprintk(VIDC_DBG, "Dev_Virt: %pa, Reg_Virt: %p\n",
			&dev->hal_data->firmware_base,
			dev->hal_data->register_base);

		rc = venus_hfi_interface_queues_init(dev);
		if (rc) {
			dprintk(VIDC_ERR, "failed to init queues\n");
			rc = -ENOMEM;
			goto err_core_init;
		}
	} else {
		dprintk(VIDC_ERR, "hal_client exists\n");
		rc = -EEXIST;
		goto err_core_init;
	}
	enable_irq(dev->hal_data->irq);
	venus_hfi_write_register(dev, VIDC_CTRL_INIT, 0x1);
	rc = venus_hfi_core_start_cpu(dev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to start core\n");
		rc = -ENODEV;
		goto err_core_init;
	}

	rc =  call_hfi_pkt_op(dev, sys_init, &pkt, HFI_VIDEO_ARCH_OX);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to create sys init pkt\n");
		goto err_core_init;
	}
	if (venus_hfi_iface_cmdq_write(dev, &pkt)) {
		rc = -ENOTEMPTY;
		goto err_core_init;
	}

	rc = call_hfi_pkt_op(dev, sys_image_version, &version_pkt);
	if (rc || venus_hfi_iface_cmdq_write(dev, &version_pkt))
		dprintk(VIDC_WARN, "Failed to send image version pkt to f/w\n");

	return rc;
err_core_init:
	venus_hfi_set_state(dev, VENUS_STATE_DEINIT);
	disable_irq_nosync(dev->hal_data->irq);
	return rc;
}

static int venus_hfi_core_release(void *device)
{
	struct venus_hfi_device *dev;
	int rc = 0;

	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "invalid device\n");
		return -ENODEV;
	}

	if (dev->hal_client) {
		if (venus_hfi_power_enable(device)) {
			dprintk(VIDC_ERR,
				"%s: Power enable failed\n", __func__);
			return -EIO;
		}

		mutex_lock(&dev->resource_lock);
		rc = venus_hfi_unset_free_imem(dev);
		mutex_unlock(&dev->resource_lock);
		if (rc)
			dprintk(VIDC_ERR,
					"Failed to unset and free imem in core release: %d\n",
					rc);
		if (!(dev->intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK))
			disable_irq_nosync(dev->hal_data->irq);
		dev->intr_status = 0;
	}
	venus_hfi_set_state(dev, VENUS_STATE_DEINIT);

	dprintk(VIDC_INFO, "HAL exited\n");
	return 0;
}

static int venus_hfi_get_q_size(struct venus_hfi_device *dev,
	unsigned int q_index)
{
	struct hfi_queue_header *queue;
	struct vidc_iface_q_info *q_info;
	u32 write_ptr, read_ptr;
	u32 rc = 0;

	if (q_index >= VIDC_IFACEQ_NUMQ) {
		dprintk(VIDC_ERR, "Invalid q index: %d\n", q_index);
		return -ENOENT;
	}

	q_info = &dev->iface_queues[q_index];
	if (!q_info) {
		dprintk(VIDC_ERR, "cannot read shared Q's\n");
		return -ENOENT;
	}

	queue = (struct hfi_queue_header *)q_info->q_hdr;
	if (!queue) {
		dprintk(VIDC_ERR, "queue not present\n");
		return -ENOENT;
	}

	write_ptr = (u32)queue->qhdr_write_idx;
	read_ptr = (u32)queue->qhdr_read_idx;
	rc = read_ptr - write_ptr;
	return rc;
}

static void venus_hfi_core_clear_interrupt(struct venus_hfi_device *device)
{
	u32 intr_status = 0;

	if (!device) {
		dprintk(VIDC_ERR, "%s: NULL device\n", __func__);
		return;
	}

	intr_status = venus_hfi_read_register(
			device,
			VIDC_WRAPPER_INTR_STATUS);

	if (intr_status & VIDC_WRAPPER_INTR_STATUS_A2H_BMSK ||
		intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK ||
		intr_status &
			VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK) {
		device->intr_status |= intr_status;
		device->reg_count++;
		dprintk(VIDC_DBG,
			"INTERRUPT for device: %p: times: %d interrupt_status: %d\n",
			device, device->reg_count, intr_status);
	} else {
		device->spur_count++;
		dprintk(VIDC_INFO,
			"SPURIOUS_INTR for device: %p: times: %d interrupt_status: %d\n",
			device, device->spur_count, intr_status);
	}

	venus_hfi_write_register(device, VIDC_CPU_CS_A2HSOFTINTCLR, 1);
	venus_hfi_write_register(device, VIDC_WRAPPER_INTR_CLEAR, intr_status);
	dprintk(VIDC_DBG, "Cleared WRAPPER/A2H interrupt\n");
}

static int venus_hfi_core_ping(void *device)
{
	struct hfi_cmd_sys_ping_packet pkt;
	int rc = 0;
	struct venus_hfi_device *dev;

	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "invalid device\n");
		return -ENODEV;
	}

	rc = call_hfi_pkt_op(dev, sys_ping, &pkt);
	if (rc) {
		dprintk(VIDC_ERR, "core_ping: failed to create packet\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_core_trigger_ssr(void *device,
	enum hal_ssr_trigger_type type)
{
	struct hfi_cmd_sys_test_ssr_packet pkt;
	int rc = 0;
	struct venus_hfi_device *dev;

	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "invalid device\n");
		return -ENODEV;
	}

	rc = call_hfi_pkt_op(dev, ssr_cmd, type, &pkt);
	if (rc) {
		dprintk(VIDC_ERR, "core_ping: failed to create packet\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_session_set_property(void *sess,
					enum hal_property ptype, void *pdata)
{
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	struct hfi_cmd_session_set_property_packet *pkt =
		(struct hfi_cmd_session_set_property_packet *) &packet;
	struct hal_session *session = sess;
	struct venus_hfi_device *device;
	int rc = 0;

	if (!session || !session->device || !pdata) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	device = session->device;

	dprintk(VIDC_INFO, "in set_prop,with prop id: %#x\n", ptype);

	rc = call_hfi_pkt_op(device, session_set_property,
			pkt, session, ptype, pdata);
	if (rc) {
		dprintk(VIDC_ERR, "set property: failed to create packet\n");
		return -EINVAL;
	}

	if (venus_hfi_iface_cmdq_write(session->device, pkt))
		return -ENOTEMPTY;

	return rc;
}

static int venus_hfi_session_get_property(void *sess,
					enum hal_property ptype)
{
	struct hfi_cmd_session_get_property_packet pkt = {0};
	struct hal_session *session = sess;
	int rc = 0;
	struct venus_hfi_device *device;

	if (!session || !session->device) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	device = session->device;

	dprintk(VIDC_INFO, "%s: property id: %d\n", __func__, ptype);

	rc = call_hfi_pkt_op(device, session_get_property,
				&pkt, session, ptype);
	if (rc) {
		dprintk(VIDC_ERR, "get property profile: pkt failed\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(session->device, &pkt)) {
		rc = -ENOTEMPTY;
		dprintk(VIDC_ERR, "%s cmdq_write error\n", __func__);
	}
err_create_pkt:
	return rc;
}

static void venus_hfi_set_default_sys_properties(
		struct venus_hfi_device *device)
{
	if (venus_hfi_sys_set_debug(device, msm_vidc_fw_debug))
		dprintk(VIDC_WARN, "Setting fw_debug msg ON failed\n");
	if (venus_hfi_sys_set_idle_message(device,
		device->res->sys_idle_indicator || msm_vidc_sys_idle_indicator))
		dprintk(VIDC_WARN, "Setting idle response ON failed\n");
	if (venus_hfi_sys_set_power_control(device, msm_vidc_fw_low_power_mode))
		dprintk(VIDC_WARN, "Setting h/w power collapse ON failed\n");
}

static int venus_hfi_session_clean(void *session)
{
	struct hal_session *sess_close;
	struct venus_hfi_device *device;
	if (!session) {
		dprintk(VIDC_ERR, "Invalid Params %s\n", __func__);
		return -EINVAL;
	}
	sess_close = session;
	device = sess_close->device;
	venus_hfi_flush_debug_queue(sess_close->device, NULL);
	dprintk(VIDC_DBG, "deleted the session: %p\n",
			sess_close);
	mutex_lock(&device->session_lock);
	list_del(&sess_close->list);
	kfree(sess_close);
	mutex_unlock(&device->session_lock);
	return 0;
}

static void *venus_hfi_session_init(void *device, void *session_id,
		enum hal_domain session_type, enum hal_video_codec codec_type)
{
	struct hfi_cmd_sys_session_init_packet pkt;
	struct hal_session *new_session;
	struct venus_hfi_device *dev;

	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "invalid device\n");
		return NULL;
	}

	new_session = (struct hal_session *)
		kzalloc(sizeof(struct hal_session), GFP_KERNEL);
	if (!new_session) {
		dprintk(VIDC_ERR, "new session fail: Out of memory\n");
		return NULL;
	}
	new_session->session_id = session_id;
	new_session->is_decoder = (session_type == HAL_VIDEO_DOMAIN_DECODER);
	new_session->device = dev;

	mutex_lock(&dev->session_lock);
	list_add_tail(&new_session->list, &dev->sess_head);
	mutex_unlock(&dev->session_lock);

	venus_hfi_set_default_sys_properties(device);

	if (call_hfi_pkt_op(dev, session_init, &pkt,
			new_session, session_type, codec_type)) {
		dprintk(VIDC_ERR, "session_init: failed to create packet\n");
		goto err_session_init_fail;
	}

	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		goto err_session_init_fail;
	return (void *) new_session;

err_session_init_fail:
	venus_hfi_session_clean(new_session);
	return NULL;
}

static int venus_hfi_send_session_cmd(void *session_id,
	 int pkt_type)
{
	struct vidc_hal_session_cmd_pkt pkt;
	int rc = 0;
	struct hal_session *session = session_id;
	struct venus_hfi_device *device;

	if (!session || !session->device) {
		dprintk(VIDC_ERR, "invalid session");
		return -ENODEV;
	}
	device = session->device;

	rc = call_hfi_pkt_op(device, session_cmd,
			&pkt, pkt_type, session);
	if (rc) {
		dprintk(VIDC_ERR, "send session cmd: create pkt failed\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_session_end(void *session)
{
	struct hal_session *sess;
	if (!session) {
		dprintk(VIDC_ERR, "Invalid Params %s\n", __func__);
		return -EINVAL;
	}
	sess = session;
	if (msm_vidc_fw_coverage) {
		if (venus_hfi_sys_set_coverage(sess->device,
				msm_vidc_fw_coverage))
			dprintk(VIDC_WARN, "Fw_coverage msg ON failed\n");
	}
	return venus_hfi_send_session_cmd(session,
		HFI_CMD_SYS_SESSION_END);
}

static int venus_hfi_session_abort(void *session)
{
	venus_hfi_flush_debug_queue(
		((struct hal_session *)session)->device, NULL);
	return venus_hfi_send_session_cmd(session,
		HFI_CMD_SYS_SESSION_ABORT);
}

static int venus_hfi_session_set_buffers(void *sess,
				struct vidc_buffer_addr_info *buffer_info)
{
	struct hfi_cmd_session_set_buffers_packet *pkt;
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	int rc = 0;
	struct hal_session *session = sess;
	struct venus_hfi_device *device;

	if (!session || !session->device || !buffer_info) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	device = session->device;

	if (buffer_info->buffer_type == HAL_BUFFER_INPUT)
		return 0;

	pkt = (struct hfi_cmd_session_set_buffers_packet *)packet;

	rc = call_hfi_pkt_op(device, session_set_buffers,
			pkt, session, buffer_info);
	if (rc) {
		dprintk(VIDC_ERR, "set buffers: failed to create packet\n");
		goto err_create_pkt;
	}

	dprintk(VIDC_INFO, "set buffers: %#x\n", buffer_info->buffer_type);
	if (venus_hfi_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_release_buffers(void *sess,
				struct vidc_buffer_addr_info *buffer_info)
{
	struct hfi_cmd_session_release_buffer_packet *pkt;
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	int rc = 0;
	struct hal_session *session = sess;
	struct venus_hfi_device *device;

	if (!session || !session->device || !buffer_info) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	device = session->device;

	if (buffer_info->buffer_type == HAL_BUFFER_INPUT)
		return 0;

	pkt = (struct hfi_cmd_session_release_buffer_packet *) packet;

	rc = call_hfi_pkt_op(device, session_release_buffers,
			pkt, session, buffer_info);
	if (rc) {
		dprintk(VIDC_ERR, "release buffers: failed to create packet\n");
		goto err_create_pkt;
	}

	dprintk(VIDC_INFO, "Release buffers: %#x\n", buffer_info->buffer_type);
	if (venus_hfi_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_load_res(void *sess)
{
	return venus_hfi_send_session_cmd(sess,
		HFI_CMD_SESSION_LOAD_RESOURCES);
}

static int venus_hfi_session_release_res(void *sess)
{
	return venus_hfi_send_session_cmd(sess,
		HFI_CMD_SESSION_RELEASE_RESOURCES);
}

static int venus_hfi_session_start(void *sess)
{
	return venus_hfi_send_session_cmd(sess,
		HFI_CMD_SESSION_START);
}

static int venus_hfi_session_stop(void *sess)
{
	return venus_hfi_send_session_cmd(sess,
		HFI_CMD_SESSION_STOP);
}

static int venus_hfi_session_etb(void *sess,
				struct vidc_frame_data *input_frame)
{
	int rc = 0;
	struct hal_session *session = sess;
	struct venus_hfi_device *device;

	if (!session || !session->device || !input_frame) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	device = session->device;

	if (session->is_decoder) {
		struct hfi_cmd_session_empty_buffer_compressed_packet pkt;

		rc = call_hfi_pkt_op(device, session_etb_decoder,
				&pkt, session, input_frame);
		if (rc) {
			dprintk(VIDC_ERR,
			"Session etb decoder: failed to create pkt\n");
			goto err_create_pkt;
		}
		dprintk(VIDC_DBG, "Q DECODER INPUT BUFFER\n");
		if (venus_hfi_iface_cmdq_write(session->device, &pkt))
			rc = -ENOTEMPTY;
	} else {
		struct hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
			pkt;


		rc = call_hfi_pkt_op(device, session_etb_encoder,
					 &pkt, session, input_frame);
		if (rc) {
			dprintk(VIDC_ERR,
			"Session etb encoder: failed to create pkt\n");
			goto err_create_pkt;
		}
		dprintk(VIDC_DBG, "Q ENCODER INPUT BUFFER\n");
		if (venus_hfi_iface_cmdq_write(session->device, &pkt))
			rc = -ENOTEMPTY;
	}
err_create_pkt:
	return rc;
}

static int venus_hfi_session_ftb(void *sess,
				struct vidc_frame_data *output_frame)
{
	struct hfi_cmd_session_fill_buffer_packet pkt;
	int rc = 0;
	struct hal_session *session = sess;
	struct venus_hfi_device *device;

	if (!session || !session->device || !output_frame) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	device = session->device;

	rc = call_hfi_pkt_op(device, session_ftb,
			&pkt, session, output_frame);
	if (rc) {
		dprintk(VIDC_ERR, "Session ftb: failed to create pkt\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_parse_seq_hdr(void *sess,
					struct vidc_seq_hdr *seq_hdr)
{
	struct hfi_cmd_session_parse_sequence_header_packet *pkt;
	int rc = 0;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct hal_session *session = sess;
	struct venus_hfi_device *device;

	if (!session || !session->device || !seq_hdr) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	device = session->device;

	pkt = (struct hfi_cmd_session_parse_sequence_header_packet *) packet;

	rc = call_hfi_pkt_op(device, session_parse_seq_header,
			pkt, session, seq_hdr);
	if (rc) {
		dprintk(VIDC_ERR,
		"Session parse seq hdr: failed to create pkt\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_get_seq_hdr(void *sess,
				struct vidc_seq_hdr *seq_hdr)
{
	struct hfi_cmd_session_get_sequence_header_packet *pkt;
	int rc = 0;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct hal_session *session = sess;
	struct venus_hfi_device *device;

	if (!session || !session->device || !seq_hdr) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}
	device = session->device;

	pkt = (struct hfi_cmd_session_get_sequence_header_packet *) packet;

	rc = call_hfi_pkt_op(device, session_get_seq_hdr,
			pkt, session, seq_hdr);
	if (rc) {
		dprintk(VIDC_ERR,
				"Session get seq hdr: failed to create pkt\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_get_buf_req(void *sess)
{
	struct hfi_cmd_session_get_property_packet pkt;
	int rc = 0;
	struct hal_session *session = sess;
	struct venus_hfi_device *device;

	if (!session || !session->device) {
		dprintk(VIDC_ERR, "invalid session");
		return -ENODEV;
	}
	device = session->device;

	rc = call_hfi_pkt_op(device, session_get_buf_req,
			&pkt, session);
	if (rc) {
		dprintk(VIDC_ERR,
				"Session get buf req: failed to create pkt\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_flush(void *sess, enum hal_flush flush_mode)
{
	struct hfi_cmd_session_flush_packet pkt;
	int rc = 0;
	struct hal_session *session = sess;
	struct venus_hfi_device *device;

	if (!session || !session->device) {
		dprintk(VIDC_ERR, "invalid session");
		return -ENODEV;
	}
	device = session->device;

	rc = call_hfi_pkt_op(device, session_flush,
			&pkt, session, flush_mode);
	if (rc) {
		dprintk(VIDC_ERR, "Session flush: failed to create pkt\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_check_core_registered(
	struct hal_device_data core, phys_addr_t fw_addr,
	u8 *reg_addr, u32 reg_size, phys_addr_t irq)
{
	struct venus_hfi_device *device;
	struct list_head *curr, *next;

	if (core.dev_count) {
		list_for_each_safe(curr, next, &core.dev_head) {
			device = list_entry(curr,
				struct venus_hfi_device, list);
			if (device && device->hal_data->irq == irq &&
				(CONTAINS(device->hal_data->
						firmware_base,
						FIRMWARE_SIZE, fw_addr) ||
				CONTAINS(fw_addr, FIRMWARE_SIZE,
						device->hal_data->
						firmware_base) ||
				CONTAINS(device->hal_data->
						register_base,
						reg_size, reg_addr) ||
				CONTAINS(reg_addr, reg_size,
						device->hal_data->
						register_base) ||
				OVERLAPS(device->hal_data->
						register_base,
						reg_size, reg_addr, reg_size) ||
				OVERLAPS(reg_addr, reg_size,
						device->hal_data->
						register_base, reg_size) ||
				OVERLAPS(device->hal_data->
						firmware_base,
						FIRMWARE_SIZE, fw_addr,
						FIRMWARE_SIZE) ||
				OVERLAPS(fw_addr, FIRMWARE_SIZE,
						device->hal_data->
						firmware_base,
						FIRMWARE_SIZE))) {
				return 0;
			} else {
				dprintk(VIDC_INFO, "Device not registered\n");
				return -EINVAL;
			}
		}
	} else {
		dprintk(VIDC_INFO, "no device Registered\n");
	}
	return -EINVAL;
}

static void venus_hfi_process_sys_watchdog_timeout(
				struct venus_hfi_device *device)
{
	struct msm_vidc_cb_cmd_done cmd_done;
	memset(&cmd_done, 0, sizeof(struct msm_vidc_cb_cmd_done));
	cmd_done.device_id = device->device_id;
	device->callback(SYS_WATCHDOG_TIMEOUT, &cmd_done);
}

static int venus_hfi_core_pc_prep(void *device)
{
	struct hfi_cmd_sys_pc_prep_packet pkt;
	int rc = 0;
	struct venus_hfi_device *dev = device;

	if (!dev) {
		dprintk(VIDC_ERR, "invalid device\n");
		return -ENODEV;
	}

	rc = call_hfi_pkt_op(dev, sys_pc_prep, &pkt);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to create sys pc prep pkt\n");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_prepare_pc(struct venus_hfi_device *device)
{
	int rc = 0;
	init_completion(&pc_prep_done);
	rc = venus_hfi_core_pc_prep(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to prepare venus for power off");
		goto err_pc_prep;
	}
	rc = wait_for_completion_timeout(&pc_prep_done,
			msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR,
				"Wait interrupted or timeout for PC_PREP_DONE: %d\n",
				rc);
		venus_hfi_flush_debug_queue(device, NULL);
		rc = -EIO;
		goto err_pc_prep;
	}
	rc = 0;
err_pc_prep:
	return rc;
}

static void venus_hfi_pm_hndlr(struct work_struct *work)
{
	int rc = 0;
	u32 ctrl_status = 0;
	struct venus_hfi_device *device = list_first_entry(
			&hal_ctxt.dev_head, struct venus_hfi_device, list);
	if (!device) {
		dprintk(VIDC_ERR, "%s: NULL device\n", __func__);
		return;
	}
	if (!device->power_enabled) {
		dprintk(VIDC_DBG, "%s: Power already disabled\n",
				__func__);
		return;
	}

	mutex_lock(&device->write_lock);
	mutex_lock(&device->read_lock);
	rc = venus_hfi_core_in_valid_state(device);
	mutex_unlock(&device->read_lock);
	mutex_unlock(&device->write_lock);

	if (!rc) {
		dprintk(VIDC_WARN,
			"Core is in bad state, Skipping power collapse\n");
		return;
	}

	dprintk(VIDC_DBG, "Prepare for power collapse\n");

	if (device->resources.imem.type) {
		mutex_lock(&device->resource_lock);
		rc = venus_hfi_unset_free_imem(device);
		mutex_unlock(&device->resource_lock);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to unset IMEM for PC: %d\n",
					rc);
			goto err_unset_imem;
		}
	}

	rc = venus_hfi_prepare_pc(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to prepare for PC %d\n", rc);
		goto err_prepare_pc;
	}

	mutex_lock(&device->write_lock);

	if (device->last_packet_type != HFI_CMD_SYS_PC_PREP) {
		dprintk(VIDC_DBG,
			"Last command (%#x) is not PC_PREP cmd\n",
			device->last_packet_type);
		goto skip_power_off;
	}

	if (venus_hfi_get_q_size(device, VIDC_IFACEQ_MSGQ_IDX) ||
		venus_hfi_get_q_size(device, VIDC_IFACEQ_CMDQ_IDX)) {
		dprintk(VIDC_DBG, "Cmd/msg queues are not empty\n");
		goto skip_power_off;
	}

	ctrl_status = venus_hfi_read_register(device, VIDC_CPU_CS_SCIACMDARG0);
	if (!(ctrl_status & VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY)) {
		dprintk(VIDC_DBG,
			"Venus is not ready for power collapse (%#x)\n",
			ctrl_status);
		goto skip_power_off;
	}

	rc = venus_hfi_power_off(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed venus power off\n");
		goto err_power_off;
	}

	/* Cancel pending delayed works if any */
	cancel_delayed_work(&venus_hfi_pm_work);

	mutex_unlock(&device->write_lock);
	return;

err_power_off:
skip_power_off:

	/*
	* When power collapse is escaped, driver no need to inform Venus.
	* Venus is self-sufficient to come out of the power collapse at
	* any stage. Driver can skip power collapse and continue with
	* normal execution.
	*/

	/* Cancel pending delayed works if any */
	cancel_delayed_work(&venus_hfi_pm_work);
	dprintk(VIDC_WARN, "Power off skipped (last pkt %#x, status: %#x)\n",
		device->last_packet_type, ctrl_status);

	mutex_unlock(&device->write_lock);
err_prepare_pc:
	venus_hfi_alloc_imem(device, device->res->imem_size);
err_unset_imem:
	return;
}

static void venus_hfi_process_msg_event_notify(
	struct venus_hfi_device *device, void *packet)
{
	struct hfi_sfr_struct *vsfr = NULL;
	struct hfi_msg_event_notify_packet *event_pkt;
	struct vidc_hal_msg_pkt_hdr *msg_hdr;

	msg_hdr = (struct vidc_hal_msg_pkt_hdr *)packet;
	event_pkt =
		(struct hfi_msg_event_notify_packet *)msg_hdr;
	if (event_pkt && event_pkt->event_id ==
		HFI_EVENT_SYS_ERROR) {

		venus_hfi_set_state(device, VENUS_STATE_DEINIT);

		/* Once SYS_ERROR received from HW, it is safe to halt the AXI.
		 * With SYS_ERROR, Venus FW may have crashed and HW might be
		 * active and causing unnecessary transactions. Hence it is
		 * safe to stop all AXI transactions from venus sub-system. */
		if (venus_hfi_halt_axi(device))
			dprintk(VIDC_WARN,
				"Failed to halt AXI after SYS_ERROR\n");

		vsfr = (struct hfi_sfr_struct *)
				device->sfr.align_virtual_addr;
		if (vsfr) {
			void *p = memchr(vsfr->rg_data, '\0',
							vsfr->bufSize);
			/* SFR isn't guaranteed to be NULL terminated
			since SYS_ERROR indicates that Venus is in the
			process of crashing.*/
			if (p == NULL)
				vsfr->rg_data[vsfr->bufSize - 1] = '\0';
			dprintk(VIDC_ERR, "SFR Message from FW: %s\n",
				vsfr->rg_data);
		}
	}
}

static void venus_hfi_flush_debug_queue(
	struct venus_hfi_device *device, u8 *packet)
{
	bool local_packet = false;

	if (!device) {
		dprintk(VIDC_ERR, "%s: Invalid params\n", __func__);
		return;
	}
	if (!packet) {
		packet = kzalloc(VIDC_IFACEQ_VAR_HUGE_PKT_SIZE, GFP_TEMPORARY);
		if (!packet) {
			dprintk(VIDC_ERR, "In %s() Fail to allocate mem\n",
				__func__);
			return;
		}
		local_packet = true;
	}

	while (!venus_hfi_iface_dbgq_read(device, packet)) {
		struct hfi_msg_sys_coverage_packet *pkt =
			(struct hfi_msg_sys_coverage_packet *) packet;
		if (pkt->packet_type == HFI_MSG_SYS_COV) {
			int stm_size = 0;
			dprintk(VIDC_DBG,
				"DbgQ pkt size: %d\n", pkt->msg_size);
			stm_size = stm_log_inv_ts(0, 0,
				pkt->rg_msg_data, pkt->msg_size);
			if (stm_size == 0)
				dprintk(VIDC_ERR,
					"In %s, stm_log returned size of 0\n",
					__func__);
		} else {
			struct hfi_msg_sys_debug_packet *pkt =
				(struct hfi_msg_sys_debug_packet *) packet;
			dprintk(VIDC_FW, "%s", pkt->rg_msg_data);
		}
	}
	if (local_packet)
		kfree(packet);
}

static void venus_hfi_response_handler(struct venus_hfi_device *device)
{
	u8 *packet = NULL;
	u32 rc = 0;
	struct hfi_sfr_struct *vsfr = NULL;

	packet = kzalloc(VIDC_IFACEQ_VAR_HUGE_PKT_SIZE, GFP_TEMPORARY);
	if (!packet) {
		dprintk(VIDC_ERR, "In %s() Fail to allocate mem\n",  __func__);
		return;
	}
	dprintk(VIDC_INFO, "#####venus_hfi_response_handler#####\n");
	if (device) {
		if (device->intr_status &
			VIDC_WRAPPER_INTR_CLEAR_A2HWD_BMSK) {
			dprintk(VIDC_ERR, "Received: Watchdog timeout %s\n",
				__func__);
			vsfr = (struct hfi_sfr_struct *)
					device->sfr.align_virtual_addr;
			if (vsfr)
				dprintk(VIDC_ERR,
					"SFR Message from FW: %s\n",
						vsfr->rg_data);
			venus_hfi_process_sys_watchdog_timeout(device);
		}

		while (!venus_hfi_iface_msgq_read(device, packet)) {
			rc = hfi_process_msg_packet(device->callback,
				device->device_id,
				(struct vidc_hal_msg_pkt_hdr *) packet,
				&device->sess_head, &device->session_lock);
			if (rc == HFI_MSG_EVENT_NOTIFY) {
				venus_hfi_process_msg_event_notify(
					device, (void *)packet);
			} else if (rc == HFI_MSG_SYS_RELEASE_RESOURCE) {
				dprintk(VIDC_DBG,
					"Received HFI_MSG_SYS_RELEASE_RESOURCE\n");
				complete(&release_resources_done);
			} else if (rc == HFI_MSG_SYS_INIT_DONE) {
				dprintk(VIDC_DBG,
					"Received HFI_MSG_SYS_INIT_DONE\n");
				if (venus_hfi_alloc_set_imem(device, true))
					dprintk(VIDC_WARN,
						"Failed to allocate IMEM. Performance will be impacted\n");
			}
		}
		venus_hfi_flush_debug_queue(device, packet);
		switch (rc) {
		case HFI_MSG_SYS_PC_PREP_DONE:
			dprintk(VIDC_DBG,
					"Received HFI_MSG_SYS_PC_PREP_DONE\n");
			complete(&pc_prep_done);
			break;
		}
	} else {
		dprintk(VIDC_ERR, "SPURIOUS_INTERRUPT\n");
	}
	kfree(packet);
}

static void venus_hfi_core_work_handler(struct work_struct *work)
{
	struct venus_hfi_device *device = list_first_entry(
		&hal_ctxt.dev_head, struct venus_hfi_device, list);

	dprintk(VIDC_INFO, "GOT INTERRUPT\n");
	if (!device->callback) {
		dprintk(VIDC_ERR, "No interrupt callback function: %p\n",
				device);
		return;
	}
	if (venus_hfi_power_enable(device)) {
		dprintk(VIDC_ERR, "%s: Power enable failed\n", __func__);
		return;
	}
	if (device->res->sw_power_collapsible) {
		dprintk(VIDC_DBG, "Cancel and queue delayed work again.\n");
		cancel_delayed_work(&venus_hfi_pm_work);
		if (!queue_delayed_work(device->venus_pm_workq,
			&venus_hfi_pm_work,
			msecs_to_jiffies(msm_vidc_pwr_collapse_delay))) {
			dprintk(VIDC_DBG, "PM work already scheduled\n");
		}
	}
	venus_hfi_core_clear_interrupt(device);
	venus_hfi_response_handler(device);
	if (!(device->intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK))
		enable_irq(device->hal_data->irq);
}
static DECLARE_WORK(venus_hfi_work, venus_hfi_core_work_handler);

static irqreturn_t venus_hfi_isr(int irq, void *dev)
{
	struct venus_hfi_device *device = dev;
	dprintk(VIDC_INFO, "vidc_hal_isr %d\n", irq);
	disable_irq_nosync(irq);
	queue_work(device->vidc_workq, &venus_hfi_work);
	return IRQ_HANDLED;
}

static int venus_hfi_init_regs_and_interrupts(
		struct venus_hfi_device *device,
		struct msm_vidc_platform_resources *res)
{
	struct hal_data *hal = NULL;
	int rc = 0;

	rc = venus_hfi_check_core_registered(hal_ctxt,
			res->firmware_base,
			(u8 *)(unsigned long)res->register_base,
			res->register_size, res->irq);
	if (!rc) {
		dprintk(VIDC_ERR, "Core present/Already added\n");
		rc = -EEXIST;
		goto err_core_init;
	}

	dprintk(VIDC_DBG, "HAL_DATA will be assigned now\n");
	hal = (struct hal_data *)
		kzalloc(sizeof(struct hal_data), GFP_KERNEL);
	if (!hal) {
		dprintk(VIDC_ERR, "Failed to alloc\n");
		rc = -ENOMEM;
		goto err_core_init;
	}
	hal->irq = res->irq;
	hal->firmware_base = res->firmware_base;
	hal->register_base = ioremap_nocache(res->register_base,
			(unsigned long)res->register_size);
	hal->register_size = res->register_size;
	if (!hal->register_base) {
		dprintk(VIDC_ERR,
			"could not map reg addr %pa of size %d\n",
			&res->register_base, res->register_size);
		goto error_irq_fail;
	}

	device->hal_data = hal;
	rc = request_irq(res->irq, venus_hfi_isr, IRQF_TRIGGER_HIGH,
			"msm_vidc", device);
	if (unlikely(rc)) {
		dprintk(VIDC_ERR, "() :request_irq failed\n");
		goto error_irq_fail;
	}
	disable_irq_nosync(res->irq);
	dprintk(VIDC_INFO,
		"firmware_base = %pa, register_base = %pa, register_size = %d\n",
		&res->firmware_base, &res->register_base,
		res->register_size);
	return rc;

error_irq_fail:
	kfree(hal);
err_core_init:
	return rc;

}

static inline void venus_hfi_deinit_clocks(struct venus_hfi_device *device)
{
	struct clock_info *cl;

	venus_hfi_for_each_clock_reverse(device, cl) {
		if (cl->clk) {
			clk_put(cl->clk);
			cl->clk = NULL;
		}
	}
}

static inline int venus_hfi_init_clocks(struct venus_hfi_device *device)
{
	int rc = 0;
	struct clock_info *cl = NULL;

	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}

	venus_hfi_for_each_clock(device, cl) {
		int i = 0;

		dprintk(VIDC_DBG, "%s: scalable? %d\n",
				cl->name, !!cl->count);
		for (i = 0; i < cl->count; ++i) {
			dprintk(VIDC_DBG,
				"\tload = %d, freq = %d codecs supported %#x\n",
				cl->load_freq_tbl[i].load,
				cl->load_freq_tbl[i].freq,
				cl->load_freq_tbl[i].supported_codecs);
		}
	}

	venus_hfi_for_each_clock(device, cl) {
		if (!cl->clk) {
			cl->clk = clk_get(&device->res->pdev->dev, cl->name);
			if (IS_ERR_OR_NULL(cl->clk)) {
				dprintk(VIDC_ERR,
					"Failed to get clock: %s\n", cl->name);
				rc = PTR_ERR(cl->clk) ?: -EINVAL;
				cl->clk = NULL;
				goto err_clk_get;
			}
		}
	}

	return 0;

err_clk_get:
	venus_hfi_deinit_clocks(device);
	return rc;
}


static inline void venus_hfi_disable_unprepare_clks(
	struct venus_hfi_device *device)
{
	struct clock_info *cl;

	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return;
	}

	if (device->clk_state == DISABLED_UNPREPARED) {
		dprintk(VIDC_DBG, "Clocks already unprepared and disabled\n");
		return;
	}

	/*
	* Make the clock state variable as unprepared before actually
	* unpreparing clocks. This will make sure that when we check
	* the state, we have the right clock state. We are not taking
	* any action based unprepare failures. So it is safe to do
	* before the call. This is also in sync with prepare_enable
	* state update.
	*/

	device->clk_state = DISABLED_UNPREPARED;

	venus_hfi_for_each_clock(device, cl) {
		 usleep_range(100, 500);
		dprintk(VIDC_DBG, "Clock: %s disable and unprepare\n",
				cl->name);
		clk_disable_unprepare(cl->clk);
	}
}

static inline int venus_hfi_prepare_enable_clks(struct venus_hfi_device *device)
{
	struct clock_info *cl = NULL, *cl_fail = NULL;
	int rc = 0;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}

	if (device->clk_state == ENABLED_PREPARED) {
		dprintk(VIDC_DBG, "Clocks already prepared and enabled\n");
		return 0;
	}

	venus_hfi_for_each_clock(device, cl) {
		/*
		 * For the clocks we control, set the rate prior to preparing
		 * them.  Since we don't really have a load at this point, scale
		 * it to the lowest frequency possible
		 */
		if (cl->count)
			clk_set_rate(cl->clk, clk_round_rate(cl->clk, 0));

		rc = clk_prepare_enable(cl->clk);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to enable clocks\n");
			cl_fail = cl;
			goto fail_clk_enable;
		}

		dprintk(VIDC_DBG, "Clock: %s prepared and enabled\n", cl->name);
	}

	device->clk_state = ENABLED_PREPARED;
	venus_hfi_write_register(device, VIDC_WRAPPER_CLOCK_CONFIG, 0);
	venus_hfi_write_register(device, VIDC_WRAPPER_CPU_CLOCK_CONFIG, 0);
	return rc;

fail_clk_enable:
	venus_hfi_for_each_clock(device, cl) {
		if (cl_fail == cl)
			break;
		usleep_range(100, 500);
		dprintk(VIDC_ERR, "Clock: %s disable and unprepare\n",
			cl->name);
		clk_disable_unprepare(cl->clk);
	}
	device->clk_state = DISABLED_UNPREPARED;
	return rc;
}

static void venus_hfi_deinit_bus(struct venus_hfi_device *device)
{
	struct bus_info *bus = NULL;
	if (!device)
		return;

	kfree(device->bus_vote.data);
	device->bus_vote = DEFAULT_BUS_VOTE;

	venus_hfi_for_each_bus_reverse(device, bus) {
		devfreq_remove_device(bus->devfreq);
		bus->devfreq = NULL;
		dev_set_drvdata(bus->dev, NULL);

		msm_bus_scale_unregister(bus->client);
		bus->client = NULL;
	}
}

static int venus_hfi_init_bus(struct venus_hfi_device *device)
{
	struct bus_info *bus = NULL;
	int rc = 0;

	if (!device)
		return -EINVAL;

	venus_hfi_for_each_bus(device, bus) {
		struct devfreq_dev_profile profile = {
			.initial_freq = 0,
			.polling_ms = INT_MAX,
			.freq_table = bus->range,
			.max_state = ARRAY_SIZE(bus->range),
			.target = venus_hfi_devfreq_target,
			.get_dev_status = venus_hfi_devfreq_get_status,
			.exit = NULL,
		};

		/*
		 * This is stupid, but there's no other easy way to ahold
		 * of struct bus_info in venus_hfi_devfreq_*()
		 */
		WARN(dev_get_drvdata(bus->dev), "%s's drvdata already set\n",
				dev_name(bus->dev));
		dev_set_drvdata(bus->dev, device);

		bus->client = msm_bus_scale_register(bus->master, bus->slave,
				bus->name, false);
		if (IS_ERR_OR_NULL(bus->client)) {
			rc = PTR_ERR(bus->client) ?: -EBADHANDLE;
			dprintk(VIDC_ERR, "Failed to register bus %s: %d\n",
					bus->name, rc);
			bus->client = NULL;
			goto err_add_dev;
		}

		bus->devfreq_prof = profile;
		bus->devfreq = devfreq_add_device(bus->dev,
				&bus->devfreq_prof, bus->governor, NULL);
		if (IS_ERR_OR_NULL(bus->devfreq)) {
			rc = PTR_ERR(bus->devfreq) ?: -EBADHANDLE;
			dprintk(VIDC_ERR,
					"Failed to add devfreq device for bus %s and governor %s: %d\n",
					bus->name, bus->governor, rc);
			bus->devfreq = NULL;
			goto err_add_dev;
		}

		/*
		 * Devfreq starts monitoring immediately, since we are just
		 * initializing stuff at this point, force it to suspend
		 */
		devfreq_suspend_device(bus->devfreq);
	}

	device->bus_vote = DEFAULT_BUS_VOTE;
	return 0;

err_add_dev:
	venus_hfi_deinit_bus(device);
	return rc;
}

static void venus_hfi_deinit_regulators(struct venus_hfi_device *device)
{
	struct regulator_info *rinfo = NULL;

	venus_hfi_for_each_regulator_reverse(device, rinfo) {
		if (rinfo->regulator) {
			regulator_put(rinfo->regulator);
			rinfo->regulator = NULL;
		}
	}
}

static int venus_hfi_init_regulators(struct venus_hfi_device *device)
{
	int rc = 0;
	struct regulator_info *rinfo = NULL;

	venus_hfi_for_each_regulator(device, rinfo) {
		rinfo->regulator = regulator_get(&device->res->pdev->dev,
				rinfo->name);
		if (IS_ERR_OR_NULL(rinfo->regulator)) {
			rc = PTR_ERR(rinfo->regulator) ?: -EBADHANDLE;
			dprintk(VIDC_ERR, "Failed to get regulator: %s\n",
					rinfo->name);
			rinfo->regulator = NULL;
			goto err_reg_get;
		}
	}

	return 0;

err_reg_get:
	venus_hfi_deinit_regulators(device);
	return rc;
}

static int venus_hfi_init_resources(struct venus_hfi_device *device,
				struct msm_vidc_platform_resources *res)
{
	int rc = 0;

	rc = venus_hfi_init_regulators(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get all regulators\n");
		return -ENODEV;
	}

	rc = venus_hfi_init_clocks(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init clocks\n");
		rc = -ENODEV;
		goto err_init_clocks;
	}

	rc = venus_hfi_init_bus(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init bus: %d\n", rc);
		goto err_init_bus;
	}

	return rc;

err_init_bus:
	venus_hfi_deinit_clocks(device);
err_init_clocks:
	venus_hfi_deinit_regulators(device);
	return rc;
}

static void venus_hfi_deinit_resources(struct venus_hfi_device *device)
{
	venus_hfi_deinit_bus(device);
	venus_hfi_deinit_clocks(device);
	venus_hfi_deinit_regulators(device);
}

static int protect_cp_mem(struct venus_hfi_device *device)
{
	struct tzbsp_memprot memprot;
	unsigned int resp = 0;
	int rc = 0;
	struct context_bank_info *cb;
	struct scm_desc desc = {0};

	if (!device)
		return -EINVAL;

	memprot.cp_start = 0x0;
	memprot.cp_size = 0x0;
	memprot.cp_nonpixel_start = 0x0;
	memprot.cp_nonpixel_size = 0x0;

	list_for_each_entry(cb, &device->res->context_banks, list) {
		if (!strcmp(cb->name, "venus_ns")) {
			desc.args[1] = memprot.cp_size =
				cb->addr_range.start;
			dprintk(VIDC_DBG, "%s memprot.cp_size: %#x\n",
				__func__, memprot.cp_size);
		}

		if (!strcmp(cb->name, "venus_sec_non_pixel")) {
			desc.args[2] = memprot.cp_nonpixel_start =
				cb->addr_range.start;
			desc.args[3] = memprot.cp_nonpixel_size =
				cb->addr_range.size;
			dprintk(VIDC_DBG,
				"%s memprot.cp_nonpixel_start: %#x size: %#x\n",
				__func__, memprot.cp_nonpixel_start,
				memprot.cp_nonpixel_size);
		}
	}

	if (!is_scm_armv8()) {
		rc = scm_call(SCM_SVC_MP, TZBSP_MEM_PROTECT_VIDEO_VAR, &memprot,
			sizeof(memprot), &resp, sizeof(resp));
	} else {
		desc.arginfo = SCM_ARGS(4);
		rc = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
			       TZBSP_MEM_PROTECT_VIDEO_VAR), &desc);
		resp = desc.ret[0];
	}

	if (rc) {
		dprintk(VIDC_ERR, "Failed to protect memory(%d) response: %d\n",
				rc, resp);
	}

	trace_venus_hfi_var_done(
		memprot.cp_start, memprot.cp_size,
		memprot.cp_nonpixel_start, memprot.cp_nonpixel_size);
	return rc;
}

static int venus_hfi_disable_regulator(struct regulator_info *rinfo)
{
	int rc = 0;

	dprintk(VIDC_DBG, "Disabling regulator %s\n", rinfo->name);

	/*
	* This call is needed. Driver needs to acquire the control back
	* from HW in order to disable the regualtor. Else the behavior
	* is unknown.
	*/

	rc = venus_hfi_acquire_regulator(rinfo);

	if (rc) {
		/* This is somewhat fatal, but nothing we can do
		 * about it. We can't disable the regulator w/o
		 * getting it back under s/w control */
		dprintk(VIDC_WARN,
			"Failed to acquire control on %s\n",
			rinfo->name);

		goto disable_regulator_failed;
	}
	rc = regulator_disable(rinfo->regulator);
	if (rc) {
		dprintk(VIDC_WARN,
			"Failed to disable %s: %d\n",
			rinfo->name, rc);
		goto disable_regulator_failed;
	}

	return 0;
disable_regulator_failed:

	/* Bring attention to this issue */
	WARN_ON(1);
	return rc;
}

static int venus_hfi_enable_hw_power_collapse(struct venus_hfi_device *device)
{
	int rc = 0;

	if (!msm_vidc_fw_low_power_mode) {
		dprintk(VIDC_DBG, "Not enabling hardware power collapse\n");
		return 0;
	}

	rc = venus_hfi_hand_off_regulators(device);
	if (rc)
		dprintk(VIDC_WARN,
			"%s : Failed to enable HW power collapse %d\n",
				__func__, rc);
	return rc;
}

static int venus_hfi_enable_regulators(struct venus_hfi_device *device)
{
	int rc = 0, c = 0;
	struct regulator_info *rinfo;

	dprintk(VIDC_DBG, "Enabling regulators\n");

	venus_hfi_for_each_regulator(device, rinfo) {
		rc = regulator_enable(rinfo->regulator);
		if (rc) {
			dprintk(VIDC_ERR,
					"Failed to enable %s: %d\n",
					rinfo->name, rc);
			goto err_reg_enable_failed;
		}
		dprintk(VIDC_DBG, "Enabled regulator %s\n",
				rinfo->name);
		c++;
	}

	return 0;

err_reg_enable_failed:
	venus_hfi_for_each_regulator_reverse_continue(device, rinfo, c)
		venus_hfi_disable_regulator(rinfo);

	return rc;
}

static int venus_hfi_disable_regulators(struct venus_hfi_device *device)
{
	struct regulator_info *rinfo;

	dprintk(VIDC_DBG, "Disabling regulators\n");

	venus_hfi_for_each_regulator_reverse(device, rinfo)
		venus_hfi_disable_regulator(rinfo);

	return 0;
}

static int venus_hfi_load_fw(void *dev)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;

	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid paramter: %p\n",
			__func__, device);
		return -EINVAL;
	}

	/* Initialize hardware resources */
	rc = venus_hfi_init_resources(device, device->res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init resources: %d\n", rc);
		goto fail_init_res;
	}

	rc = venus_hfi_initialize_packetization(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to initialize packetization\n");
		goto fail_init_pkt;
	}
	trace_msm_v4l2_vidc_fw_load_start("msm_v4l2_vidc venus_fw load start");

	/* Vote for all hardware resources */
	rc = venus_hfi_vote_buses(device, device->bus_vote.data,
			device->bus_vote.data_count);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to vote buses when loading firmware: %d\n",
				rc);
		goto fail_vote_buses;
	}

	rc = venus_hfi_enable_regulators(device);
	if (rc) {
		dprintk(VIDC_ERR, "%s : Failed to enable GDSC, Err = %d\n",
			__func__, rc);
		goto fail_enable_gdsc;
	}

	/* iommu_attach makes call to TZ for restore_sec_cfg. With this call
	 * TZ accesses the VMIDMT block which needs all the Venus clocks.
	 */
	rc = venus_hfi_prepare_enable_clks(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to enable clocks: %d\n", rc);
		goto fail_enable_clks;
	}

	if ((!device->res->use_non_secure_pil && !device->res->firmware_base)
			|| device->res->use_non_secure_pil) {

		if (!device->resources.fw.cookie)
			device->resources.fw.cookie =
				subsystem_get_with_fwname("venus",
				device->res->fw_name);

		if (IS_ERR_OR_NULL(device->resources.fw.cookie)) {
			dprintk(VIDC_ERR, "Failed to download firmware\n");
			rc = -ENOMEM;
			goto fail_load_fw;
		}
	}
	device->power_enabled = true;

	/* Hand off control of regulators to h/w _after_ enabling clocks */
	venus_hfi_enable_hw_power_collapse(device);

	if (!device->res->use_non_secure_pil && !device->res->firmware_base) {
		rc = protect_cp_mem(device);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to protect memory\n");
			goto fail_protect_mem;
		}
	}

	trace_msm_v4l2_vidc_fw_load_end("msm_v4l2_vidc venus_fw load end");
	return rc;
fail_protect_mem:
	device->power_enabled = false;
	if (device->resources.fw.cookie)
		subsystem_put(device->resources.fw.cookie);
	device->resources.fw.cookie = NULL;
fail_load_fw:
	venus_hfi_disable_unprepare_clks(device);
fail_enable_clks:
	venus_hfi_disable_regulators(device);
fail_enable_gdsc:
	venus_hfi_unvote_buses(device);
fail_vote_buses:
fail_init_pkt:
	venus_hfi_deinit_resources(device);
fail_init_res:
	trace_msm_v4l2_vidc_fw_load_end("msm_v4l2_vidc venus_fw load end");
	return rc;
}

static void venus_hfi_unload_fw(void *dev)
{
	struct venus_hfi_device *device = dev;
	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid paramter: %p\n",
			__func__, device);
		return;
	}
	if (device->resources.fw.cookie) {
		flush_workqueue(device->vidc_workq);
		cancel_delayed_work(&venus_hfi_pm_work);
		flush_workqueue(device->venus_pm_workq);
		subsystem_put(device->resources.fw.cookie);
		venus_hfi_interface_queues_release(dev);

		/* Halt the AXI to make sure there are no pending transactions.
		 * Clocks should be unprepared after making sure axi is halted.
		 */
		if (venus_hfi_halt_axi(device))
			dprintk(VIDC_WARN, "Failed to halt AXI\n");
		venus_hfi_disable_unprepare_clks(device);
		venus_hfi_disable_regulators(device);
		venus_hfi_unvote_buses(device);
		device->power_enabled = false;
		device->resources.fw.cookie = NULL;
		venus_hfi_deinit_resources(device);
	}
}

static int venus_hfi_resurrect_fw(void *dev)
{
	struct venus_hfi_device *device = dev;
	int rc = 0;

	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid paramter: %p\n",
			__func__, device);
		return -EINVAL;
	}

	rc = venus_hfi_core_release(device);
	if (rc) {
		dprintk(VIDC_ERR, "%s - failed to release venus core rc = %d\n",
				__func__, rc);
		goto exit;
	}

	dprintk(VIDC_ERR, "praying for firmware resurrection\n");
	venus_hfi_unload_fw(device);

	rc = venus_hfi_vote_buses(device, device->bus_vote.data,
			device->bus_vote.data_count);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to scale buses\n");
		goto exit;
	}

	rc = venus_hfi_load_fw(device);
	if (rc) {
		dprintk(VIDC_ERR, "%s - failed to load venus fw rc = %d\n",
				__func__, rc);
		goto exit;
	}

	dprintk(VIDC_ERR, "Hurray!! firmware has restarted\n");
exit:
	return rc;
}

static int venus_hfi_get_fw_info(void *dev, enum fw_info info)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;

	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid paramter: %p\n",
			__func__, device);
		return -EINVAL;
	}

	switch (info) {
	case FW_BASE_ADDRESS:
		rc = (u32)device->hal_data->firmware_base;
		if ((phys_addr_t)rc != device->hal_data->firmware_base) {
			dprintk(VIDC_INFO,
				"%s: firmware_base (%pa) truncated to %#x",
				__func__, &device->hal_data->firmware_base, rc);
		}
		break;

	case FW_REGISTER_BASE:
		rc = (u32)device->res->register_base;
		if ((phys_addr_t)rc != device->res->register_base) {
			dprintk(VIDC_INFO,
				"%s: register_base (%pa) truncated to %#x",
				__func__, &device->res->register_base, rc);
		}
		break;

	case FW_REGISTER_SIZE:
		rc = device->hal_data->register_size;
		break;

	case FW_IRQ:
		rc = device->hal_data->irq;
		break;

	default:
		dprintk(VIDC_ERR, "Invalid fw info requested\n");
	}
	return rc;
}

int venus_hfi_get_stride_scanline(int color_fmt,
	int width, int height, int *stride, int *scanlines) {
	*stride = VENUS_Y_STRIDE(color_fmt, width);
	*scanlines = VENUS_Y_SCANLINES(color_fmt, height);
	return 0;
}

int venus_hfi_get_core_capabilities(void)
{
	int rc = 0;
	rc = HAL_VIDEO_ENCODER_ROTATION_CAPABILITY |
		HAL_VIDEO_ENCODER_SCALING_CAPABILITY |
		HAL_VIDEO_ENCODER_DEINTERLACE_CAPABILITY |
		HAL_VIDEO_DECODER_MULTI_STREAM_CAPABILITY;
	return rc;
}

static int venus_hfi_initialize_packetization(struct venus_hfi_device *device)
{
	int rc = 0;
	const char *hfi_version;

	if (!device || !device->res) {
		dprintk(VIDC_ERR, "%s - invalid param\n", __func__);
		return -EINVAL;
	}

	hfi_version = device->res->hfi_version;

	if (!hfi_version) {
		device->packetization_type = HFI_PACKETIZATION_LEGACY;
	} else if (!strcmp(hfi_version, "3xx")) {
		device->packetization_type = HFI_PACKETIZATION_3XX;
	} else {
		dprintk(VIDC_ERR, "Unsupported hfi version\n");
		return -EINVAL;
	}

	device->pkt_ops = hfi_get_pkt_ops_handle(device->packetization_type);
	if (!device->pkt_ops) {
		rc = -EINVAL;
		dprintk(VIDC_ERR, "Failed to get pkt_ops handle\n");
	}

	return rc;
}

static void *venus_hfi_add_device(u32 device_id,
			struct msm_vidc_platform_resources *res,
			hfi_cmd_response_callback callback)
{
	struct venus_hfi_device *hdevice = NULL;
	int rc = 0;

	if (!res || !callback) {
		dprintk(VIDC_ERR, "Invalid Parameters\n");
		return NULL;
	}

	dprintk(VIDC_INFO, "entered , device_id: %d\n", device_id);

	hdevice = (struct venus_hfi_device *)
			kzalloc(sizeof(struct venus_hfi_device), GFP_KERNEL);
	if (!hdevice) {
		dprintk(VIDC_ERR, "failed to allocate new device\n");
		goto err_alloc;
	}

	rc = venus_hfi_init_regs_and_interrupts(hdevice, res);
	if (rc)
		goto err_init_regs;

	hdevice->res = res;
	hdevice->device_id = device_id;
	hdevice->callback = callback;

	hdevice->vidc_workq = create_singlethread_workqueue(
		"msm_vidc_workerq_venus");
	if (!hdevice->vidc_workq) {
		dprintk(VIDC_ERR, ": create vidc workq failed\n");
		goto error_createq;
	}
	hdevice->venus_pm_workq = create_singlethread_workqueue(
			"pm_workerq_venus");
	if (!hdevice->venus_pm_workq) {
		dprintk(VIDC_ERR, ": create pm workq failed\n");
		goto error_createq_pm;
	}

	mutex_init(&hdevice->read_lock);
	mutex_init(&hdevice->write_lock);
	mutex_init(&hdevice->session_lock);
	mutex_init(&hdevice->resource_lock);

	if (!hal_ctxt.dev_count)
		INIT_LIST_HEAD(&hal_ctxt.dev_head);

	INIT_LIST_HEAD(&hdevice->list);
	INIT_LIST_HEAD(&hdevice->sess_head);
	list_add_tail(&hdevice->list, &hal_ctxt.dev_head);
	hal_ctxt.dev_count++;

	return (void *) hdevice;
error_createq_pm:
	destroy_workqueue(hdevice->vidc_workq);
error_createq:
err_init_regs:
	kfree(hdevice);
err_alloc:
	return NULL;
}

static void *venus_hfi_get_device(u32 device_id,
				struct msm_vidc_platform_resources *res,
				hfi_cmd_response_callback callback)
{
	if (!res || !callback) {
		dprintk(VIDC_ERR, "Invalid params: %p %p\n", res, callback);
		return NULL;
	}

	return venus_hfi_add_device(device_id, res, callback);
}

void venus_hfi_delete_device(void *device)
{
	struct venus_hfi_device *close, *tmp, *dev;

	if (device) {
		venus_hfi_iommu_detach(device);
		dev = (struct venus_hfi_device *) device;
		list_for_each_entry_safe(close, tmp, &hal_ctxt.dev_head, list) {
			if (close->hal_data->irq == dev->hal_data->irq) {
				hal_ctxt.dev_count--;
				list_del(&close->list);
				destroy_workqueue(close->vidc_workq);
				destroy_workqueue(close->venus_pm_workq);
				free_irq(dev->hal_data->irq, close);
				iounmap(dev->hal_data->register_base);
				kfree(close->hal_data);
				kfree(close);
				break;
			}
		}

	}
}

static void venus_init_hfi_callbacks(struct hfi_device *hdev)
{
	hdev->core_init = venus_hfi_core_init;
	hdev->core_release = venus_hfi_core_release;
	hdev->core_pc_prep = venus_hfi_core_pc_prep;
	hdev->core_ping = venus_hfi_core_ping;
	hdev->core_trigger_ssr = venus_hfi_core_trigger_ssr;
	hdev->session_init = venus_hfi_session_init;
	hdev->session_end = venus_hfi_session_end;
	hdev->session_abort = venus_hfi_session_abort;
	hdev->session_clean = venus_hfi_session_clean;
	hdev->session_set_buffers = venus_hfi_session_set_buffers;
	hdev->session_release_buffers = venus_hfi_session_release_buffers;
	hdev->session_load_res = venus_hfi_session_load_res;
	hdev->session_release_res = venus_hfi_session_release_res;
	hdev->session_start = venus_hfi_session_start;
	hdev->session_stop = venus_hfi_session_stop;
	hdev->session_etb = venus_hfi_session_etb;
	hdev->session_ftb = venus_hfi_session_ftb;
	hdev->session_parse_seq_hdr = venus_hfi_session_parse_seq_hdr;
	hdev->session_get_seq_hdr = venus_hfi_session_get_seq_hdr;
	hdev->session_get_buf_req = venus_hfi_session_get_buf_req;
	hdev->session_flush = venus_hfi_session_flush;
	hdev->session_set_property = venus_hfi_session_set_property;
	hdev->session_get_property = venus_hfi_session_get_property;
	hdev->scale_clocks = venus_hfi_scale_clocks;
	hdev->vote_bus = venus_hfi_vote_buses;
	hdev->unvote_bus = venus_hfi_unvote_buses;
	hdev->load_fw = venus_hfi_load_fw;
	hdev->unload_fw = venus_hfi_unload_fw;
	hdev->resurrect_fw = venus_hfi_resurrect_fw;
	hdev->get_fw_info = venus_hfi_get_fw_info;
	hdev->get_stride_scanline = venus_hfi_get_stride_scanline;
	hdev->get_core_capabilities = venus_hfi_get_core_capabilities;
	hdev->power_enable = venus_hfi_power_enable;
	hdev->suspend = venus_hfi_suspend;
	hdev->get_core_clock_rate = venus_hfi_get_core_clock_rate;
	hdev->get_default_properties = venus_hfi_get_default_properties;
}

int venus_hfi_initialize(struct hfi_device *hdev, u32 device_id,
		struct msm_vidc_platform_resources *res,
		hfi_cmd_response_callback callback)
{
	int rc = 0;

	if (!hdev || !res || !callback) {
		dprintk(VIDC_ERR, "Invalid params: %p %p %p\n",
			hdev, res, callback);
		rc = -EINVAL;
		goto err_venus_hfi_init;
	}
	hdev->hfi_device_data = venus_hfi_get_device(device_id, res, callback);

	if (IS_ERR_OR_NULL(hdev->hfi_device_data)) {
		rc = PTR_ERR(hdev->hfi_device_data) ?: -EINVAL;
		goto err_venus_hfi_init;
	}

	venus_init_hfi_callbacks(hdev);

err_venus_hfi_init:
	return rc;
}

