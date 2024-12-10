/** @file  mlanutl.h
 *
 * @brief This file contains definitions for application
 *
 *
 * Copyright 2011-2022 NXP
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
     11/26/2008: initial version
************************************************************************/
#ifndef _MLANUTL_H_
#define _MLANUTL_H_

/** Include header files */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include <netinet/ether.h>
#include <linux/if_packet.h>

/** Type definition: boolean */
typedef enum { FALSE, TRUE } boolean;

/** 16 bits byte swap */
#define swap_byte_16(x)                                                        \
	((t_u16)((((t_u16)(x)&0x00ffU) << 8) | (((t_u16)(x)&0xff00U) >> 8)))

/** 32 bits byte swap */
#define swap_byte_32(x)                                                        \
	((t_u32)((((t_u32)(x)&0x000000ffUL) << 24) |                           \
		 (((t_u32)(x)&0x0000ff00UL) << 8) |                            \
		 (((t_u32)(x)&0x00ff0000UL) >> 8) |                            \
		 (((t_u32)(x)&0xff000000UL) >> 24)))

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

/** TLV header */
#define TLVHEADER /** Tag */                                                   \
	t_u16 tag;                                                             \
	/** Length */                                                          \
	t_u16 length

/** Length of TLV header */
#define TLVHEADER_LEN 4

/** Character, 1 byte */
typedef signed char t_s8;
/** Unsigned character, 1 byte */
typedef unsigned char t_u8;

/** Short integer */
typedef signed short t_s16;
/** Unsigned short integer */
typedef unsigned short t_u16;

/** Integer */
typedef signed int t_s32;
/** Unsigned integer */
typedef unsigned int t_u32;

/** Long long integer */
typedef signed long long t_s64;
/** Unsigned long long integer */
typedef unsigned long long t_u64;
/** Boolean type */
typedef t_u8 t_bool;

/** Void pointer (4-bytes) */
typedef void t_void;

enum _mlan_act_ioctl {
	MLAN_ACT_SET = 1,
	MLAN_ACT_GET,
	MLAN_ACT_CANCEL,
	MLAN_ACT_CLEAR,
	MLAN_ACT_RESET,
	MLAN_ACT_DEFAULT
};
/** The attribute pack used for structure packing */
#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__ __attribute__((packed))
#endif

/** Success */
#define MLAN_STATUS_SUCCESS (0)
/** Failure */
#define MLAN_STATUS_FAILURE (-1)
/** Not found */
#define MLAN_STATUS_NOTFOUND (1)

/** IOCTL number */
#define MLAN_ETH_PRIV (SIOCDEVPRIVATE + 14)

/** Command buffer max length */
#define BUFFER_LENGTH (4 * 1024)

/** Find number of elements */
#define NELEMENTS(x) (sizeof(x) / sizeof(x[0]))

/** BIT value */
#define MBIT(x) (((t_u32)1) << (x))

#ifndef MIN
/** Find minimum value */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif /* MIN */

/** Length of ethernet address */
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

/** Action field value: get */
#define ACTION_GET 0
/** Action field value: set */
#define ACTION_SET 1
/** Action field value: add */
#define ACTION_ADD 2
/** Action field value: remove */
#define ACTION_REMOVE 3

/** Maximum number of TID */
#define MAX_NUM_TID 8

/** Used just for Turbo mode */
#define OID_WMM_TURBO_MODE 0x27

/** Host Command ID : 802.11 SNMP MIB */
#define HostCmd_CMD_802_11_SNMP_MIB 0x0016

/** Device name */
extern char dev_name[IFNAMSIZ];

#define HOSTCMD "hostcmd"

/** NXP private command identifier */
#define CMD_NXP "MRVL_CMD"

struct command_node {
	char *name;
	int (*handler)(int, char **);
};

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

/** data structure for cmd getdatarate */
struct eth_priv_data_rate {
	/** Tx data rate */
	t_u32 tx_data_rate;
	/** Rx data rate */
	t_u32 rx_data_rate;

	/** Tx channel bandwidth */
	t_u32 tx_bw;
	/** Tx guard interval */
	t_u32 tx_gi;
	/** Rx channel bandwidth */
	t_u32 rx_bw;
	/** Rx guard interval */
	t_u32 rx_gi;
	/** MCS index */
	t_u32 tx_mcs_index;
	t_u32 rx_mcs_index;
	/** NSS */
	t_u32 tx_nss;
	t_u32 rx_nss;
	/* LG rate: 0, HT rate: 1, VHT rate: 2 */
	t_u32 tx_rate_format;
	t_u32 rx_rate_format;
};

/** data structure for cmd getlog */
struct eth_priv_get_log {
	/** Multicast transmitted frame count */
	t_u32 mcast_tx_frame;
	/** Failure count */
	t_u32 failed;
	/** Retry count */
	t_u32 retry;
	/** Multi entry count */
	t_u32 multi_retry;
	/** Duplicate frame count */
	t_u32 frame_dup;
	/** RTS success count */
	t_u32 rts_success;
	/** RTS failure count */
	t_u32 rts_failure;
	/** Ack failure count */
	t_u32 ack_failure;
	/** Rx fragmentation count */
	t_u32 rx_frag;
	/** Multicast Tx frame count */
	t_u32 mcast_rx_frame;
	/** FCS error count */
	t_u32 fcs_error;
	/** Tx frame count */
	t_u32 tx_frame;
	/** WEP ICV error count */
	t_u32 wep_icv_error[4];
	/** beacon recv count */
	t_u32 bcn_rcv_cnt;
	/** beacon miss count */
	t_u32 bcn_miss_cnt;
	/** received amsdu count*/
	t_u32 amsdu_rx_cnt;
	/** received msdu count in amsdu*/
	t_u32 msdu_in_rx_amsdu_cnt;
	/** tx amsdu count*/
	t_u32 amsdu_tx_cnt;
	/** tx msdu count in amsdu*/
	t_u32 msdu_in_tx_amsdu_cnt;
	/** Tx frag count */
	t_u32 tx_frag_cnt;
	/** Qos Tx frag count */
	t_u32 qos_tx_frag_cnt[8];
	/** Qos failed count */
	t_u32 qos_failed_cnt[8];
	/** Qos retry count */
	t_u32 qos_retry_cnt[8];
	/** Qos multi retry count */
	t_u32 qos_multi_retry_cnt[8];
	/** Qos frame dup count */
	t_u32 qos_frm_dup_cnt[8];
	/** Qos rts success count */
	t_u32 qos_rts_suc_cnt[8];
	/** Qos rts failure count */
	t_u32 qos_rts_failure_cnt[8];
	/** Qos ack failure count */
	t_u32 qos_ack_failure_cnt[8];
	/** Qos Rx frag count */
	t_u32 qos_rx_frag_cnt[8];
	/** Qos Tx frame count */
	t_u32 qos_tx_frm_cnt[8];
	/** Qos discarded frame count */
	t_u32 qos_discarded_frm_cnt[8];
	/** Qos mpdus Rx count */
	t_u32 qos_mpdus_rx_cnt[8];
	/** Qos retry rx count */
	t_u32 qos_retries_rx_cnt[8];
	/** CMACICV errors count */
	t_u32 cmacicv_errors;
	/** CMAC replays count */
	t_u32 cmac_replays;
	/** mgmt CCMP replays count */
	t_u32 mgmt_ccmp_replays;
	/** TKIP ICV errors count */
	t_u32 tkipicv_errors;
	/** TKIP replays count */
	t_u32 tkip_replays;
	/** CCMP decrypt errors count */
	t_u32 ccmp_decrypt_errors;
	/** CCMP replays count */
	t_u32 ccmp_replays;
	/** Tx amsdu count */
	t_u32 tx_amsdu_cnt;
	/** failed amsdu count */
	t_u32 failed_amsdu_cnt;
	/** retry amsdu count */
	t_u32 retry_amsdu_cnt;
	/** multi-retry amsdu count */
	t_u32 multi_retry_amsdu_cnt;
	/** Tx octets in amsdu count */
	t_u64 tx_octets_in_amsdu_cnt;
	/** amsdu ack failure count */
	t_u32 amsdu_ack_failure_cnt;
	/** Rx amsdu count */
	t_u32 rx_amsdu_cnt;
	/** Rx octets in amsdu count */
	t_u64 rx_octets_in_amsdu_cnt;
	/** Tx ampdu count */
	t_u32 tx_ampdu_cnt;
	/** tx mpdus in ampdu count */
	t_u32 tx_mpdus_in_ampdu_cnt;
	/** tx octets in ampdu count */
	t_u64 tx_octets_in_ampdu_cnt;
	/** ampdu Rx count */
	t_u32 ampdu_rx_cnt;
	/** mpdu in Rx ampdu count */
	t_u32 mpdu_in_rx_ampdu_cnt;
	/** Rx octets ampdu count */
	t_u64 rx_octets_in_ampdu_cnt;
	/** ampdu delimiter CRC error count */
	t_u32 ampdu_delimiter_crc_error_cnt;
	/** Rx Stuck Related Info*/
	/** Rx Stuck Issue count */
	t_u32 rx_stuck_issue_cnt[2];
	/** Rx Stuck Recovery count polling based */
	t_u32 rx_stuck_poll_recovery_cnt;
	/** Rx Stuck Recovery count interrupt based */
	t_u32 rx_stuck_intr_recovery_cnt;
	/** Rx Stuck TSF */
	t_u64 rx_stuck_tsf[2];
	/** Tx Watchdog Recovery Related Info */
	/** Tx Watchdog Recovery count */
	t_u32 tx_watchdog_recovery_cnt;
	/** Tx Watchdog TSF */
	t_u64 tx_watchdog_tsf[2];
	/** Channel Switch Related Info */
	/** Channel Switch Announcement Sent */
	t_u32 channel_switch_ann_sent;
	/** Channel Switch State */
	t_u32 channel_switch_state;
	/** Register Class */
	t_u32 reg_class;
	/** Channel Number */
	t_u32 channel_number;
	/** Channel Switch Mode */
	t_u32 channel_switch_mode;
	/** Reset Rx Mac Recovery Count */
	t_u32 rx_reset_mac_recovery_cnt;
	/** ISR2 Not Done Count*/
	t_u32 rx_Isr2_NotDone_Cnt;
	/** GDMA Abort Count */
	t_u32 gdma_abort_cnt;
	/** Rx Reset MAC Count */
	t_u32 g_reset_rx_mac_cnt;
	// Ownership error counters
	/*Error Ownership error count*/
	t_u32 dwCtlErrCnt;
	/*Control Ownership error count*/
	t_u32 dwBcnErrCnt;
	/*Control Ownership error count*/
	t_u32 dwMgtErrCnt;
	/*Control Ownership error count*/
	t_u32 dwDatErrCnt;
	/*BIGTK MME good count*/
	t_u32 bigtk_mmeGoodCnt;
	/*BIGTK Replay error count*/
	t_u32 bigtk_replayErrCnt;
	/*BIGTK MIC error count*/
	t_u32 bigtk_micErrCnt;
	/*BIGTK MME not included count*/
	t_u32 bigtk_mmeNotFoundCnt;
};

/** MLAN MAC Address Length */
#define MLAN_MAC_ADDR_LENGTH 6
#define COUNTRY_CODE_LEN 3
/** Type definition of eth_priv_countrycode for CMD_COUNTRYCODE */
struct eth_priv_countrycode {
	/** Country Code */
	t_u8 country_code[COUNTRY_CODE_LEN];
};

/** Type enumeration of WMM AC_QUEUES */
typedef enum _mlan_wmm_ac_e {
	WMM_AC_BK,
	WMM_AC_BE,
	WMM_AC_VI,
	WMM_AC_VO
} __ATTRIB_PACK__ mlan_wmm_ac_e;

/** IEEE Type definitions  */
typedef enum _IEEEtypes_ElementId_e {
	SSID = 0,
	SUPPORTED_RATES = 1,
	FH_PARAM_SET = 2,
	DS_PARAM_SET = 3,
	CF_PARAM_SET = 4,

	COUNTRY_INFO = 7,

	POWER_CONSTRAINT = 32,
	POWER_CAPABILITY = 33,
	TPC_REQUEST = 34,
	TPC_REPORT = 35,
	SUPPORTED_CHANNELS = 36,
	CHANNEL_SWITCH_ANN = 37,
	QUIET = 40,
	HT_CAPABILITY = 45,
	HT_OPERATION = 61,
	BSSCO_2040 = 72,
	OVERLAPBSSSCANPARAM = 74,
	EXT_CAPABILITY = 127,

	VHT_CAPABILITY = 191,
	VHT_OPERATION = 192,
	EXT_BSS_LOAD = 193,
	BW_CHANNEL_SWITCH = 194,
	VHT_TX_POWER_ENV = 195,
	EXT_POWER_CONSTR = 196,
	AID_INFO = 197,
	QUIET_CHAN = 198,
	OPER_MODE_NTF = 199,

	ERP_INFO = 42,
	EXTENDED_SUPPORTED_RATES = 50,

	VENDOR_SPECIFIC_221 = 221,
	WMM_IE = VENDOR_SPECIFIC_221,

	WPS_IE = VENDOR_SPECIFIC_221,

	WPA_IE = VENDOR_SPECIFIC_221,
	RSN_IE = 48,
	EXTENSION = 255,
} __ATTRIB_PACK__ IEEEtypes_ElementId_e;

typedef enum _IEEEtypes_Ext_ElementId_e {
	HE_CAPABILITY = 35,
	HE_OPERATION = 36,
	HE_6G_CAPABILITY = 59
} IEEEtypes_Ext_ElementId_e;

/** Capability Bit Map*/
#ifdef BIG_ENDIAN_SUPPORT
typedef struct _IEEEtypes_CapInfo_t {
	t_u8 rsrvd1 : 2;
	t_u8 dsss_ofdm : 1;
	t_u8 rsvrd2 : 2;
	t_u8 short_slot_time : 1;
	t_u8 rsrvd3 : 1;
	t_u8 spectrum_mgmt : 1;
	t_u8 chan_agility : 1;
	t_u8 pbcc : 1;
	t_u8 short_preamble : 1;
	t_u8 privacy : 1;
	t_u8 cf_poll_rqst : 1;
	t_u8 cf_pollable : 1;
	t_u8 ibss : 1;
	t_u8 ess : 1;
} __ATTRIB_PACK__ IEEEtypes_CapInfo_t, *pIEEEtypes_CapInfo_t;
#else
typedef struct _IEEEtypes_CapInfo_t {
	/** Capability Bit Map : ESS */
	t_u8 ess : 1;
	/** Capability Bit Map : IBSS */
	t_u8 ibss : 1;
	/** Capability Bit Map : CF pollable */
	t_u8 cf_pollable : 1;
	/** Capability Bit Map : CF poll request */
	t_u8 cf_poll_rqst : 1;
	/** Capability Bit Map : privacy */
	t_u8 privacy : 1;
	/** Capability Bit Map : Short preamble */
	t_u8 short_preamble : 1;
	/** Capability Bit Map : PBCC */
	t_u8 pbcc : 1;
	/** Capability Bit Map : Channel agility */
	t_u8 chan_agility : 1;
	/** Capability Bit Map : Spectrum management */
	t_u8 spectrum_mgmt : 1;
	/** Capability Bit Map : Reserved */
	t_u8 rsrvd3 : 1;
	/** Capability Bit Map : Short slot time */
	t_u8 short_slot_time : 1;
	/** Capability Bit Map : APSD */
	t_u8 apsd : 1;
	/** Capability Bit Map : Reserved */
	t_u8 rsvrd2 : 1;
	/** Capability Bit Map : DSS OFDM */
	t_u8 dsss_ofdm : 1;
	/** Capability Bit Map : Reserved */
	t_u8 rsrvd1 : 2;
} __ATTRIB_PACK__ IEEEtypes_CapInfo_t, *pIEEEtypes_CapInfo_t;
#endif /* BIG_ENDIAN_SUPPORT */

/** IEEE IE header */
typedef struct _IEEEtypes_Header_t {
	/** Element ID */
	t_u8 element_id;
	/** Length */
	t_u8 len;
} __ATTRIB_PACK__ IEEEtypes_Header_t, *pIEEEtypes_Header_t;

/** IEEE IE header */
#define IEEE_HEADER_LEN sizeof(IEEEtypes_Header_t)

/** Maximum size of IEEE Information Elements */
#define IEEE_MAX_IE_SIZE 256

/** Vendor specific IE header */
typedef struct _IEEEtypes_VendorHeader_t {
	/** Element ID */
	t_u8 element_id;
	/** Length */
	t_u8 len;
	/** OUI */
	t_u8 oui[3];
	/** OUI type */
	t_u8 oui_type;
	/** OUI subtype */
	t_u8 oui_subtype;
	/** Version */
	t_u8 version;
} __ATTRIB_PACK__ IEEEtypes_VendorHeader_t, *pIEEEtypes_VendorHeader_t;

/** Vendor specific IE */
typedef struct _IEEEtypes_VendorSpecific_t {
	/** Vendor specific IE header */
	IEEEtypes_VendorHeader_t vend_hdr;
	/** IE Max - size of previous fields */
	t_u8 data[IEEE_MAX_IE_SIZE - sizeof(IEEEtypes_VendorHeader_t)];
} __ATTRIB_PACK__ IEEEtypes_VendorSpecific_t, *pIEEEtypes_VendorSpecific_t;

/** IEEE IE */
typedef struct _IEEEtypes_Generic_t {
	/** Generic IE header */
	IEEEtypes_Header_t ieee_hdr;
	/** IE Max - size of previous fields */
	t_u8 data[IEEE_MAX_IE_SIZE - sizeof(IEEEtypes_Header_t)];
} __ATTRIB_PACK__ IEEEtypes_Generic_t, *pIEEEtypes_Generic_t;

/** Convert character to integer */
#define CHAR2INT(x) (((x) >= 'A') ? ((x) - 'A' + 10) : ((x) - '0'))

/** Command RET code, MSB is set to 1 */
#define HostCmd_RET_BIT 0x8000
/** General purpose action : Get */
#define HostCmd_ACT_GEN_GET 0x0000
/** General purpose action : Set */
#define HostCmd_ACT_GEN_SET 0x0001
/** General purpose action : Clear */
#define HostCmd_ACT_GEN_CLEAR 0x0004
/** General purpose action : Remove */
#define HostCmd_ACT_GEN_REMOVE 0x0004

/** TLV  type ID definition */
#define PROPRIETARY_TLV_BASE_ID 0x0100

/** MrvlIEtypesHeader_t */
typedef struct MrvlIEtypesHeader {
	/** Header type */
	t_u16 type;
	/** Header length */
	t_u16 len;
} __ATTRIB_PACK__ MrvlIEtypesHeader_t;

/** MrvlIEtypes_Data_t */
typedef struct MrvlIEtypes_Data_t {
	/** Header */
	MrvlIEtypesHeader_t header;
	/** Data */
	t_u8 data[1];
} __ATTRIB_PACK__ MrvlIEtypes_Data_t;

/** channel band */
enum {
	BAND_2GHZ = 0,
	BAND_5GHZ = 1,
	BAND_6GHZ = 2,
	BAND_4GHZ = 3,
};

/** channel offset */
enum {
	SEC_CHAN_NONE = 0,
	SEC_CHAN_ABOVE = 1,
	SEC_CHAN_5MHZ = 2,
	SEC_CHAN_BELOW = 3
};

/** channel bandwidth */
enum {
	CHAN_BW_20MHZ = 0,
	CHAN_BW_10MHZ,
	CHAN_BW_40MHZ,
	CHAN_BW_80MHZ,
};

