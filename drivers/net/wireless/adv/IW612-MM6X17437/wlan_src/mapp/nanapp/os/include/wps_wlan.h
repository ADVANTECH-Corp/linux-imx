/** @file wps_wlan.h
 *  @brief This file contains definition for WLAN driver control/command.
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

#ifndef _WPS_WLAN_H_
#define _WPS_WLAN_H_

#include "mwu_if_manager.h"

#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__ __attribute__((packed))
#endif

/*
 *  ctype from older glib installations defines BIG_ENDIAN.  Check it
 *   and undef it if necessary to correctly process the wlan header
 *   files
 */
#if (BYTE_ORDER == LITTLE_ENDIAN)
#undef BIG_ENDIAN
#endif

/** WPS session on */
#define WPS_SESSION_ON 1
/** WPS session off */
#define WPS_SESSION_OFF 0

/** Keep all AP in scan result */
#define FILTER_NONE 0
/** Keep PIN AP in scan result */
#define FILTER_PIN 1
/** Keep PBC AP in scan result */
#define FILTER_PBC 2

/** Perform Active Scan */
#define ACTIVE_SCAN 1
/** Perform Passive Scan */
#define PASSIVE_SCAN 2

/** IEEE Type definitions  */
typedef enum _IEEEtypes_ElementId_e {
	SSID = 0,
	SUPPORTED_RATES = 1,
	FH_PARAM_SET = 2,
	DS_PARAM_SET = 3,
	CF_PARAM_SET = 4,

	IBSS_PARAM_SET = 6,

	EXTENDED_SUPPORTED_RATES = 50,
	EXTENDED_CAP = 127,

	VENDOR_SPECIFIC_221 = 221,
	WPA_IE = VENDOR_SPECIFIC_221,
	WPS_IE = VENDOR_SPECIFIC_221,

	RSN_IE = 48,

} __ATTRIB_PACK__ IEEEtypes_ElementId_e;

/** Vendor specific IE header */
typedef struct _IEEEtypes_VendorHeader_t {
	/** Element ID */
	u8 element_id;
	/** Length */
	u8 len;
	/** OUI */
	u8 oui[3];
	/** OUI type */
	u8 oui_type;
	/** OUI subtype */
	u8 oui_subtype;
	/** Version */
	u8 version;
} __ATTRIB_PACK__ IEEEtypes_VendorHeader_t, *pIEEEtypes_VendorHeader_t;

/** Maximum size of IEEE Information Elements */
#define IEEE_MAX_IE_SIZE 256

/** NF value for default scan */
#define MRVDRV_NF_DEFAULT_SCAN_VALUE (-96)

/** IEEEtypesHeader_t */
typedef struct _IEEEtypes_Header_t {
	/** Type */
	u8 Type;
	/** Length */
	u8 Len;
} __ATTRIB_PACK__ IEEEtypes_Header_t;

/** Vendor specific IE */
typedef struct _IEEEtypes_VendorSpecific_t {
	/** Vendor specific IE header */
	IEEEtypes_VendorHeader_t vend_hdr;
	/** IE Max - size of previous fields */
	u8 data[IEEE_MAX_IE_SIZE - sizeof(IEEEtypes_VendorHeader_t)];
} __ATTRIB_PACK__ IEEEtypes_VendorSpecific_t, *pIEEEtypes_VendorSpecific_t;

/** IEEE IE */
typedef struct _IEEEtypes_Generic_t {
	/** Generic IE header */
	IEEEtypes_Header_t ieee_hdr;
	/** IE Max - size of previous fields */
	u8 data[IEEE_MAX_IE_SIZE - sizeof(IEEEtypes_Header_t)];
} IEEEtypes_Generic_t, *pIEEEtypes_Generic_t;

/** MLAN 802.11 MAC Address */
typedef u8 mlan_802_11_mac_addr[ETH_ALEN];

/** Event header */
typedef struct _eventheader {
	/** Event ID */
	u32 event_id;
	/** Event data */
	u8 event_data[0];
} event_header;

