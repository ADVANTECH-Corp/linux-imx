/** @file wlan_hostcmd.h
 *  @brief This file contains definitions for NXP WLAN driver host command.
 *
 *  Copyright 2012-2020, 2024 NXP
 *
 *  NXP CONFIDENTIAL
 *  The source code contained or described herein and all documents related to
 *  the source code ("Material") are owned by NXP or its
 *  suppliers or licensors. Title to the Material remains with NXP
 *  or its suppliers and licensors. The Material contains trade secrets and
 *  proprietary and confidential information of NXP or its suppliers and
 *  licensors. The Material is protected by worldwide copyright and trade secret
 *  laws and treaty provisions. No part of the Material may be used, copied,
 *  reproduced, modified, published, uploaded, posted, transmitted, distributed,
 *  or disclosed in any way without NXP's prior express written permission.
 *
 *  No license under any patent, copyright, trade secret or other intellectual
 *  property right is granted to or conferred upon you by disclosure or delivery
 *  of the Materials, either expressly, by implication, inducement, estoppel or
 *  otherwise. Any license under such intellectual property rights must be
 *  express and approved by NXP in writing.
 *
 */

#ifndef _WLAN_HOSTCMD_H_
#define _WLAN_HOSTCMD_H_

#ifndef __ATTRIB_PACK__
/** Pack attribute */
#define __ATTRIB_PACK__ __attribute__((packed))
#endif
#include <sys/socket.h>

/** Command buffer size */
#define MRVDRV_SIZE_OF_CMD_BUFFER (2 * 1024)

/** Get */
#define HostCmd_ACT_GEN_GET 0x0000
/** Set */
#define HostCmd_ACT_GEN_SET 0x0001

/** TLV Type : SSID */
#define TLV_TYPE_SSID 0x0000
/** TLV Type : Rates  */
#define TLV_TYPE_RATES 0x0001
/** TLV Type : PHY_FH */
#define TLV_TYPE_PHY_FH 0x0002
/** TLV Type : PHY_DS */
#define TLV_TYPE_PHY_DS 0x0003
/** TLV Type : CF */
#define TLV_TYPE_CF 0x0004
/** TLV Type : IBSS */
#define TLV_TYPE_IBSS 0x0006

/** TLV type ID definition */
#define PROPRIETARY_TLV_BASE_ID 0x0100

/** TLV Type : Key material */
#define TLV_TYPE_KEY_MATERIAL (PROPRIETARY_TLV_BASE_ID + 0)
/** TLV Type : Channel list */
#define TLV_TYPE_CHANLIST (PROPRIETARY_TLV_BASE_ID + 1)
/** TLV Type : probes number */
#define TLV_TYPE_NUMPROBES (PROPRIETARY_TLV_BASE_ID + 2)
/** TLV Type : RSSI Low */
#define TLV_TYPE_RSSI_LOW (PROPRIETARY_TLV_BASE_ID + 4)
/** TLV Type : SNR Low */
#define TLV_TYPE_SNR_LOW (PROPRIETARY_TLV_BASE_ID + 5)
/** TLV Type : Fail count */
#define TLV_TYPE_FAILCOUNT (PROPRIETARY_TLV_BASE_ID + 6)
/** TLV Type : Beacon miss */
#define TLV_TYPE_BCNMISS (PROPRIETARY_TLV_BASE_ID + 7)
/** TLV Type : LED GPIO */
#define TLV_TYPE_LED_GPIO (PROPRIETARY_TLV_BASE_ID + 8)
/** TLV Type : LED Behaviour */
#define TLV_TYPE_LEDBEHAVIOR (PROPRIETARY_TLV_BASE_ID + 9)
/** TLV Type : Pass through */
#define TLV_TYPE_PASSTHROUGH (PROPRIETARY_TLV_BASE_ID + 10)
/** TLV Type : Power 2.4GHZ */
#define TLV_TYPE_POWER_TBL_2_4GHZ (PROPRIETARY_TLV_BASE_ID + 12)
/** TLV Type : Power 5 GHZ */
#define TLV_TYPE_POWER_TBL_5GHZ (PROPRIETARY_TLV_BASE_ID + 13)
/** TLV Type : Broadcast probe */
#define TLV_TYPE_BCASTPROBE (PROPRIETARY_TLV_BASE_ID + 14)
/** TLV Type : SSID probe number */
#define TLV_TYPE_NUMSSID_PROBE (PROPRIETARY_TLV_BASE_ID + 15)
/** TLV Type : WMM status */
#define TLV_TYPE_WMMQSTATUS (PROPRIETARY_TLV_BASE_ID + 16)
/** TLV Type : Crypto data */
#define TLV_TYPE_CRYPTO_DATA (PROPRIETARY_TLV_BASE_ID + 17)
/** TLV Type : Wild card SSID */
#define TLV_TYPE_WILDCARDSSID (PROPRIETARY_TLV_BASE_ID + 18)
/** TLV Type : TSF Timestamp */
#define TLV_TYPE_TSFTIMESTAMP (PROPRIETARY_TLV_BASE_ID + 19)