/** Band_Config_t */
typedef struct _Band_Config_t {
#ifdef BIG_ENDIAN_SUPPORT
	/** Channel Selection Mode - (00)=manual, (01)=ACS,  (02)=user*/
	t_u8 scanMode : 2;
	/** Secondary Channel Offset - (00)=None, (01)=Above, (11)=Below */
	t_u8 chan2Offset : 2;
	/** Channel Width - (00)=20MHz, (10)=40MHz, (11)=80MHz */
	t_u8 chanWidth : 2;
	/** Band Info - (00)=2.4GHz, (01)=5GHz */
	t_u8 chanBand : 2;
#else
	/** Band Info - (00)=2.4GHz, (01)=5GHz */
	t_u8 chanBand : 2;
	/** Channel Width - (00)=20MHz, (10)=40MHz, (11)=80MHz */
	t_u8 chanWidth : 2;
	/** Secondary Channel Offset - (00)=None, (01)=Above, (11)=Below */
	t_u8 chan2Offset : 2;
	/** Channel Selection Mode - (00)=manual, (01)=ACS, (02)=Adoption mode*/
	t_u8 scanMode : 2;
#endif
} __ATTRIB_PACK__ Band_Config_t;

/** Maximum length of lines in configuration file */
#define MAX_CONFIG_LINE 1024
/** MAC BROADCAST */
#define MAC_BROADCAST 0x1FF
/** MAC MULTICAST */
#define MAC_MULTICAST 0x1FE

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

/** max mod group */
#define MAX_MOD_GROUP 35

/** modulation setting */
typedef struct _mod_group_setting {
	/** modulation group */
	t_u8 mod_group;
	/** power */
	t_u8 power;
} __ATTRIB_PACK__ mod_group_setting;

/** chan trpc config */
typedef struct _ChanTRPCConfig_t {
	/** start freq */
	t_u16 start_freq;
	/* channel width */
	t_u8 width;
	/** channel number */
	t_u8 chan_num;
	mod_group_setting mod_group[MAX_MOD_GROUP];
} __ATTRIB_PACK__ ChanTRPCConfig_t;

/** MrvlIETypes_ChanTRPCConfig_t */
typedef struct _MrvlIETypes_ChanTRPCConfig_t {
	/** Header */
	MrvlIEtypesHeader_t header;
	/** start freq */
	t_u16 start_freq;
	/* channel width */
	t_u8 width;
	/** channel number */
	t_u8 chan_num;
	/** mode groups */
	mod_group_setting mod_group[];
} __ATTRIB_PACK__ MrvlIETypes_ChanTRPCConfig_t;

/*This command gets/sets the Transmit Rate-based Power Control (TRPC) channel
 * configuration.*/
#define HostCmd_CHANNEL_TRPC_CONFIG 0x00fb

/** TLV OF CHAN_TRPC_CONFIG */
#define TLV_TYPE_CHAN_TRPC_CONFIG (PROPRIETARY_TLV_BASE_ID + 137)

/** mlan_ds_misc_chan_trpc_cfg */
typedef struct _mlan_ds_misc_chan_trpc_cfg {
	/** sub_band */
	t_u16 sub_band;
	/** length */
	t_u16 length;
	/** trpc buf */
	t_u8 trpc_buf[BUFFER_LENGTH];
} __ATTRIB_PACK__ mlan_ds_misc_chan_trpc_cfg;

struct eth_priv_addba {
	t_u32 time_out;
	t_u32 tx_win_size;
	t_u32 rx_win_size;
	t_u32 tx_amsdu;
	t_u32 rx_amsdu;
};

struct eth_priv_htcapinfo {
	t_u32 ht_cap_info_bg;
	t_u32 ht_cap_info_a;
};

/** data_structure for cmd vhtcfg */
struct eth_priv_vhtcfg {
	/** Band (1: 2.4G, 2: 5 G, 3: both 2.4G and 5G) */
	t_u32 band;
	/** TxRx (1: Tx, 2: Rx, 3: both Tx and Rx) */
	t_u32 txrx;
	/** BW CFG (0: 11N CFG, 1: vhtcap) */
	t_u32 bwcfg;
	/** VHT capabilities. */
	t_u32 vht_cap_info;
	/** VHT Tx mcs */
	t_u32 vht_tx_mcs;
	/** VHT Rx mcs */
	t_u32 vht_rx_mcs;
	/** VHT rx max rate */
	t_u16 vht_rx_max_rate;
	/** VHT max tx rate */
	t_u16 vht_tx_max_rate;
	/** Skip usr 11ac mcs cfg */
	t_bool skip_usr_11ac_mcs_cfg;
};

/** data structure for cmd txratecfg */
struct eth_priv_tx_rate_cfg {
	/* LG rate: 0, HT rate: 1, VHT rate: 2 */
	t_u32 rate_format;
	/** Rate/MCS index (0xFF: auto) */
	t_u32 rate_index;
	/** Rate rate */
	t_u32 rate;
	/** NSS */
	t_u32 nss;
	/** Rate Setting */
	t_u16 rate_setting;
};

#define MLAN_11AXCMD_CFG_ID_SR_OBSS_PD_OFFSET 1
#define MLAN_11AXCMD_CFG_ID_SR_ENABLE 2
#define MLAN_11AXCMD_CFG_ID_BEAM_CHANGE 3
#define MLAN_11AXCMD_CFG_ID_HTC_ENABLE 4
#define MLAN_11AXCMD_CFG_ID_TXOP_RTS 5
#define MLAN_11AXCMD_CFG_ID_TX_OMI 6
#define MLAN_11AXCMD_CFG_ID_OBSSNBRU_TOLTIME 7
#define MLAN_11AXCMD_CFG_ID_SET_BSRP 8
#define MLAN_11AXCMD_CFG_ID_LLDE 9
#define MLAN_11AXCMD_CFG_ID_RUTXPWR 10

#define MLAN_11AXCMD_SR_SUBID 0x102
#define MLAN_11AXCMD_BEAM_SUBID 0x103
#define MLAN_11AXCMD_HTC_SUBID 0x104
#define MLAN_11AXCMD_TXOMI_SUBID 0x105
#define MLAN_11AXCMD_OBSS_TOLTIME_SUBID 0x106
#define MLAN_11AXCMD_TXOPRTS_SUBID 0x108
#define MLAN_11AXCMD_SET_BSRP_SUBID 0x109
#define MLAN_11AXCMD_LLDE_SUBID 0x110
#define MLAN_11AXCMD_RUTXSUBPWR_SUBID 0x118

#define MRVL_DOT11AX_ENABLE_SR_TLV_ID (PROPRIETARY_TLV_BASE_ID + 322)
#define MRVL_DOT11AX_OBSS_PD_OFFSET_TLV_ID (PROPRIETARY_TLV_BASE_ID + 323)

/** Type definition of mlan_ds_11ax_he_capa for MLAN_OID_11AX_HE_CFG */
typedef struct _mlan_ds_11ax_he_capa {
	/** tlv id of he capability */
	t_u16 id;
	/** length of the payload */
	t_u16 len;
	/** extension id */
	t_u8 ext_id;
	/** he mac capability info */
	t_u8 he_mac_cap[6];
	/** he phy capability info */
	t_u8 he_phy_cap[11];
	/** he txrx mcs support for 80MHz */
	t_u8 he_txrx_mcs_support[4];
	/** val for txrx mcs 160Mhz or 80+80, and PPE thresholds */
	t_u8 val[28];
} __ATTRIB_PACK__ mlan_ds_11ax_he_capa, *pmlan_ds_11ax_he_capa;

/** Type definition of mlan_ds_11ax_he_cfg for MLAN_OID_11AX_HE_CFG */
typedef struct _mlan_ds_11ax_he_cfg {
	/** band, BIT0:2.4G, BIT1:5G*/
	t_u8 band;
	/** mlan_ds_11ax_he_capa */
	mlan_ds_11ax_he_capa he_cap;
} __ATTRIB_PACK__ mlan_ds_11ax_he_cfg, *pmlan_ds_11ax_he_cfg;

/** Type definition of mlan_11axcmdcfg_obss_pd_offset for MLAN_OID_11AX_CMD_CFG
 */
typedef struct _mlan_11axcmdcfg_obss_pd_offset {
	/** <NON_SRG_OffSET, SRG_OFFSET> */
	t_u8 offset[2];
} __ATTRIB_PACK__ mlan_11axcmdcfg_obss_pd_offset;

/** Type definition of mlan_11axcmdcfg_sr_control for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_11axcmdcfg_sr_control {
	/** 1 enable, 0 disable */
	t_u8 control;
} __ATTRIB_PACK__ mlan_11axcmdcfg_sr_control;

/** Type definition of mlan_ds_11ax_sr_cmd for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_ds_11ax_sr_cmd {
	/** type*/
	t_u16 type;
	/** length of TLV */
	t_u16 len;
	/** value */
	union {
		mlan_11axcmdcfg_obss_pd_offset obss_pd_offset;
		mlan_11axcmdcfg_sr_control sr_control;
	} param;
} __ATTRIB_PACK__ mlan_ds_11ax_sr_cmd, *pmlan_ds_11ax_sr_cmd;

/** Type definition of mlan_ds_11ax_beam_cmd for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_ds_11ax_beam_cmd {
	/** command value: 1 is disable, 0 is enable*/
	t_u8 value;
} mlan_ds_11ax_beam_cmd, *pmlan_ds_11ax_beam_cmd;

/** Type definition of mlan_ds_11ax_htc_cmd for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_ds_11ax_htc_cmd {
	/** command value: 1 is enable, 0 is disable*/
	t_u8 value;
} mlan_ds_11ax_htc_cmd, *pmlan_ds_11ax_htc_cmd;

/** Type definition of mlan_ds_11ax_txop_cmd for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_ds_11ax_txop_cmd {
	/** Two byte rts threshold value of which only 10 bits, bit 0 to bit 9
	 * are valid */
	t_u16 rts_thres;
} mlan_ds_11ax_txop_cmd, *pmlan_ds_11ax_txop_cmd;

/** Type definition of mlan_ds_11ax_txomi_cmd for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_ds_11ax_txomi_cmd {
	/* 11ax spec 9.2.4.6a.2 OM Control 12 bits. Bit 0 to bit 11 */
	t_u16 omi;
	/* tx option
	 * 0: send OMI in QoS NULL; 1: send OMI in QoS data; 0xFF: set OMI in
	 * both
	 */
	t_u8 tx_option;
	/* if OMI is sent in QoS data, specify the number of consecutive data
	 * packets containing the OMI
	 */
	t_u8 num_data_pkts;
} __ATTRIB_PACK__ mlan_ds_11ax_txomi_cmd, *pmlan_ds_11ax_txomi_cmd;

/** Type definition of mlan_ds_11ax_toltime_cmd for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_ds_11ax_toltime_cmd {
	/* OBSS Narrow Bandwidth RU Tolerance Time */
	t_u32 tol_time;
} mlan_ds_11ax_toltime_cmd, *pmlan_ds_11ax_toltime_cmd;

/** Type definition of mlan_ds_11ax_set_bsrp_cmd for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_ds_11ax_set_bsrp_cmd {
	/** command value: 1 is enable, 0 is disable*/
	t_u8 value;
} mlan_ds_11ax_set_bsrp_cmd, *pmlan_ds_11ax_set_bsrp_cmd;

/** Type definition of mlan_ds_11ax_llde_cmd for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_ds_11ax_llde_cmd {
	t_u8 llde; // Uplink LLDE: enable=1,disable=0
	t_u8 mode; // operation mode: default=0,carplay=1,gameplay=2
	t_u8 fixrate; // trigger frame rate: auto=0xff
	t_u8 triggerlimit; // cap airtime limit index: auto=0xff
	t_u8 peakULrate; // cap peak UL rate
	t_u8 dl_llde; // Downlink LLDE: enable=1,disable=0
	t_u16 pollinterval; // Set trigger frame interval(us): auto=0
	t_u16 txOpDuration; // Set TxOp duration
	t_u16 llde_ctrl; // for other configurations
	t_u16 mu_rts_successcnt;
	t_u16 mu_rts_failcnt;
	t_u16 basic_trigger_successcnt;
	t_u16 basic_trigger_failcnt;
	t_u16 tbppdu_nullcnt;
	t_u16 tbppdu_datacnt;
} mlan_ds_11ax_llde_cmd, *pmlan_ds_11ax_llde_cmd;

/** Type definition of mlan_ds_11ax_rutxpwr_cmd for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_ds_11ax_rutxpwr_cmd {
	MrvlIEtypesHeader_t header;
	/** Sub-Band */
	t_u8 subBand;
	/** column,row are 3 for table,however column are 7 for FC and 6 for
	 * other SOCs */
	t_u8 col;
	/*ru tx data */
	t_u8 rutxSubPwr[89];
} mlan_ds_11ax_rutxpwr_cmd, *pmlan_ds_11ax_rutxpwr_cmd;

/** Type definition of mlan_ds_11ax_cmd_cfg for MLAN_OID_11AX_CMD_CFG */
typedef struct _mlan_ds_11ax_cmd_cfg {
	/** Sub-command */
	t_u32 sub_command;
	/** Sub-id */
	t_u32 sub_id;
	/** 802.11n configuration parameter */
	union {
		/** SR configuration for MLAN_11AXCMD_SR_SUBID */
		mlan_ds_11ax_sr_cmd sr_cfg;
		/** Beam configuration for MLAN_11AXCMD_BEAM_SUBID */
		mlan_ds_11ax_beam_cmd beam_cfg;
		/** HTC configuration for MLAN_11AXCMD_HTC_SUBID */
		mlan_ds_11ax_htc_cmd htc_cfg;
		/** HTC configuration for MLAN_11AXCMD_TXOPRTS_SUBID */
		mlan_ds_11ax_txop_cmd txop_cfg;
		/** HTC configuration for MLAN_11AXCMD_TXOMI_SUBID */
		mlan_ds_11ax_txomi_cmd txomi_cfg;
		/** HTC configuration for MLAN_11AXCMD_TXOMI_SUBID */
		mlan_ds_11ax_toltime_cmd toltime_cfg;
		/** BSRP configuration for MLAN_11AXCMD_SET_BSRP_SUBID */
		mlan_ds_11ax_set_bsrp_cmd setbsrp_cfg;
		/** LLDE configuration for MLAN_11AXCMD_LLDE_SUBID */
		mlan_ds_11ax_llde_cmd llde_cfg;
		/** rutxpwr for subband/channel for
		 * MLAN_11AXCMD_RUTXSUBPWR_SUBID */
		mlan_ds_11ax_rutxpwr_cmd rutxpwr_cfg;
	} param;
} mlan_ds_11ax_cmd_cfg, *pmlan_ds_11ax_cmd_cfg;

/** Maximum number of AC QOS queues available in the driver/firmware */
#define MAX_AC_QUEUES 4
#define MAX_LED_ARGS 4
#define MAX_FW_STATES 6

/** Read/Write Mac register */
#define HostCmd_CMD_MAC_REG_ACCESS 0x0019
/** Read/Write BBP register */
#define HostCmd_CMD_BBP_REG_ACCESS 0x001a
/** Read/Write RF register */
#define HostCmd_CMD_RF_REG_ACCESS 0x001b

/** Data structure of WMM Aci/Aifsn */
typedef struct _IEEEtypes_WmmAciAifsn_t {
#ifdef BIG_ENDIAN_SUPPORT
	/** Reserved */
	t_u8 reserved : 1;
	/** Aci */
	t_u8 aci : 2;
	/** Acm */
	t_u8 acm : 1;
	/** Aifsn */
	t_u8 aifsn : 4;
#else
	/** Aifsn */
	t_u8 aifsn : 4;
	/** Acm */
	t_u8 acm : 1;
	/** Aci */
	t_u8 aci : 2;
	/** Reserved */
	t_u8 reserved : 1;
#endif
} __ATTRIB_PACK__ IEEEtypes_WmmAciAifsn_t, *pIEEEtypes_WmmAciAifsn_t;

/** Data structure of WMM ECW */
typedef struct _IEEEtypes_WmmEcw_t {
#ifdef BIG_ENDIAN_SUPPORT
	/** Maximum Ecw */
	t_u8 ecw_max : 4;
	/** Minimum Ecw */
	t_u8 ecw_min : 4;
#else
	/** Minimum Ecw */
	t_u8 ecw_min : 4;
	/** Maximum Ecw */
	t_u8 ecw_max : 4;
#endif
} __ATTRIB_PACK__ IEEEtypes_WmmEcw_t, *pIEEEtypes_WmmEcw_t;

/** Data structure of WMM AC parameters  */
typedef struct _IEEEtypes_WmmAcParameters_t {
	IEEEtypes_WmmAciAifsn_t aci_aifsn; /**< AciAifSn */
	IEEEtypes_WmmEcw_t ecw; /**< Ecw */
	t_u16 tx_op_limit; /**< Tx op limit */
} __ATTRIB_PACK__ IEEEtypes_WmmAcParameters_t, *pIEEEtypes_WmmAcParameters_t;

/** HostCmd_DS_802_11_CFG_DATA */
typedef struct MAPP_HostCmd_DS_802_11_CFG_DATA {
	/** Action */
	t_u16 action;
	/** Type */
	t_u16 type;
	/** Data length */
	t_u16 data_len;
	/** Data */
	t_u8 data[1];
} __ATTRIB_PACK__ HostCmd_DS_802_11_CFG_DATA;

/** Host Command ID : Configuration data */
#define HostCmd_CMD_CFG_DATA 0x008f

/** mlan_ioctl_11h_tpc_resp */
typedef struct {
	int status_code; /**< Firmware command result status code */
	int tx_power; /**< Reported TX Power from the TPC Report */
	int link_margin; /**< Reported Link margin from the TPC Report */
	int rssi; /**< RSSI of the received TPC Report frame */
} __ATTRIB_PACK__ mlan_ioctl_11h_tpc_resp;

/** Host Command ID : 802.11 TPC adapt req */
#define HostCmd_CMD_802_11_TPC_ADAPT_REQ 0x0060

/** HostCmd_DS_802_11_CRYPTO */
typedef struct MAPP_HostCmd_DS_802_11_CRYPTO {
	t_u16 encdec; /**< Decrypt=0, Encrypt=1 */
	t_u16 algorithm; /**< RC4=1 AES=2 , AES_KEY_WRAP=3 */
	t_u16 key_IV_length; /**< Length of Key IV (bytes)   */
	t_u8 keyIV[32]; /**< Key IV */
	t_u16 key_length; /**< Length of Key (bytes) */
	t_u8 key[32]; /**< Key */
	MrvlIEtypes_Data_t data; /**< Plain text if encdec=Encrypt, Ciphertext
				    data if encdec=Decrypt*/
} __ATTRIB_PACK__ HostCmd_DS_802_11_CRYPTO;

/** HostCmd_DS_802_11_CRYPTO_AES_CCM */
typedef struct MAPP_HostCmd_DS_802_11_CRYPTO_AES_CCM {
	t_u16 encdec; /**< Decrypt=0, Encrypt=1 */
	t_u16 algorithm; /**< AES_CCM=4 */
	t_u16 key_length; /**< Length of Key (bytes)  */
	t_u8 key[32]; /**< Key  */
	t_u16 nonce_length; /**< Length of Nonce (bytes) */
	t_u8 nonce[14]; /**< Nonce */
	t_u16 AAD_length; /**< Length of AAD (bytes) */
	t_u8 AAD[32]; /**< AAD */
	MrvlIEtypes_Data_t data; /**< Plain text if encdec=Encrypt, Ciphertext
				    data if encdec=Decrypt*/
} __ATTRIB_PACK__ HostCmd_DS_802_11_CRYPTO_AES_CCM;

