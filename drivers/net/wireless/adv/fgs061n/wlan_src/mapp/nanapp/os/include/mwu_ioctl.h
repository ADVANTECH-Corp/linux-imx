/*
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
/**
 *  mwu_ioctl.h
 *  This file contains definition for WLAN driver control/command.
 */

#ifndef __MWU_IOCTL__
#define __MWU_IOCTL__

#include "mwu_defs.h"
#include "wps_def.h"
#include "wps_wlan.h" /* for bss_config_t */
#include "mwu_key_material.h"
#include "wireless_copy.h"

/* Initial number of total private ioctl calls */
#define IW_INIT_PRIV_NUM 128
/* Maximum number of total private ioctl calls supported */
#define IW_MAX_PRIV_NUM 1024
/* Maximum number of channels that can be sent in a setuserscan ioctl */
#define MWU_IOCTL_USER_SCAN_CHAN_MAX 50
#define MWU_MAX_AP_CLIENTS 32

/*
 * Bit 0 : Assoc Req
 * Bit 1 : Assoc Resp
 * Bit 2 : ReAssoc Req
 * Bit 3 : ReAssoc Resp
 * Bit 4 : Probe Req
 * Bit 5 : Probe Resp
 * Bit 8 : Beacon
 */
/** Mask for Assoc request frame */
#define MGMT_MASK_ASSOC_REQ 0x01
/** Mask for ReAssoc request frame */
#define MGMT_MASK_REASSOC_REQ 0x04
/** Mask for Assoc response frame */
#define MGMT_MASK_ASSOC_RESP 0x02
/** Mask for ReAssoc response frame */
#define MGMT_MASK_REASSOC_RESP 0x08
/** Mask for probe request frame */
#define MGMT_MASK_PROBE_REQ 0x10
/** Mask for probe response frame */
#define MGMT_MASK_PROBE_RESP 0x20
/** Mask for beacon frame */
#define MGMT_MASK_BEACON 0x100
/** Mask to clear previous settings */
#define MGMT_MASK_CLEAR 0x000

typedef struct _mwu_ioctl_user_scan_ssid {
	/** SSID */
	char ssid[MAX_SSID_LEN + 1];
	/** Maxmimum length of SSID */
	u8 max_len;
} __ATTRIB_PACK__ mwu_ioctl_user_scan_ssid;

typedef struct _mwu_ioctl_user_scan_chan {
	/** Channel Number to scan */
	u8 chan_number;
	/** Radio type: 'B/G' Band = 0, 'A' Band = 1 */
	u8 radio_type;
	/** Scan type: Active = 1, Passive = 2 */
	u8 scan_type;
	/** Reserved */
	u8 reserved;
	/** Scan duration in milliseconds; if 0 default used */
	u32 scan_time;
} __ATTRIB_PACK__ mwu_ioctl_user_scan_chan;

typedef struct _mwu_ioctl_user_scan_cfg {
	/** Flag set to keep the previous scan table intact */
	u8 keep_previous_scan; /* Do not erase the existing scan results */
	/** BSS mode to be sent in the firmware command */
	u8 bss_mode;
	/** Configure the number of probe requests for active scans */
	u8 num_probes;
	/** Reserved */
	u8 reserved;
	/** BSSID filter sent in the firmware command to limit the results */
	u8 specific_bssid[ETH_ALEN];
	/** SSID filter list used in the firmware to limit the scan results*/
	mwu_ioctl_user_scan_ssid ssid_list[MRVDRV_MAX_SSID_LIST_LENGTH];
	/** Variable number (fixed maximum) of channels to scan up */
	mwu_ioctl_user_scan_chan chan_list[MWU_IOCTL_USER_SCAN_CHAN_MAX];
} __ATTRIB_PACK__ mwu_ioctl_user_scan_cfg;

/* associated station to AP info */
typedef struct _mwu_ap_sta_info {
	/** STA MAC address */
	unsigned char mac_address[ETH_ALEN];
	/** Power mfg status */
	unsigned char power_mfg_status;
	/** RSSI */
	char rssi;
} sta_info;

/* sta_list structure */
typedef struct _mwu_ap_sta_list {
	/** station count */
	unsigned short sta_count;
	/** station list */
	sta_info sta_info[MWU_MAX_AP_CLIENTS];
} ap_sta_list;

/** data structure for cmd bandcfg */
struct mrvl_priv_cmd_bandcfg {
	/** Infra band */
	unsigned int config_bands;
	/** Ad-hoc start band */
	unsigned int adhoc_start_band;
	/** Ad-hoc start channel */
	unsigned int adhoc_channel;
	unsigned int adhoc_chan_bandwidth;
	/** fw supported band */
	unsigned int fw_bands;
};

