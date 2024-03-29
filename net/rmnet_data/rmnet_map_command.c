/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <uapi/linux/rmnet_data.h>
#include <uapi/linux/net_map.h>
#include <net/pkt_sched.h>
#include "rmnet_data_config.h"
#include "rmnet_map.h"
#include "rmnet_data_private.h"
#include "rmnet_data_vnd.h"
#include "rmnet_data_stats.h"

RMNET_LOG_MODULE(RMNET_DATA_LOGMASK_MAPC);

unsigned long int rmnet_map_command_stats[RMNET_MAP_COMMAND_ENUM_LENGTH];
module_param_array(rmnet_map_command_stats, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rmnet_map_command_stats, "MAP command statistics");

/**
 * rmnet_map_do_flow_control() - Process MAP flow control command
 * @skb: Socket buffer containing the MAP flow control message
 * @config: Physical end-point configuration of ingress device
 * @enable: boolean for enable/disable
 *
 * Process in-band MAP flow control messages. Assumes mux ID is mapped to a
 * RmNet Data vitrual network device.
 *
 * Return:
 *      - RMNET_MAP_COMMAND_UNSUPPORTED on any error
 *      - RMNET_MAP_COMMAND_ACK on success
 */
static uint8_t rmnet_map_do_flow_control(struct sk_buff *skb,
					 struct rmnet_phys_ep_conf_s *config,
					 int enable)
{
	struct rmnet_map_control_command_s *cmd;
	struct net_device *vnd;
	struct rmnet_logical_ep_conf_s *ep;
	uint8_t mux_id;
	uint16_t  ip_family;
	uint16_t  fc_seq;
	uint32_t  qos_id;
	int r;

	if (unlikely(!skb || !config))
		BUG();

	mux_id = RMNET_MAP_GET_MUX_ID(skb);
	cmd = RMNET_MAP_GET_CMD_START(skb);

	if (mux_id >= RMNET_DATA_MAX_LOGICAL_EP) {
		LOGD("Got packet on %s with bad mux id %d",
		     skb->dev->name, mux_id);
		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_MAPC_BAD_MUX);
		return RX_HANDLER_CONSUMED;
	}

	ep = &(config->muxed_ep[mux_id]);

	if (!ep->refcount) {
		LOGD("Packet on %s:%d; has no logical endpoint config",
		     skb->dev->name, mux_id);

		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_MAPC_MUX_NO_EP);
			return RX_HANDLER_CONSUMED;
	}

	vnd = ep->egress_dev;

	ip_family = cmd->flow_control.ip_family;
	fc_seq = ntohs(cmd->flow_control.flow_control_seq_num);
	qos_id = ntohl(cmd->flow_control.qos_id);

	/* Ignore the ip family and pass the sequence number for both v4 and v6
	 * sequence. User space does not support creating dedicated flows for
	 * the 2 protocols
	 */
	r = rmnet_vnd_do_flow_control(vnd, qos_id, fc_seq, fc_seq, enable);
	LOGD("dev:%s, qos_id:0x%08X, ip_family:%hd, fc_seq %hd, en:%d",
	     skb->dev->name, qos_id, ip_family & 3, fc_seq, enable);

	if (r)
		return RMNET_MAP_COMMAND_UNSUPPORTED;
	else
		return RMNET_MAP_COMMAND_ACK;
}

/**
 * rmnet_map_send_ack() - Send N/ACK message for MAP commands
 * @skb: Socket buffer containing the MAP command message
 * @type: N/ACK message selector
 *
 * skb is modified to contain the message type selector. The message is then
 * transmitted on skb->dev. Note that this function grabs global Tx lock on
 * skb->dev for latency reasons.
 *
 * Return:
 *      - void
 */
static void rmnet_map_send_ack(struct sk_buff *skb,
			       unsigned char type)
{
	struct net_device *dev;
	struct rmnet_map_control_command_s *cmd;
	unsigned long flags;
	int xmit_status;

	if (!skb)
		BUG();

	dev = skb->dev;

	cmd = RMNET_MAP_GET_CMD_START(skb);
	cmd->cmd_type = type & 0x03;

	spin_lock_irqsave(&(skb->dev->tx_global_lock), flags);
	xmit_status = skb->dev->netdev_ops->ndo_start_xmit(skb, skb->dev);
	spin_unlock_irqrestore(&(skb->dev->tx_global_lock), flags);
}

/**
 * rmnet_map_command() - Entry point for handling MAP commands
 * @skb: Socket buffer containing the MAP command message
 * @config: Physical end-point configuration of ingress device
 *
 * Process MAP command frame and send N/ACK message as appropriate. Message cmd
 * name is decoded here and appropriate handler is called.
 *
 * Return:
 *      - RX_HANDLER_CONSUMED. Command frames are always consumed.
 */
rx_handler_result_t rmnet_map_command(struct sk_buff *skb,
				      struct rmnet_phys_ep_conf_s *config)
{
	struct rmnet_map_control_command_s *cmd;
	unsigned char command_name;
	unsigned char rc = 0;

	if (unlikely(!skb))
		BUG();

	cmd = RMNET_MAP_GET_CMD_START(skb);
	command_name = cmd->command_name;

	if (command_name < RMNET_MAP_COMMAND_ENUM_LENGTH)
		rmnet_map_command_stats[command_name]++;

	switch (command_name) {
	case RMNET_MAP_COMMAND_FLOW_ENABLE:
		rc = rmnet_map_do_flow_control(skb, config, 1);
		break;

	case RMNET_MAP_COMMAND_FLOW_DISABLE:
		rc = rmnet_map_do_flow_control(skb, config, 0);
		break;

	default:
		rmnet_map_command_stats[RMNET_MAP_COMMAND_UNKNOWN]++;
		LOGM("Uknown MAP command: %d", command_name);
		rc = RMNET_MAP_COMMAND_UNSUPPORTED;
		break;
	}
	rmnet_map_send_ack(skb, rc);
	return RX_HANDLER_CONSUMED;
}