/** HostCmd_DS_802_11_CRYPTO_WAPI */
typedef struct MAPP_HostCmd_DS_802_11_CRYPTO_WAPI {
	t_u16 encdec; /**< Decrypt=0, Encrypt=1 */
	t_u16 algorithm; /**< WAPI =5 */
	t_u16 key_length; /**< Length of Key (bytes)  */
	t_u8 key[32]; /**< Key  */
	t_u16 nonce_length; /**< Length of Nonce (bytes) */
	t_u8 nonce[16]; /**< Nonce */
	t_u16 AAD_length; /**< Length of AAD (bytes) */
	t_u8 AAD[48]; /**< AAD */
	t_u16 data_length; /**< Length of data (bytes)  */
} __ATTRIB_PACK__ HostCmd_DS_802_11_CRYPTO_WAPI;
/** WAPI cipher test */
#define CIPHER_TEST_WAPI (5)
/** AES CCM cipher test */
#define CIPHER_TEST_AES_CCM (4)
/** GCMP cipher test */
#define CIPHER_TEST_GCMP (6)
/** Host Command ID : 802.11 crypto */
#define HostCmd_CMD_802_11_CRYPTO 0x0078

/** HostCmd_DS_802_11_SUBSCRIBE_EVENT */
typedef struct MAPP_HostCmd_DS_802_11_SUBSCRIBE_EVENT {
	/** Action */
	t_u16 action;
	/** Events */
	t_u16 events;
} __ATTRIB_PACK__ HostCmd_DS_802_11_SUBSCRIBE_EVENT;

/** MrvlIEtypes_RssiParamSet_t */
typedef struct MrvlIEtypes_RssiThreshold {
	/** Header */
	MrvlIEtypesHeader_t header;
	/** RSSI value */
	t_u8 RSSI_value;
	/** RSSI frequency */
	t_u8 RSSI_freq;
} __ATTRIB_PACK__ MrvlIEtypes_RssiThreshold_t;

/** MrvlIEtypes_SnrThreshold_t */
typedef struct MrvlIEtypes_SnrThreshold {
	/** Header */
	MrvlIEtypesHeader_t header;
	/** SNR value */
	t_u8 SNR_value;
	/** SNR frequency */
	t_u8 SNR_freq;
} __ATTRIB_PACK__ MrvlIEtypes_SnrThreshold_t;

/** MrvlIEtypes_FailureCount_t */
typedef struct MrvlIEtypes_FailureCount {
	/** Header */
	MrvlIEtypesHeader_t header;
	/** Failure value */
	t_u8 fail_value;
	/** Failure frequency */
	t_u8 fail_freq;
} __ATTRIB_PACK__ MrvlIEtypes_FailureCount_t;

/** MrvlIEtypes_BeaconsMissed_t */
typedef struct MrvlIEtypes_BeaconsMissed {
	/** Header */
	MrvlIEtypesHeader_t header;
	/** Number of beacons missed */
	t_u8 beacon_missed;
	/** Reserved */
	t_u8 reserved;
} __ATTRIB_PACK__ MrvlIEtypes_BeaconsMissed_t;

/** MrvlIEtypes_LinkQuality_t */
typedef struct MrvlIEtypes_LinkQuality {
	/** Header */
	MrvlIEtypesHeader_t header;
	/** Link SNR threshold */
	t_u16 link_SNR_thrs;
	/** Link SNR frequency */
	t_u16 link_SNR_freq;
	/** Minimum rate value */
	t_u16 min_rate_val;
	/** Minimum rate frequency */
	t_u16 min_rate_freq;
	/** Tx latency value */
	t_u32 tx_latency_val;
	/** Tx latency threshold */
	t_u32 tx_latency_thrs;
} __ATTRIB_PACK__ MrvlIEtypes_LinkQuality_t;

/** Host Command ID : 802.11 subscribe event */
#define HostCmd_CMD_802_11_SUBSCRIBE_EVENT 0x0075

/** TLV type : Beacon RSSI low */
#define TLV_TYPE_RSSI_LOW (PROPRIETARY_TLV_BASE_ID + 0x04) /* 0x0104 */
/** TLV type : Beacon SNR low */
#define TLV_TYPE_SNR_LOW (PROPRIETARY_TLV_BASE_ID + 0x05) /* 0x0105 */
/** TLV type : Fail count */
#define TLV_TYPE_FAILCOUNT (PROPRIETARY_TLV_BASE_ID + 0x06) /* 0x0106 */
/** TLV type : BCN miss */
#define TLV_TYPE_BCNMISS (PROPRIETARY_TLV_BASE_ID + 0x07) /* 0x0107 */
/** TLV type : Beacon RSSI high */
#define TLV_TYPE_RSSI_HIGH (PROPRIETARY_TLV_BASE_ID + 0x16) /* 0x0116 */
/** TLV type : Beacon SNR high */
#define TLV_TYPE_SNR_HIGH (PROPRIETARY_TLV_BASE_ID + 0x17) /* 0x0117 */

/** TLV type :Link Quality */
#define TLV_TYPE_LINK_QUALITY (PROPRIETARY_TLV_BASE_ID + 0x24) /* 0x0124 */

/** TLV type : Data RSSI low */
#define TLV_TYPE_RSSI_LOW_DATA (PROPRIETARY_TLV_BASE_ID + 0x26) /* 0x0126 */
/** TLV type : Data SNR low */
#define TLV_TYPE_SNR_LOW_DATA (PROPRIETARY_TLV_BASE_ID + 0x27) /* 0x0127 */
/** TLV type : Data RSSI high */
#define TLV_TYPE_RSSI_HIGH_DATA (PROPRIETARY_TLV_BASE_ID + 0x28) /* 0x0128 */
/** TLV type : Data SNR high */
#define TLV_TYPE_SNR_HIGH_DATA (PROPRIETARY_TLV_BASE_ID + 0x29) /* 0x0129 */

/** MrvlIEtypes_PreBeaconLost_t */
typedef struct MrvlIEtypes_PreBeaconLost {
	/** Header */
	MrvlIEtypesHeader_t header;
	/** Pre-Beacon Lost */
	t_u8 pre_beacon_lost;
	/** Reserved */
	t_u8 reserved;
} __ATTRIB_PACK__ MrvlIEtypes_PreBeaconLost_t;

/** TLV type: Pre-Beacon Lost */
#define TLV_TYPE_PRE_BEACON_LOST (PROPRIETARY_TLV_BASE_ID + 0x49) /* 0x0149 */

/** AutoTx_MacFrame_t */
typedef struct AutoTx_MacFrame {
	t_u16 interval; /**< in seconds */
	t_u8 priority; /**< User Priority: 0~7, ignored if non-WMM */
	t_u8 reserved; /**< set to 0 */
	t_u8 getTodToAForPkts;
	t_u16 frame_len; /**< Length of MAC frame payload */
	t_u8 dest_mac_addr[MLAN_MAC_ADDR_LENGTH]; /**< Destination MAC address
						   */
	t_u8 src_mac_addr[MLAN_MAC_ADDR_LENGTH]; /**< Source MAC address */
	t_u8 payload[]; /**< Payload */
} __ATTRIB_PACK__ AutoTx_MacFrame_t;

/** MrvlIEtypes_AutoTx_t */
typedef struct MrvlIEtypes_AutoTx {
	MrvlIEtypesHeader_t header; /**< Header */
	AutoTx_MacFrame_t auto_tx_mac_frame; /**< Auto Tx MAC frame */
} __ATTRIB_PACK__ MrvlIEtypes_AutoTx_t;

/** HostCmd_DS_802_11_AUTO_TX */
typedef struct MAPP_HostCmd_DS_802_11_AUTO_TX {
	/** Action */
	t_u16 action; /* 0 = ACT_GET; 1 = ACT_SET; */
	MrvlIEtypes_AutoTx_t auto_tx; /**< Auto Tx */
} __ATTRIB_PACK__ HostCmd_DS_802_11_AUTO_TX;

/** Host Command ID : 802.11 auto Tx */
#define HostCmd_CMD_802_11_AUTO_TX 0x0082

/** TLV type : Auto Tx */
#define TLV_TYPE_AUTO_TX (PROPRIETARY_TLV_BASE_ID + 0x18) /* 0x0118 */

/** Host Command ID : CAU register access */
#define HostCmd_CMD_CAU_REG_ACCESS 0x00ed

/** Host Command ID : Memory access */
#define HostCmd_CMD_MEM_ACCESS 0x0086

typedef struct {
	t_u32 timeSinceLastQuery_ms; /**< Duration of stats collection */

	t_u16 bcnCnt; /**< Number of beacons received */
	t_u16 bcnMiss; /**< Estimate of beacons missed */
	t_s16 bcnRssiAvg; /**< Avg beacon RSSI */
	t_s16 bcnSnrAvg; /**< Avg beacon SNR */

	t_u32 rxPkts; /**< Number of packets received */
	t_s16 rxRssiAvg; /**< Avg received packet RSSI */
	t_s16 rxSnrAvg; /**< Avg received packet SNR */

	t_u32 txPkts; /**< Number of packets transmitted */
	t_u32 txAttempts; /**< Number of attempts made */
	t_u32 txFailures; /**< Number of pkts that failed */
	t_u8 txInitRate; /**< Current rate adaptation TX rateid */
	t_u8 reserved[3]; /**< Reserved */

	t_u16 txQueuePktCnt[MAX_AC_QUEUES]; /**< Number of packets per AC */
	t_u32 txQueueDelay[MAX_AC_QUEUES]; /**< Averge queue delay per AC*/
} __ATTRIB_PACK__ HostCmd_DS_LINK_STATS_SUMMARY;

#define HostCmd_CMD_LINK_STATS_SUMMARY 0x00d3

/** Type enumeration of WMM AC_QUEUES */
typedef enum _wmm_ac {
	AC_BE,
	AC_BK,
	AC_VI,
	AC_VO,
} wmm_ac;

/** Data structure of Host command WMM_PARAM_CFG  */
typedef struct _HostCmd_DS_WMM_PARAM_CONFIG {
	/** action */
	t_u16 action;
	/** AC Parameters Record WMM_AC_BE, WMM_AC_BK, WMM_AC_VI, WMM_AC_VO */
	IEEEtypes_WmmAcParameters_t ac_params[MAX_AC_QUEUES];
} __ATTRIB_PACK__ HostCmd_DS_WMM_PARAM_CONFIG;

/** Host Command ID : Configure ADHOC_OVER_IP parameters */
#define HostCmd_CMD_WMM_PARAM_CONFIG 0x023a

/** HostCmd_DS_REG */
typedef struct MAPP_HostCmd_DS_REG {
	/** Read or write */
	t_u16 action;
	/** Register offset */
	t_u16 offset;
	/** Value */
	t_u32 value;
} __ATTRIB_PACK__ HostCmd_DS_REG;

/** HostCmd_DS_MEM */
typedef struct MAPP_HostCmd_DS_MEM {
	/** Read or write */
	t_u16 action;
	/** Reserved */
	t_u16 reserved;
	/** Address */
	t_u32 addr;
	/** Value */
	t_u32 value;
} __ATTRIB_PACK__ HostCmd_DS_MEM;

typedef struct _HostCmd_DS_MEF_CFG {
	/** Criteria */
	t_u32 Criteria;
	/** Number of entries */
	t_u16 NumEntries;
} __ATTRIB_PACK__ HostCmd_DS_MEF_CFG;

typedef struct _MEF_CFG_DATA {
	/** Size */
	t_u16 size;
	/** Data */
	HostCmd_DS_MEF_CFG data;
} __ATTRIB_PACK__ MEF_CFG_DATA;

/** cloud keep alive parameters */
typedef struct _cloud_keep_alive {
	/** id */
	t_u8 mkeep_alive_id;
	/** enable/disable of this id */
	t_u8 enable;
	/** enable/disable reset*/
	t_u8 reset;
	/** Reserved */
	t_u8 reserved;
	/** Destination MAC address */
	t_u8 dst_mac[ETH_ALEN];
	/** Source MAC address */
	t_u8 src_mac[ETH_ALEN];
	/** packet send period */
	t_u32 sendInterval;
	/** packet retry interval */
	t_u32 retryInterval;
	/** packet retry count */
	t_u8 retryCount;
	/** packet length */
	t_u8 pkt_len;
	/** packet content */
	t_u8 pkt[255];
} __ATTRIB_PACK__ cloud_keep_alive;

/** cloud keep alive parameters */
typedef struct _cloud_keep_alive_rx {
	/** id */
	t_u8 mkeep_alive_id;
	/** enable/disable of this id */
	t_u8 enable;
	/** enable/disable reset*/
	t_u8 reset;
	/** Reserved */
	t_u8 reserved;
	/** Destination MAC address */
	t_u8 dst_mac[ETH_ALEN];
	/** Source MAC address */
	t_u8 src_mac[ETH_ALEN];
	/** packet length */
	t_u8 pkt_len;
	/** packet content */
	t_u8 pkt[100];
} __ATTRIB_PACK__ cloud_keep_alive_rx;

#define CHANNEL_SPEC_SNIFFER_MODE 1

/** Channel usability flags */
#define NXP_CHANNEL_DISABLED MBIT(7)
#define NXP_CHANNEL_NOHT160 MBIT(4)
#define NXP_CHANNEL_NOHT80 MBIT(3)
#define NXP_CHANNEL_NOHT40 MBIT(2)
#define NXP_CHANNEL_DFS MBIT(1)
#define NXP_CHANNEL_PASSIVE MBIT(0)

/** Socket */
extern t_s32 sockfd;

#if defined(STA_SUPPORT)
struct eth_priv_pmfcfg {
	/* Management Frame Protection Capability */
	t_u8 mfpc;
	/* Management Frame Protection Required */
	t_u8 mfpr;
};
#endif

#define MAX_NUM_MAC 2

struct dmcsChanStatus_t {
	/** Channel number */
	t_u8 channel;
	/** Number of ap on this channel */
	t_u8 ap_count;
	/** Number of station on this channel */
	t_u8 sta_count;
};

struct dmcsStatus_t {
	/** Radio ID */
	t_u8 radio_id;
	/** Running mode */
	t_u8 running_mode;
	/** Current channel status */
	struct dmcsChanStatus_t chan_status[2];
};

/** data structure for cmd dmcs */
struct eth_priv_dmcs_status {
	/** Current mapping policy */
	t_u8 mapping_policy;
	/** radio status of DMCS */
	struct dmcsStatus_t radio_status[MAX_NUM_MAC];
};

/** data_structure for cmd opermodecfg */
struct eth_priv_opermodecfg {
	/** channel width: 1-20MHz, 2-40MHz, 3-80MHz, 4-160MHz or 80+80MHz */
	t_u8 bw;
	/** Rx NSS */
	t_u8 nss;
};

/** data structure for cmd bandcfg */
struct eth_priv_bandcfg {
	/** Infra band */
	t_u32 config_bands;
	/** fw supported band */
	t_u32 fw_bands;
};

struct eth_priv_esuppmode_cfg {
	/* RSN mode */
	t_u16 rsn_mode;
	/* Pairwise cipher */
	t_u8 pairwise_cipher;
	/* Group cipher */
	t_u8 group_cipher;
};

#ifdef UAP_SUPPORT
/** Max clients supported by AP */
#define MAX_AP_CLIENTS 64
/** station stats */
typedef struct _sta_stats {
	/** last_rx_in_msec */
	t_u64 last_rx_in_msec;
	/** rx_packets */
	t_u32 rx_packets;
	/** tx packets */
	t_u32 tx_packets;
	/** rx bytes */
	t_u32 rx_bytes;
	/** tx bytes */
	t_u32 tx_bytes;
} sta_stats;

/** associated station info */
struct ap_client_info {
	/** STA MAC address */
	t_u8 mac_address[MLAN_MAC_ADDR_LENGTH];
	/** Power Mgmt status */
	t_u8 power_mgmt_status;
	/** RSSI */
	t_s8 rssi;
	/** station bandmode */
	t_u16 bandmode;
	/** station stats */
	sta_stats stats;
	/** ie length */
	t_u16 ie_len;
};

/** Per station Maximum IE buffer SIZE */
#define MAX_STA_LIST_IE_SIZE 13

/** Type definition of eth_priv_getstalist */
struct eth_priv_getstalist {
	/** station count */
	t_u16 sta_count;
	/** station list */
	struct ap_client_info client_info[MAX_AP_CLIENTS];
	/* ie_buf will be append at the end */
};
#endif

/** Type definition of mlan_ds_hs_cfg for MLAN_OID_PM_CFG_HS_CFG */
struct eth_priv_hs_cfg {
	/** MTRUE to invoke the HostCmd, MFALSE otherwise */
	t_u32 is_invoke_hostcmd;
	/** Host sleep config condition */
	/** Bit0: broadcast data
	 *  Bit1: unicast data
	 *  Bit2: mac event
	 *  Bit3: multicast data
	 */
	t_u32 conditions;
	/** GPIO pin or 0xff for interface */
	t_u32 gpio;
	/** Gap in milliseconds or or 0xff for special setting (host acknowledge
	 * required) */
	t_u32 gap;
	/** Host sleep wake interval */
	t_u32 hs_wake_interval;
	/** Parameter type*/
	t_u8 param_type_ind;
	/** Indication GPIO pin number */
	t_u32 ind_gpio;
	/** Level on ind_GPIO pin for normal wakeup source */
	t_u32 level;
	/** Parameter type*/
	t_u8 param_type_ext;
	/** Force ignore events*/
	t_u32 event_force_ignore;
	/** Events use ext gap to wake up host*/
	t_u32 event_use_ext_gap;
	/** Ext gap*/
	t_u8 ext_gap;
	/** GPIO wave level*/
	t_u8 gpio_wave;
};

typedef struct _eth_priv_mgmt_frame_wakeup {
	/** action - bitmap
	 ** On matching rx'd pkt and filter during NON_HOSTSLEEP mode:
	 **   Action[1]=0  Discard
	 **   Action[1]=1  Allow
	 ** Note that default action on non-match is "Allow".
	 **
	 ** On matching rx'd pkt and filter during HOSTSLEEP mode:
	 **   Action[1:0]=00  Discard and Not Wake host
	 **   Action[1:0]=01  Discard and Wake host
	 **   Action[1:0]=10  Invalid
	 ** Note that default action on non-match is "Discard and Not Wake
	 *host".
	 **/
	t_u32 action;
	/** Frame type(p2p, tdls...)
	 ** type=0: invalid
	 ** type=1: p2p
	 ** type=0xff: management frames(assoc res/rsp,probe req/rsp,...)
	 ** type=others: reserved
	 **/
	t_u32 type;
	/** Frame mask according to each type
	 ** When type=1 for p2p, frame-mask have following define:
	 **    Bit      Frame
	 **     0       GO Negotiation Request
	 **     1       GO Negotiation Response
	 **     2       GO Negotiation Confirmation
	 **     3       P2P Invitation Request
	 **     4       P2P Invitation Response
	 **     5       Device Discoverability Request
	 **     6       Device Discoverability Response
	 **     7       Provision Discovery Request
	 **     8       Provision Discovery Response
	 **     9       Notice of Absence
	 **     10      P2P Presence Request
	 **     11      P2P Presence Response
	 **     12      GO Discoverability Request
	 **     13-31   Reserved
	 **
	 ** When type=others, frame-mask is reserved.
	 **/
	t_u32 frame_mask;
} eth_priv_mgmt_frame_wakeup;