/*Please note, these are the internal events received from wext and passed to
 * STA SM */
enum wlan_sta_kernel_event {
	WLAN_STA_KERNEL_EVENT_ASSOCIATED,
	WLAN_STA_KERNEL_EVENT_AUTHENTICATED,
	WLAN_STA_KERNEL_EVENT_LINK_LOST,
};

/**
 *                 IEEE 802.11 MAC Message Data Structures
 *
 * Each IEEE 802.11 MAC message includes a MAC header, a frame body (which
 * can be empty), and a frame check sequence field. This section gives the
 * structures that used for the MAC message headers and frame bodies that
 * can exist in the three types of MAC messages - 1) Control messages,
 * 2) Data messages, and 3) Management messages.
 */
#ifdef BIG_ENDIAN
typedef struct _IEEEtypes_FrameCtl_t {
	/** Order */
	u8 order : 1;
	/** Wep */
	u8 wep : 1;
	/** More Data */
	u8 more_data : 1;
	/** Power Mgmt */
	u8 pwr_mgmt : 1;
	/** Retry */
	u8 retry : 1;
	/** More Frag */
	u8 more_frag : 1;
	/** From DS */
	u8 from_ds : 1;
	/** To DS */
	u8 to_ds : 1;
	/** Sub Type */
	u8 sub_type : 4;
	/** Type */
	u8 type : 2;
	/** Protocol Version */
	u8 protocol_version : 2;
} IEEEtypes_FrameCtl_t;
#else
typedef struct _IEEEtypes_FrameCtl_t {
	/** Protocol Version */
	u8 protocol_version : 2;
	/** Type */
	u8 type : 2;
	/** Sub Type */
	u8 sub_type : 4;
	/** To DS */
	u8 to_ds : 1;
	/** From DS */
	u8 from_ds : 1;
	/** More Frag */
	u8 more_frag : 1;
	/** Retry */
	u8 retry : 1;
	/** Power Mgmt */
	u8 pwr_mgmt : 1;
	/** More Data */
	u8 more_data : 1;
	/** Wep */
	u8 wep : 1;
	/** Order */
	u8 order : 1;
} IEEEtypes_FrameCtl_t;
#endif

/** wlan_802_11_header */
typedef struct _wlan_802_11_header {
	/** Frame Control */
	u16 frm_ctl;
	/** Duration ID */
	u16 duration_id;
	/** Address1 */
	mlan_802_11_mac_addr addr1;
	/** Address2 */
	mlan_802_11_mac_addr addr2;
	/** Address3 */
	mlan_802_11_mac_addr addr3;
	/** Sequence Control */
	u16 seq_ctl;
	/** Address4 */
	mlan_802_11_mac_addr addr4;
} __ATTRIB_PACK__ wlan_802_11_header;

/** MrvlIETypes_MgmtFrameSet_t */
typedef struct _MrvlIETypes_MgmtFrameSet_t {
	/** Type */
	u16 type;
	/** Length */
	u16 len;
	/** Frame Control */
	IEEEtypes_FrameCtl_t frame_control;
	/** Frame Contents */
	u8 frame_contents[0];
} MrvlIETypes_MgmtFrameSet_t;

/** Event body : STA associated/ discovery request */
typedef struct _eventbuf_discovery_request {
	/** Reserved */
	u8 reserved[2];
	/** MAC address of associated STA */
	u8 sta_mac[ETH_ALEN];
	/** Discovery request/response buffer */
	u8 discovery_payload[0];
} eventbuf_discovery_request, eventbuf_station_connect;

/** Event body : RSN Connect */
typedef struct _eventbuf_rsn_connect {
	/** Reserved */
	u8 reserved[2];
	/** MAC address of Station */
	u8 sta_mac_address[ETH_ALEN];
	/** WPA/WPA2 TLV IEs */
	u8 tlv_list[0];
} eventbuf_rsn_connect;