/* for WPS */

/** TLV Type :WPS selected registrar TLV */
#define TLV_TYPE_WPS_SELECTED_REGISTRAR_TLV (PROPRIETARY_TLV_BASE_ID + 25)
/** TLV Type :WPS Enrollee TMO Tlv */
#define TLV_TYPE_WPS_ENROLLEE_TMO_TLV (PROPRIETARY_TLV_BASE_ID + 26)
/** TLV Type :WPS Enrollee probe req TLV */
#define TLV_TYPE_WPS_ENROLLEE_PROBE_REQ_TLV (PROPRIETARY_TLV_BASE_ID + 27)
/** TLV Type :WPS Registrar beacon TLV */
#define TLV_TYPE_WPS_REGISTRAR_BEACON_TLV (PROPRIETARY_TLV_BASE_ID + 28)
/** TLV Type :WPS Registrar probe response Tlv */
#define TLV_TYPE_WPS_REGISTRAR_PROBE_RESP_TLV (PROPRIETARY_TLV_BASE_ID + 29)
/** TLV type : STA Mac address */
#define TLV_TYPE_STA_MAC_ADDRESS (PROPRIETARY_TLV_BASE_ID + 32)
/** TLV: Management Frame from FW */
#define MRVL_MGMT_FRAME_TLV_ID (PROPRIETARY_TLV_BASE_ID + 104)
/** TLV: Management IE list */
#define MRVL_MGMT_IE_LIST_TLV_ID (PROPRIETARY_TLV_BASE_ID + 105)

/** length of NXP TLV */
#define TLV_HEADER_SIZE 4

/** TLV related data structures*/
/** MrvlIEtypesHeader_t */
typedef struct _MrvlIEtypesHeader {
	/** Type */
	u16 Type;
	/** Length */
	u16 Len;
} __ATTRIB_PACK__ MrvlIEtypesHeader_t;

typedef struct _HostCmd_DS_GEN {
	/** Command */
	u16 Command;
	/** Size */
	u16 Size;
	/** sequence number */
	u16 SeqNum;
	/** Result */
	u16 Result;
} __ATTRIB_PACK__ HostCmd_DS_GEN, *PHostCmd_DS_GEN;

/** Sizeof DS_GEN */
#define S_DS_GEN sizeof(HostCmd_DS_GEN)

struct mw_param {
	s32 value; /* The value of the parameter itself */
	u8 fixed; /* Hardware should not use auto select */
	u8 disabled; /* Disable the feature */
	u16 flags; /* Various specifc flags (if any) */
};

struct mw_point {
	u8 *pointer; /* Pointer to the data  (in user space) */
	u16 length; /* number of fields or size in bytes */
	u16 flags; /* Optional params */
};

union mwreq_data {
	/* Config - generic */
	char name[IFNAMSIZ];
	/* Name : used to verify the presence of  wireless extensions.
	 * Name of the protocol/provider... */

	struct mw_point essid; /* Extended network name */
	u32 mode; /* Operation mode */
	struct mw_param power; /* PM duration/timeout */
	struct sockaddr ap_addr; /* Access point address */
	struct mw_param param; /* Other small parameters */
	struct mw_point data; /* Other large parameters */
};

struct mwreq {
	union {
		char ifrn_name[IFNAMSIZ]; /* if name, e.g. "eth0" */
	} ifr_ifrn;

	/* Data part (defined just above) */
	union mwreq_data u;
};

/** MRVL command header buffer */
typedef struct _mrvl_cmd_head_buf {
	/** Buf Size */
	u32 buf_size;
	/** Command Code */
	u16 cmd_code;
	/** Size */
	u16 size;
	/** Sequence Number */
	u16 seq_num;
	/** Result */
	s16 result;

	u8 cmd_data[0];

} __ATTRIB_PACK__ mrvl_cmd_head_buf;