/** Type definition of eth_priv_scan_time_params */
struct eth_priv_scan_time_params {
	/** Scan channel time for specific scan in milliseconds */
	t_u32 specific_scan_time;
	/** Scan channel time for active scan in milliseconds */
	t_u32 active_scan_time;
	/** Scan channel time for passive scan in milliseconds */
	t_u32 passive_scan_time;
};

/** Type definition of eth_priv_scan_cfg */
struct eth_priv_scan_cfg {
	/** Scan type */
	t_u32 scan_type;
	/** BSS mode for scanning */
	t_u32 scan_mode;
	/** Scan probe */
	t_u32 scan_probe;
	/** Scan time parameters */
	struct eth_priv_scan_time_params scan_time;
	/** First passive scan then active scan */
	t_u8 passive_to_active_scan;
	/** Extended Scan */
	t_u32 ext_scan;
	/* Time gap between two scan channels in milliseconds*/
	t_u32 scan_chan_gap;
};

enum _mlan_rate_format {
	MLAN_RATE_FORMAT_LG = 0,
	MLAN_RATE_FORMAT_HT,
	MLAN_RATE_FORMAT_VHT,
	MLAN_RATE_FORMAT_AUTO = 0xFF,
};

/** HT channel bandwidth */
typedef enum _mlan_ht_bw {
	MLAN_HT_BW20,
	MLAN_HT_BW40,
	/** VHT channel bandwidth */
	MLAN_VHT_BW80,
	MLAN_VHT_BW160,
} mlan_ht_bw;
/** Type definition of mlan_power group info */
struct eth_priv_power_group {
	/** rate format (LG rate: 0; HT rate: 1; VHT rate: 2; no auto ctrl:
	 * 0xFF) */
	t_u32 rate_format;
	/** bandwidth (LG: 20 MHz; HT: 20/40 MHz; VHT: 80/160/80+80 MHz) */
	t_u8 bandwidth;
	/** NSS */
	t_u32 nss;
	/** LG: first rate index; HT/VHT: first MCS */
	t_u8 first_rate_ind;
	/** LG: last rate index; HT/VHT: last MCS */
	t_u8 last_rate_ind;
	/** minmum tx power (dBm) */
	t_s8 power_min;
	/** maximum tx power (dBm) */
	t_s8 power_max;
	/** tx power step (dB) */
	t_s8 power_step;
};

/** max of power groups */
#define MAX_POWER_GROUP 64
/** Type definition of mlan_power_cfg_ext */
struct eth_priv_power_cfg_ext {
	/** number of power_groups */
	t_u32 num_pwr_grp;
	/** array of power groups */
	struct eth_priv_power_group power_group[MAX_POWER_GROUP];
};

/** Type definition of eth_priv_ds_ps_cfg */
struct eth_priv_ds_ps_cfg {
	/** PS null interval in seconds */
	t_u32 ps_null_interval;
	/** Multiple DTIM interval */
	t_u32 multiple_dtim_interval;
	/** Listen interval */
	t_u32 listen_interval;
	/** Beacon miss timeout in milliseconds */
	t_u32 bcn_miss_timeout;
	/** Delay to PS in milliseconds */
	t_s32 delay_to_ps;
	/** PS mode */
	t_u32 ps_mode;
};

#ifdef STA_SUPPORT

/** Maximum length of SSID */
#define MRVDRV_MAX_SSID_LENGTH 32

/** Maximum length of SSID list */
#define MRVDRV_MAX_SSID_LIST_LENGTH 10

/** Maximum length of BSSID list */
#define MAX_BSSID_FILTER_LIST 5

/** Bssid structure */
typedef struct {
	/* bssid */
	t_u8 bssid[ETH_ALEN];
} __ATTRIB_PACK__ wlan_ioctl_user_scan_bssid;

/** Maximum number of channels that can be sent in a setuserscan ioctl */
#define WLAN_IOCTL_USER_SCAN_CHAN_MAX 50

/** Maximum channel scratch */
#define MAX_CHAN_SCRATCH 100

/** Maximum channel number for b/g band */
#define MAX_CHAN_BG_BAND 14

/** Maximum number of probes to send on each channel */
#define MAX_PROBES 5

/** Scan all the channels in specified band */
#define BAND_SPECIFIED 0x80

/** Enumeration for scan mode */
enum {
	MLAN_SCAN_MODE_UNCHANGED = 0,
	MLAN_SCAN_MODE_BSS = 1,
	MLAN_SCAN_MODE_ANY = 3
};

/** Enumeration for scan type */
enum {
	MLAN_SCAN_TYPE_UNCHANGED = 0,
	MLAN_SCAN_TYPE_ACTIVE,
	MLAN_SCAN_TYPE_PASSIVE
};

/** Enumeration for passive to active scan */
enum _mlan_pass_to_act_scan {
	MLAN_PASS_TO_ACT_SCAN_UNCHANGED = 0,
	MLAN_PASS_TO_ACT_SCAN_EN,
	MLAN_PASS_TO_ACT_SCAN_DIS
};

typedef struct _IEEEtypes_RSNHeader_t {
	/** Element ID */
	t_u8 element_id;
	/** Length */
	t_u8 len;
	/** Version */
	t_u16 version;
} __ATTRIB_PACK__ IEEEtypes_RSNHeader_t, *pIEEEtypes_RSNHeader_t;

typedef struct _IEEEtypes_RSNIE_t {
	/** RSN IE header */
	IEEEtypes_RSNHeader_t rsn_hdr;
	/** IE Max - size of previous fields */
	t_u8 data[1];
} __ATTRIB_PACK__ IEEEtypes_RSNIE_t, *pIEEEtypes_RSNIE_t;

typedef struct _wlan_get_scan_table_fixed {
	/** BSSID of this network */
	t_u8 bssid[MLAN_MAC_ADDR_LENGTH];
	/** Channel this beacon/probe response was detected */
	t_u8 channel;
	/** RSSI for the received packet */
	t_u8 rssi;
	/** Channel Load (max = 100) */
	t_u8 chan_load;
	/** TSF value from the firmware at packet reception */
	t_u64 network_tsf;
} wlan_get_scan_table_fixed;

/**
 *  Structure passed in the wlan_ioctl_get_scan_table_info for each
 *    BSS returned in the WLAN_GET_SCAN_RESP IOCTL
 */
typedef struct _wlan_ioctl_get_scan_table_entry {
	/**
	 *  Fixed field length included in the response.
	 *
	 *  Length value is included so future fixed fields can be added to the
	 *   response without breaking backwards compatibility.  Use the length
	 *   to find the offset for the bssInfoLength field, not a sizeof()
	 * calc.
	 */
	t_u32 fixed_field_length;

	/**
	 *  Length of the BSS Information (probe resp or beacon) that
	 *    follows after the fixed_field_length
	 */
	t_u32 bss_info_length;

	/**
	 *  Always present, fixed length data fields for the BSS
	 */
	wlan_get_scan_table_fixed fixed_fields;

	/*
	 *  Probe response or beacon scanned for the BSS.
	 *
	 *  Field layout:
	 *   - TSF              8 octets
	 *   - Beacon Interval  2 octets
	 *   - Capability Info  2 octets
	 *
	 *   - IEEE Infomation Elements; variable number & length per 802.11
	 * spec
	 */
	/* t_u8  bss_info_buffer[1];*/
} wlan_ioctl_get_scan_table_entry;

/**
 *  Structure to store BSS info (probe resp or beacon) & IEEE IE info for each
 *  BSS returned in WLAN_GET_SCAN_RESP IOCTL
 */
typedef struct _wlan_ioctl_get_bss_info {
	/**
	 *  Length of the BSS Information (probe resp or beacon) that
	 *    follows after the fixed_field
	 */
	t_u32 bss_info_length;

	/**
	 *  Probe response or beacon scanned for the BSS.
	 *
	 *  Field layout:
	 */
	/** TSF              8 octets */
	t_u8 tsf[8];
	/** Beacon Interval  2 octets */
	t_u16 beacon_interval;
	/** Capability Info  2 octets */
	IEEEtypes_CapInfo_t cap_info;

	/**
	 *  IEEE Infomation Elements; variable number & length per 802.11 spec
	 */
	/** SSID */
	char ssid[MRVDRV_MAX_SSID_LENGTH + 1];
	/** SSID Length */
	t_u32 ssid_len;
	/** WMM Capability */
	char wmm_cap;
	/** WPS Capability */
	char wps_cap;
	/** Privacy Capability - WEP/WPA/RSN */
	char priv_cap;
	/** HT (11N) Capability */
	char ht_cap;
	/** VHT (11AC) Capability */
	char vht_cap[2];
	/** HE (11AX) Capability */
	char he_cap[2];
	/* 802.11k Capability */
	char dot11k_cap;
	/** 802.11r Capability */
	char dot11r_cap;
} wlan_ioctl_get_bss_info;

/**
 *  Structure to save of scan table info for each BSS returned
 * in WLAN_GET_SCAN_RESP IOCTL
 */
struct wlan_ioctl_get_scan_list {
	/** fixed info */
	wlan_ioctl_get_scan_table_entry fixed_buf;
	/** variable info - BSS info (probe resp or beacon) & IEEE IE info */
	wlan_ioctl_get_bss_info bss_info_buf;
	/** pointer to next node in list */
	struct wlan_ioctl_get_scan_list *next;
	/** pointer to previous node in list */
	struct wlan_ioctl_get_scan_list *prev;
};

/**
 *  Sructure to retrieve the scan table
 */
typedef struct {
	/**
	 *  - Zero based scan entry to start retrieval in command request
	 *  - Number of scans entries returned in command response
	 */
	t_u32 scan_number;
	/**
	 * Buffer marker for multiple wlan_ioctl_get_scan_table_entry
	 * structures. Each struct is padded to the nearest 32 bit boundary.
	 */
	t_u8 scan_table_entry_buf[1];

} wlan_ioctl_get_scan_table_info;

enum ieee80211_channel_flags {
	IEEE80211_CHAN_DISABLED = 1 << 0,
	IEEE80211_CHAN_NO_IR = 1 << 1,
	/* hole at 1<<2 */
	IEEE80211_CHAN_RADAR = 1 << 3,
	IEEE80211_CHAN_NO_HT40PLUS = 1 << 4,
	IEEE80211_CHAN_NO_HT40MINUS = 1 << 5,
	IEEE80211_CHAN_NO_OFDM = 1 << 6,
	IEEE80211_CHAN_NO_80MHZ = 1 << 7,
	IEEE80211_CHAN_NO_160MHZ = 1 << 8,
	IEEE80211_CHAN_INDOOR_ONLY = 1 << 9,
	IEEE80211_CHAN_IR_CONCURRENT = 1 << 10,
	IEEE80211_CHAN_NO_20MHZ = 1 << 11,
	IEEE80211_CHAN_NO_10MHZ = 1 << 12,
};

typedef struct {
	/** center freq */
	t_u16 center_freq;
	/** chan num */
	t_u16 hw_value;
	/** chan flags */
	t_u32 flags;
	/** max power */
	int max_power;
	/** dfs state */
	t_u8 dfs_state;
} __ATTRIB_PACK__ wlan_ieee80211_chan;

typedef struct {
	/** num of chan */
	t_u8 num_chan;
	/** chan_list */
	wlan_ieee80211_chan chan_list[];
} __ATTRIB_PACK__ wlan_ieee80211_chan_list;

typedef struct {
	t_u8 chan_number; /**< Channel Number to scan */
	t_u8 radio_type; /**< Radio type: 'B/G' Band = 0, 'A' Band = 1 */
	t_u8 scan_type; /**< Scan type: Active = 1, Passive = 2 */
	t_u8 reserved; /**< Reserved */
	t_u32 scan_time; /**< Scan duration in milliseconds; if 0 default used
			  */
} __ATTRIB_PACK__ wlan_ioctl_user_scan_chan;

typedef struct {
	char ssid[MRVDRV_MAX_SSID_LENGTH + 1]; /**< SSID */
	t_u8 max_len; /**< Maximum length of SSID */
} __ATTRIB_PACK__ wlan_ioctl_user_scan_ssid;

typedef struct {
	/** Flag set to keep the previous scan table intact */
	t_u8 keep_previous_scan; /* Do not erase the existing scan results */

	/** BSS mode to be sent in the firmware command */
	t_u8 bss_mode;

	/** Configure the number of probe requests for active chan scans */
	t_u8 num_probes;

	/** Reserved */
	t_u8 reserved;

	/** BSSID filter sent in the firmware command to limit the results */
	t_u8 specific_bssid[ETH_ALEN];

	/** SSID filter list used in the to limit the scan results */
	wlan_ioctl_user_scan_ssid ssid_list[MRVDRV_MAX_SSID_LIST_LENGTH];

	/** Variable number (fixed maximum) of channels to scan up */
	wlan_ioctl_user_scan_chan chan_list[WLAN_IOCTL_USER_SCAN_CHAN_MAX];

	/** Gap between two scans */
	t_u16 scan_chan_gap;
	/** scan type: 0 legacy, 1: enhance scan*/
	t_u8 ext_scan_type;
	/** flag to filer only probe response */
	t_u8 proberesp_only;
	t_u8 random_mac[MLAN_MAC_ADDR_LENGTH];
	/** Number of BSSIDs in bssid_list*/
	t_u8 bssid_num;
	/** List of BSSIDs to be filterd*/
	wlan_ioctl_user_scan_bssid bssid_list[MAX_BSSID_FILTER_LIST];
	/** use scan setting from scan_cfg only  */
	t_u8 scan_cfg_only;
} __ATTRIB_PACK__ wlan_ioctl_user_scan_cfg;

/** Max Ie length */
#define MAX_IE_SIZE 256

/** custom IE */
typedef struct _custom_ie {
	/** IE Index */
	t_u16 ie_index;
	/** Mgmt Subtype Mask */
	t_u16 mgmt_subtype_mask;
	/** IE Length */
	t_u16 ie_length;
	/** IE buffer */
	t_u8 ie_buffer[];
} __ATTRIB_PACK__ custom_ie;

/**
 * Hex or Decimal to Integer
 * @param   num string to convert into decimal or hex
 */
#define A2HEXDECIMAL(num)                                                      \
	(strncasecmp("0x", (num), 2) ? (unsigned int)strtoll((num), NULL, 0) : \
				       a2hex((num)))

/** Convert TLV header from little endian format to CPU format */
#define endian_convert_tlv_header_in(x)                                        \
	{                                                                      \
		(x)->tag = le16_to_cpu((x)->tag);                              \
		(x)->length = le16_to_cpu((x)->length);                        \
	}

/** Convert TLV header to little endian format from CPU format */
#define endian_convert_tlv_header_out(x)                                       \
	{                                                                      \
		(x)->tag = cpu_to_le16((x)->tag);                              \
		(x)->length = cpu_to_le16((x)->length);                        \
	}
/** Max IE index to FW */
#define MAX_MGMT_IE_INDEX_TO_FW 4
/** Max IE index per BSS */
#define MAX_MGMT_IE_INDEX 16

/** Private command ID to pass custom IE list */
#define CUSTOM_IE_CFG (SIOCDEVPRIVATE + 13)
/* TLV Definitions */

/** Maximum IE buffer length */
#define MAX_IE_BUFFER_LEN 256

/** TLV: Management IE list */
#define MRVL_MGMT_IE_LIST_TLV_ID (PROPRIETARY_TLV_BASE_ID + 0x69) /* 0x0169 */

/** TLV: Max Management IE */
#define MRVL_MAX_MGMT_IE_TLV_ID (PROPRIETARY_TLV_BASE_ID + 0xaa) /* 0x01aa */

/** custom IE info */
typedef struct _custom_ie_info {
	/** size of buffer */
	t_u16 buf_size;
	/** no of buffers of buf_size */
	t_u16 buf_count;
} __ATTRIB_PACK__ custom_ie_info;

/** TLV buffer : Max Mgmt IE */
typedef struct _tlvbuf_max_mgmt_ie {
	/** Type */
	t_u16 type;
	/** Length */
	t_u16 len;
	/** No of tuples */
	t_u16 count;
	/** custom IE info tuples */
	custom_ie_info info[];
} __ATTRIB_PACK__ tlvbuf_max_mgmt_ie;

/** TLV buffer : custom IE */
typedef struct _eth_priv_ds_misc_custom_ie {
	/** Type */
	t_u16 type;
	/** Length */
	t_u16 len;
	/** IE data */
	custom_ie ie_data[];
} __ATTRIB_PACK__ eth_priv_ds_misc_custom_ie;

/*-----------------------------------------------------------------*/
/** Register Memory Access Group */
/*-----------------------------------------------------------------*/
/** Enumeration for register type */
enum _mlan_reg_type {
	MLAN_REG_MAC = 1,
	MLAN_REG_BBP,
	MLAN_REG_RF,
	MLAN_REG_CAU = 5,
};

/** Type definition of mlan_ds_reg_rw for MLAN_OID_REG_RW */
struct eth_priv_ds_reg_rw {
	/** Register type */
	t_u32 type;
	/** Offset */
	t_u32 offset;
	/** Value */
	t_u32 value;
};

/** Maximum EEPROM data */
#define MAX_EEPROM_DATA 256

/** Type definition of mlan_ds_read_eeprom for MLAN_OID_EEPROM_RD */
struct eth_priv_ds_read_eeprom {
	/** Multiples of 4 */
	t_u16 offset;
	/** Number of bytes */
	t_u16 byte_count;
	/** Value */
	t_u8 value[MAX_EEPROM_DATA];
};

/** Type definition of mlan_ds_mem_rw for MLAN_OID_MEM_RW */
struct eth_priv_ds_mem_rw {
	/** Address */
	t_u32 addr;
	/** Value */
	t_u32 value;
};

/** Type definition of mlan_ds_reg_mem for MLAN_IOCTL_REG_MEM */
struct eth_priv_ds_reg_mem {
	/** Sub-command */
	t_u32 sub_command;
	/** Register memory access parameter */
	union {
		/** Register access for MLAN_OID_REG_RW */
		struct eth_priv_ds_reg_rw reg_rw;
		/** EEPROM access for MLAN_OID_EEPROM_RD */
		struct eth_priv_ds_read_eeprom rd_eeprom;
		/** Memory access for MLAN_OID_MEM_RW */
		struct eth_priv_ds_mem_rw mem_rw;
	} param;
};

/** Data structure of WMM QoS information */
typedef struct _IEEEtypes_WmmQosInfo_t {
#ifdef BIG_ENDIAN_SUPPORT
	/** QoS UAPSD */
	t_u8 qos_uapsd : 1;
	/** Reserved */
	t_u8 reserved : 3;
	/** Parameter set count */
	t_u8 para_set_count : 4;
#else
	/** Parameter set count */
	t_u8 para_set_count : 4;
	/** Reserved */
	t_u8 reserved : 3;
	/** QoS UAPSD */
	t_u8 qos_uapsd : 1;
#endif /* BIG_ENDIAN_SUPPORT */
} __ATTRIB_PACK__ IEEEtypes_WmmQosInfo_t;

/** Max channel value for BG band */
#define MAX_BG_CHANNEL 14
/** band BG */
#define BAND_BG_TDLS 0
/** band A */
#define BAND_A_TDLS 1
/** NO PERIODIC SWITCH */
#define NO_PERIODIC_SWITCH 0
/** Enable periodic channel switch */
#define ENABLE_PERIODIC_SWITCH 1
/** Min channel value for BG band */
#define MIN_BG_CHANNEL 1
/** Max channel value for A band */
#define MIN_A_CHANNEL 36
/** Max channel value for A band */
#define MAX_A_CHANNEL 252

