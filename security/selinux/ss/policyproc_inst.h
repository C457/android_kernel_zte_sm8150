/* policyproc_inst.h -- preprocess/postprocess policy as binary image
 *
 * Copyright 2017-2018 ZTE Corp.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Written by Jia Jia <jia.jia@zte.com.cn>
 */
#ifndef _SS_POLICYPROC_INST_H_
#define _SS_POLICYPROC_INST_H_

/*
 * Macro Definition
 */
#define PP_DATUMS_NUM 8  /* datums number */


/*
 * Type Definition
 */
/*
 * Policy Description Definition
 * NOTICE: Update system/sepolicy/tools/sepolicy-parser/sepolicy-appendix.json required
 */
typedef enum {
	PERM_SU = 0,
} pp_perm_desc_item_t;

typedef enum {
	/* Policy Group 1 */
	AVC_ADBD_SU = 0,
	/* Policy Group 2 */
	AVC_ADBD_KERNEL,
	/* Policy Group 3 */
	AVC_GETLOG_PROPERTIES_SERIAL,
	AVC_GETLOG_SELF,
	/* Policy Group 4 */
	AVC_SDLOG_DEFAULT_PROP,
	AVC_SDLOG_PROPERTIES_SERIAL,
	AVC_SDLOG_RADIO_PROP,
	AVC_SDLOG_SELF,
	AVC_SDLOG_SYSTEM_FILE,
	/* Policy Group 5 */
	AVC_SU_TEE_DEV_CHR_FILE,
} pp_avc_desc_item_t;

typedef struct {
	const char *name;
} pp_perm_desc_t;

typedef struct {
	const char *sname;
	const char *tname;
	const char *cname;
	uint16_t specified;
	const char *pname;
	uint16_t sdatums;
	uint16_t tdatums;
	uint16_t cdatums;
	uint32_t pdatums[PP_DATUMS_NUM];
} pp_avc_desc_t;


/*
 * Global Variable Definition
 */
/*
 * Policy Instruction Definition
 * NOTICE: Update system/sepolicy/tools/sepolicy-parser/sepolicy-appendix.json required
 */
static pp_perm_desc_t pp_perm_desc_list[] = {
	/* permissive policy: permissive su */
	[PERM_SU] = {
		.name = "su",
	},
};

static pp_avc_desc_t pp_avc_desc_list[] = {
	/*
	 * Policy Group 1
	 * Purpose: allow adbd to root device
	 */
	/* avc policy: allow adbd su:process dyntransition */
	[AVC_ADBD_SU] = {
		.sname     = "adbd",
		.tname     = "su",
		.cname     = "process",
		.specified = AVTAB_ALLOWED,
		.pname     = "dyntransition",
	},

	/*
	 * Policy Group 2
	 * Purpose: allow adbd to setenforce
	 * Running: adb shell "echo 0 > /sys/fs/selinux/enforce"
	 */
	/* avc policy: allow adbd kernel:security setenforce */
	[AVC_ADBD_KERNEL] = {
		.sname     = "adbd",
		.tname     = "kernel",
		.cname     = "security",
		.specified = AVTAB_ALLOWED,
		.pname     = "setenforce",
	},

	/*
	 * Policy Group 3
	 * Purpose: allow getlogtofile to perform operation
	 */
	/* avc policy: allow getlog debugfs:filesystem mount */

	/* avc policy: allow getlog properties_serial:file execute */
	[AVC_GETLOG_PROPERTIES_SERIAL] = {
		.sname     = "getlog",
		.tname     = "properties_serial",
		.cname     = "file",
		.specified = AVTAB_ALLOWED,
		.pname     = "execute",
	},

	/* avc policy: allow getlog self:capability { dac_override sys_ptrace } */
	[AVC_GETLOG_SELF] = {
		.sname     = "getlog",
		.tname     = "getlog",
		.cname     = "capability",
		.specified = AVTAB_ALLOWED,
		.pname     = "sys_ptrace",
	},

	/*
	 * Policy Group 4
	 * Purpose: allow sdlog to perform operation
	 */
	/* avc policy: allow sdlog default_prop:file execute */
	[AVC_SDLOG_DEFAULT_PROP] = {
		.sname     = "sdlog",
		.tname     = "default_prop",
		.cname     = "file",
		.specified = AVTAB_ALLOWED,
		.pname     = "execute",
	},

	/* avc policy: allow sdlog properties_serial:file execute */
	[AVC_SDLOG_PROPERTIES_SERIAL] = {
		.sname     = "sdlog",
		.tname     = "properties_serial",
		.cname     = "file",
		.specified = AVTAB_ALLOWED,
		.pname     = "execute",
	},

	/* avc policy: allow sdlog radio_prop:file execute */
	[AVC_SDLOG_RADIO_PROP] = {
		.sname     = "sdlog",
		.tname     = "radio_prop",
		.cname     = "file",
		.specified = AVTAB_ALLOWED,
		.pname     = "execute",
	},

	/* allow sdlog self:capability dac_override;*/
	[AVC_SDLOG_SELF] = {
		.sname     = "sdlog",
		.tname     = "sdlog",
		.cname     = "capability",
		.specified = AVTAB_ALLOWED,
		.pname     = "dac_override",
	},

	/* avc policy: allow sdlog system_file:file { entrypoint execute_no_trans } */
	[AVC_SDLOG_SYSTEM_FILE] = {
		.sname     = "sdlog",
		.tname     = "system_file",
		.cname     = "file",
		.specified = AVTAB_ALLOWED,
		.pname     = "entrypoint execute_no_trans",
	},

	/*
	 * Policy Group 5
	 * Purpose: allow su to access tee device
	 */
	/* avc policy: allow su tee_device:chr_file { ioctl open read write } */
	[AVC_SU_TEE_DEV_CHR_FILE] = {
		.sname     = "su",
		.tname     = "tee_device",
		.cname     = "chr_file",
		.specified = AVTAB_ALLOWED,
		.pname     = "ioctl open read write",
	},

};

#endif /* _SS_POLICYPROC_INST_H_ */