/** Private command structure */
typedef struct _mrvl_priv_cmd {
	/** Command buffer */
	u8 *buf;
	/** Used length */
	u32 used_len;
	/** Total length */
	u32 total_len;
} __ATTRIB_PACK__ mrvl_priv_cmd;

#define HostCmd_CMD_KEY_MATERIAL 0x005e
#define HostCmd_CMD_STA_ADD 0x0253
#define MAC_ADDR_LENGTH 6

typedef PACK_START struct {
	/** Action 0-GET, 1-SET, 4 CLEAR*/
	u16 Action;
	u32 add; /*add=1 for add_sta*/
	u8 peerMacAddr[MAC_ADDR_LENGTH]; /*Peer mac address*/
	u16 Aid;
	u32 bitmapRateSet; /* Bitmap from mac80211 */
	u8 mcs_mask[10];
	u8 ht_supported;
	u16 ht_cap;
	u8 bw;
	u8 vht_supported;
	u32 vht_cap;
	u16 vht_rx_mcs;
	u8 max_ampdu_factor;
} PACK_END host_cmd_sta_add_cmd_hdr;

#define HostCmd_CMD_802_11D_DOMAIN_INFO 0x005b
typedef struct _ieeetypes_subband_set {
	u8 first_chan;
	u8 no_of_chan;
	u8 max_tx_pwr;
} ieeetypes_subband_set_t;

typedef struct domain_param {
	u16 tag;
	u16 length;
	u8 country_code[3];
	ieeetypes_subband_set_t subband[0];
} domain_param_t;

typedef struct _apcmdbuf_cfg_80211d {
	u32 buf_size;
	u16 cmd_code;
	u16 size;
	u16 seq_num;
	s16 result;
	u16 action;
	domain_param_t domain;
} apcmdbuf_cfg_80211d;

/** NXP Private Command ioctl number */
#define MRVLPRIVCMD (SIOCDEVPRIVATE + 14)
#define UAPPRIVCMD (SIOCDEVPRIVATE + 2)
/** NXP private command identifier */
#define CMD_NXP "MRVL_CMD"
/** MRVLPRIVCMD custom IE */
#define PRIV_CMD_CUSTOMIE "customie"
/** MRVLPRIVCMD get STA list */
#define PRIV_CMD_GET_STA_LIST "getstalist"

#define PRIV_CMD_KEY_MATERIAL "key_material"

/** MRVLPRIVCMD AP Deauth */
#define PRIV_CMD_AP_DEAUTH "apdeauth"
#define PRIV_CMD_BANDCFG "bandcfg"
/** MRVLPRIVCMD AP start */
#define PRIV_CMD_AP_START "AP_BSS_START"
/** MRVLPRIVCMD AP stop */
#define PRIV_CMD_AP_STOP "AP_BSS_STOP"
/** MRVLPRIVCMD BSS_CONFIG */
#define PRIV_CMD_BSS_CONFIG "bssconfig"
/** NXP private command equivalent to SIOCSIWAP */
#define PRIV_CMD_SET_AP "setap"
/** NXP private command equivalent to SIOCSIWMODE */
#define PRIV_CMD_SET_BSS_MODE "setbssmode"
/** NXP private command equivalent to SIOCSIWPOWER */
#define PRIV_CMD_SET_POWER "setpower"
/** NXP private command equivalent to SIOCSIWESSID */
#define PRIV_CMD_SET_ESSID "setessid"
/** NXP private command equivalent to SIOCSIWAUTH */
#define PRIV_CMD_SET_AUTH "setauth"
/** NXP private command equivalent to SIOCGIWAP */
#define PRIV_CMD_GET_AP "getap"
/** NXP private command equivalent to SIOCGIWPOWER */
#define PRIV_CMD_GET_POWER "getpower"
/** NXP private command for STA deauth */
#define PRIV_CMD_DEAUTH "deauth"
/** NXP private command to set/get bss role */
#define PRIV_CMD_BSSROLE "bssrole"
/** NXP private command for wps session */
#define PRIV_CMD_WPSSESSION "wpssession"
/** NXP private command to set/get deepsleep */
#define PRIV_CMD_DEEPSLEEP "deepsleep"
/** NXP private command for setuserscan */
#define PRIV_CMD_SETUSERSCAN "setuserscan"
/** NXP private command for scan config */
#define PRIV_CMD_SCANCFG "scancfg"
/** NXP private command for passphrase */
#define PRIV_CMD_PASSPHRASE "passphrase"
/** NXP private command for hostcmd */
#define PRIV_CMD_HOSTCMD "hostcmd"
/** NXP private command for get scan table */
#define PRIV_CMD_GET_SCAN_TABLE "getscantable"
/** MRVLPRIVCMD Set/Get HotSpot state */
#define PRIV_CMD_HOTSPOTCFG "hotspotcfg"
/** MRVLPRIVCMD register passthrough mgmt frame */
#define PRIV_CMD_MGMT_FRAME_CTRL "mgmtframectrl"