/** Host Command ioctl number */
#define TDLS_IOCTL (SIOCDEVPRIVATE + 5)
/** TDLS action definitions */
/** Action ID for TDLS config */
#define ACTION_TDLS_CONFIG 0x0000
/** Action ID for TDLS setinfo request */
#define ACTION_TDLS_SETINFO 0x0001
/** Action ID for TDLS Discovery request */
#define ACTION_TDLS_DISCOVERY 0x0002
/** Action ID for TDLS setup request */
#define ACTION_TDLS_SETUP 0x0003
/** Action ID for TDLS Teardown request */
#define ACTION_TDLS_TEARDOWN 0x0004
/** Action ID for TDLS power mode */
#define ACTION_TDLS_POWER_MODE 0x0005
/**Action ID for init TDLS Channel Switch*/
#define ACTION_TDLS_INIT_CHAN_SWITCH 0x06
/** Action ID for stop TDLS Channel Switch */
#define ACTION_TDLS_STOP_CHAN_SWITCH 0x07
/** Action ID for configure CS related parameters */
#define ACTION_TDLS_CS_PARAMS 0x08
/** Action ID for TDLS Disable Channel switch */
#define ACTION_TDLS_CS_DISABLE 0x09
/** Action ID for TDLS Link status */
#define ACTION_TDLS_LINK_STATUS 0x000A
/** Action ID for Host TDLS config uapsd and CS */
#define ACTION_HOST_TDLS_CONFIG 0x000D
/** Action ID for TDLS CS immediate return */
#define ACTION_TDLS_DEBUG_CS_RET_IM 0xFFF7
/** Action ID for TDLS Stop RX */
#define ACTION_TDLS_DEBUG_STOP_RX 0xFFF8
/** Action ID for TDLS Allow weak security */
#define ACTION_TDLS_DEBUG_ALLOW_WEAK_SECURITY 0xFFF9
/** Action ID for TDLS Ignore key lifetime expiry */
#define ACTION_TDLS_DEBUG_IGNORE_KEY_EXPIRY 0xFFFA
/** Action ID for TDLS Higher/Lower mac Test */
#define ACTION_TDLS_DEBUG_HIGHER_LOWER_MAC 0xFFFB
/** Action ID for TDLS Prohibited Test */
#define ACTION_TDLS_DEBUG_SETUP_PROHIBITED 0xFFFC
/** Action ID for TDLS Existing link Test */
#define ACTION_TDLS_DEBUG_SETUP_SAME_LINK 0xFFFD
/** Action ID for TDLS Fail Setup Confirm */
#define ACTION_TDLS_DEBUG_FAIL_SETUP_CONFIRM 0xFFFE
/** Action ID for TDLS WRONG BSS Test */
#define ACTION_TDLS_DEBUG_WRONG_BSS 0xFFFF

/** TLV type : Rates */
#define TLV_TYPE_RATES 0x0001
/** TLV type : Domain */
#define TLV_TYPE_DOMAIN 0x0007
/** TLV type : Supported channels */
#define TLV_TYPE_SUPPORTED_CHANNELS 0x0024
/** TLV type : HT Capabilities */
#define TLV_TYPE_HT_CAP 0x002d
/** TLV type : Qos Info */
#define TLV_TYPE_QOSINFO 0x002e
/** TLV type : RSN IE */
#define TLV_TYPE_RSN_IE 0x0030
/** TLV type : extended supported rates */
#define TLV_EXTENDED_SUPPORTED_RATES 0x0032
/** TLV type :  timeout interval    */
#define TLV_TIMEOUT_INTERVAL 0x0038
/** TLV type : Regulatory classes */
#define TLV_TYPE_REGULATORY_CLASSES 0x003b
/** TLV type : HT Information */
#define TLV_TYPE_HT_INFO 0x003d
/** TLV type : 20/40 BSS Coexistence */
#define TLV_TYPE_2040BSS_COEXISTENCE 0x0048
/** TLv Type : Link identifier */
#define TLV_LINK_IDENTIFIER 0x0065

/** Country code length */
#define COUNTRY_CODE_LEN 3
/** Length of Group Cipher Suite */
#define GROUP_CIPHER_SUITE_LEN 4
/** Length of Pairwise Cipher Suite */
#define PAIRWISE_CIPHER_SUITE_LEN 4
/** Length of AKM Suite */
#define AKM_SUITE_LEN 4
/** PMKID length */
#define PMKID_LEN 16
/** Maximum number of pairwise_cipher_suite */
#define MAX_PAIRWISE_CIPHER_SUITE_COUNT 2
/** Maximum number of AKM suite */
#define MAX_AKM_SUITE_COUNT 2
/** Maximum number of PMKID list count */
#define MAX_PMKID_COUNT 2
/** Length of MCS set */
#define MCS_SET_LEN 16
/** Version in RSN IE */
#define VERSION_RSN_IE 0x0001

/** tdls setinfo */
typedef struct _tdls_setinfo {
	/** Action */
	t_u16 action;
	/** (TLV + capInfo) length */
	t_u16 tlv_len;
	/** Capability Info */
	t_u16 cap_info;
	/** tdls info */
	t_u8 tlv_buffer[];
} __ATTRIB_PACK__ tdls_setinfo;

/** tdls discovery */
typedef struct _tdls_discovery {
	/** Action */
	t_u16 action;
	/** Peer MAC address */
	t_u8 peer_mac[ETH_ALEN];
} __ATTRIB_PACK__ tdls_discovery;

/** tdls link status */
typedef struct _tdls_links_status {
	/** Action */
	t_u16 action;
} __ATTRIB_PACK__ tdls_link_status;

/** tdls discovery response */
typedef struct _tdls_discovery_resp {
	/** Action */
	t_u16 action;
	/** payload length */
	t_u16 payload_len;
	/** peer mac Address */
	t_u8 peer_mac[ETH_ALEN];
	/** RSSI */
	t_s8 rssi;
	/** Cap Info */
	t_u16 cap_info;
	/** TLV buffer */
	t_u8 tlv_buffer[];
} __ATTRIB_PACK__ tdls_discovery_resp;

/** tdls each link rate information */
typedef struct _tdls_link_rate_info {
	/** Tx Data Rate */
	t_u8 tx_data_rate;
	/** Tx Rate HT info*/
	t_u8 tx_rate_htinfo;
} __ATTRIB_PACK__ tdls_link_rate_info;

/** tdls each link status */
typedef struct _tdls_each_link_status {
	/** peer mac Address */
	t_u8 peer_mac[ETH_ALEN];
	/** Link Flags */
	t_u8 link_flags;
	/** Traffic Status */
	t_u8 traffic_status;
	/** Tx Failure Count */
	t_u8 tx_fail_count;
	/** Channel Number */
	t_u32 active_channel;
	/** Last Data RSSI in dBm */
	t_s16 data_rssi_last;
	/** Last Data NF in dBm */
	t_s16 data_nf_last;
	/** AVG DATA RSSI in dBm */
	t_s16 data_rssi_avg;
	/** AVG DATA NF in dBm */
	t_s16 data_nf_avg;
	union {
		/** tdls rate info */
		tdls_link_rate_info rate_info;
		/** final rate value*/
		t_u16 final_data_rate;
	} u;
	/** Security Method */
	t_u8 security_method;
	/** Key Lifetime */
	t_u32 key_lifetime;
	/** Key Length */
	t_u8 key_length;
	/** actual key */
	t_u8 key[];
} __ATTRIB_PACK__ tdls_each_link_status;

/** tdls link status response */
typedef struct _tdls_link_status_resp {
	/** Action */
	t_u16 action;
	/** payload length */
	t_u16 payload_len;
	/** number of links */
	t_u8 active_links;
	/** structure for link status */
	tdls_each_link_status link_stats[1];
} __ATTRIB_PACK__ tdls_link_status_resp;

/** tdls setup */
typedef struct _tdls_setup {
	/** Action */
	t_u16 action;
	/** Peer MAC address */
	t_u8 peer_mac[ETH_ALEN];
	/** Time to wait for response from peer*/
	t_u32 wait_time;
	/** Key Life Time */
	t_u32 key_life_time;
} __ATTRIB_PACK__ tdls_setup;

/** tdls tear down */
typedef struct _tdls_tear_down {
	/** Action */
	t_u16 action;
	/** Peer MAC address */
	t_u8 peer_mac[ETH_ALEN];
	/** Reason code */
	t_u16 reason_code;
} __ATTRIB_PACK__ tdls_teardown;

/** tdls power mode */
typedef struct _tdls_power_mode {
	/** Action */
	t_u16 action;
	/** Peer MAC address */
	t_u8 peer_mac[ETH_ALEN];
	/** Power mode */
	t_u16 power_mode;
} __ATTRIB_PACK__ tdls_powermode;

/** tdls channel switch info */
typedef struct _tdls_channel_switch {
	/** Action */
	t_u16 action;
	/** peer mac Address */
	t_u8 peer_mac[ETH_ALEN];
	/** Channel Switch primary channel no */
	t_u8 primary_channel;
	/** Channel Switch secondary channel offset */
	t_u8 secondary_channel_offset;
	/** Channel Switch Band */
	t_u8 band;
	/** Channel Switch time*/
	t_u16 switch_time;
	/** Channel Switch timeout*/
	t_u16 switch_timeout;
	/** Channel Regulatory class*/
	t_u8 regulatory_class;
	/** Channel Switch periodicity*/
	t_u8 periodicity;
} __ATTRIB_PACK__ tdls_channel_switch;

/** tdls stop channel switch */
typedef struct _tdls_stop_chan_switch {
	/** Action */
	t_u16 action;
	/** Peer MAC address */
	t_u8 peer_mac[ETH_ALEN];
} __ATTRIB_PACK__ tdls_stop_chan_switch;

/** tdls disable channel switch */
typedef struct _tdls_disable_cs {
	/** Action */
	t_u16 action;
	/** Data*/
	t_u16 data;
} __ATTRIB_PACK__ tdls_disable_cs, tdls_config;

/** tdls channel switch parameters */
typedef struct _tdls_cs_params {
	/** Action */
	t_u16 action;
	/** unit time, multiples of 10ms */
	t_u8 unit_time;
	/** threshold for other link */
	t_u8 threshold_otherlink;
	/** threshold for direct link */
	t_u8 threshold_directlink;
} __ATTRIB_PACK__ tdls_cs_params;

/** Host tdls config */
typedef struct _host_tdls_cfg {
	/** Action */
	t_u16 action;
	/** support uapsd */
	t_u8 uapsd_support;
	/** channel_switch */
	t_u8 cs_support;
	/** TLV  length */
	t_u16 tlv_len;
	/** tdls info */
	t_u8 tlv_buffer[];
} __ATTRIB_PACK__ host_tdls_cfg;

/** tdls debug */
typedef struct _tdls_debug {
	/** Action */
	t_u16 action;
	/** Data */
	t_u8 data[];
} __ATTRIB_PACK__ tdls_debug;

/** TLV header */
#define TLVHEADER /** Tag */                                                   \
	t_u16 tag;                                                             \
	/** Length */                                                          \
	t_u16 length

/** Length of TLV header */
#define TLVHEADER_LEN 4

/** Data structure for subband set */
typedef struct _IEEEtypes_SubbandSet_t {
	/** First channel */
	t_u8 first_chan;
	/** Number of channels */
	t_u8 no_of_chan;
	/** Maximum Tx power */
	t_u8 max_tx_pwr;
} __ATTRIB_PACK__ IEEEtypes_SubbandSet_t, *pIEEEtypes_SubbandSet_t;

/** tlvbuf_DomainParamSet_t */
typedef struct _tlvbuf_DomainParamSet {
	/** Header */
	TLVHEADER;
	/** Country code */
	t_u8 country_code[COUNTRY_CODE_LEN];
	/** Set of subbands */
	IEEEtypes_SubbandSet_t sub_band[];
} __ATTRIB_PACK__ tlvbuf_DomainParamSet_t;

/** Qos Info TLV */
typedef struct _tlvbuf_QosInfo_t {
	/** Header */
	TLVHEADER;
	/** QosInfo */
	union {
		/** QosInfo bitfield */
		IEEEtypes_WmmQosInfo_t qos_info;
		/** QosInfo byte */
		t_u8 qos_info_byte;
	} u;
} __ATTRIB_PACK__ tlvbuf_QosInfo_t;

/** HT Capabilities Data */
typedef struct _HTCap_t {
	/** HT Capabilities Info field */
	t_u16 ht_cap_info;
	/** A-MPDU Parameters field */
	t_u8 ampdu_param;
	/** Supported MCS Set field */
	t_u8 supported_mcs_set[16];
	/** HT Extended Capabilities field */
	t_u16 ht_ext_cap;
	/** Transmit Beamforming Capabilities field */
	t_u32 tx_bf_cap;
	/** Antenna Selection Capability field */
	t_u8 asel;
} __ATTRIB_PACK__ HTCap_t, *pHTCap_t;

/** HT Information Data */
typedef struct _HTInfo_t {
	/** Primary channel */
	t_u8 pri_chan;
	/** Field 2 */
	t_u8 field2;
	/** Field 3 */
	t_u16 field3;
	/** Field 4 */
	t_u16 field4;
	/** Bitmap indicating MCSs supported by all HT STAs in the BSS */
	t_u8 basic_mcs_set[16];
} __ATTRIB_PACK__ HTInfo_t, *pHTInfo_t;

/** 20/40 BSS Coexistence Data */
typedef struct _BSSCo2040_t {
	/** 20/40 BSS Coexistence value */
	t_u8 bss_co_2040_value;
} __ATTRIB_PACK__ BSSCo2040_t, *pBSSCo2040_t;

/** HT Capabilities element */
typedef struct _tlvbuf_HTCap_t {
	/** Header */
	TLVHEADER;
	/** HTCap struct */
	HTCap_t ht_cap;
} __ATTRIB_PACK__ tlvbuf_HTCap_t;

/** HT Information element */
typedef struct _tlvbuf_HTInfo_t {
	/** Header */
	TLVHEADER;

	/** HTInfo struct */
	HTInfo_t ht_info;
} __ATTRIB_PACK__ tlvbuf_HTInfo_t;

/** 20/40 BSS Coexistence element */
typedef struct _tlvbuf_2040BSSCo_t {
	/** Header */
	TLVHEADER;

	/** BSSCo2040_t struct */
	BSSCo2040_t bss_co_2040;
} __ATTRIB_PACK__ tlvbuf_2040BSSCo_t;

/** Extended Capabilities element */
typedef struct _tlvbuf_ExtCap_t {
	/** Header */
	TLVHEADER;
	/** ExtCap_t struct */
	t_u8 ext_cap[];
} __ATTRIB_PACK__ tlvbuf_ExtCap_t;

/** tlvbuf_RatesParamSet_t */
typedef struct _tlvbuf_RatesParamSet_t {
	/** Header */
	TLVHEADER;
	/** Rates */
	t_u8 rates[];
} __ATTRIB_PACK__ tlvbuf_RatesParamSet_t;

/*  IEEE Supported Channel sub-band description (7.3.2.19) */
/*  Sub-band description used in the supported channels element. */
typedef struct _IEEEtypes_SupportChan_Subband_t {
	t_u8 start_chan; /**< Starting channel in the subband */
	t_u8 num_chans; /**< Number of channels in the subband */

} __ATTRIB_PACK__ IEEEtypes_SupportChan_Subband_t;

/*  IEEE Supported Channel element (7.3.2.19) */
/**
 *  Sent in association requests. Details the sub-bands and number
 *    of channels supported in each subband
 */
typedef struct _tlvbuf_SupportedChannels_t {
	/** Header */
	TLVHEADER; /**< IEEE Element ID = 36 */
	/** Configured sub-bands information in the element */
	IEEEtypes_SupportChan_Subband_t subband[];
} __ATTRIB_PACK__ tlvbuf_SupportedChannels_t;

#define VHT_MCS_MAP_LEN 2
/** VHT MCS rate set field, refer to 802.11ac */
typedef struct _VHT_MCS_set {
	t_u16 rx_mcs_map;
	t_u16 rx_max_rate; /* bit 29-31 reserved */
	t_u16 tx_mcs_map;
	t_u16 tx_max_rate; /* bit 61-63 reserved */
} __ATTRIB_PACK__ VHT_MCS_set_t, *pVHT_MCS_set_t;

typedef struct _VHT_capa {
	t_u32 vht_cap_info;
	VHT_MCS_set_t mcs_sets;
} __ATTRIB_PACK__ VHT_capa_t, *pVHT_capa_t;

/** VHT Capabilities IE */
typedef struct _IEEEtypes_VHTCap_t {
	/** Header */
	TLVHEADER;
	/** VHTInfo struct */
	VHT_capa_t vht_cap;
} __ATTRIB_PACK__ tlvbuf_VHTCap_t, *ptlvbuf_VHTCap_t;

/** VHT Operations IE */
typedef struct _IEEEtypes_VHTOprat_t {
	/** Header */
	TLVHEADER;
	/** VHTOpra struct */
	t_u8 chan_width;
	t_u8 chan_cf1;
	t_u8 chan_cf2;
	/** Basic MCS set map, each 2 bits stands for a Nss */
	t_u8 basic_mcs_map[VHT_MCS_MAP_LEN];
} __ATTRIB_PACK__ tlvbuf_VHTOpra_t, *ptlvbuf_VHTOpra_t;

/*  IEEE Supported Regulatory Classes description (7.3.2.54) */
typedef struct _IEEEtypes_RegulatoryClass_t {
	/** current regulatory class */
	t_u8 cur_regulatory_class;
	/** supported regulatory class list */
	t_u8 regulatory_classes_list[];

} __ATTRIB_PACK__ IEEEtypes_RegulatoryClass_t;

/*  IEEE Supported Regulatory Classes TLV (7.3.2.54) */
typedef struct _tlvbuf_RegulatoryClass_t {
	/** Header */
	TLVHEADER; /**< IEEE Element ID = 59 */
	/** supported regulatory class */
	IEEEtypes_RegulatoryClass_t regulatory_class;
} __ATTRIB_PACK__ tlvbuf_RegulatoryClass_t;

/** tlvbuf_RsnParamSet_t */
typedef struct _tlvbuf_RsnParamSet_t {
	/** Header */
	TLVHEADER;
	/** Version */
	t_u16 version;
	/** Group Cipher Suite */
	t_u8 group_cipher_suite[4];
	/** Pairwise Cipher Suite count */
	t_u16 pairwise_cipher_count;
	/** Pairwise Cipher Suite */
	t_u8 pairwise_cipher_suite[];
	/* below is only to keep a reference
	 * of the remaining elements in the
	 * structure */
#if 0
    /** AKM Suite Sount */
    t_u16 akm_suite_count;
    /** AKM Suite */
    t_u8 akm_suite[0];
    /** RSN Capability */
    t_u16 rsn_capability;
    /** PMKID Count */
    t_u16 pmkid_count;
    /** PMKID list */
    t_u8 pmkid_list[];
#endif
} __ATTRIB_PACK__ tlvbuf_RsnParamSet_t;

/** Size of a TSPEC.  Used to allocate necessary buffer space in commands */
#define WMM_TSPEC_SIZE 63

/** Maximum number of User Priorities */
#define MAX_USER_PRIORITIES 8

/** Extra IE bytes allocated in messages for appended IEs after a TSPEC */
#define WMM_ADDTS_EXTRA_IE_BYTES 256

/**
 *  @brief Enumeration for the command result from an ADDTS or DELTS command
 */