#define MRVDRV_MAX_SSID_LIST_LENGTH 10
/** Maximum number of channels that can be sent in a setuserscan ioctl */
#define WLAN_IOCTL_USER_SCAN_CHAN_MAX 50

/** Type definition of eth_priv_scan_cfg */
typedef struct _wlan_ioctl_scan_cfg {
	/** Scan type
	 *  0: Unchanged, 1: Active, 2: Passive */
	u32 scan_type;
	/** BSS mode for scanning
	 *  0: Unchanged, 1: BSS, 2: IBSS, 3: Any */
	u32 scan_mode;
	/** Scan probe
	 *  0: Unchanged, 1-4: Number of probes per channel */
	u32 scan_probe;
	/** Scan channel time for specific scan in milliseconds */
	u32 specific_scan_time;
	/** Scan channel time for active scan in milliseconds */
	u32 active_scan_time;
	/** Scan channel time for passive scan in milliseconds */
	u32 passive_scan_time;
	/** Extended Scan
	 *  0: Legacy scan, 1: Extended scan */
	u32 ext_scan;
} wlan_ioctl_scan_cfg;

typedef struct _wlan_ioctl_user_scan_chan {
	u8 chan_number; /**< Channel Number to scan */
	u8 radio_type; /**< Radio type: 'B/G' Band = 0, 'A' Band = 1 */
	u8 scan_type; /**< Scan type: Active = 1, Passive = 2 */
	u8 reserved; /**< Reserved */
	u32 scan_time; /**< Scan duration in milliseconds; if 0 default used */
} __ATTRIB_PACK__ wlan_ioctl_user_scan_chan;

typedef struct _wlan_ioctl_user_scan_ssid {
	char ssid[MAX_SSID_LEN + 1]; /**< SSID */
	u8 max_len; /**< Maximum length of SSID */
} __ATTRIB_PACK__ wlan_ioctl_user_scan_ssid;

typedef struct _wlan_ioctl_user_scan_cfg {
	/** Flag set to keep the previous scan table intact */
	u8 keep_previous_scan; /* Do not erase the existing scan results */

	/** BSS mode to be sent in the firmware command */
	u8 bss_mode;

	/** Configure the number of probe requests for active chan scans */
	u8 num_probes;

	/** Reserved */
	u8 reserved;

	/** BSSID filter sent in the firmware command to limit the results */
	u8 specific_bssid[ETH_ALEN];
	/** SSID filter list used in the to limit the scan results */
	wlan_ioctl_user_scan_ssid ssid_list[MRVDRV_MAX_SSID_LIST_LENGTH];

	/** Variable number (fixed maximum) of channels to scan up */
	wlan_ioctl_user_scan_chan chan_list[WLAN_IOCTL_USER_SCAN_CHAN_MAX];

} __ATTRIB_PACK__ wlan_ioctl_user_scan_cfg;

/** Maximum MGMT IE index count */
#define MAX_MGMT_IE_INDEX 14

/** Maximum number of MAC addresses for one-shot filter modifications */
#define MAX_MAC_ONESHOT_FILTER 16
/** Standard DEAUTH code for AP deauth */
#define DEAUTH_REASON_PREV_AUTH_INVALID 0x0002

/** mlan_802_11_ssid data structure */
typedef struct _mlan_802_11_ssid {
	/** SSID Length */
	u32 ssid_len;
	/** SSID information field */
	u8 ssid[MAX_SSID_LEN];
} mlan_802_11_ssid;

/** scan_chan_list data structure */
typedef struct _scan_chan_list {
	/** Channel number*/
	u8 chan;
	/** band config type */
	Band_Config_t bandcfg;
} scan_chan_list;

/** mac_filter data structure */
typedef struct _mac_filter {
	/** Mac filter mode */
	u16 filter_mode;
	/** Mac adress count */
	u16 mac_count;
	/** Mac address list */
	mlan_802_11_mac_addr mac_list[MAX_MAC_ONESHOT_FILTER];
} mac_filter;