int mwu_apcmd_sys_reset(struct mwu_iface_info *cur_if);

/**
 *   Get private info
 *
 * params
 *     iface          A pointer to net name
 *
 * return             The number of private ioctls if success, MWU_ERR_COM if
 * fail
 */
int mwu_get_private_info(char *iface);

/**
 *   Get Sub command ioctl number
 *
 *  params
 *      i            command index
 *      priv_cnt     Total number of private ioctls available in driver
 *      ioctl_val    A pointer to return ioctl number
 *      subioctl_val A pointer to return sub-ioctl number
 *
 *  return           MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_get_subioctl_no(int i, int priv_cnt, int *ioctl_val, int *subioctl_val);

/**
 *   Get ioctl number
 *
 *  params
 *      ifname       A pointer to net name
 *      priv_cmd     A pointer to priv command buffer
 *      ioctl_val    A pointer to return ioctl number
 *      subioctl_val A pointer to return sub-ioctl number
 *
 *  return           MWU_ERR_SUCCESS--success, otherwise--fail
 */
int mwu_get_ioctl_no(char *ifname, const char *priv_cmd, int *ioctl_val,
		     int *subioctl_val);

/**
 *   Retrieve the ioctl and sub-ioctl numbers for the given ioctl string
 *
 *  param
 *      ifname       A pointer to net name
 *      ioctl_name   Private IOCTL string name
 *      ioctl_val    A pointer to return ioctl number
 *      subioctl_val A pointer to return sub-ioctl number
 *
 *  return
 *      MWU_ERR_SUCCESS   --   success
 *      otherwise-        --   MWU_ERR_COM
 *      fail
 */
int mwu_get_priv_ioctl(char *ifname, char *ioctl_name, int *ioctl_val,
		       int *subioctl_val);

/**
 *  Get associated ESSID from WLAN driver (SIOCGIWESSID)
 *
 * params
 *     ifname       A pointer to net name
 *     ssid         Buffer for the SSID, must be 32 bytes long
 *
 * return           SSID length on success, MWU_ERR_COM on failure
 */
int mwu_get_ssid(char *ifname, unsigned char *ssid);