typedef enum {
	TSPEC_RESULT_SUCCESS = 0,
	TSPEC_RESULT_EXEC_FAILURE = 1,
	TSPEC_RESULT_TIMEOUT = 2,
	TSPEC_RESULT_DATA_INVALID = 3,
} __ATTRIB_PACK__ mlan_wmm_tspec_result_e;

/**
 *  @brief Enumeration for the action field in the Queue configure command
 */
typedef enum {
	WMM_QUEUE_CONFIG_ACTION_GET = 0,
	WMM_QUEUE_CONFIG_ACTION_SET = 1,
	WMM_QUEUE_CONFIG_ACTION_DEFAULT = 2,

	WMM_QUEUE_CONFIG_ACTION_MAX
} __ATTRIB_PACK__ mlan_wmm_queue_config_action_e;

/**
 *   @brief Enumeration for the action field in the queue stats command
 */
typedef enum {
	WMM_STATS_ACTION_START = 0,
	WMM_STATS_ACTION_STOP = 1,
	WMM_STATS_ACTION_GET_CLR = 2,
	WMM_STATS_ACTION_SET_CFG = 3, /* Not currently used */
	WMM_STATS_ACTION_GET_CFG = 4, /* Not currently used */

	WMM_STATS_ACTION_MAX

} __ATTRIB_PACK__ mlan_wmm_stats_action_e;

/** Data structure of WMM Info IE  */
typedef struct _IEEEtypes_WmmInfo_t {
	/**
	 * WMM Info IE - Vendor Specific Header:
	 *   element_id  [221/0xdd]
	 *   Len         [7]
	 *   Oui         [00:50:f2]
	 *   OuiType     [2]
	 *   OuiSubType  [0]
	 *   Version     [1]
	 */
	IEEEtypes_VendorHeader_t vend_hdr;

	/** QoS information */
	IEEEtypes_WmmQosInfo_t qos_info;

} __ATTRIB_PACK__ IEEEtypes_WmmInfo_t, *pIEEEtypes_WmmInfo_t;

/** Data structure of WMM parameter IE  */
typedef struct _IEEEtypes_WmmParameter_t {
	/**
	 * WMM Parameter IE - Vendor Specific Header:
	 *   element_id  [221/0xdd]
	 *   Len         [24]
	 *   Oui         [00:50:f2]
	 *   OuiType     [2]
	 *   OuiSubType  [1]
	 *   Version     [1]
	 */
	IEEEtypes_VendorHeader_t vend_hdr;

	/** QoS information */
	IEEEtypes_WmmQosInfo_t qos_info;
	/** Reserved */
	t_u8 reserved;

	/** AC Parameters Record WMM_AC_BE, WMM_AC_BK, WMM_AC_VI, WMM_AC_VO */
	IEEEtypes_WmmAcParameters_t ac_params[MAX_AC_QUEUES];

} __ATTRIB_PACK__ IEEEtypes_WmmParameter_t, *pIEEEtypes_WmmParameter_t;

/**
 *  @brief IOCTL structure to send an ADDTS request and retrieve the response.
 *
 *  IOCTL structure from the application layer relayed to firmware to
 *    instigate an ADDTS management frame with an appropriate TSPEC IE as well
 *    as any additional IEs appended in the ADDTS Action frame.
 *
 *  @sa wlan_wmm_addts_req_ioctl
 */
typedef struct {
	mlan_wmm_tspec_result_e commandResult; /**< Firmware execution result */

	t_u32 timeout_ms; /**< Timeout value in milliseconds */
	t_u8 ieeeStatusCode; /**< IEEE status code */

	t_u32 ieDataLen;
	t_u8 ieData[WMM_TSPEC_SIZE /**< TSPEC to send in the ADDTS */
		    + WMM_ADDTS_EXTRA_IE_BYTES]; /**< ADDTS extra IE buf */
} wlan_ioctl_wmm_addts_req_t;

/**
 *  @brief IOCTL structure to send a DELTS request.
 *
 *  IOCTL structure from the application layer relayed to firmware to
 *    instigate an DELTS management frame with an appropriate TSPEC IE.
 *
 *  @sa wlan_wmm_delts_req_ioctl
 */
typedef struct {
	mlan_wmm_tspec_result_e commandResult; /**< Firmware execution result */
	t_u8 ieeeReasonCode; /**< IEEE reason code sent, unused for WMM */

	t_u32 ieDataLen;
	t_u8 ieData[WMM_TSPEC_SIZE]; /**< TSPEC to send in the DELTS */
} wlan_ioctl_wmm_delts_req_t;

/**
 *  @brief IOCTL structure to configure a specific AC Queue's parameters
 *
 *  IOCTL structure from the application layer relayed to firmware to
 *    get, set, or default the WMM AC queue parameters.
 *
 *  - msduLifetimeExpiry is ignored if set to 0 on a set command
 *
 *  @sa wlan_wmm_queue_config_ioctl
 */
typedef struct {
	mlan_wmm_queue_config_action_e action; /**< Set, Get, or Default */
	mlan_wmm_ac_e accessCategory; /**< WMM_AC_BK(0) to WMM_AC_VO(3) */
	t_u16 msduLifetimeExpiry; /**< lifetime expiry in TUs */
	t_u8 supportedRates[10]; /**< Not supported yet */
} wlan_ioctl_wmm_queue_config_t;

/** Number of bins in the histogram for the HostCmd_DS_WMM_QUEUE_STATS */
#define WMM_STATS_PKTS_HIST_BINS 7

/**
 *  @brief IOCTL structure to start, stop, and get statistics for a WMM AC
 *
 *  IOCTL structure from the application layer relayed to firmware to
 *    start or stop statistical collection for a given AC.  Also used to
 *    retrieve and clear the collected stats on a given AC.
 *
 *  @sa wlan_wmm_queue_stats_ioctl
 */
typedef struct {
	mlan_wmm_stats_action_e action; /**< Start, Stop, or Get  */
	t_u8 userPriority; /**< User Priority (0 to 7) */
	t_u16 pktCount; /**< Number of successful packets transmitted */
	t_u16 pktLoss; /**< Packets lost; not included in pktCount   */
	t_u32 avgQueueDelay; /**< Average Queue delay in microseconds */
	t_u32 avgTxDelay; /**< Average Transmission delay in microseconds */
	t_u16 usedTime; /**< Calculated used time - units of 32 microsec */
	t_u16 policedTime; /**< Calculated policed time - units of 32 microsec
			    */

	/** @brief Queue Delay Histogram; number of packets per queue delay
	 * range
	 *
	 *  [0] -  0ms <= delay < 5ms
	 *  [1] -  5ms <= delay < 10ms
	 *  [2] - 10ms <= delay < 20ms
	 *  [3] - 20ms <= delay < 30ms
	 *  [4] - 30ms <= delay < 40ms
	 *  [5] - 40ms <= delay < 50ms
	 *  [6] - 50ms <= delay < msduLifetime (TUs)
	 */
	t_u16 delayHistogram[WMM_STATS_PKTS_HIST_BINS];
} wlan_ioctl_wmm_queue_stats_t;

/**
 *  @brief IOCTL and command sub structure for a Traffic stream status.
 */
typedef struct {
	t_u8 tid; /**< TSID: Range: 0->7 */
	t_u8 valid; /**< TSID specified is valid  */
	t_u8 accessCategory; /**< AC TSID is active on */
	t_u8 userPriority; /**< UP specified for the TSID */

	t_u8 psb; /**< Power save mode for TSID: 0 (legacy), 1 (UAPSD) */
	t_u8 flowDir; /**< Upstream (0), Downlink(1), Bidirectional(3) */
	t_u16 mediumTime; /**< Medium time granted for the TSID */
} __ATTRIB_PACK__ HostCmd_DS_WMM_TS_STATUS, wlan_ioctl_wmm_ts_status_t,
	wlan_cmd_wmm_ts_status_t;

/**
 *  @brief IOCTL sub structure for a specific WMM AC Status
 */
typedef struct {
	/** WMM Acm */
	t_u8 wmmAcm;
	/** Flow required flag */
	t_u8 flowRequired;
	/** Flow created flag */
	t_u8 flowCreated;
	/** Disabled flag */
	t_u8 disabled;
	/** delivery enabled */
	t_u8 deliveryEnabled;
	/** trigger enabled */
	t_u8 triggerEnabled;
} wlan_ioctl_wmm_queue_status_ac_t;

/**
 *  @brief IOCTL structure to retrieve the WMM AC Queue status
 *
 *  IOCTL structure from the application layer to retrieve:
 *     - ACM bit setting for the AC
 *     - Firmware status (flow required, flow created, flow disabled)
 *
 *  @sa wlan_wmm_queue_status_ioctl
 */
typedef struct {
	/** WMM AC queue status */
	wlan_ioctl_wmm_queue_status_ac_t acStatus[MAX_AC_QUEUES];
} wlan_ioctl_wmm_queue_status_t;
#endif /* STA_SUPPORT */

/** TLV type : Extended capabilities */
#define TLV_TYPE_EXTCAP 0x007f

/** Get the current TSF */
#define HostCmd_CMD_GET_TSF 0x0080

/** scan mode */
enum {
	SCAN_MODE_MANUAL = 0,
	SCAN_MODE_ACS,
	SCAN_MODE_USER,
};

/** Failure */
#define MLAN_EVENT_FAILURE -1

/** Netlink protocol number */
#define NETLINK_NXP (MAX_LINKS - 1)
/** Netlink maximum payload size */
#define NL_MAX_PAYLOAD (1024 * 3)
/** Netlink multicast group number */
#define NL_MULTICAST_GROUP 1
/** Default wait time in seconds for events */
#define UAP_RECV_WAIT_DEFAULT 10
/** Maximum number of devices */
#define MAX_NO_OF_DEVICES 4

/* Event buffer */
typedef struct _evt_buf {
	/** Flag to check if event data is present in the buffer or not  */
	int flag;
	/** Event length */
	int length;
	/** Event data */
	t_u8 buffer[NL_MAX_PAYLOAD];
} __ATTRIB_PACK__ evt_buf;

/** Event header */
typedef struct _event_header {
	/** Event ID */
	t_u32 event_id;
	/** Event data */
	t_u8 event_data[];
} __ATTRIB_PACK__ event_header;

/** Event ID length */
#define EVENT_ID_LEN 4

/** Event definition:  Radar Detected by card */
#define EVENT_CHANNEL_REPORT_RDY 0x00000054

/** Host Command ID : Channel report request */
#define HostCmd_CMD_CHAN_REPORT_REQUEST 0x00dd
/** TLV type : Chan Load */
#define TLV_TYPE_CHANRPT_CHAN_LOAD (PROPRIETARY_TLV_BASE_ID + 0x59) /* 0x0159  \
								     */
/** TLV type : Noise Historgram */
#define TLV_TYPE_CHANRPT_NOISE_HIST                                            \
	(PROPRIETARY_TLV_BASE_ID + 0x5a) /* 0x015a */

typedef struct {
	t_u16 startFreq;
	t_u8 chanWidth;
	t_u8 chanNum;

} __ATTRIB_PACK__ MrvlChannelDesc_t;

typedef struct {
	MrvlChannelDesc_t chanDesc; /**< Channel band, number */
	t_u32 millisecDwellTime; /**< Channel dwell time in milliseconds */
} __ATTRIB_PACK__ HostCmd_DS_CHAN_RPT_REQ;

typedef struct {
	t_u32 commandResult; /**< Rpt request command result (0 == SUCCESS) */
	t_u64 startTsf; /**< TSF Measurement started */
	t_u32 duration; /**< Duration of measurement in microsecs */

	t_u8 tlvBuffer[1]; /**< TLV Buffer */
} __ATTRIB_PACK__ HostCmd_DS_CHAN_RPT_RSP;

typedef struct {
	MrvlIEtypesHeader_t Header; /**< Header */

	t_u8 ccaBusyFraction; /**< Parts per 255 channel was busy */
} __ATTRIB_PACK__ MrvlIEtypes_ChanRptChanLoad_t;

typedef struct {
	MrvlIEtypesHeader_t header; /**< Header */

	t_s16 anpi; /**< ANPI calculated from the histogram */
	/** RPI histogram bins. The number of bins utilized is variable and must
		    be calculated by the header length */
	t_u8 rpiDensities[11];
} __ATTRIB_PACK__ MrvlIEtypes_ChanRptNoiseHist_t;

/** EDMAC control parameters */
#define HostCmd_CMD_ED_CTRL 0x0130
typedef struct _ed_mac_ctrl {
	/** EU adaptivity for 2.4ghz band */
	t_u16 ed_ctrl_2g;
	/** Energy detect threshold offset for 2.4ghz */
	t_s16 ed_offset_2g;
	/** EU adaptivity for 5ghz band */
	t_u16 ed_ctrl_5g;
	/** Energy detect threshold offset for 5ghz */
	t_s16 ed_offset_5g;
} ed_mac_ctrl;

/** Type definition of aggr_ctrl */
typedef struct _aggr_ctrl {
	/** Enable */
	t_u16 enable;
	/** Aggregation alignment */
	t_u16 aggr_align;
	/** Aggregation max packet/size */
	t_u16 aggr_max_size;
	/** Aggregation max packet number */
	t_u16 aggr_max_num;
	/** Aggrgation timeout, in microseconds */
	t_u16 aggr_tmo;
} aggr_ctrl;

/** Type definition of mlan_ds_misc_aggr_ctrl for MLAN_OID_MISC_AGGR_CTRL */
typedef struct _aggr_ctrl_params {
	/** Tx aggregation control */
	aggr_ctrl tx;
} aggr_ctrl_params;

typedef struct _usb_aggr_ctrl {
	/** Enable */
	t_u16 enable;
	/** Aggregation mode */
	t_u16 aggr_mode;
	/** Aggregation alignment */
	t_u16 aggr_align;
	/** Aggregation max packet/size */
	t_u16 aggr_max;
	/** Aggrgation timeout, in microseconds */
	t_u16 aggr_tmo;
} usb_aggr_ctrl;

/** Type definition of mlan_ds_misc_usb_aggr_ctrl for
 * MLAN_OID_MISC_USB_AGGR_CTRL */
typedef struct _aggr_params {
	/** Tx aggregation control */
	usb_aggr_ctrl tx_aggr_ctrl;
	/** Rx deaggregation control */
	usb_aggr_ctrl rx_deaggr_ctrl;
} aggr_params;

/** pkt_header */
typedef struct _pkt_header {
	/** pkt_len */
	t_u32 pkt_len;
	/** pkt_type */
	t_u32 TxPktType;
	/** tx control */
	t_u32 TxControl;
} pkt_header;

/** eth_priv_802_11_header packet from FW with length */
typedef struct _eth_priv_mgmt_frame_tx {
	/** Packet Length */
	t_u16 frm_len;
	/** Frame Control */
	t_u16 frm_ctl;
	/** Duration ID */
	t_u16 duration_id;
	/** Address1 */
	t_u8 addr1[ETH_ALEN];
	/** Address2 */
	t_u8 addr2[ETH_ALEN];
	/** Address3 */
	t_u8 addr3[ETH_ALEN];
	/** Sequence Control */
	t_u16 seq_ctl;
	/** Address4 */
	t_u8 addr4[ETH_ALEN];
	/** Frame payload */
	t_u8 payload[];
} __ATTRIB_PACK__ eth_priv_mgmt_frame_tx;

/** frame tx ioctl number */
#define FRAME_TX_IOCTL (SIOCDEVPRIVATE + 12)

/** TLV ID for multi chan info */
#define MULTI_CHAN_INFO_TLV_ID (PROPRIETARY_TLV_BASE_ID + 0xb7)
/** TLV ID for multi chan group info */
#define MULTI_CHAN_GROUP_INFO_TLV_ID (PROPRIETARY_TLV_BASE_ID + 0xb8)
/** BSS number mask */
#define BSS_NUM_MASK 0xf

typedef struct _eth_priv_multi_chan_cfg {
	/** Channel Time in us*/
	t_u32 channel_time;
	/** Buffer Weight */
	t_u8 buffer_weight;
	/** tlv len */
	t_u16 tlv_len;
	/** TLV Buffer */
	t_u8 tlv_buf[];
} __ATTRIB_PACK__ eth_priv_multi_chan_cfg;

typedef struct _eth_priv_drcs_cfg {
	/** Channel Index*/
	t_u16 chan_idx;
	/** Channel time (in TU) for chan_idx*/
	t_u8 chantime;
	/** Channel swith time (in TU) for chan_idx*/
	t_u8 switchtime;
	/** Undoze time (in TU) for chan_idx*/
	t_u8 rx_wait_time;
	/** Rx traffic control scheme when channel switch*/
	/** only valid for GC/STA interface*/
	t_u8 mode;
} __ATTRIB_PACK__ eth_priv_drcs_cfg;

typedef struct _ChannelBandInfo {
	Band_Config_t bandcfg;
	t_u8 chan_num;
} __ATTRIB_PACK__ ChannelBandInfo;

typedef struct _MrvlIETypes_multi_chan_group_info_t {
	/** Header */
	MrvlIEtypesHeader_t header;
	t_u8 chan_group_id;
	t_u8 chan_buff_weight;
	ChannelBandInfo chan_band_info;
	t_u32 channel_time;
	t_u32 reserved;
	union {
		t_u8 sdio_func_num;
		t_u8 usb_epnum;
	} __ATTRIB_PACK__ hid_num;
	t_u8 num_intf;
	t_u8 bss_type_numlist[];
} __ATTRIB_PACK__ MrvlIEtypes_multi_chan_group_info_t;

typedef struct _MrvlIETypes_multi_chan_info_t {
	/** Header */
	MrvlIEtypesHeader_t header;
	/** multi channel operation status */
	t_u16 status;
	/** Tlv buffer */
	t_u8 tlv_buffer[];
} __ATTRIB_PACK__ MrvlIEtypes_multi_chan_info_t;

/** Type definition of mlan_ds_gpio_tsf_latch */
typedef struct _mlan_ds_gpio_tsf_latch {
	/**clock sync Mode */
	t_u8 clock_sync_mode;
	/**clock sync Role */
	t_u8 clock_sync_Role;
	/**clock sync GPIO Pin Number */
	t_u8 clock_sync_gpio_pin_number;
	/**clock sync GPIO Level or Toggle */
	t_u8 clock_sync_gpio_level_toggle;
	/**clock sync GPIO Pulse Width */
	t_u16 clock_sync_gpio_pulse_width;
} mlan_ds_gpio_tsf_latch;

/** Type definition of mlan_ds_tsf_info */
typedef struct _mlan_ds_tsf_info {
	/**get tsf info format */
	t_u16 tsf_format;
	/**tsf info */
	t_u16 tsf_info;
	/**tsf */
	t_u64 tsf;
	/**Positive or negative offset in microsecond from Beacon TSF to GPIO
	 * toggle TSF  */
	t_s32 tsf_offset;
} mlan_ds_tsf_info;

/** Type definition of mlan_ds_tsf */
typedef struct _mlan_ds_tsf {
	/** Set / Get */
	t_u16 action;
	/**tsf */
	t_u64 tsf;
} __ATTRIB_PACK__ mlan_ds_tsf;

/** Type definition of mlan_ds_cross_chip_synch */
typedef struct _mlan_ds_cross_chip_synch {
	/**cross chip sync action 0-GET, 1-SET */
	t_u16 action;
	/**cross chip sync start or stop */
	t_u8 start_stop;
	/**cross chip sync role, master or slave */
	t_u8 role;
	/**cross chip sync periodicty of toggle in us */
	t_u32 period;
	/**cross chip sync initial TSF low */
	t_u32 init_tsf_low;
	/**cross chip sync intial TSF high */
	t_u32 init_tsf_high;
} mlan_ds_cross_chip_synch;