/** wpa parameter */
typedef struct _wpa_param {
	/** Pairwise cipher WPA */
	u8 pairwise_cipher_wpa;
	/** Pairwise cipher WPA2 */
	u8 pairwise_cipher_wpa2;
	/** Group cipher */
	u8 group_cipher;
	/** RSN replay protection */
	u8 rsn_protection;
	/** Passphrase length */
	u32 length;
	/** Passphrase */
	u8 passphrase[64];
	/** Group key rekey time */
	u32 gk_rekey_time;
} wpa_param;

/** wep key */
typedef struct _wep_key {
	/** Key index 0-3 */
	u8 key_index;
	/** Is default */
	u8 is_default;
	/** Length */
	u16 length;
	/** Key data */
	u8 key[26];
} wep_key;

/** Data structure of WMM parameter IE  */
typedef struct _wmm_parameter_t {
	/** OuiType:  00:50:f2:02 */
	u8 ouitype[4];
	/** Oui subtype: 01 */
	u8 ouisubtype;
	/** version: 01 */
	u8 version;
	/** QoS information */
	u8 qos_info;
	/** Reserved */
	u8 reserved;
	/** AC Parameters Record WMM_AC_BE, WMM_AC_BK, WMM_AC_VI, WMM_AC_VO */
	// wmm_ac_parameters_t ac_params[MAX_AC_QUEUES];
} wmm_parameter_t, *pwmm_parameter_t;

/** BSS config structure */
typedef struct _bss_config_t {
	/** AP mac addr */
	mlan_802_11_mac_addr mac_addr;
	/** SSID */
	mlan_802_11_ssid ssid;
	/** Broadcast ssid control */
	u8 bcast_ssid_ctl;
	/** Radio control */
	u8 radio_ctl;
	/** dtim period */
	u8 dtim_period;
	/** beacon period */
	u16 beacon_period;
	/** rates */
	u8 rates[14];
	/** Tx data rate */
	u16 tx_data_rate;
	/** tx beacon rate */
	u16 tx_beacon_rate;
	/** multicast/broadcast data rate */
	u16 mcbc_data_rate;
	/** Tx power level */
	u8 tx_power_level;
	/** Tx antenna */
	u8 tx_antenna;
	/** Rx anteena */
	u8 rx_antenna;
	/** packet forward control */
	u8 pkt_forward_ctl;
	/** max station count */
	u16 max_sta_count;
	/** mac filter */
	mac_filter filter;
	/** station ageout timer in the unit of 100ms  */
	u32 sta_ageout_timer;
	/** PS station ageout timer in the unit of 100ms  */
	u32 ps_sta_ageout_timer;
	/** RTS threshold */
	u16 rts_threshold;
	/** fragmentation threshold */
	u16 frag_threshold;
	/**  retry_limit */
	u16 retry_limit;
	/**  pairwise update timeout */
	u32 pairwise_update_timeout;
	/** pairwise handshake retries */
	u32 pwk_retries;
	/**  groupwise update timeout */
	u32 groupwise_update_timeout;
	/** groupwise handshake retries */
	u32 gwk_retries;
	/** preamble type */
	u8 preamble_type;
	/** band cfg */
	Band_Config_t bandcfg;
	/** channel */
	u8 channel;
	/** auth mode */
	u16 auth_mode;
	/** encryption protocol */
	u16 protocol;
	/** key managment type */
	u16 key_mgmt;
	/** wep param */
	wep_key wep_cfg[4];
	/** wpa param */
	wpa_param wpa_cfg;
	/** Mgmt IE passthru mask */
	u32 mgmt_ie_passthru_mask;
	/*
	 * 11n HT Cap  HTCap_t  ht_cap
	 */
	/** HT Capabilities Info field */
	u16 ht_cap_info;
	/** A-MPDU Parameters field */
	u8 ampdu_param;
	/** Supported MCS Set field */
	u8 supported_mcs_set[16];
	/** HT Extended Capabilities field */
	u16 ht_ext_cap;
	/** Transmit Beamforming Capabilities field */
	u32 tx_bf_cap;
	/** Antenna Selection Capability field */
	u8 asel;
	/** Enable 2040 coex */
	u8 enable_2040coex;
	/** key management operation */
	u16 key_mgmt_operation;
	/** BSS status */
	u16 bss_status;
	/** WFD psk */
	u8 psk[32];
	/** num of channels */
	u32 num_of_chan;
	/** scan channel list in ACS mode */
	scan_chan_list chan_list[MAX_CHANNELS];
	/** Wmm parameters */
	wmm_parameter_t wmm_para;
} bss_config_t;