/* Modes of operation */
#define MW_MODE_AUTO 0 /* Let the driver decides */
#define MW_MODE_ADHOC 1 /* Single cell network */
#define MW_MODE_INFRA 2 /* Multi cell network, roaming, ... */
#define MW_MODE_MASTER 3 /* Synchronisation master or Access Point */
#define MW_MODE_REPEAT 4 /* Wireless Repeater (forwarder) */
#define MW_MODE_SECOND 5 /* Secondary master/repeater (backup) */
#define MW_MODE_MONITOR 6 /* Passive monitor (listen only) */
#define MW_MODE_MESH 7 /* Mesh (IEEE 802.11s) network */

/* PRIV_CMD_SET_AUTH struct mw_param flags */
#define MW_AUTH_INDEX 0x0FFF
#define MW_AUTH_FLAGS 0xF000
/* PRIV_CMD_SET_AUTH/PRIV_CMD_GET_AUTH parameters (0 .. 4095)
 * (MW_AUTH_INDEX mask in struct mw_param flags; this is the index of the
 * parameter that is being set/get to; value will be read/written to
 * struct mw_param value field) */
#define MW_AUTH_WPA_VERSION 0
#define MW_AUTH_CIPHER_PAIRWISE 1
#define MW_AUTH_CIPHER_GROUP 2
#define MW_AUTH_KEY_MGMT 3
#define MW_AUTH_TKIP_COUNTERMEASURES 4
#define MW_AUTH_DROP_UNENCRYPTED 5
#define MW_AUTH_80211_AUTH_ALG 6
#define MW_AUTH_WPA_ENABLED 7
#define MW_AUTH_RX_UNENCRYPTED_EAPOL 8
#define MW_AUTH_ROAMING_CONTROL 9
#define MW_AUTH_PRIVACY_INVOKED 10
#define MW_AUTH_CIPHER_GROUP_MGMT 11
#define MW_AUTH_MFP 12

/* MW_AUTH_WPA_VERSION values (bit field) */
#define MW_AUTH_WPA_VERSION_DISABLED 0x00000001
#define MW_AUTH_WPA_VERSION_WPA 0x00000002
#define MW_AUTH_WPA_VERSION_WPA2 0x00000004

/* MW_AUTH_PAIRWISE_CIPHER, MW_AUTH_GROUP_CIPHER, and MW_AUTH_CIPHER_GROUP_MGMT
 *  * values (bit field) */
#define MW_AUTH_CIPHER_NONE 0x00000001
#define MW_AUTH_CIPHER_WEP40 0x00000002
#define MW_AUTH_CIPHER_TKIP 0x00000004
#define MW_AUTH_CIPHER_CCMP 0x00000008
#define MW_AUTH_CIPHER_WEP104 0x00000010
#define MW_AUTH_CIPHER_AES_CMAC 0x00000020

/* MW_AUTH_KEY_MGMT values (bit field) */
#define MW_AUTH_KEY_MGMT_802_1X 1
#define MW_AUTH_KEY_MGMT_PSK 2

/* MW_AUTH_80211_AUTH_ALG values (bit field) */
#define MW_AUTH_ALG_OPEN_SYSTEM 0x00000001
#define MW_AUTH_ALG_SHARED_KEY 0x00000002
#define MW_AUTH_ALG_LEAP 0x00000004

/* MW_AUTH_ROAMING_CONTROL values */
#define MW_AUTH_ROAMING_ENABLE 0 /* driver/firmware based roaming */
#define MW_AUTH_ROAMING_DISABLE                                                \
	1 /* user space program used for roaming                               \
	   * control */

