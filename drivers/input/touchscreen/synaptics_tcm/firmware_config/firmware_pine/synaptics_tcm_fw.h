/************************************************************************
*
* File Name: synaptics_tcm_fw.h
*
*  *   Version: v1.0
*
************************************************************************/
#ifndef _SYNAPTICS_TCM_FW_H_
#define _SYNAPTICS_TCM_FW_H_

/********************** Upgrade ***************************

  auto upgrade, please keep enable
*********************************************************/
#define STARTUP_REFLASH
#define FORCE_REFLASH false

/*
 *.img file for auto and manual upgrade, you must replace it with your own
 * define your own fw_img
 */
#define FW_IMAGE_NAME "syna3908_gvo_startup_update.img"

#define FW_IMAGE_NAME_MANUAL "synaptics/fw.img"

#endif