/** uAp deauth param struct */

typedef struct _deauth_param {
	u8 mac_addr[ETH_ALEN];
	u16 reason_code;
} deauth_param;

/** fw_info */
typedef struct _fw_info {
	/** subcmd */
	u32 subcmd;
	/** Get */
	u32 action;
	/** Firmware release number */
	u32 fw_release_number;
	/** Device support for MIMO abstraction of MCSs */
	u8 hw_dev_mcs_support;
	/** Region Code */
	u16 region_code;
} fw_info;
/* Function Declaration */

/**
 *  @brief Set the BSS information into the registrar credential structure
 *  @param cur_if    Current interface
 *  @param bss       BSS information
 *
 *  @return          WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int load_cred_info(struct mwu_iface_info *cur_if, bss_config_t *bss);

/**
 *  @brief convert char to hex integer
 *
 *  @param chr      char to convert
 *  @return         hex integer or 0
 */
int hexval(s32 chr);
/**
 *  @brief convert string to mac address
 *
 *  @param s                A pointer string buffer
 *  @param mac_addr         pointer to mac address array
 *  @return					none
 */
void a2_mac_addr(char *s, u8 *mac_addr);
/*
 *  @brief Process scan results get from WLAN driver
 *
 *  @param cur_if      Current interface
 *  @param dev_pwd_id  PIN or PBC
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_wlan_scan_results(struct mwu_iface_info *cur_if, u16 dev_pwd_id);
int wlan_get_scan_results(struct mwu_iface_info *cur_if,
			  struct SCAN_RESULTS *results);

/**
 *  @brief Send user scan ioctl command to WLAN driver
 *  @param cur_if           Current interface
 *  @param go_ssid          ssid of Group Owner
 *  @param ifname           Interface name
 *  @param operation        Scan operation to be performed
 *                          Either SCAN_OP_CHANNEL or SCAN_COMMON_CHANNEL_LIST,
 *
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_wlan_set_user_scan(struct mwu_iface_info *cur_if, char *go_ssid,
			   int operation);

/**
 *  @brief Generate Random PSK
 *
 *  @param dest      A pointer to the destination character array
 *  @param length    Destination string legth.
 *  @return          None.
 *  */
void wps_wlan_generate_random_psk(char *dest, unsigned short len);
/**
 *  @brief Resets AP configuration to default OOB settings
 *
 *  @param cur_if      Current interface
 *  @return            WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_wlan_reset_ap_config_to_OOB(struct mwu_iface_info *cur_if);
/**
 *  @brief Creates new AP configuration after AP has been reset to OOB settings
 *
 *  @param cur_if      Current interface
 *  @return            WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_wlan_create_ap_config_after_OOB(struct mwu_iface_info *cur_if);
/**
 *  @brief Checks if APs configuration is same as default OOB settings
 *
 *  @param bss        A pointer to bss_config_t structure.
 *  @return            WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_check_for_default_oob_settings(bss_config_t *bss);
/**
 *  @brief Reset AP configuration for Reversed Role or OOB.
 *
 *  @param cur_if       Current interface
 *
 *  @return             WPS_STATUS_SUCCESS on success, WPS_STATUS_FAIL on fail
 */
int wps_wlan_reset_ap_config(struct mwu_iface_info *cur_if);

/**
 *  @brief  Wlan event parser for FW events
 *  @param context    Pointer to Context
 *  @param if_name    Pointer to interface name from event
 *  @param evt_buffer Pointer to Event buffer
 *  @param evt_len    Event Length
 *
 *  @return           None
 */
