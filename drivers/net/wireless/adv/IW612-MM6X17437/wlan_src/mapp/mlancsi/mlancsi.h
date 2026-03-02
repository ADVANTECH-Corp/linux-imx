
/** @file  mlancsi.h
 *
 * @brief Wifi CSI processing application
 *
 *
 * Copyright 2025 NXP
 *
 * NXP CONFIDENTIAL
 * The source code contained or described herein and all documents related to
 * the source code (Materials) are owned by NXP, its
 * suppliers and/or its licensors. Title to the Materials remains with NXP,
 * its suppliers and/or its licensors. The Materials contain
 * trade secrets and proprietary and confidential information of NXP, its
 * suppliers and/or its licensors. The Materials are protected by worldwide
 * copyright and trade secret laws and treaty provisions. No part of the
 * Materials may be used, copied, reproduced, modified, published, uploaded,
 * posted, transmitted, distributed, or disclosed in any way without NXP's prior
 * express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by NXP in writing.
 *
 */

/************************************************************************
Change log:
     01/24/2022: initial version
	 06/13/2025: copy from mlanwls to mlancsi
************************************************************************/
#ifndef _MLANCSI_H_
#define _MLANCSI_H_

#include <linux/if.h>
#include "../libcsi/wls_structure_defs.h"

/** mlancsi application's version number */
#define MLANCSI_VER "1.1"

/*Command arguments index*/

#define PROTO_DOT11AZ_NTB 1
#define PROTO_DOT11AZ_TB 2
#define PROTO_DOT11MC 0

#define CSI_SUBCMD_INDEX 2
#define CSI_CFG_FILE_ARG_INDEX 3

#define DEF_CONFIG_FILE "mlancsi.conf"

/** Size of command buffer */
#define MRVDRV_SIZE_OF_CMD_BUFFER (3 * 1024)

#define CUS_EVT_MLAN_CSI "EVENT=MLAN_CSI"

/** Default scan interval in second*/
#define DEFAULT_SCAN_INTERVAL 300

/** Success */
#define MLAN_STATUS_SUCCESS (0)
/** Failure */
#define MLAN_STATUS_FAILURE (-1)
/** Not found */
#define MLAN_STATUS_NOTFOUND (1)

/** Maximum length of lines in configuration file */
#define MAX_CONFIG_LINE 1024 * 10
/** MAC BROADCAST */
#define MAC_BROADCAST 0x1FF
/** MAC MULTICAST */
#define MAC_MULTICAST 0x1FE

/** Length of ethernet address */
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#ifndef NLMSG_HDRLEN
/** NL message header length */
#define NLMSG_HDRLEN ((int)NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#endif

/** The attribute pack used for structure packing */
#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__ __attribute__((packed))
#endif

/**
 * Hex or Decimal to Integer
 * @param   num string to convert into decimal or hex
 */
#define A2HEXDECIMAL(num)                                                      \
	(strncasecmp("0x", (num), 2) ? (unsigned int)strtoll((num), NULL, 0) : \
				       a2hex((num)))

/** Convert to correct endian format */
#ifdef BIG_ENDIAN_SUPPORT
/** CPU to little-endian convert for 16-bit */
#define cpu_to_le16(x) swap_byte_16(x)
/** CPU to little-endian convert for 32-bit */
#define cpu_to_le32(x) swap_byte_32(x)
/** Little-endian to CPU convert for 16-bit */
#define le16_to_cpu(x) swap_byte_16(x)
/** Little-endian to CPU convert for 32-bit */
#define le32_to_cpu(x) swap_byte_32(x)
#else
/** Do nothing */
#define cpu_to_le16(x) (x)
/** Do nothing */
#define cpu_to_le32(x) (x)
/** Do nothing */
#define le16_to_cpu(x) (x)
/** Do nothing */
#define le32_to_cpu(x) (x)
#endif

/** Command RET code, MSB is set to 1 */
#define HostCmd_RET_BIT 0x8000

/** IOCTL number */
#define MLAN_ETH_PRIV (SIOCDEVPRIVATE + 14)

/** Maximum length of lines in configuration file */
#define MAX_CONFIG_LINE 1024 * 10

/** Netlink maximum payload size */
#define NL_MAX_PAYLOAD (1024 * 3)
/** Netlink multicast group number */
#define NL_MULTICAST_GROUP 1

/** Find number of elements */
#define NELEMENTS(x) (sizeof(x) / sizeof(x[0]))

/** Command buffer max length */
#define BUFFER_LENGTH (4 * 1024)

/** Events*/
#define EVENT_WLS_GENERIC 0x00000086
#define EVENT_CSI 0x0000008D

/** NXP private command identifier */
#define CMD_NXP "MRVL_CMD"

/** Device name */
extern char dev_name[IFNAMSIZ];

/** Type definition: boolean */
typedef enum { FALSE, TRUE } boolean;

/** Character, 1 byte */
typedef signed char t_s8;
/** Unsigned character, 1 byte */
typedef unsigned char t_u8;
/** Unsigned short integer */
typedef unsigned short t_u16;
/** Unsigned integer */
typedef unsigned int t_u32;
/** Integer */
typedef signed int t_s32;

/** Void pointer (4-bytes) */
typedef void t_void;

/** Socket */
extern t_s32 sockfd;

/** HostCmd_DS_GEN */
typedef struct MAPP_HostCmd_DS_GEN {
	/** Command */
	t_u16 command;
	/** Size */
	t_u16 size;
	/** Sequence number */
	t_u16 seq_num;
	/** Result */
	t_u16 result;
} __ATTRIB_PACK__ HostCmd_DS_GEN;

/** Size of HostCmd_DS_GEN */
#define S_DS_GEN sizeof(HostCmd_DS_GEN)

/** Private command structure */
#ifdef USERSPACE_32BIT_OVER_KERNEL_64BIT
struct eth_priv_cmd {
	/** Command buffer pointer */
	t_u64 buf;
	/** buffer updated by driver */
	int used_len;
	/** buffer sent by application */
	int total_len;
} __ATTRIB_PACK__;
#else
struct eth_priv_cmd {
	/** Command buffer */
	t_u8 *buf;
	/** Used length */
	int used_len;
	/** Total length */
	int total_len;
};
#endif

/** Event header */
typedef struct _event_header {
	/** Event ID */
	t_u32 event_id;
	/** Event data */
	t_u8 event_data[];
} __ATTRIB_PACK__ event_header;

/** Structure for ftm command private data*/
typedef struct _wls_app_data {
	/** flag for app to terminate CSI processing*/
	t_u8 terminate_app;
	/**flag for debug print level */
	t_u8 debug_level;
} wls_app_data_t;

struct command_node {
	char *name;
	int (*handler)(int, char **);
};

#endif /* _MLANCSI_H_ */