/**
 *  Set ESSID to associate to WLAN driver (SIOCSIWESSID)
 *
 * params
 *     ifname       A pointer to net name
 *     ssid         SSID
 *     ssid_len     Length of SSID (0..32)
 *     skip_scan    Skip scan during association
 *
 * return           MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_ssid(char *ifname, const unsigned char *ssid, unsigned int ssid_len,
		 int skip_scan);

/**
 *  Set auth mode to WLAN driver (SIOCSIWAUTH)
 *
 * params
 *     ifname       A pointer to net name
 *     auth_mode    Open or shared auth mode.
 *
 * return           MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_authentication(char *ifname, int auth_mode);

/**
 *  Get associated BSSID from WLAN driver (SIOCGIWAP)
 *
 * params
 *     ifname       A pointer to net name
 *     bssid        Buffer for the BSSID
 *
 * return           MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_get_wap(char *ifname, unsigned char *bssid);

/**
 *  Set BSSID to associate to WLAN driver (SIOCSIWAP)
 *
 * params
 *     ifname       A pointer to net name
 *     bssid        BSSID
 *
 * return           MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_wap(char *ifname, const unsigned char *bssid);

/**
 *  Set wireless mode to WLAN driver (infra/adhoc) (SIOCSIWMODE)
 *
 * params
 *     ifname       A pointer to net name
 *     mode         0 = infra/BSS (associate with an AP), 1 = adhoc/IBSS
 *
 * return           MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_mode(char *ifname, int mode);

/**
 *  Get interface flags from WLAN driver (SIOCGIFFLAGS)
 *
 * params
 *     ifname        A pointer to net name
 *     flags         Pointer to returned flags value
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_get_ifflags(char *ifname, int *flags);

/**
 *  Set interface flags to WLAN driver (SIOCSIFFLAGS)
 *
 * params
 *     ifname        A pointer to net name
 *     flags         New value for flags
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_ifflags(char *ifname, int flags);

/**
 *  Bring interface up
 *
 * params
 *     ifname        A pointer to net name
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_bring_if_up(char *ifname);

/**
 *  Set private ioctl command "deauth" to WLAN driver
 *
 * params
 *     ifname        A pointer to net name
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_deauth(char *ifname);

/**
 *  Process and send ie config command for (re)assoc request
 *
 * params
 *     ifname        A pointer to net name
 *     flag          FLAG: Set/clear WPS IE
 *     pie_index     A pointer to the IE buffer index
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_assoc_request_ie_config(char *ifname, int flag, short *pie_index);

/**
 *  Process and send ie config command for probe request
 *
 * params
 *     ifname        A pointer to net name
 *     flag          FLAG: Set/clear WPS IE
 *     pie_index     A pointer to the IE buffer index
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_prob_request_ie_config(char *ifname, int flag, short *pie_index);

/**
 *  Get BSS config for AP
 *
 * params
 *     ifname        A pointer to net name
 *     bss           BSS config
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_apcmd_get_bss_config(char *ifname, bss_config_t *bss);

/**
 *  Set BSS config for AP
 *
 * params
 *     ifname        A pointer to net name
 *     bss           BSS config
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_apcmd_set_bss_config(char *ifname, bss_config_t *bss);

/**
 *  Get country code
 *
 * params
 *     ifname        A pointer to net name
 *     countrycode   countrycode string
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_apcmd_get_countrycode(char *ifname, u8 *countrycode);

/**
 *  Check bss config
 *
 * params
 *     ifname        A pointer to net name
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_check_bss_config(char *ifname);

/**
 *  Start the existing AP BSS
 *
 * params
 *     ifname        A pointer to net name
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_apcmd_start_bss(char *ifname);

/**
 *  Stop the existing AP BSS
 *
 * params
 *     ifname        A pointer to net name
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_apcmd_stop_bss(char *ifname);

/**
 *  Performs the ioctl operation to set mac address to kernel (SIOCSIFHWADDR)
 *
 * params
 *     ifname        A pointer to net name
 *     mac           Mac address to set
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_intended_mac_addr(char *ifname, unsigned char *mac);

/**
 *  Performs the ioctl operation to get mac address to kernel (SIOCGIFHWADDR)
 *
 * params
 *     ifname        A pointer to net name
 *     mac           Mac address to get
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_get_mac_addr(char *ifname, unsigned char *mac);

/**
 *  Send Custom_IE command to driver
 *
 * params
 *     ifname        A pointer to net name
 *     buf           Pointer to data buffer
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_custom_ie_config(char *ifname, unsigned char *buf);

/**
 *  Send HostCmd command to driver for WIFIDIR
 *
 * params
 *     ifname        A pointer to net name
 *     buf           Pointer to data buffer
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_hostcmd(char *ifname, unsigned char *buf);

/**
 *  Send PrivCmd command to driver for WIFIDIR
 *
 * params
 *     ifname        A pointer to net name
 *     buf           Pointer to data buffer
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_privcmd(char *ifname, unsigned char *buf);

/**
 *  Set BSS role (STA/uAP)
 *
 * params
 *     ifname        A pointer to net name
 *     bss_role      An integer 0: Sta, 1:uAP
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_bss_role(char *ifname, int bss_role);

/**
 *  Get BSS role
 *
 * params
 *     ifname        A pointer to net name
 *     bss_role      Pointer to return BSS type
 */
int mwu_get_bss_role(char *ifname, int *bss_role);

/**
 *  Set deepsleep (enable/disable)
 *
 * params
 *     ifname        A pointer to net name
 *     enable        1 -- enable, 0 -- disable
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_deepsleep(char *ifname, u16 enable);

/**
 *  Process and send ie config command
 *
 * params
 *     ifname        A pointer to net name
 *     flag          FLAG: Set/clear WPS IE
 *     pie_index     A pointer to the IE buffer index
 *     ie_type       Type of IE (This is array index.
 *                   Mostly for mwu internal housekeeping)
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_ie_config(char *ifname, int flag, short *pie_index, int ie_type);

/**
 *  Set private ioctl command "wpssession" to WLAN driver
 *
 * params
 *     ifname        A pointer to net name
 *     enable        0 - WPS Session Disable, 1 - WPS Session Enable
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_session_control(char *ifname, int enable);

/**
 *  Get power management mode of WLAN driver (SIOCGIWPOWER)
 *
 * params
 *     ifname        A pointer to net name
 *     enable        Pointer of returned buffer
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_get_power_mode(char *ifname, int *enable);

/**
 *  Set power management mode of WLAN driver (SIOCGIWPOWER)
 *
 * params
 *     ifname        A pointer to net name
 *     enable        0 = Disable PS mode, 1 = Enable PS mode
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_power_mode(char *ifname, int enable);

/**
 *  Set passphrase of WLAN driver
 *
 * params
 *     ifname        A pointer to net name
 *     buf           buffer of passphrase string
 *     buf_len       length of buffer
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_passphrase(char *ifname, char *buf, int buf_len);

/**
 *  Configure a hard-coded bgscan config for use by wifidir
 *  Borrowed from mlanconfig.c and bg_scan_wifidir.conf. This function is hard
 *  coded for our WIFIDIR set up and may not be suitable for all applications.
 */
int mwu_mlanconfig_bgscan(char *ifname);