#ifdef WIFI_DIRECT_SUPPORT
/** flag for NOA */
#define WIFI_DIRECT_NOA 1
/** flag for OPP_PS */
#define WIFI_DIRECT_OPP_PS 2
/** Type definition of mlan_ds_wifi_direct_config for
 * MLAN_OID_MISC_WIFI_DIRECT_CONFIG */
typedef struct _mlan_ds_wifi_direct_config {
	/** flags for NOA/OPP_PS */
	t_u8 flags;
	/** NoA enable/disable */
	t_u8 noa_enable;
	/** index */
	t_u16 index;
	/** NoA count */
	t_u8 noa_count;
	/** NoA duration */
	t_u32 noa_duration;
	/** NoA interval */
	t_u32 noa_interval;
	/** opp ps enable/disable */
	t_u8 opp_ps_enable;
	/** CT window value */
	t_u8 ct_window;
} mlan_ds_wifi_direct_config;
#endif

/** mlan_ds_11h_nop_chan_list */
typedef struct _mlan_ds_11h_nop_chan_list {
	/** number of nop channel */
	t_u8 num_chan;
	/** chan list array */
	t_u8 chan_list[20];
} mlan_ds_11h_nop_chan_list, *pmlan_ds_11h_nop_chan_list;

/** DFS repeater mode configuration */
typedef struct _dfs_repeater {
	/** Enable DFS repeater mode */
	t_u16 action;
	/** 1 on and 0 off */
	t_u16 mode;
} dfs_repeater;

/** Maximum SSID length */
#define MAX_SSID_LENGTH 32
/** Maximum SSID length */
#define MIN_SSID_LENGTH 1
/** Maximum WPA passphrase length */
#define MAX_WPA_PASSPHRASE_LENGTH 64
/** Minimum WPA passphrase length */
#define MIN_WPA_PASSPHRASE_LENGTH 8

#define AUTO_DFS_ENABLE 0x1
#define AUTO_DFS_DISABLE 0x0
#define MAX_DFS_CHAN_LIST 16

/** Auto Zero DFS config structure */
typedef struct _auto_zero_dfs_cfg {
	/** 1: start 0: stop */
	t_u8 start_auto_zero_dfs;
	/** start channel for ZeroDFS */
	t_u8 cac_start_chan;
	/** cac timer */
	t_u32 cac_timer;
	/** bw: 0: 20MHz  1: 40Mz above  3: 40MHz below  4: Bandwidth 80MHz */
	t_u8 bw;
	/** enable uap chan switch after first CAC finished*/
	t_u8 uap_chan_switch;
	/** enable multi_chan dfs */
	t_u8 multi_chan_dfs;
	/** num of chan */
	t_u8 num_of_chan;
	/** user dfs channel list */
	t_u8 user_chan_list[MAX_DFS_CHAN_LIST];
} __ATTRIB_PACK__ auto_zero_dfs_cfg;

typedef t_u8 mlan_802_11_mac_addr[ETH_ALEN];
#define MLAN_MAX_MULTICAST_LIST_SIZE 32
/** mcast_aggr_group */
typedef struct _mcast_aggr_group {
	/** action */
	t_u32 action;
	/** mcast addr */
	t_u8 mcast_addr[ETH_ALEN];
	/** Number of multicast addresses in the list */
	t_u32 num_mcast_addr;
	/** Multicast address list */
	mlan_802_11_mac_addr mac_list[MLAN_MAX_MULTICAST_LIST_SIZE];
} mcast_aggr_group, *pmcast_aggr_group;

/** mlan_ds_mc_aggr_cfg for MLAN_OID_MISC_MC_AGGR_CFG */
typedef struct _mlan_ds_mc_aggr_cfg {
	/** action */
	t_u8 action;
	/* 1 enable, 0 disable
	 * bit 0 MC aggregation
	 * bit 1 packet expiry
	 * bit 2 CTS2Self
	 * bit 3 CTS2Self duration offset
	 * bit 6 UC non aggregation*/
	t_u8 enable_bitmap;
	/* 1 valid, 0 invalid
	 * bit 0 MC aggregation
	 * bit 1 packet expiry
	 * bit 2 CTS2Self
	 * bit 3 CTS2Self duration offset
	 * bit 6 UC non aggregation*/
	t_u8 mask_bitmap;
	/** CTS2Self duration offset */
	t_u16 cts2self_offset;
} mlan_ds_mc_aggr_cfg;

#define RETRY_BIT 0
#define CYCLE_START 1
#define CYCLE_STOP 2
#define AMPDU_START 3
#define AMPDU_STOP 4
#define MC7 7
#define MAX_PKT_SIZE 1968
#define MAX_MCAST_GROUP 5
#define MAX_AMPDU_NUM 5
#define MAX_RETRY_LIMIT 5

#define MAX_CONFIG_VARIABLE_LEN 32
#define CONFIG_LINE_BUFFER_SIZE 128
#define MAX_IP_NAME_LEN 16

#define debug_print(flag, fmt, ...)                                            \
	do {                                                                   \
		if (flag)                                                      \
			fprintf(stderr, fmt, __VA_ARGS__);                     \
	} while (0)
/* mc pkt txcontrol */
typedef struct _mc_txcontrol {
	/** Data rate in mcs index, 0-7 */
	t_u8 mcs;
	/** band width 0-20Mhz, 1-40Mhz */
	t_u8 bw;
	/** seq_num */
	t_u16 pktNumber;
	/** packet expiry time */
	t_u32 tsf;
	/** mc_pkt_flags */
	t_u8 flags;
} __ATTRIB_PACK__ mc_txcontrol;

/* per packet data from conf file */
typedef struct _mcast_tx_pkt_data {
	/** Data rate in mcs index, 0-7 */
	t_u8 dataRate;
	/** band width 0-20Mhz, 1-40Mhz */
	t_u8 bW;
	/** Data rate in mcs index for retry case, 0-7 */
	t_u8 retrydataRate;
	/** band width 0-20Mhz for retry case, 1-40Mhz */
	t_u8 retrybW;
	/** ampdu_pkt_len */
	t_u16 pkt_len;
	/** pkt expiry time */
	t_u32 expiry_time;
	/** retry packet flag, 0 to disable, 1 to enable */
	t_u8 pkt_retry;
	/** retry seq number */
	t_u32 retry_seq_num;
} mcast_tx_pkt_data;

/* define a structure for holding the values in "mcast_aggr_grp" */
typedef struct _mcast_tx_grp_data {
	/** mcast IP address */
	char mcast_ip_addr[MAX_IP_NAME_LEN];
	/** retry group flag, 0 to disable, 1 to enable */
	t_u8 retry;
	/** Tx aggregated mpdu */
	t_u32 ampduNum;
	/** mc_pkt_flags */
	t_u8 mask;
	/** dest socket structure*/
	struct sockaddr_in sockAddr_dest;
	/** first retry pkt number */
	t_u8 first_retry_pkt;
	/** last retry pkt number */
	t_u8 last_retry_pkt;
	/** multiple retry between 1 to max 5 times, 0 mean no retry */
	t_u8 retry_limit;
	/** conf file packet data */
	mcast_tx_pkt_data pkt_data[MAX_AMPDU_NUM];
	/* Sequence number */
	t_u32 seq_num;
} mcast_tx_grp_data;

#define NXP_802_11_PER_PEER_STATS_CFG_TLV_ID (PROPRIETARY_TLV_BASE_ID + 346)
#define NXP_802_11_PER_PEER_STATS_ENTRY_TLV_ID (PROPRIETARY_TLV_BASE_ID + 347)
#define OP_SET 1
#define OP_DELETE 2
#define OP_RESET 3

/* multicast packet statistics */
typedef struct MAPP_Stats_Mcast {
	/** mcast_mac */
	mlan_802_11_mac_addr mcast_mac;
	/** is_busy */
	t_u8 is_busy;
	/** reserved */
	t_u8 reserved;
	/** mcast_tx */
	t_u32 mcast_tx;
	/** mcast_tx_success */
	t_u32 mcast_tx_success;
	/** mcast_tx_timeout */
	t_u32 mcast_tx_timeout;
	/** mcast_tx_retry */
	t_u32 mcast_tx_retry;
	/** mcast_tx_retry_success */
	t_u32 mcast_tx_retry_success;
	/** mcast_tx_retry_timeout */
	t_u32 mcast_tx_retry_timeout;
} __ATTRIB_PACK__ Stats_Mcast_t;

/* unicast packet statistics */
typedef struct MAPP_Stats_Ucast {
	/** mlan_802_11_mac_addr */
	mlan_802_11_mac_addr ucast_mac;
	/** is_busy */
	t_u8 is_busy;
	/** reserved */
	t_u8 reserved;
	/** ucast_rx */
	t_u32 ucast_rx;
	/** ucast_tx */
	t_u32 ucast_tx;
	/** ucast_tx_success */
	t_u32 ucast_tx_success;
	/** ucast_tx_timeout */
	t_u32 ucast_tx_timeout;
	/** ucast_tx_retry */
	t_u32 ucast_tx_retry;
	/** ucast_tx_retry_success */
	t_u32 ucast_tx_retry_success;
	/** ucast_tx_lost */
	t_u32 ucast_tx_lost;
} __ATTRIB_PACK__ Stats_Ucast_t;

/* multicast FW statistics */
typedef struct MAPP_Stats_Mcast_FW {
	t_u32 cycle_recv_under_2300usec;
	t_u32 cycle_recv_in_time;
	t_u32 cycle_recv_over_2900usec;
	t_u32 cycle_recv_over_3500usec;
	t_u32 cycle_recv_over_5000usec;
	t_u32 cycle_recv_over_10000usec;
	t_u32 cycle_recv_over_15000usec;
	t_u32 spent_time_under_1000usec;
	t_u32 spent_time_over_1000usec;
	t_u32 spent_time_over_2000usec;
	t_u32 expiry_time_under_5000usec;
	t_u32 expired;
	t_u32 rsvd1;
	t_u32 rsvd2;
} __ATTRIB_PACK__ Stats_Mcast_FW_t;

typedef struct MAPP_Stats_Mcast_drv {
	t_u32 cycle_recv_under_2300usec;
	t_u32 cycle_recv_in_time;
	t_u32 cycle_recv_over_2900usec;
	t_u32 cycle_recv_over_3500usec;
	t_u32 cycle_recv_over_5000usec;
	t_u32 cycle_recv_over_10000usec;
	t_u32 cycle_recv_over_15000usec;
	t_u32 spent_time_under_1000usec;
	t_u32 spent_time_over_1000usec;
	t_u32 spent_time_over_2000usec;
	t_u32 spent_time_over_3000usec;
} __ATTRIB_PACK__ Stats_Mcast_Drv_t;

/** MAPP_Stats_Cfg_Params_TLV */
typedef struct MAPP_Stats_Cfg_Params_TLV {
	/** tlvHeader */
	MrvlIEtypesHeader_t tlvHeader;
	/** op */
	t_u8 op;
	/** reserved */
	t_u8 reserved;
	/** mac */
	mlan_802_11_mac_addr mac;
} __ATTRIB_PACK__ Stats_Cfg_Params_TLV_t;

#define STATS_ENTRY_TYPE_UCAST 0
#define STATS_ENTRY_TYPE_MCAST 1
#define STATS_ENTRY_TYPE_FW_MCAST 2

/** MAPP_Stats_Entry */
typedef struct MAPP_Stats_Entry {
	/** isMulticast */
	t_u8 type;
	/** buffer */
	t_u8 buffer[1];
} __ATTRIB_PACK__ Stats_Entry_t;

/** mlan_ds_stats */
typedef struct _mlan_ds_stats {
	/** action */
	t_u16 action;
	/** TLV len */
	t_u16 tlv_len;
	/** TLV buffer */
	t_u8 tlv_buf[];
} mlan_ds_stats;

typedef struct _mlan_ds_ch_load {
	/** action */
	t_u8 action;
	t_u16 ch_load_param;
	t_s16 noise;
	t_u16 rx_quality;
	t_u16 duration;
} mlan_ds_ch_load;

/** channel statictics */
typedef struct _chan_statistics_t {
	/** channle number */
	t_u8 chan_num;
	/** band info */
	Band_Config_t bandcfg;
	/** flags */
	t_u8 flags;
	/** noise */
	t_s8 noise;
	/** total network */
	t_u16 total_networks;
	/** scan duration */
	t_u16 cca_scan_duration;
	/** busy duration */
	t_u16 cca_busy_duration;
	/** min rss */
	t_u8 min_rss;
	/** max rss */
	t_u8 max_rss;
} __ATTRIB_PACK__ chan_statistics_t;

typedef struct _chan_stats {
	/** Number of records in the chan_stats */
	t_u32 num_in_chan_stats;
	/** channel statistics */
	chan_statistics_t stats[];
} chan_stats;

#define TLV_TYPE_PER_PKT_CFG 0x0001
#define MAX_NUM_ETHER_TYPE 8
#define MAX_TXRX_CTRL 3
#define TX_PKT_CTRL MBIT(0)
#define RX_PKT_INFO MBIT(1)
/** PER_PKT_CFG_TLV for per-packet configuration */
typedef struct _MrvlIEtypes_per_pkt_cfg_t {
	/** Header */
	MrvlIEtypesHeader_t header;
	/**  Tx/Rx per-packet control */
	t_u8 tx_rx_control;
	/** Number of ethernet types in ether_type array */
	t_u8 proto_type_num;
	/** Array of ether_type for per-packet control */
	t_u16 ether_type[];
} MrvlIEtypes_per_pkt_cfg_t;

#define FLAG_TX_HISTOGRAM 0x01
#define FLAG_RX_HISTOGRAM 0x02
#define DISABLE_TX_RX_HISTOGRAM 0x00
#define ENABLE_TX_RX_HISTOGRAM 0x01
#define GET_TX_RX_HISTOGRAM 0x02
/** TX histiogram ht statistic parameters */
typedef struct _tx_pkt_ht_rate_info {
	/** tx packet counter of MCS0~MCS15 */
	t_u32 htmcs_txcnt[16];
	/** tx packet's short GI counter of MCS0~MCS15 */
	t_u32 htsgi_txcnt[16];
	/** tx STBC packet counter of MCS0~MCS15 */
	t_u32 htstbcrate_txcnt[16];
} tx_pkt_ht_rate_info;
/** TX histiogram vht statistic parameters */
typedef struct _tx_pkt_vht_rate_info {
	/** tx packet counter of MCS0~MCS9 */
	t_u32 vhtmcs_txcnt[10];
	/** tx packet's short GI counter of MCS0~MCS9 */
	t_u32 vhtsgi_txcnt[10];
	/** tx STBC packet counter of MCS0~MCS9 */
	t_u32 vhtstbcrate_txcnt[10];
} tx_pkt_vht_rate_info;
/** TX histiogram he statistic parameters */
typedef struct _tx_pkt_he_rate_info {
	/** tx packet counter of MCS0~MCS11 */
	t_u32 hemcs_txcnt[12];
	/** tx STBC packet counter of MCS0~MCS11 */
	t_u32 hestbcrate_txcnt[12];
} tx_pkt_he_rate_info;
/** TX histogram statistic parameters*/
typedef struct _tx_pkt_rate_info {
	/** tx packet counter of every NSS, NSS=1,2 */
	t_u32 nss_txcnt[2];
	/** tx packet counter of every bandwith */
	t_u32 bandwidth_txcnt[3];
	/** different preamble tx packet counter */
	t_u32 preamble_txcnt[4];
	/** tx packet counter of using LDPC coding */
	t_u32 ldpc_txcnt;
	/** transmitted RTS counter */
	t_u32 rts_txcnt;
	/** RSSI of ack */
	t_s32 ack_RSSI;
} tx_pkt_rate_info;
/** RX histiogram ht statistic parameters */
typedef struct _rx_pkt_ht_rate_info {
	/** Rx packet counter of MCS0~MCS15 */
	t_u32 htmcs_rxcnt[16];
	/** Rx packet's short GI counter of MCS0~MCS15 */
	t_u32 htsgi_rxcnt[16];
	/** Rx STBC packet counter of MCS0~MCS15 */
	t_u32 htstbcrate_rxcnt[16];
} rx_pkt_ht_rate_info;
/** RX histiogram vht statistic parameters */
typedef struct _rx_pkt_vht_rate_info {
	/** Rx packet counter of MCS0~MCS9 */
	t_u32 vhtmcs_rxcnt[10];
	/** Rx packet's short GI counter of MCS0~MCS9 */
	t_u32 vhtsgi_rxcnt[10];
	/** Rx STBC packet counter of MCS0~MCS9 */
	t_u32 vhtstbcrate_rxcnt[10];
} rx_pkt_vht_rate_info;
/** RX histiogram he statistic parameters */
typedef struct _rx_pkt_he_rate_info {
	/** Rx packet counter of MCS0~MCS11 */
	t_u32 hemcs_rxcnt[12];
	/** Rx STBC packet counter of MCS0~MCS11 */
	t_u32 hestbcrate_rxcnt[12];
} rx_pkt_he_rate_info;
/** RX histogram statistic parameters*/
typedef struct _rx_pkt_rate_info {
	/** Rx packet counter of every NSS, NSS=1,2 */
	t_u32 nss_rxcnt[2];
	/** Received packet counter which using STBC */
	t_u32 nsts_rxcnt;
	/** Rx packet counter of every bandwith */
	t_u32 bandwidth_rxcnt[3];
	/** Different preamble Rx packet counter */
	t_u32 preamble_rxcnt[6];
	/** VHT SIGA2 LDPC bit*/
	t_u32 ldpc_txbfcnt[2];
	/**  Average RSSI */
	t_s32 rssi_value[2];
	/** RSSI value of path A */
	t_s32 rssi_chain0[4];
	/** RSSI value of path B */
	t_s32 rssi_chain1[4];
} rx_pkt_rate_info;
/** TX and RX histogram statistic parameters*/
typedef struct _tx_rx_histogram {
	/** Enable or disable get tx/rx histogram statistic */
	t_u8 enable;
	/** Choose to get TX, RX or both histogram statistic */
	t_u8 action;
} __ATTRIB_PACK__ tx_rx_histogram;

/** command ID for CHAN_REGION */
#define HostCmd_CMD_CHAN_REGION_CFG 0x0242
#define TLV_TYPE_CHAN_ATTR_CFG (PROPRIETARY_TLV_BASE_ID + 237) // 0x1ed
#define TLV_TYPE_REGION_INFO (PROPRIETARY_TLV_BASE_ID + 238) // 0x1ee
#define TLV_TYPE_POWER_TABLE (PROPRIETARY_TLV_BASE_ID + 262) // 0x206
#define TLV_TYPE_POWER_TABLE_ATTR (PROPRIETARY_TLV_BASE_ID + 317) // 0x23d

/** MrvlIEtypes_otp_region_info_t */
typedef struct MrvlIEtypes_otp_region_info {
	/** Header */
	MrvlIEtypesHeader_t header;
	/** country code */
	t_u8 countrycode[2];
	/** region code */
	t_u8 regioncode;
	/** environment */
	t_u8 environment;
	/** force_reg */
	t_u8 force_reg : 1;
	/** reserved */
	t_u8 reserved : 7;
	/** dfs_region */
	t_u8 dfs_region;
} __ATTRIB_PACK__ MrvlIEtypes_otp_region_info_t;