/* MW_AUTH_MFP (management frame protection) values */
#define MW_AUTH_MFP_DISABLED 0 /* MFP disabled */
#define MW_AUTH_MFP_OPTIONAL 1 /* MFP optional */
#define MW_AUTH_MFP_REQUIRED 2 /* MFP required */
/* Maximum size of returned data */
#define MW_SCAN_MAX_DATA 4096 /* In bytes */

/** custom IE */
typedef struct _custom_ie {
	/** IE Index */
	u16 ie_index;
	/** Mgmt Subtype Mask */
	u16 mgmt_subtype_mask;
	/** IE Length */
	u16 ie_length;
	/** IE buffer */
	u8 ie_buffer[0];
} __ATTRIB_PACK__ custom_ie;

/** TLV buffer : custom IE */
typedef struct _tlvbuf_custom_ie {
	/** Tag */
	u16 tag;
	/** Length */
	u16 length;
	/** custom IE data */
	custom_ie ie_data[0];
} __ATTRIB_PACK__ tlvbuf_custom_ie;

/** APCMD buffer : sys_configure */
typedef struct _apcmdbuf_bss_configure {
	/** Action : GET or SET */
	u32 Action;
} apcmdbuf_bss_configure;

/** These are mlanconfig commands. */

/* bgscan command is delivered in a buffer packed with the following:
 * HostCmd_DS_GEN
 * mlanconfig_bgscfg_cmd_hdr
 * tlvs
 */
#define HostCmd_CMD_BGSCAN_CFG 0x006b
typedef struct _mlanconfig_bgscfg_cmd_hdr {
	u8 Action;
	u8 ConfigType;
	u8 Enable;
	u8 BssType;
	u8 ChannelsPerScan;
	u8 Reserved1[3];
	u32 ScanInterval;
	u32 StoreCondition;
	u32 ReportConditions;
	u8 Reserved3[2];
	u8 tlvs[0];
} __ATTRIB_PACK__ mlanconfig_bgscfg_cmd_hdr;

/* generic TLV structure */
typedef struct _tlv {
	u16 type;
	u16 len;
	u8 value[0];
} __ATTRIB_PACK__ tlv;

/* ssid tlv: Just a generic tlv who's L is the length of the string V (not
 * including the terminating NULL).
 */
#define MLANCONFIG_SSID_HEADER_TYPE 0x0112
typedef struct _ssid_header_tlv {
	u16 type;
	u16 len;
	u8 MaxSSIDLen;
	u8 ssid[7];
} __ATTRIB_PACK__ ssid_header_tlv;

#define MLANCONFIG_PROBE_HEADER_TYPE 0x0102
typedef struct _probe_header_tlv {
	u16 type;
	u16 len;
	u16 NumProbes;
} __ATTRIB_PACK__ probe_header_tlv;

#define MLANCONFIG_CHANNEL_HEADER_TYPE 0x0101
typedef struct _channel_header_tlv {
	u16 type;
	u16 len;
	Band_Config_t Chan1_bandcfg;
	u8 Chan1_ChanNumber;
	u8 Chan1_ScanType;
	u16 Chan1_MinScanTime;
	u16 Chan1_ScanTime;
	Band_Config_t Chan2_bandcfg;
	u8 Chan2_ChanNumber;
	u8 Chan2_ScanType;
	u16 Chan2_MinScanTime;
	u16 Chan2_ScanTime;
	Band_Config_t Chan3_bandcfg;
	u8 Chan3_ChanNumber;
	u8 Chan3_ScanType;
	u16 Chan3_MinScanTime;
	u16 Chan3_ScanTime;
} __ATTRIB_PACK__ channel_header_tlv;

#define MLANCONFIG_SNR_HEADER_TYPE 0x0105
typedef struct _snr_header_tlv {
	u16 type;
	u16 len;
	u8 SNRValue;
	u8 SNRFreq;
} __ATTRIB_PACK__ snr_header_tlv;

#define MLANCONFIG_START_LATER_HEADER_TYPE 0x011e
typedef struct _start_later_header_tlv {
	u16 type;
	u16 len;
	u16 StartLaterValue;
} __ATTRIB_PACK__ start_later_header_tlv;

#define SIZEOF_VALUE(t) (sizeof(t) - sizeof(tlv))

#endif /* _WLAN_HOSTCMD_H_ */