void wps_wlan_event_parser(void *context, char *if_name, char *evt_buffer,
			   int evt_len);

/**
 *  @brief Loads the AP configuration into data structures
 *
 *  @param cur_if     Current interface
 *
 *  @return            WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_load_uap_cred(struct mwu_iface_info *cur_if);

/**
 *  @brief Loads the STA configuration into data structures
 *
 *  @param cur_if     Current interface
 *
 *  @return            WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_load_wsta_registrar_cred(struct mwu_iface_info *cur_if);
/**
 *  @brief  Validate checksum of PIN
 *
 *  @param PIN      PIN value
 *  @return         Validation result 1 - Success 0 - Failure
 */
int ValidateChecksum(struct mwu_iface_info *cur_if, unsigned long int PIN);

/**
 *  @brief  Update SET/CLEAR the group formation bit for GO.
 *  @param  cur_if     Current interface
 *  @param  set_clear  Set or clear the bit.
 *
 *  @return  WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_wlan_update_group_formation_config(struct mwu_iface_info *cur_if,
					   int set_clear, unsigned char config);

#define MAX_WEP_KEY_LEN 27

/** uAP Protocol: Open */
#define UAP_PROTO_OPEN 1
/** uAP Protocol: WEP Static */
#define UAP_PROTO_WEP_STATIC 2
/** uAP Protocol: WPA */
#define UAP_PROTO_WPA 8
/** uAP Protocol: WPA2 */
#define UAP_PROTO_WPA2 32
/** uAP Protocol: WPA+WPA2 */
#define UAP_PROTO_MIXED 40

/** uAP Auth Mode: Open */
#define UAP_AUTH_MODE_OPEN 0
/** uAP Auth Mode: Shared Key */
#define UAP_AUTH_MODE_SHARED 1

/** uAP Cipher: None */
#define UAP_CIPH_NONE 0
/** uAP Cipher: TKIP */
#define UAP_CIPH_TKIP 4
/** uAP Cipher: AES */
#define UAP_CIPH_AES 8
/** uAP Cipher:  AES+TKIP*/
#define UAP_CIPH_AES_TKIP 12

/** WPA Passphrase length*/
#define WPA_PASSPHARE_LENGTH 64
#define MAX_PASSPHRASE_LENGTH 63

/* Scan Operation */
#define SCAN_OP_CHANNEL 1
#define SCAN_COMMON_CHANNEL_LIST 2

/** WPS IE Operation type */
/** Clear WPS IE */
#define CLEAR_WPS_IE 0x00
/** Set WPS IE */
#define SET_WPS_IE 0x10
/** Get WPS IE */
#define GET_WPS_IE 0x20

/** Probe request Enable OR Mask */
#define WPS_ORMASK_ENABLE_PROBE_REQ 0x0010
/** Probe request Disable AND Mask */
#define WPS_ANDMASK_DISABLE_PROBE_REQ 0xffef

/** WPS IE set type : session inactive */
#define WPS_AP_SESSION_INACTIVE 0x00
/** WPS IE set type : session active ap */
#define WPS_AP_SESSION_ACTIVE 0x01
/** WPS IE set type : session active sta */
#define WPS_STA_SESSION_ACTIVE 0x02
/** WPS IE set type : session inactive AP AssocResp */
#define WPS_AP_SESSION_ACTIVE_AR 0x03
/** WPS IE set type : session inactive AP ReAssocResp */
#define WPS_AP_SESSION_INACTIVE_AR 0x04