/** MrvlIEtypes_otp_region_info_t */
typedef struct MrvlIEtypes_power_table_attr {
	MrvlIEtypesHeader_t header;
	/** rows bg = num of channel in 2g */
	t_u8 rows_bg;
	/** cols_bg = num of 2g power modules + 1 */
	t_u8 cols_bg;
	/** rows a = num of channel in 5g */
	t_u8 rows_a;
	/** cols a = num of 5g power modules + 1  */
	t_u8 cols_a;
} __ATTRIB_PACK__ MrvlIEtypes_power_table_attr_t;

typedef struct {
	/** chan number */
	t_u8 chan_no;
	/** chan attr */
	t_u8 chan_attr;
} __ATTRIB_PACK__ chan_attr_t;

/** MrvlIEtypes_chan_attr_t */
typedef struct MrvlIEtypes_chan_attr {
	MrvlIEtypesHeader_t header;
	chan_attr_t chan_attr[];
} __ATTRIB_PACK__ MrvlIEtypes_chan_attr_t;

typedef struct {
	/** chan number */
	t_u8 chan_no;
	/** power modules */
	t_u8 power[];
} __ATTRIB_PACK__ chan_power_t;

/** MrvlIEtypes_power_tbl_t */
typedef struct MrvlIEtypes_power_tbl {
	MrvlIEtypesHeader_t header;
	chan_power_t chan_power[];
} __ATTRIB_PACK__ MrvlIEtypes_power_table_t;

typedef enum {
	PTVER_2X2_AC = 0,
	PTVER_1X1_AC = 1,
	PTVER_2X2_N = 2,
	PTVER_1X1_N = 3,
	PTVER_1X1_AC_11P = 5,
	PTVER_2X2_AC_2GVHT = 6,
	PTVER_1X1_AC_2GVHT = 7,
	PTVER_1X1_AC_2GVHT_11P = 8,
	PTVER_2X2_AX = 9,
	PTVER_2X2_AX_2GVHT = 10
} __ATTRIB_PACK__ ptver_type_e;

/** mlan_ds_misc_chan_chnrgpwr_cfg */
typedef struct _mlan_ds_misc_chnrgpwr_cfg {
	/** length */
	t_u16 length;
	/** chnrgpwr buf */
	t_u8 chnrgpwr_buf[BUFFER_LENGTH];
} __ATTRIB_PACK__ mlan_ds_misc_chnrgpwr_cfg;

#define OTP_IDENTIFIER 0x8888
/* mfg_rghdr_t*/
typedef struct {
	/* identifier, show always 0x8888 */
	t_u16 identifier;
	/** version */
	t_u8 version;
	/** num of entry */
	t_u8 numberofentries;
	/** ptbase version */
	t_u8 ptbaseversion;
	/** reserved */
	t_u8 reserved;
	/** CRC */
	t_u32 crc;
} __ATTRIB_PACK__ mfg_rghdr_t;

/* mfg_rgdatahdr_t */
typedef struct {
	/** country code */
	t_u8 countrycode[2];
	/** enviroment */
	t_u8 environment;
	/** region code */
	t_u8 regioncode;
	/** compressmethod */
	t_u16 compressmethod;
	/** length */
	t_u16 length;
	/** compressed table */
	t_u8 compressedtable;
} __ATTRIB_PACK__ mfg_rgdatahdr_t;

#define MAX_MCS_POWER_INDEX 36
#define MAX_BG_CHAN 14
#define MAX_A_CHAN 40

typedef struct {
	/** chan number */
	t_u8 chan_no;
	/** power value */
	t_u8 pwr[MAX_MCS_POWER_INDEX];
	/** chan attr */
	t_u8 chan_attr;
} __ATTRIB_PACK__ rgchan_pwr_t;

/** region_chan_pwr_tbl*/
typedef struct {
	/** country code */
	t_u8 countrycode[2];
	/** enviromnent */
	t_u8 environment;
	/** region code */
	t_u8 regioncode;
	/** 2g pwr tbl */
	rgchan_pwr_t bg_pwr_tbl[MAX_BG_CHAN];
	/** 5g pwr tbl */
	rgchan_pwr_t a_pwr_tbl[MAX_A_CHAN];
	/** max chan number in 5g */
	int max_chan_bg;
	/** max power modules in 2g */
	int max_power_bg;
	int max_chan_a;
	/** max power modules in 5g */
	int max_power_a;
	/** ptbase version for display */
	t_u8 ptbaseversion;
} __ATTRIB_PACK__ region_chan_pwr_tbl;

#define ETH_P_WSMP 0x88dc
#define BUF_SIZ 1024
typedef struct {
	t_u8 version;
	t_u8 sec_type;
	t_u8 chan;
	t_u8 rate;
	t_u8 tx_pow;
	t_u8 app_class;
	t_u8 acm_len;
	t_u16 len;
} __ATTRIB_PACK__ wsmp_header;

typedef struct {
	t_u16 rx_datarate; // Data rate
	t_u8 rx_channel; // Channel number
	t_u8 rx_antenna; // received antenna
	t_s8 rx_RSSI; // RSSI info
	t_u8 reserved[3]; // Reserved fields
} __ATTRIB_PACK__ dot11_rxcontrol;

typedef struct {
	t_u16 tx_datarate; // Data rate in unit of 0.5Mbps
	t_u8 tx_channel; // Channel number to transmit the frame
	t_u8 tx_Bw; // Bandwidth to transmit the frame
	t_u8 tx_power; // Power to be used for transmission
	t_u8 pkt_priority; // Priority of the packet to be transmitted
	t_u8 retry_limit; // tx retry limit
	t_u8 reserved[1]; // Reserved fields
} __ATTRIB_PACK__ dot11_txcontrol;

#if defined(PCIE)
/* ssu_params_cfg */
typedef struct _ssu_params_cfg {
	/* ssu mode */
	t_u8 ssu_mode;
	/* 0-3; # of FFT samples to skip*/
	t_u32 nskip;
	/* 0-3: # of FFT samples selected to dump */
	t_u32 nsel;
	/* 0-3: Down sample ADC input for buffering*/
	t_u32 adcdownsample;
	/* 0-1: Mask out ADC Data from spectral packet */
	t_u32 mask_adc_pkt;
	/* 0-1: Enable 16-Bit FFT output data precision in spectral packet */
	t_u32 out_16bits;
	/* 0-1: Enable power spectrum in dB for spectral packe */
	t_u32 spec_pwr_enable;
	/* 0-1: Enable spectral packet rate reduction in DB output format */
	t_u32 rate_deduction;
	/* 0-7: Number of spectral packets over which spectral data is to be
	 * averaged. */
	t_u32 n_pkt_avg;
} __ATTRIB_PACK__ ssu_params_cfg;
#endif

/** Type defination of gpio_cfg_ops */
typedef struct _gpio_cfg_ops {
	/** Get or Set action */
	t_u8 action;
	/** Operation type */
	t_u8 opsType;
	/** pin number */
	t_u8 pin_num;
	/** pin value */
	t_u8 value;
} __ATTRIB_PACK__ gpio_cfg_ops;

/** BF Global Configuration */
#define BF_GLOBAL_CONFIGURATION 0x00
/** Performs NDP sounding for PEER specified */
#define TRIGGER_SOUNDING_FOR_PEER 0x01
/** TX BF interval for channel sounding */
#define SET_GET_BF_PERIODICITY 0x02
/** Tell FW not to perform any sounding for peer */
#define TX_BF_FOR_PEER_ENBL 0x03
/** TX BF SNR threshold for peer */
#define SET_SNR_THR_PEER 0x04
/** TX Sounding*/
#define TX_SOUNDING_CFG 0x05

/** bf global cfg args */
typedef struct _bf_global_cfg {
	/** Global enable/disable bf */
	t_u8 bf_enbl;
	/** Global enable/disable sounding */
	t_u8 sounding_enbl;
	/** FB Type */
	t_u8 fb_type;
	/** SNR Threshold */
	t_u8 snr_threshold;
	/** Sounding interval in milliseconds */
	t_u16 sounding_interval;
	/** BF mode */
	t_u8 bf_mode;
	/** Reserved */
	t_u8 reserved;
} bf_global_cfg;

/** trigger sounding args */
typedef struct _trigger_sound {
	/** Peer MAC address */
	t_u8 peer_mac[MLAN_MAC_ADDR_LENGTH];
	/** Status */
	t_u8 status;
} trigger_sound;

/** bf periodicity args */
typedef struct _bf_periodicity_cfg {
	/** Peer MAC address */
	t_u8 peer_mac[MLAN_MAC_ADDR_LENGTH];
	/** Current Tx BF Interval in milliseconds */
	t_u16 interval;
	/** Status */
	t_u8 status;
} bf_periodicity_cfg;

/** tx bf peer args */
typedef struct _tx_bf_peer_cfg {
	/** Peer MAC address */
	t_u8 peer_mac[MLAN_MAC_ADDR_LENGTH];
	/** Reserved */
	t_u16 reserved;
	/** Enable/Disable Beamforming */
	t_u8 bf_enbl;
	/** Enable/Disable sounding */
	t_u8 sounding_enbl;
	/** FB Type */
	t_u8 fb_type;
} tx_bf_peer_cfg;

/** SNR threshold args */
typedef struct _snr_thr_cfg {
	/** Peer MAC address */
	t_u8 peer_mac[MLAN_MAC_ADDR_LENGTH];
	/** SNR for peer */
	t_u8 snr;
} snr_thr_cfg;

/** TWT setup parameters */
typedef struct _twt_setup {
	/** Implicit, 0: TWT session is explicit, 1: Session is implicit */
	t_u8 implicit;
	/** Announced, 0: Unannounced, 1: Announced TWT */
	t_u8 announced;
	/** Trigger Enabled, 0: Non-Trigger enabled, 1: Trigger enabled TWT */
	t_u8 trigger_enabled;
	/** TWT Information Disabled, 0: TWT info enabled, 1: TWT info disabled
	 */
	t_u8 twt_info_disabled;
	/** Negotiation Type, 0: Future Individual TWT SP start time, 1: Next
	 * Wake TBTT time */
	t_u8 negotiation_type;
	/** TWT Wakeup Duration, time after which the TWT requesting STA can
	 * transition to doze state */
	t_u8 twt_wakeup_duration;
	/** Flow Identifier. Range: [0-7]*/
	t_u8 flow_identifier;
	/** Hard Constraint, 0: FW can tweak the TWT setup parameters if it is
	 *rejected by AP.
	 ** 1: Firmware should not tweak any parameters. */
	t_u8 hard_constraint;
	/** TWT Exponent, Range: [0-63] */
	t_u8 twt_exponent;
	/** TWT Mantissa Range: [0-sizeof(UINT16)] */
	t_u16 twt_mantissa;
	/** TWT Request Type, 0: REQUEST_TWT, 1: SUGGEST_TWT*/
	t_u8 twt_request;
} __ATTRIB_PACK__ twt_setup;

/** TWT tear down parameters */
typedef struct _twt_teardown {
	/** TWT Flow Identifier. Range: [0-7] */
	t_u8 flow_identifier;
	/** Negotiation Type. 0: Future Individual TWT SP start time, 1: Next
	 * Wake TBTT time */
	t_u8 negotiation_type;
	/** Tear down all TWT. 1: To teardown all TWT, 0 otherwise */
	t_u8 teardown_all_twt;
} __ATTRIB_PACK__ twt_teardown;

#define WLAN_BTWT_REPORT_LEN 9
#define WLAN_BTWT_REPORT_MAX_NUM 4

typedef struct _twt_report {
	/** TWT report type, 0: BTWT id */
	t_u8 type;
	/** TWT report length of value in data */
	t_u8 length;
	t_u8 reserve[2];
	/** TWT report buffer */
	t_u8 data[WLAN_BTWT_REPORT_LEN * WLAN_BTWT_REPORT_MAX_NUM];
} __ATTRIB_PACK__ twt_report;

/** TWT information parameters */
typedef struct _twt_information {
	/** TWT Flow Identifier. Range: [0-7] */
	t_u8 flow_identifier;
	/** Suspend Duration.
	 * 0:Suspend forever;
	 * Else:Suspend agreement for specific duration in milli seconds, after
	 * than resume the agreement and enter SP immediately */
	t_u32 suspend_duration;
} __ATTRIB_PACK__ twt_information;

/** rx_abort_cfg parameters */
typedef struct _rx_abort_cfg_para {
	/** enable/disable Rx abort */
	int enable;
	/** rx weak rssi pkt threshold */
	int rssi_threshold;
} rx_abort_cfg_para;
/** ofdm_desense_cfg parameters */
typedef struct _ofdm_desense_cfg_para {
	/** enable/disable ofdm desense abort */
	int enable;
	/** rx weak cca pkt threshold */
	int cca_threshold;
} ofdm_desense_cfg_para;
/** rx_abort_cfg_ext parameters */
typedef struct _rx_abort_cfg_ext_para {
	/** enable/disable Rx abort */
	int enable;
	/** rssi margin */
	int rssi_margin;
	/** ceil rssi threshold */
	int ceil_rssi_threshold;
	/** floor rssi threshold */
	int floor_rssi_threshold;
	/** current dynamic rssi threshold */
	int current_dynamic_rssi_threshold;
	/** rssi config: default or user configured */
	int rssi_default_config;
	/** EDMAC status */
	int edmac_enable;
} rx_abort_cfg_ext_para;

/** nav mitigation parameters */
typedef struct _nav_mitigation_para {
	/** start/stop nav mitigation */
	int start_nav_mitigation;
	/** Duration value in us to set as threshold in ACT_SET action */
	int threshold;
	/** Counter for NAV mitigation detected */
	int detect_cnt;
	/** Counter for NAV mitigation stopped */
	int stop_cnt;
} nav_mitigation_para;

#define TX_AMPDU_RTS_CTS 0
#define TX_AMPDU_CTS_2_SELF 1
#define TX_AMPDU_DISABLE_PROTECTION 2
#define TX_AMPDU_DYNAMIC_RTS_CTS 3

/** tx_ampdu_prot_mode parameters */
typedef struct _tx_ampdu_prot_mode_para {
	/** set prot mode */
	int mode;
} tx_ampdu_prot_mode_para;

/** dot11mc_unassoc_ftm_cfg parameters */
typedef struct _dot11mc_unassoc_ftm_cfg_para {
	/** set state */
	int state;
} dot11mc_unassoc_ftm_cfg_para;

/** rate adapt cfg parameters */
typedef struct _rate_adapt_cfg_para {
	/** SR(Success Rate) rateadapt */
	int sr_rateadapt;
	/** set low threshold */
	int ra_low_thresh;
	/** set high threshold */
	int ra_high_thresh;
	/** set timer interval */
	int ra_interval;
} rate_adapt_cfg_para;

#define CCK_DESENSE_MODE_DISABLED 0
#define CCK_DESENSE_MODE_DYNAMIC 1
#define CCK_DESENSE_MODE_DYN_ENH 2
/** cck_desense_cfg parameters */
typedef struct _cck_desense_cfg_para {
	/** cck desense mode: 0:disable 1:normal 2:dynamic */
	int mode;
	/** specify rssi margin */
	int margin;
	/** specify ceil rssi threshold */
	int ceil_thresh;
	/** cck desense "on" interval count */
	int num_on_intervals;
	/** cck desense "off" interval count */
	int num_off_intervals;
} cck_desense_cfg_para;

typedef enum _dfs_state {
	/** Channel can be used, CAC (Channel Availability Check) must be done
	   before using it */
	DFS_USABLE = 0,
	/** Channel is not available, radar was detected */
	DFS_UNAVAILABLE = 1,
	/** Channel is Available, CAC is done and is free of radar */
	DFS_AVAILABLE = 2,
} dfs_state;
t_void hexdump(char *prompt, t_void *p, t_s32 len, char delim);

#define MAX_TP_ACCOUNT_DROP_POINT_NUM 5
#define RX_DROP_P1 (MAX_TP_ACCOUNT_DROP_POINT_NUM)
#define RX_DROP_P2 (MAX_TP_ACCOUNT_DROP_POINT_NUM + 1)
#define RX_DROP_P3 (MAX_TP_ACCOUNT_DROP_POINT_NUM + 2)
#define RX_DROP_P4 (MAX_TP_ACCOUNT_DROP_POINT_NUM + 3)
#define RX_DROP_P5 (MAX_TP_ACCOUNT_DROP_POINT_NUM + 4)

#define TXRX_MAX_SAMPLE 50

/** tp_acnt parameters */
typedef struct _tp_acnt {
	/** TX accounting */
	unsigned long tx_packets[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long tx_packets_last[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long tx_packets_rate[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long tx_bytes[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long tx_bytes_last[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long tx_bytes_rate[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long tx_amsdu_cnt;
	unsigned long tx_amsdu_cnt_last;
	unsigned long tx_amsdu_cnt_rate;
	unsigned long tx_amsdu_pkt_cnt;
	unsigned long tx_amsdu_pkt_cnt_last;
	unsigned long tx_amsdu_pkt_cnt_rate;
	unsigned long tx_intr_cnt;
	unsigned long tx_intr_last;
	unsigned long tx_intr_rate;
	unsigned long tx_pending;
	unsigned long tx_xmit_skb_realloc_cnt;
	unsigned long tx_stop_queue_cnt;
	unsigned long tx_delay_driver[TXRX_MAX_SAMPLE];
	unsigned long tx_delay1_driver[TXRX_MAX_SAMPLE];

	/** RX accounting */
	unsigned long rx_packets[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long rx_packets_last[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long rx_packets_rate[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long rx_bytes[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long rx_bytes_last[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long rx_bytes_rate[MAX_TP_ACCOUNT_DROP_POINT_NUM];
	unsigned long rx_amsdu_cnt;
	unsigned long rx_amsdu_cnt_last;
	unsigned long rx_amsdu_cnt_rate;
	unsigned long rx_amsdu_pkt_cnt;
	unsigned long rx_amsdu_pkt_cnt_last;
	unsigned long rx_amsdu_pkt_cnt_rate;
	unsigned long rx_intr_cnt;
	unsigned long rx_intr_last;
	unsigned long rx_intr_rate;
	unsigned long rx_pending;
	unsigned long rx_paused_cnt;
	unsigned long rx_rdptr_full_cnt;
	unsigned long rx_delay1_driver[TXRX_MAX_SAMPLE];
	unsigned long rx_delay2_driver[TXRX_MAX_SAMPLE];
	unsigned long rx_delay_kernel[TXRX_MAX_SAMPLE];
	unsigned long rx_amsdu_delay[TXRX_MAX_SAMPLE];
	unsigned long rx_amsdu_copy_delay[TXRX_MAX_SAMPLE];
	t_u8 rx_amsdu_index;
	t_u8 rx_index;
	t_u8 tx_index;
	/** TP account mode 0-disable 1-enable */
	unsigned int on;
	/** Tx packet drop point */
	unsigned int drop_point;
} tp_acnt_t;
#define EXT_LTE_RESP_GETSTAT 0xA5
#define EXT_LTE_RESP_RSTSTAT 0x5A
/** Host Command ID:  ROBUST_COEX */
#define HostCmd_ROBUST_COEX 0x00e0
typedef struct _host_RobustCoexLteStats_t {
	unsigned int Count_LTE_TX_NOTIFY;
	unsigned int Count_LTE_RX_PROTECT;
	unsigned int Count_LTE_TX_SUSPEND;
	unsigned int Count_LTE_RX_NOTIFY;
	unsigned char ResponseType;
} __ATTRIB_PACK__ host_RobustCoexLteStats_t;

/** turbo_mode parameters */
typedef struct _turbo_mode_para {
	t_u16 action;
	t_u16 oid;
	t_u16 size;
	/** set prot mode */
	t_u8 mode;
} __ATTRIB_PACK__ turbo_mode_para;

#endif /* _MLANUTL_H_ */