/**
 *  Set user scan of WLAN driver
 *
 *  Original setuserscan can take many parameters (refer to
 * mwu_ioctl_user_scan_cfg). As we just need to configure channel right now, the
 * function is simplified on purpose.
 *
 * params
 *     ifname        A pointer to net name
 *     chan_list     channel list on which to scan
 *     ssid          ssid to direct probe req (usually a WC ssid like DIRECT-*)
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_user_scan(char *ifname, int chan_list[MWU_IOCTL_USER_SCAN_CHAN_MAX],
		      char *ssid);

/**
 *  Set uap command "sta_deauth_ext" to WLAN driver
 *
 * params
 *     ifname        A pointer to net name
 *     sta_mac       MAC address of STA
 *
 * return            MWU_ERR_SUCCESS--success, MWU_ERR_COM--fail
 */
int mwu_set_ap_deauth(char *ifname, unsigned char *sta_mac);

/**
 *  Get uap command "sta_list" to wlan driver
 *
 *  params
 *      ifname      A pointer to interface name
 *      sta_list    structure poiter to hold sta_list
 *
 *  return          MWU_ERR_SUCCESS - Success
 *                  MWU_ERR_COM     - Fail
 */
int mwu_get_ap_sta_list(char *ifname, struct AP_STA_LIST *list);

/**
 * Get uap command "bss_config" to wlan driver
 */
int mwu_apcmd_get_bss_config(char *ifname, bss_config_t *bss);

/**
 * Get IE index of specific type
 *
 * Params
 *     ifname      A pointer to interface name
 *     type        The type of the index
 *     ie_idx      A pointer to store fetched index
 *
 * return          MWU_ERR_SUCCESS - Success
 *                 otherwise       - Fail
 */
int mwu_get_ie_index(char *ifname, int type, short *ie_idx);

/**
 * Set IE index of specific type
 *
 * Params
 *     ifname      A pointer to interface name
 *     type        The type of the index
 *     ie_idx      Value of IE index to set
 *
 * return          MWU_ERR_SUCCESS - Success
 *                 otherwise       - Fail
 */
int mwu_set_ie_index(char *ifname, int type, short ie_idx);

/**
 * Clear IE buffer by specific IE index
 *
 * Params
 *     ifname      A pointer to interface name
 *     type        The type of the index
 *
 * return          MWU_ERR_SUCCESS - Success
 *                 otherwise       - Fail
 */
int mwu_clear_ie(char *ifname, int type);

/**
 * Set specific IE buffer using flag
 *
 * Params
 *     ifname      A pointer to interface name
 *     ie_type        The type of the index
 *     flag        Flag of set WPS IE
 *
 * return          MWU_ERR_SUCCESS - Success
 *                 otherwise       - Fail
 */
int mwu_set_ie(char *ifname, int ie_type, int flag);

/**
 * Get range
 *
 * Params
 *     ifname      A pointer to interface name
 *
 * return          we_version_compiled value; < 0 if failed.
 */
int mwu_get_range(char *ifname);

/**
 * Send ifreq
 *
 * Params
 *     ifname      A pointer to interface name
 *     ifr         ioctl request structure
 *     ioctl_val   ioctl value
 * return          MWU_ERR_SUCCESS - Success
 *                 otherwise       - Fail
 */
int send_ifreq_ioctl(const char *ifname, struct ifreq *ifr, int ioctl_val);

/* convinience function to set both beacon and probe response IEs to session
 * active and inactive */
int mwu_set_beacon_probe_resp_ie(struct mwu_iface_info *cur_if, int flag);

/* convinience function to clear both beacon and probe response IEs to session
 * active and inactive */
int mwu_clear_beacon_probe_resp_ie(struct mwu_iface_info *cur_if);

/* convinience funciton to prepare the MRVL_CMD - Prepends MRVL_CMD and cmd str
 * to buffer, populates the command header length*/
int prepare_buffer(u8 *buffer, char *cmd, u16 *mrvl_header_len);

/* Convinience function to set firmware managed IEs (P2P and WPS) for P2P
 * operation
 *
 * params
 * cur_if           pointer to mwu_iface_info struct
 * ie_type          type of IE to set (can be IE_CFG_WIFIDIR or
 * IE_CFG_WIFIDIR_WPS) buf              pointer to buffer data buf_len length of
 * buffer to set mrvl_header_len  length of header (usually MRVL_CMD + <command
 * str>)
 *
 * return
 * MWU_ERR_SUCCESS  success
 * MWU_ERR_COM      fail
 *
 * */
int mwu_set_fw_managed_ie(struct mwu_iface_info *cur_if, int ie_type, u8 *buf,
			  u16 buf_len, u16 mrvl_header_len);
int mwu_get_bandcfg(char *ifname, int *rf_bands);
int mwu_get_fw_info(char *ifname, fw_info *pfw_info);
#endif