/** IE set type : wps + session active ap */
#define SET_WPS_AP_SESSION_ACTIVE_IE (SET_WPS_IE | WPS_AP_SESSION_ACTIVE)
/** IE set type : wps + session inactive ap */
#define SET_WPS_AP_SESSION_INACTIVE_IE (SET_WPS_IE | WPS_AP_SESSION_INACTIVE)
/** IE set type : wps + session active sta */
#define SET_WPS_STA_SESSION_ACTIVE_IE (SET_WPS_IE | WPS_STA_SESSION_ACTIVE)
/** IE set type : wps + session active ap */
#define SET_WPS_AP_SESSION_ACTIVE_AR_IE (SET_WPS_IE | WPS_AP_SESSION_ACTIVE_AR)
/** IE set type : wps + session active ap */
#define SET_WPS_AP_SESSION_INACTIVE_AR_IE                                      \
	(SET_WPS_IE | WPS_AP_SESSION_INACTIVE_AR)
/** IE clear type : wps + session active ap */
#define CLEAR_AR_WPS_IE (CLEAR_WPS_IE | WPS_AP_SESSION_ACTIVE_AR)

/* offset of WPSE IE in mgmt frame event */
#define WPSE_IE_OFFSET 7

/**WPS Probe Request IEEE frame contents offset:
 * FrameCtl(2), Duration(2),
 * DestAddr(6),SrcAddr(6), BSSID(6), SeqCtl(2), WLANAddr4(6) */
#define WPS_PROBE_REQ_IE_OFFSET 30

/**
 *  @brief  Process scan operation
 *
 *  @param cur_if     Current interface
 *  @return           WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_wlan_scan(struct mwu_iface_info *cur_if);

/**
 *  @brief Copy Credential data into BSS configuration
 *  @param bss       A pointer to the bss_config_t structure
 *  @param pCred     A pointer to the CREDENTIAL_DATA structure
 *
 *  @return          WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_cred_to_bss_config(bss_config_t *bss, struct CREDENTIAL_DATA *pCred,
			   u8 load_by_oob);
/**
 *  @brief mapping RF band by frequency
 *
 *  @param freq     frequency value
 *  @return         channel number
 */
int wps_wlan_freq_to_band(int freq);

/**
 *  @brief This function implements reset device state to OOB handling for
 *  1. WPS
 *  2. WPSE
 *  3. Customer specific execution.
 *  @param cur_if      Current interface
 *
 *  @return            WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_handle_reset_oob(struct mwu_iface_info *cur_if);

/**
 *  @brief convert frequency to channel
 *
 *  @param freq     frequency value
 *  @return         channel number
 */
int mapping_freq_to_chan(int freq);

int wps_wlan_sort_scan_list(struct mwu_iface_info *cur_if);

/**
 *  @brief mapping RF band by channel
 *  @param freq     frequency value
 *  @return         channel number
 */
int wps_wlan_chan_to_band(int chan);

/**
 *  @brief mapping RF band by channel
 *  @param freq     frequency value
 *  @return         channel number
 */
int wps_wlan_chan_to_band(int chan);

/**
 *  @brief Converts colon separated MAC address to hex value
 *
 *  @param mac      A pointer to the colon separated MAC string
 *  @param raw      A pointer to the hex data buffer
 *  @return         SUCCESS or FAILURE
 *                  WIFIDIR_RET_MAC_BROADCAST  - if broadcast mac
 *                  WIFIDIR_RET_MAC_MULTICAST - if multicast mac
 */
int mac2raw(char *mac, u8 *raw);

/**
 *  @brief Prints a MAC address in colon separated form from raw data
 *
 *  @param raw      A pointer to the hex data buffer
 *  @return         N/A
 */
void print_mac(u8 *raw);

/**
 *  @brief Generate UUID using local MAC address
 *
 *  @param mac_addr     A pointer to local MAC address
 *  @param wps_s        A pointer to UUID
 *  @return             None
 */
void wps_generate_uuid_using_mac_addr(const u8 *mac_addr, u8 *uuid);

void wps_wlan_scan_completion_handler(char *ifname);

/**
 * os_get_random - Get cryptographically strong pseudo random data
 * @buf: Buffer for pseudo random data
 * @len: Length of the buffer
 * Returns: 0 on success, -1 on failure
 */
int os_get_random(unsigned char *buf, size_t len);

void inc_byte_array(u8 *counter, size_t len);

#endif /* _WPS_WLAN_H_ */
