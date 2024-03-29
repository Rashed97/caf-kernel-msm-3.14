/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MC_ADMIN_IOCTL_H__
#define __MC_ADMIN_IOCTL_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MC_ADMIN_DEVNODE "mobicore"

/* Driver/daemon commands */
enum {
	/* Command 0 is reserved */
	MC_DRV_GET_ROOT_CONTAINER = 1,
	MC_DRV_GET_SP_CONTAINER = 2,
	MC_DRV_GET_TRUSTLET_CONTAINER = 3,
	MC_DRV_GET_TRUSTLET = 4,
};

/* MobiCore IOCTL magic number */
#define MC_IOC_MAGIC    'M'

struct mc_admin_request {
	uint32_t	request_id;	/* Unique request identifier */
	uint32_t	command;	/* Command to daemon */
	struct mc_uuid_t uuid;		/* UUID of trustlet, if relevant */
	uint32_t	is_gp;		/* Whether trustlet is GP */
	uint32_t	spid;		/* SPID of trustlet, if relevant */
};

struct mc_admin_response {
	uint32_t	request_id;	/* Unique request identifier */
	uint32_t	error_no;	/* Errno from daemon */
	uint32_t	spid;		/* SPID of trustlet, if relevant */
	uint32_t	service_type;	/* Type of trustlet being returned */
	uint32_t	length;		/* Length of data to get */
	/* Any data follows */
};

struct mc_admin_driver_info {
	/* Version, and something else..*/
	uint32_t	drv_version;
	uint32_t	initial_cmd_id;
};

struct mc_admin_load_info {
	uint32_t	spid;		/* SPID of trustlet, if relevant */
	uint64_t	address;	/* Address of the data */
	uint32_t	length;		/* Length of data to get */
};

#define MC_ADMIN_IO_GET_DRIVER_REQUEST \
	_IOR(MC_IOC_MAGIC, 0, struct mc_admin_request)
#define MC_ADMIN_IO_GET_INFO  \
	_IOR(MC_IOC_MAGIC, 1, struct mc_admin_driver_info)
#define MC_ADMIN_IO_LOAD_DRIVER \
	_IOW(MC_IOC_MAGIC, 2, struct mc_admin_load_info)
#define MC_ADMIN_IO_LOAD_TOKEN \
	_IOW(MC_IOC_MAGIC, 3, struct mc_admin_load_info)
#define MC_ADMIN_IO_LOAD_CHECK \
	_IOW(MC_IOC_MAGIC, 4, struct mc_admin_load_info)

#ifdef __cplusplus
}
#endif
#endif				/* __MC_ADMIN_IOCTL_H__ */
