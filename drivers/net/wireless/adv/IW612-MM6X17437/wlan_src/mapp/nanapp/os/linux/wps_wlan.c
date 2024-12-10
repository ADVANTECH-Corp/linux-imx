/** @file wps_wlan.c
 *  @brief This file contains functions for WLAN driver control/command.
 *
 *  Copyright 2012-2020 NXP
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

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#if defined(ANDROID) && !defined(HAVE_GLIBC)
#include <linux/if.h>
#endif
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <net/if_arp.h> /* For ARPHRD_ETHER */
#include <errno.h>
#if defined(ANDROID) && !defined(HAVE_GLIBC)
#include <linux/wireless.h>
#else
#include "wireless_copy.h"
#endif
#include <arpa/inet.h>

#define UTIL_LOG_TAG "WPS_WLAN"
#include "wps_msg.h"
#include "wps_def.h"
#include "wps_wlan.h"
#include "wps_os.h"
#include "wlan_hostcmd.h"
#include "mwu_log.h"
#include "util.h"
//#include    "wlan_wifidir.h"
//#include    "wifidisplay.h"
//#include    "wifidir_sm.h"
#include "mwu_timer.h"
#include "mwu_ioctl.h"
#include "mwu.h"
//#include    "wifidir_lib.h"
#ifdef CONFIG_HOTSPOT
#include "mhotspotmod_mwu.h"
#endif
#ifdef ANDROID
#undef perror
#define perror(str) mwu_printf(MSG_ERROR, str ": %s\n", strerror(errno))
#endif

#include "nan_lib.h"

#include "mlocation_lib.h"
#include "mlocation_api.h"

#ifdef CONFIG_HOTSPOT
/* @TODO: need to find a correct place for following declarations */
extern u8 hotspot_is_init(void);
extern u8 hotspot_is_ifname(char *ifname);
extern void hotspot_driver_event(char *ifname, u8 *buffer, u16 size);
#endif

/** Interface name */
char dev_name[IFNAMSIZ + 1] = "mlan0";
/* @TODO: Remove extern */
extern int mwpsmod_prepare_scan_done_event(struct mwu_iface_info *cur_if);
/** Chan-Freq mapping table */
typedef struct _CHANNEL_FREQ_ENTRY {
	/** Channel Number */
	int Channel;
	/** Frequency of this Channel */
	int Freq;
} CHANNEL_FREQ_ENTRY;

/** Initial number of total private ioctl calls */
#define IW_INIT_PRIV_NUM 128
/** Maximum number of total private ioctl calls supported */
#define IW_MAX_PRIV_NUM 1024
/** MRV Event point off*/
#define MRV_EV_POINT_OFF                                                       \
	(((char *)&(((struct iw_point *)NULL)->length)) - (char *)NULL)

/** Convert character to integer */
#define CHAR2INT(x) (((x) >= 'A') ? ((x) - 'A' + 10) : ((x) - '0'))

#define FLUSH_PREV_RESULT 0
#define KEEP_PREV_RESULT 1

#define BUF_HEADER_SIZE 4

/********************************************************
	Local Variables
********************************************************/
/** Channels for 802.11b/g */
static CHANNEL_FREQ_ENTRY channel_freq_UN_BG[] = {
	{1, 2412},  {2, 2417},	{3, 2422},  {4, 2427}, {5, 2432},
	{6, 2437},  {7, 2442},	{8, 2447},  {9, 2452}, {10, 2457},
	{11, 2462}, {12, 2467}, {13, 2472}, {14, 2484}};

/** Channels for 802.11a/j */
static CHANNEL_FREQ_ENTRY channel_freq_UN_AJ[] = {
	{34, 5170},  {38, 5190},  {42, 5210},  {46, 5230},  {36, 5180},
	{40, 5200},  {44, 5220},  {48, 5240},  {52, 5260},  {56, 5280},
	{60, 5300},  {64, 5320},  {100, 5500}, {104, 5520}, {108, 5540},
	{112, 5560}, {116, 5580}, {120, 5600}, {124, 5620}, {128, 5640},
	{132, 5660}, {136, 5680}, {140, 5700}, {149, 5745}, {153, 5765},
	{157, 5785}, {161, 5805}, {165, 5825},
	/*  {240, 4920},
	    {244, 4940},
	    {248, 4960},
	    {252, 4980},
	channels for 11J JP 10M channel gap */
};

/********************************************************
	Global Variables
********************************************************/

/********************************************************
	Local Functions
********************************************************/
/**
 *  @brief convert char to hex integer
 *
 *  @param chr      char to convert
 *  @return         hex integer or 0
 */
int hexval(s32 chr)
{
	if (chr >= '0' && chr <= '9')
		return chr - '0';
	if (chr >= 'A' && chr <= 'F')
		return chr - 'A' + 10;
	if (chr >= 'a' && chr <= 'f')
		return chr - 'a' + 10;

	return 0;
}

/**
 *  @brief convert char to hex integer
 *
 *  @param chr      char
 *  @return         hex integer
 */
char hexc2bin(char chr)
{
	if (chr >= '0' && chr <= '9')
		chr -= '0';
	else if (chr >= 'A' && chr <= 'F')
		chr -= ('A' - 10);
	else if (chr >= 'a' && chr <= 'f')
		chr -= ('a' - 10);

	return chr;
}

/**
 *  @brief convert string to hex integer
 *
 *  @param s        A pointer string buffer
 *  @return         hex integer
 */
u32 a2hex(char *s)
{
	u32 val = 0;
	if (!strncasecmp("0x", s, 2)) {
		s += 2;
	}

	while (*s && isxdigit(*s)) {
		val = (val << 4) + hexc2bin(*s++);
	}

	return val;
}

/**
 *  @brief convert string to mac address
 *
 *  @param s                A pointer string buffer
 *  @param mac_addr         pointer to mac address array
 *  @return					none
 */
void a2_mac_addr(char *s, u8 *mac_addr)
{
	u32 val = 0, count = 0;
	if (!strncasecmp("0x", s, 2)) {
		s += 2;
	}

	while (*s && isxdigit(*s)) {
		val = (val << 4) + hexc2bin(*s++);
		val = (val << 4) + hexc2bin(*s++);
		mac_addr[count] = val;
		count++;
		val = 0;
	}

	return;
}

/**
 *  @brief convert string to integer
 *
 *  @param ptr      A pointer to data buffer
 *  @param chr      A pointer to return integer
 *  @return         A pointer to next data field
 */
s8 *convert2hex(s8 *ptr, u8 *chr)
{
	u8 val;

	for (val = 0; *ptr && isxdigit(*ptr); ptr++) {
		val = (val * 16) + hexval(*ptr);
	}

	*chr = val;

	return ptr;
}

int mapping_freq_to_chan(int freq)
{
	int i, table;
	int chan = 0;

	table = sizeof(channel_freq_UN_BG) / sizeof(CHANNEL_FREQ_ENTRY);
	for (i = 0; i < table; i++) {
		if (freq == channel_freq_UN_BG[i].Freq) {
			chan = channel_freq_UN_BG[i].Channel;
			break;
		}
	}

	table = sizeof(channel_freq_UN_AJ) / sizeof(CHANNEL_FREQ_ENTRY);
	for (i = 0; i < table; i++) {
		if (freq == channel_freq_UN_AJ[i].Freq) {
			chan = channel_freq_UN_AJ[i].Channel;
			break;
		}
	}

	return chan;
}

int mapping_chan_to_freq(int chan)
{
	int i, table;
	int freq = 0;

	table = sizeof(channel_freq_UN_BG) / sizeof(CHANNEL_FREQ_ENTRY);
	for (i = 0; i < table; i++) {
		if (chan == channel_freq_UN_BG[i].Channel) {
			freq = channel_freq_UN_BG[i].Freq;
			break;
		}
	}

	table = sizeof(channel_freq_UN_AJ) / sizeof(CHANNEL_FREQ_ENTRY);
	for (i = 0; i < table; i++) {
		if (chan == channel_freq_UN_AJ[i].Channel) {
			freq = channel_freq_UN_AJ[i].Freq;
			break;
		}
	}

	return freq;
}

/*
 *  @brief convert hex char to integer
 *
 *  @param c        Hex char
 *  @return         Integer value or WPS_STATUS_FAIL
 */
int hex2num(s8 c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return WPS_STATUS_FAIL;
}

/*
 *  @brief convert integer to hex char
 *
 *  @param c        Integer value or WPS_STATUS_FAIL
 *  @return         Hex char
 */

int num2hex(s8 c)
{
	if ((c + '0') >= '0' && (c + '0') <= '9')
		return c + '0';
	if ((c - 10 + 'a') >= 'a' && (c - 10 + 'a') <= 'f')
		return c - 10 + 'a';
	if ((c - 10 + 'A') >= 'A' && (c - 10 + 'A') <= 'F')
		return c - 10 + 'A';
	return WPS_STATUS_FAIL;
}

/*
 *  @brief convert hex char to integer
 *
 *  @param c        char
 *  @return         Integer value or WPS_STATUS_FAIL
 */
static int hex2byte(const s8 *hex)
{
	s32 a, b;
	a = hex2num(*hex++);
	if (a < 0)
		return WPS_STATUS_FAIL;
	b = hex2num(*hex++);
	if (b < 0)
		return WPS_STATUS_FAIL;
	return (a << 4) | b;
}

/*
 *  @brief convert hex char to integer
 *
 *  @param hex      A pointer to hex string
 *  @param buf      buffer to storage the data
 *  @param len
 *  @return         WPS_STATUS_SUCCESS--success, otherwise --fail
 */
int hexstr2bin(const char *hex, u8 *buf, size_t len)
{
	s32 a;
	u32 i;
	const s8 *ipos = (s8 *)hex;
	u8 *opos = buf;

	for (i = 0; i < len; i++) {
		a = hex2byte(ipos);
		if (a < 0)
			return WPS_STATUS_FAIL;
		*opos++ = a;
		ipos += 2;
	}
	return WPS_STATUS_SUCCESS;
}

/*
 *  @brief convert binary data to Hex string
 *
 *  @param bin      A pointer to binary data
 *  @param hex      A pointer to hex string
 *  @param len
 *  @return         WPS_STATUS_SUCCESS--success, otherwise --fail
 */

int bin2hexstr(const u8 *bin, s8 *hex, size_t len)
{
	u32 i;

	for (i = 0; i < len; i++) {
		hex[2 * i] = num2hex(bin[i] >> 4);
		hex[(2 * i) + 1] = num2hex(bin[i] & 0x0f);
	}

	return WPS_STATUS_SUCCESS;
}

/**
 *  @brief Convert RSSI to quality
 *
 *  @param rssi     RSSI in dBm
 *
 *  @return         Quality of the link (0-5)
 */
static u8 mwu_rssi_to_quality(s16 rssi)
{
/** Macro for RSSI range */
#define MWU_RSSI_NO_SIGNAL -90
#define MWU_RSSI_VERY_LOW -80
#define MWU_RSSI_LOW -70
#define MWU_RSSI_GOOD -60
#define MWU_RSSI_VERY_GOOD -50
#define MWU_RSSI_INVALID 0
	if (rssi <= MWU_RSSI_NO_SIGNAL || rssi == MWU_RSSI_INVALID)
		return 0;
	else if (rssi <= MWU_RSSI_VERY_LOW)
		return 1;
	else if (rssi <= MWU_RSSI_LOW)
		return 2;
	else if (rssi <= MWU_RSSI_GOOD)
		return 3;
	else if (rssi <= MWU_RSSI_VERY_GOOD)
		return 4;
	else
		return 5;
}

#if 0
/**
 *  @brief Converts colon separated MAC address to hex value
 *
 *  @param mac      A pointer to the colon separated MAC string
 *  @param raw      A pointer to the hex data buffer
 *  @return         SUCCESS or FAILURE
 *                  WIFIDIR_RET_MAC_BROADCAST  - if broadcast mac
 *                  WIFIDIR_RET_MAC_MULTICAST - if multicast mac
 */
int
mac2raw(char *mac, u8 * raw)
{
    unsigned int temp_raw[ETH_ALEN];
    int num_tokens = 0;
    int i;
    if (strlen(mac) != ((2 * ETH_ALEN) + (ETH_ALEN - 1))) {
        return FAILURE;
    }
    num_tokens = sscanf(mac, "%2x:%2x:%2x:%2x:%2x:%2x",
                        temp_raw + 0, temp_raw + 1, temp_raw + 2, temp_raw + 3,
                        temp_raw + 4, temp_raw + 5);
    if (num_tokens != ETH_ALEN) {
        return FAILURE;
    }
    for (i = 0; i < num_tokens; i++)
        raw[i] = (u8) temp_raw[i];

    if (memcmp(raw, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) == 0) {
        return WIFIDIR_RET_MAC_BROADCAST;
    } else if (raw[0] & 0x01) {
        return WIFIDIR_RET_MAC_MULTICAST;
    }
    return SUCCESS;
}

/**
 *  @brief Prints a MAC address in colon separated form from raw data
 *
 *  @param raw      A pointer to the hex data buffer
 *  @return         N/A
 */
void
print_mac(u8 * raw)
{
    printf("%02x:%02x:%02x:%02x:%02x:%02x", (unsigned int) raw[0],
           (unsigned int) raw[1], (unsigned int) raw[2], (unsigned int) raw[3],
           (unsigned int) raw[4], (unsigned int) raw[5]);
    return;
}
#endif

/**
 *  @brief          Get scan result from WLAN driver
 *
 *  @param cur_if   Current interface
 *  @param results  A pointer to scan result buffer
 *  @return         Number of scan if success, WPS_STATUS_FAIL if fail
 */
int wlan_get_scan_results(struct mwu_iface_info *cur_if,
			  struct SCAN_RESULTS *results)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *temp = NULL;
	u8 *buffer = NULL;
	u8 *pcurrent;
	u8 *pnext;
	u32 apNum = 0;
	u8 ie_len = 0;
	unsigned int scan_start;
	int idx = 0, ret = 0;
	IEEEtypes_ElementId_e *pelement_id;
	u8 *pelement_len;
	char ssid[33];
	u16 cap_info;
	u8 tsf[8];
	u16 beacon_interval;

	IEEEtypes_VendorSpecific_t *pwpa_ie;
	const u8 wpa_oui[4] = {0x00, 0x50, 0xf2, 0x01};

	IEEEtypes_VendorSpecific_t *pwps_ie;
	const u8 wps_oui[4] = {0x00, 0x50, 0xf2, 0x04};

	IEEEtypes_VendorSpecific_t *pwifidirect_ie;
	const u8 wifidirect_oui[4] = {0x50, 0x6F, 0x9A, 0x09};

	IEEEtypes_Generic_t *prsn_ie;
	wlan_ioctl_get_scan_table_info *prsp_info;
	wlan_get_scan_table_fixed fixed_fields;
	u32 fixed_field_length = 0;
	s32 bss_info_length = 0;

	ENTER();

	mrvl_cmd = (mrvl_priv_cmd *)malloc(sizeof(mrvl_priv_cmd));
	if (!mrvl_cmd) {
		mwu_printf(MSG_ERROR,
			   "ERR:Cannot allocate buffer for command!\n");
		return WPS_STATUS_FAIL;
	}
	memset(mrvl_cmd, 0, sizeof(mrvl_priv_cmd));

	buffer = (u8 *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (!buffer) {
		mwu_printf(MSG_ERROR,
			   "ERR:Cannot allocate buffer for command!\n");
		return WPS_STATUS_FAIL;
	}

	memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	scan_start = 1;

	/* Try to read the results */
	do {
		/* Fill up buffer */
		mrvl_cmd->buf = buffer;
		mrvl_cmd->used_len = 0;
		mrvl_cmd->total_len = MRVDRV_SIZE_OF_CMD_BUFFER;

		/* Copy NXP command string */
		temp = mrvl_cmd->buf;
		strncpy((char *)temp, CMD_NXP, strlen(CMD_NXP));
		temp += (strlen(CMD_NXP));
		/* Insert command string*/
		strncpy((char *)temp, PRIV_CMD_GET_SCAN_TABLE,
			strlen(PRIV_CMD_GET_SCAN_TABLE));
		temp += (strlen(PRIV_CMD_GET_SCAN_TABLE));

		prsp_info = (wlan_ioctl_get_scan_table_info
				     *)(buffer + strlen(CMD_NXP) +
					strlen(PRIV_CMD_GET_SCAN_TABLE));
		prsp_info->scan_number = scan_start;

		if ((ret = mwu_privcmd(cur_if->ifname, (u8 *)mrvl_cmd)) != 0) {
			mwu_printf(MSG_ERROR, "Get scan results failed !\n");
			FREE(mrvl_cmd);
			FREE(buffer);
			LEAVE();
			return WPS_STATUS_FAIL;
		} else {
			/* We have the results, go to process them */
			mwu_printf(DEBUG_WLAN,
				   "Scan results ready, process them ... ");
		}

		prsp_info = (wlan_ioctl_get_scan_table_info *)buffer;
		pcurrent = 0;
		pnext = prsp_info->scan_table_entry_buf;
		for (idx = 0; (unsigned int)idx < prsp_info->scan_number;
		     idx++) {
			/*
			 * Set pcurrent to pnext in case pad bytes are at the
			 * end of the last IE we processed.
			 */
			pcurrent = pnext;

			memcpy((u8 *)&fixed_field_length, (u8 *)pcurrent,
			       sizeof(fixed_field_length));
			pcurrent += sizeof(fixed_field_length);

			memcpy((u8 *)&bss_info_length, (u8 *)pcurrent,
			       sizeof(bss_info_length));
			pcurrent += sizeof(bss_info_length);

			memcpy((u8 *)&fixed_fields, (u8 *)pcurrent,
			       sizeof(fixed_fields));
			pcurrent += fixed_field_length;

			/* Set next to be the start of the next scan entry */
			pnext = pcurrent + bss_info_length;

			/* Copy bssid */
			memcpy(results[scan_start + idx - 1].bssid,
			       fixed_fields.bssid, ETH_ALEN);
			/* Copy frequency */
			results[scan_start + idx - 1].freq =
				mapping_chan_to_freq(fixed_fields.channel);
			/* Copy quality */
			results[scan_start + idx - 1].qual =
				mwu_rssi_to_quality(fixed_fields.rssi);
			/* Copy noise */
			results[scan_start + idx - 1].noise =
				MRVDRV_NF_DEFAULT_SCAN_VALUE;
			/* Copy level */
			results[scan_start + idx - 1].level =
				0x100 + fixed_fields.rssi;

			if (bss_info_length >=
			    (sizeof(tsf) + sizeof(beacon_interval) +
			     sizeof(cap_info))) {
				/* Time stamp is 8 byte long */
				memcpy(tsf, pcurrent, sizeof(tsf));
				pcurrent += sizeof(tsf);
				bss_info_length -= sizeof(tsf);

				/* Beacon interval is 2 byte long */
				memcpy(&beacon_interval, pcurrent,
				       sizeof(beacon_interval));
				/* Endian convert needed here */
				beacon_interval =
					wlan_le16_to_cpu(beacon_interval);
				pcurrent += sizeof(beacon_interval);
				bss_info_length -= sizeof(beacon_interval);

				/* Capability information is 2 byte long */
				memcpy(&cap_info, pcurrent, sizeof(cap_info));
				/* Endian convert needed here */
				cap_info = wlan_le16_to_cpu(cap_info);
				memcpy(&results[scan_start + idx - 1].caps,
				       &cap_info, sizeof(cap_info));
				pcurrent += sizeof(cap_info);
				bss_info_length -= sizeof(cap_info);
			}

			memset(ssid, 0, MAX_SSID_LEN + 1);
			while (bss_info_length >= 2) {
				pelement_id = (IEEEtypes_ElementId_e *)pcurrent;
				pelement_len = pcurrent + 1;
				pcurrent += 2;

				switch (*pelement_id) {
				case SSID:
					if (*pelement_len &&
					    *pelement_len <= MAX_SSID_LEN) {
						memcpy(results[scan_start +
							       idx - 1]
							       .ssid,
						       pcurrent, *pelement_len);
						results[scan_start + idx - 1]
							.ssid_len =
							*pelement_len;
						apNum++;
					}
					break;
				case WPA_IE:
					/* WPA IE */
					pwpa_ie = (IEEEtypes_VendorSpecific_t *)
						pelement_id;
					ie_len = sizeof(IEEEtypes_Header_t) +
						 pwpa_ie->vend_hdr.len;
					if ((memcmp(pwpa_ie->vend_hdr.oui,
						    wpa_oui,
						    sizeof(pwpa_ie->vend_hdr
								   .oui)) ==
					     0) &&
					    (pwpa_ie->vend_hdr.oui_type ==
					     wpa_oui[3])) {
						memcpy(results[scan_start +
							       idx - 1]
							       .wpa_ie,
						       pwpa_ie, ie_len);
						results[scan_start + idx - 1]
							.wpa_ie_len = ie_len;
					} else {
						/* WPS IE */
						pwps_ie =
							(IEEEtypes_VendorSpecific_t
								 *)pelement_id;
						ie_len =
							sizeof(IEEEtypes_Header_t) +
							pwps_ie->vend_hdr.len;
						if ((memcmp(pwps_ie->vend_hdr
								    .oui,
							    wps_oui,
							    sizeof(pwps_ie->vend_hdr
									   .oui)) ==
						     0) &&
						    (pwps_ie->vend_hdr.oui_type ==
						     wps_oui[3])) {
							memcpy(results[scan_start +
								       idx - 1]
								       .wps_ie,
							       pwps_ie, ie_len);
							results[scan_start +
								idx - 1]
								.wps_ie_len =
								ie_len;
						} else {
							/* WIFIDIRECT IE */
							pwifidirect_ie =
								(IEEEtypes_VendorSpecific_t
									 *)
									pelement_id;
							ie_len =
								sizeof(IEEEtypes_Header_t) +
								pwifidirect_ie
									->vend_hdr
									.len;
							if ((memcmp(pwifidirect_ie
									    ->vend_hdr
									    .oui,
								    wifidirect_oui,
								    sizeof(pwifidirect_ie
										   ->vend_hdr
										   .oui)) ==
							     0) &&
							    (pwifidirect_ie
								     ->vend_hdr
								     .oui_type ==
							     wifidirect_oui[3])) {
								memcpy(results[scan_start +
									       idx -
									       1]
									       .wifidir_ie,
								       pwifidirect_ie,
								       ie_len);
								results[scan_start +
									idx - 1]
									.wifidir_ie_len =
									ie_len;
							}
						}
					}
					break;

				case EXTENDED_CAP:
					prsn_ie = (IEEEtypes_Generic_t *)
						pelement_id;
					ie_len = prsn_ie->ieee_hdr.Len;
					memcpy(results[scan_start + idx - 1]
						       .extended_cap,
					       prsn_ie->data, ie_len);
					mwu_hexdump(MSG_ERROR, "EXT CAP",
						    (const unsigned char *)
							    prsn_ie->data,
						    ie_len);
					break;

				case RSN_IE:
					/* RSN IE */
					prsn_ie = (IEEEtypes_Generic_t *)
						pelement_id;
					ie_len = sizeof(IEEEtypes_Header_t) +
						 prsn_ie->ieee_hdr.Len;
					memcpy(results[scan_start + idx - 1]
						       .rsn_ie,
					       prsn_ie, ie_len);
					results[scan_start + idx - 1]
						.rsn_ie_len = ie_len;
					break;
				default:
					break;
				}
				pcurrent += *pelement_len;
				bss_info_length -= (2 + *pelement_len);
			}
		}
		scan_start += prsp_info->scan_number;

	} while (prsp_info->scan_number);

	FREE(mrvl_cmd);
	FREE(buffer);

	LEAVE();
	return apNum;
}

/********************************************************
	Global Functions
********************************************************/

/**
 *  @brief Configure scan parameters
 *
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
static int wps_wlan_scan_cfg(struct mwu_iface_info *cur_if,
			     wlan_ioctl_scan_cfg *scan_cfg)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	unsigned char *buf = NULL, *temp = NULL;
	int ret = WPS_STATUS_SUCCESS, mrvl_header_len = 0;

	ENTER();

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL) {
		LEAVE();
		return MWU_ERR_NOMEM;
	}

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_SCANCFG);

	/* Fill up buffer */
	mrvl_cmd = (mrvl_priv_cmd *)buf;
	mrvl_cmd->buf = buf + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len =
		MRVDRV_SIZE_OF_CMD_BUFFER - mrvl_header_len - BUF_HEADER_SIZE;
	/* Copy NXP command string */
	temp = mrvl_cmd->buf;
	strncpy((char *)temp, CMD_NXP, strlen(CMD_NXP));
	temp += (strlen(CMD_NXP));
	/* Insert command string*/
	strncpy((char *)temp, PRIV_CMD_SCANCFG, strlen(PRIV_CMD_SCANCFG));
	temp += (strlen(PRIV_CMD_SCANCFG));

	/* Prepare command parameters */
	snprintf((char *)temp, mrvl_cmd->total_len - sizeof(mrvl_priv_cmd),
		 "%d %d %d %d %d %d", scan_cfg->scan_type, scan_cfg->scan_mode,
		 scan_cfg->scan_probe, scan_cfg->specific_scan_time,
		 scan_cfg->active_scan_time, scan_cfg->passive_scan_time);

	ret = mwu_privcmd(cur_if->ifname, (u8 *)mrvl_cmd);

	if (buf)
		free(buf);

	LEAVE();
	return ret;
}

/**
 *  @brief Send scan ioctl command to WLAN driver
 *
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
static int wps_wlan_set_scan(struct mwu_iface_info *cur_if, int rf_band,
			     u8 prev_results)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	unsigned char *buf = NULL, *temp = NULL;
	int ret = WPS_STATUS_SUCCESS, mrvl_header_len = 0;
	wlan_ioctl_user_scan_cfg scan_req;
	u8 temp_rf_band = 0;
	short probe_ie_idx = -1;

	ENTER();

	memset(&scan_req, 0x00, sizeof(scan_req));

	if (prev_results == KEEP_PREV_RESULT)
		scan_req.keep_previous_scan = prev_results;

	if (rf_band == RF_24_G) {
		temp_rf_band = cur_if->pwps_info->enrollee.rf_bands;
		/* Update Probe Request IE */
		cur_if->pwps_info->enrollee.rf_bands = RF_24_G;

		if (mwu_get_ie_index(cur_if->ifname, IE_CFG_PROBE,
				     &probe_ie_idx) != MWU_ERR_SUCCESS) {
			ERR("Failed to get Probe Request IE index.\n");
			/* Restore original band */
			cur_if->pwps_info->enrollee.rf_bands = temp_rf_band;
			return WPS_STATUS_FAIL;
		}
		if (probe_ie_idx == -1) {
			mwu_printf(DEBUG_INIT, "GET_PROBE_REQ_WPS_IE\n");
			ret = mwu_prob_request_ie_config(
				cur_if->ifname, GET_WPS_IE, &probe_ie_idx);
			if (ret != WPS_STATUS_SUCCESS) {
				mwu_printf(MSG_ERROR,
					   "Failed to get STA Probe Req IEs\n");
				/* Restore original band */
				cur_if->pwps_info->enrollee.rf_bands =
					temp_rf_band;
				return ret;
			}
			if (mwu_set_ie_index(cur_if->ifname, IE_CFG_PROBE,
					     probe_ie_idx) != MWU_ERR_SUCCESS) {
				ERR("Failed to set Probe Request IE index.\n");
				/* Restore original band */
				cur_if->pwps_info->enrollee.rf_bands =
					temp_rf_band;
				return WPS_STATUS_FAIL;
			}
		}

		mwu_prob_request_ie_config(cur_if->ifname,
					   SET_WPS_STA_SESSION_ACTIVE_IE,
					   &probe_ie_idx);

		/* Restore original band */
		cur_if->pwps_info->enrollee.rf_bands = temp_rf_band;

		/* Scan BG band */
		scan_req.chan_list[0].radio_type =
			(RF_BAND_BG | SCAN_SPECIFIED_BAND);
	} else if (rf_band == RF_50_G) {
		/* Update Probe Request IE */
		temp_rf_band = cur_if->pwps_info->enrollee.rf_bands;
		cur_if->pwps_info->enrollee.rf_bands = RF_50_G;

		if (mwu_get_ie_index(cur_if->ifname, IE_CFG_PROBE,
				     &probe_ie_idx) != MWU_ERR_SUCCESS) {
			ERR("Failed to get Probe Request IE index.\n");
			/* Restore original band */
			cur_if->pwps_info->enrollee.rf_bands = temp_rf_band;
			return WPS_STATUS_FAIL;
		}
		if (probe_ie_idx == -1) {
			mwu_printf(DEBUG_INIT, "GET_PROBE_REQ_WPS_IE\n");
			ret = mwu_prob_request_ie_config(
				cur_if->ifname, GET_WPS_IE, &probe_ie_idx);
			if (ret != WPS_STATUS_SUCCESS) {
				mwu_printf(MSG_ERROR,
					   "Failed to get STA Probe Req IEs\n");
				/* Restore original band */
				cur_if->pwps_info->enrollee.rf_bands =
					temp_rf_band;
				return ret;
			}
			if (mwu_set_ie_index(cur_if->ifname, IE_CFG_PROBE,
					     probe_ie_idx) != MWU_ERR_SUCCESS) {
				ERR("Failed to set Probe Request IE index.\n");
				/* Restore original band */
				cur_if->pwps_info->enrollee.rf_bands =
					temp_rf_band;
				return WPS_STATUS_FAIL;
			}
		}

		mwu_prob_request_ie_config(cur_if->ifname,
					   SET_WPS_STA_SESSION_ACTIVE_IE,
					   &probe_ie_idx);

		/* Restore original band */
		cur_if->pwps_info->enrollee.rf_bands = temp_rf_band;

		/* Scan A band */
		scan_req.chan_list[0].radio_type =
			(RF_BAND_A | SCAN_SPECIFIED_BAND);
	}

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL)
		return MWU_ERR_NOMEM;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_SETUSERSCAN);
	/* Fill up buffer */
	mrvl_cmd = (mrvl_priv_cmd *)buf;
	mrvl_cmd->buf = buf + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len =
		MRVDRV_SIZE_OF_CMD_BUFFER - mrvl_header_len - BUF_HEADER_SIZE;
	/* Copy NXP command string */
	temp = mrvl_cmd->buf;
	strncpy((char *)temp, CMD_NXP, strlen(CMD_NXP));
	temp += (strlen(CMD_NXP));
	/* Insert command string*/
	strncpy((char *)temp, PRIV_CMD_SETUSERSCAN,
		strlen(PRIV_CMD_SETUSERSCAN));
	temp += (strlen(PRIV_CMD_SETUSERSCAN));
	memset(temp, 0, sizeof(scan_req));
	memcpy(temp, &scan_req, sizeof(scan_req));

	ret = mwu_privcmd(cur_if->ifname, (u8 *)mrvl_cmd);

	if (buf)
		free(buf);
	LEAVE();
	return ret;
}

#if 1
#if 0
int wps_wlan_sort_scan_list(struct mwu_iface_info *cur_if)
{
    struct SCAN_RESULTS tmp;
    int i, j, ret;
    int auth_mac_i, auth_mac_j;
    int wsc_ver_i, wsc_ver_j;
    int selected_registrar_i, selected_registrar_j;

    ENTER();

    /* Sort the Registrar list in following order
     *
     * 1. WSC 2.0 APs with Selected_Registrar = TRUE and
     *    Enrollee's MAC address in Authorized MACs sub-element in beacon and probe responses.
     *
     * 2. WSC 2.0 APs with Selected_registrar = TRUE and
     *    Wildcard Mac address in Authorized MACs sub-element in beacon and Probe resposes,
     *    ordered by decreasing RSSI.
     *
     * 3. WSC 1.0 APs with Selected_registrar = TRUE and ordered by decreasing RSSI.
     *
     * 4. Rest WSC 1.0 and 2.0 APs, ordered by decreasing RSSI.
     **/

    for(i = 0; i < cur_if->pwps_info->wps_data.num_scan_results; i++) {

        for(j = i+1; j < cur_if->pwps_info->wps_data.num_scan_results; j++) {

            wsc_ver_i = 0;
            wsc_ver_j = 0;
            auth_mac_i = 0;
            auth_mac_j = 0;
            selected_registrar_i = 0;
            selected_registrar_j = 0;

            /* Parsing Authorized MAC address field for ENROLLEE mac
               address */
            if (cur_if->pwps_info->wps_data.scan_results[i].wps_ie_len != 0) {
                ret =
                    wps_probe_response_authorized_enrollee_mac_parser
                    (cur_if, (u8 *) & cur_if->pwps_info->wps_data.scan_results[i].wps_ie,
                     cur_if->pwps_info->wps_data.scan_results[i].wps_ie_len);
                if (ret != WPS_STATUS_SUCCESS)
                    WARN("Failed to parse Authorizes enrollee mac");
            }

            auth_mac_i = cur_if->pwps_info->enrollee_in_authorized_mac;

            /* Parsing Authorized MAC address field for ENROLLEE mac
               address */
            if (cur_if->pwps_info->wps_data.scan_results[j].wps_ie_len != 0) {
                ret =
                    wps_probe_response_authorized_enrollee_mac_parser
                    (cur_if, (u8 *) &cur_if->pwps_info->wps_data.scan_results[j].wps_ie,
                     cur_if->pwps_info->wps_data.scan_results[j].wps_ie_len);
                if (ret != WPS_STATUS_SUCCESS)
                    WARN("Failed to parse Authorizes enrollee mac");
            }
            auth_mac_j = cur_if->pwps_info->enrollee_in_authorized_mac;

            /* read the wsc version */
            wsc_ver_i = wps_probe_response_wsc_version_parser(
                    (u8 *) &cur_if->pwps_info->wps_data.scan_results[i].wps_ie,
                    cur_if->pwps_info->wps_data.scan_results[i].wps_ie_len);
            wsc_ver_j = wps_probe_response_wsc_version_parser(
                    (u8 *) &cur_if->pwps_info->wps_data.scan_results[j].wps_ie,
                    cur_if->pwps_info->wps_data.scan_results[j].wps_ie_len);

            /* read the selected registrar field */
            wps_probe_response_selected_registrar_parser(
                    (u8 *) &cur_if->pwps_info->wps_data.scan_results[i].wps_ie,
                    cur_if->pwps_info->wps_data.scan_results[i].wps_ie_len, &selected_registrar_i);
            wps_probe_response_selected_registrar_parser(
                    (u8 *) &cur_if->pwps_info->wps_data.scan_results[j].wps_ie,
                    cur_if->pwps_info->wps_data.scan_results[j].wps_ie_len, &selected_registrar_j);

            /* Sorting: based on WSC version and Selected_Registrar attribute */
            if(wsc_ver_i < wsc_ver_j) {
                if(selected_registrar_i > selected_registrar_j)
                    continue;
                else
                    goto SWAP;
            } else if(wsc_ver_i > wsc_ver_j) {
                if(selected_registrar_i >= selected_registrar_j)
                    continue;
                else
                    goto SWAP;
            } else if(wsc_ver_i == wsc_ver_j) {
                if(selected_registrar_i < selected_registrar_j)
                    goto SWAP;
                else if(selected_registrar_i > selected_registrar_j)
                    continue;
            }

            /* Sorting: based on Authorized MAC attribute and RSSI */

            /* enrollee_in_authorized_mac could have following values:
             * 0: Enrollee is not authorized to register with this Registrar
             * 1: Registrar's Authorized enrollee list contains wildcard MAC address
             * 2: Registrar's Authorized enrollee list contains Enrollee's MAC address
             **/
            if(auth_mac_i < auth_mac_j) {
                goto SWAP;
            } else if (auth_mac_i == auth_mac_j) {
                /* Compare RSSI value */
                if(cur_if->pwps_info->wps_data.scan_results[i].level <
                    cur_if->pwps_info->wps_data.scan_results[j].level) {
                    goto SWAP;
                } else {
                    continue;
                }
            } else {
                continue;
            }

SWAP:
            /* swap scan results at index i and j */
            memset(&tmp, 0, sizeof(struct SCAN_RESULTS));
            memcpy(&tmp, &cur_if->pwps_info->wps_data.scan_results[i], sizeof(struct SCAN_RESULTS));
            memcpy(&cur_if->pwps_info->wps_data.scan_results[i],
                    &cur_if->pwps_info->wps_data.scan_results[j], sizeof(struct SCAN_RESULTS));
            memcpy(&cur_if->pwps_info->wps_data.scan_results[j], &tmp, sizeof(struct SCAN_RESULTS));
        }
    }

    LEAVE();
    return WPS_STATUS_SUCCESS;
}
#endif
#if 0
/**
 *  @brief Process scan results get from WLAN driver
 *
 *  @param wps_s       Pointer to WPS global structure
 *  @param filter      Filter AP in scan result
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int
wps_wlan_scan_results(struct mwu_iface_info *cur_if, u16 filter)
{
    struct SCAN_RESULTS *results = NULL, *tmp = NULL;
    int num;
    u16 type = 0xffff;
    char mac[ETH_ALEN], ssid[MAX_SSID_LEN + 1];
    int i;

    ENTER();

    results = malloc(SCAN_AP_LIMIT * sizeof(struct SCAN_RESULTS));
    if (results == NULL) {
        mwu_printf(MSG_WARNING, "Failed to allocate memory for scan "
                   "results");
        LEAVE();
        return WPS_STATUS_FAIL;
    }

    memset(results, 0, SCAN_AP_LIMIT * sizeof(struct SCAN_RESULTS));

    num = wlan_get_scan_results(cur_if, results);
    mwu_printf(DEBUG_WLAN, "Scan results: %d", num);

    if (num < 0) {
        mwu_printf(DEBUG_WLAN, "Failed to get scan results");
        FREE(results);
        LEAVE();
        return WPS_STATUS_FAIL;
    }
    if (num > SCAN_AP_LIMIT) {
        mwu_printf(DEBUG_WLAN, "Not enough room for all APs (%d < %d)",
                   num, SCAN_AP_LIMIT);
        num = SCAN_AP_LIMIT;
    }
    if (filter == FILTER_PBC) {
        int k, match = 0;

        tmp = malloc(num * sizeof(struct SCAN_RESULTS));
        if (tmp == NULL) {
            mwu_printf(MSG_WARNING,
                       "Failed to allocate memory for scan results");
            FREE(results);
            LEAVE();
            return WPS_STATUS_FAIL;
        }

        for (k = 0; k < num; k++) {
            if (results[k].wps_ie_len > 0) {
                type =
                    wps_probe_response_device_password_id_parser(results[k].
                                                                 wps_ie,
                                                                 results[k].
                                                                 wps_ie_len);

                if (type == DEVICE_PASSWORD_PUSH_BUTTON) {
                    memcpy((void *) &tmp[match], (void *) &results[k],
                           sizeof(struct SCAN_RESULTS));
                    match++;
                }
            }
        }                       /* end of k loop */

        FREE(results);

        cur_if->pwps_info->wps_data.num_scan_results = match;
        if (match > 0) {
            FREE(cur_if->pwps_info->wps_data.scan_results);
            cur_if->pwps_info->wps_data.scan_results = tmp;
        } else {
            FREE(tmp);
        }
    } else if (filter == FILTER_PIN) {
        int k, match = 0;

        tmp = malloc(num * sizeof(struct SCAN_RESULTS));
        if (tmp == NULL) {
            mwu_printf(MSG_WARNING,
                       "Failed to allocate memory for scan results");
            FREE(results);
            LEAVE();
            return WPS_STATUS_FAIL;
        }

        for (k = 0; k < num; k++) {
            if (results[k].wps_ie_len > 0) {
                type =
                    wps_probe_response_device_password_id_parser(results[k].
                                                                 wps_ie,
                                                                 results[k].
                                                                 wps_ie_len);

                if (type == DEVICE_PASSWORD_ID_PIN) {
                    memcpy((void *) &tmp[match], (void *) &results[k],
                           sizeof(struct SCAN_RESULTS));
                    match++;
                }
            }
        }                       /* end of k loop */

        FREE(results);

        cur_if->pwps_info->wps_data.num_scan_results = match;
        if (match > 0) {
            FREE(cur_if->pwps_info->wps_data.scan_results);
            cur_if->pwps_info->wps_data.scan_results = tmp;
        } else {
            FREE(tmp);
        }
    } else {
        int k, match = 0;

        /* Only save Infra mode AP */
        tmp = malloc(num * sizeof(struct SCAN_RESULTS));
        if (tmp == NULL) {
            mwu_printf(MSG_WARNING,
                       "Failed to allocate memory for scan results");
            FREE(results);
            LEAVE();
            return WPS_STATUS_FAIL;
        }

        for (k = 0; k < num; k++) {
            INFO("%d. SSID [%s] MAC [" UTIL_MACSTR "]\n", k, results[k].ssid, UTIL_MAC2STR(results[k].bssid));
            if (results[k].caps & IEEE80211_CAP_ESS &&
                results[k].wps_ie_len > 0) {
                /* save all infra APs with wps enabled */
                    memcpy((void *) &tmp[match], (void *) &results[k],
                           sizeof(struct SCAN_RESULTS));
                    match++;
            }
        }                       /* end of k loop */

        FREE(results);

        cur_if->pwps_info->wps_data.num_scan_results = match;
        if (match > 0) {
            FREE(cur_if->pwps_info->wps_data.scan_results);
            cur_if->pwps_info->wps_data.scan_results = tmp;
        } else {
            FREE(tmp);
        }
    }

    mwu_printf(DEBUG_WIFIDIR_DISCOVERY, "\nScan Result : WPS capable %d entries.\n",
            cur_if->pwps_info->wps_data.num_scan_results);

    if (cur_if->role == WPS_ENROLLEE || cur_if->role == WIFIDIR_ROLE) {
        memcpy(mac, cur_if->pwps_info->registrar.mac_address, ETH_ALEN);
        strncpy(ssid, (char *)cur_if->pwps_info->registrar.go_ssid, MAX_SSID_LEN + 1);
    } else {
        memcpy(mac, cur_if->pwps_info->enrollee.mac_address, ETH_ALEN);
        strncpy(ssid, (char *)"", MAX_SSID_LEN + 1);
    }

    INFO("SEARCH FOR ----------> SSID [%s] MAC [" UTIL_MACSTR "]\n",
            ssid, UTIL_MAC2STR(mac));

    for (i = 0; i < cur_if->pwps_info->wps_data.num_scan_results; i++) {
        INFO("SSID [%s] MAC [" UTIL_MACSTR "]\n",
            cur_if->pwps_info->wps_data.scan_results[i].ssid,
            UTIL_MAC2STR(cur_if->pwps_info->wps_data.scan_results[i].bssid));

        if (!memcmp(mac, cur_if->pwps_info->wps_data.scan_results[i].bssid, ETH_ALEN) &&
                (!strlen(ssid) ||
                 !memcmp(ssid, cur_if->pwps_info->wps_data.scan_results[i].ssid,
                     strlen(ssid) + 1))) {
            cur_if->pwps_info->dev_found = i;
            if((cur_if->pwifidirect_info) &&
                    (cur_if->pwifidirect_info->cur_peer != NULL)) {
                cur_if->pwifidirect_info->cur_peer->op_channel =
                    mapping_freq_to_chan(cur_if->pwps_info->wps_data.scan_results[i].freq);
            }
            mwu_printf(DEBUG_WIFIDIR_DISCOVERY,
                    "\nSelected Device found at %d.\n", i);
            break;
        }
    }

    if (i == cur_if->pwps_info->wps_data.num_scan_results) {
        /*No peer has been found*/
        mwu_printf(MSG_WARNING,
                "No Peer has been found!");
        cur_if->pwps_info->dev_found = -1;
    }

    LEAVE();
    return WPS_STATUS_SUCCESS;
}
#endif
#endif

/**
 *  @brief Set WPS IE for foreground scan to WLAN Driver
 *
 *  @param cur_if   Current interface
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_wlan_set_fgscan_wps_ie(struct mwu_iface_info *cur_if)
{
	u8 *buf;
	int wps_ie_size, buf_len;
	int ret = WPS_STATUS_SUCCESS;
	struct iwreq iwr;
	int ioctl_val, subioctl_val;

	ENTER();

	buf = (u8 *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	buf[0] = 221; /* ElementID of WPS_IE */
	wps_ie_size = wps_sta_probe_request_prepare(cur_if, &buf[2]);
	buf[1] = (u8)wps_ie_size; /* IE len */
	buf_len = wps_ie_size + 2; /* Add IE_ID and IE_Len fileds */

	mwu_hexdump(DEBUG_WLAN, "WPS_IE", (const unsigned char *)buf, buf_len);

	/*
	 * Always try private ioctl "setgenie" first
	 * If private ioctl failed and if driver build with WE version >= 18,
	 * try standard ioctl "SIOCSIWGENIE".
	 */
	if (mwu_get_priv_ioctl(cur_if->ifname, "setgenie", &ioctl_val,
			       &subioctl_val) == WPS_STATUS_FAIL) {
		mwu_printf(MSG_ERROR, "ERROR : ioctl[setgenie] NOT SUPPORT !");
#if (WIRELESS_EXT >= 18)
		if (cur_if->we_version_compiled < 18) {
			ret = WPS_STATUS_FAIL;
			goto _exit_;
		} else
			goto _we18_;
#else
		ret = WPS_STATUS_FAIL;
		goto _exit_;
#endif
	}

	/*
	 * Set up and execute the ioctl call
	 */
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, (char *)dev_name, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t)buf;
	iwr.u.data.length = buf_len;
	iwr.u.data.flags = subioctl_val;

	if (ioctl(cur_if->ioctl_sock, ioctl_val, &iwr) < 0) {
		perror("ioctl[setgenie]");
#if (WIRELESS_EXT >= 18)
		if (cur_if->we_version_compiled < 18) {
			ret = WPS_STATUS_FAIL;
			goto _exit_;
		}
#else
		ret = WPS_STATUS_FAIL;
		goto _exit_;
#endif
	} else {
		ret = WPS_STATUS_SUCCESS;
		goto _exit_;
	}

#if (WIRELESS_EXT >= 18)
_we18_:
	/*
	 * If private ioctl failed and if driver build with WE version >= 18,
	 * try standard ioctl "SIOCSIWGENIE".
	 */
	if (cur_if->we_version_compiled >= 18) {
		/*
		 * Driver WE version >= 18, use standard ioctl
		 */
		memset(&iwr, 0, sizeof(iwr));
		strncpy(iwr.ifr_name, (char *)dev_name, IFNAMSIZ);
		iwr.u.data.pointer = (caddr_t)buf;
		iwr.u.data.length = buf_len;

		if (ioctl(cur_if->ioctl_sock, SIOCSIWGENIE, &iwr) < 0) {
			perror("ioctl[SIOCSIWGENIE]");
			ret = WPS_STATUS_FAIL;
			goto _exit_;
		} else {
			ret = WPS_STATUS_SUCCESS;
			goto _exit_;
		}
	}
#endif /* (WIRELESS_EXT >= 18) */

_exit_:
	FREE(buf);

	LEAVE();
	return ret;
}

/**
 *  @brief Reset WPS IE for foreground scan to WLAN Driver
 *
 *  @param cur_if   Current interface
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_wlan_reset_wps_ie(struct mwu_iface_info *cur_if)
{
	u8 buf[10];
	int buf_len;
	u8 wps_oui[4] = {0x00, 0x50, 0xF2, 0x04};
	int ret = WPS_STATUS_SUCCESS;
	struct iwreq iwr;
	int ioctl_val, subioctl_val;

	ENTER();

	buf[0] = 221; /* ElementID of WPS_IE */
	memcpy(&buf[2], wps_oui, sizeof(wps_oui));
	buf[1] = sizeof(wps_oui); /* IE len */
	buf_len = sizeof(wps_oui) + 2; /* Add IE_ID and IE_Len fileds */

	/*
	 * Always try private ioctl "setgenie" first
	 * If private ioctl failed and if driver build with WE version >= 18,
	 * try standard ioctl "SIOCSIWGENIE".
	 */
	if (mwu_get_priv_ioctl(cur_if->ifname, "setgenie", &ioctl_val,
			       &subioctl_val) == WPS_STATUS_FAIL) {
		mwu_printf(MSG_ERROR, "ERROR : ioctl[setgenie] NOT SUPPORT !");
#if (WIRELESS_EXT >= 18)
		if (cur_if->we_version_compiled < 18) {
			ret = WPS_STATUS_FAIL;
			goto _exit_;
		} else
			goto _we18_;
#else
		ret = WPS_STATUS_FAIL;
		goto _exit_;
#endif
	}

	/*
	 * Set up and execute the ioctl call
	 */
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, (char *)dev_name, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t)buf;
	iwr.u.data.length = buf_len;
	iwr.u.data.flags = subioctl_val;

	if (ioctl(cur_if->ioctl_sock, ioctl_val, &iwr) < 0) {
		perror("ioctl[setgenie]");
#if (WIRELESS_EXT >= 18)
		if (cur_if->we_version_compiled < 18) {
			ret = WPS_STATUS_FAIL;
			goto _exit_;
		}
#else
		ret = WPS_STATUS_FAIL;
		goto _exit_;
#endif
	} else {
		ret = WPS_STATUS_SUCCESS;
		goto _exit_;
	}

#if (WIRELESS_EXT >= 18)
_we18_:
	/*
	 * If private ioctl failed and if driver build with WE version >= 18,
	 * try standard ioctl "SIOCSIWGENIE".
	 */
	if (cur_if->we_version_compiled >= 18) {
		/*
		 * Driver WE version >= 18, use standard ioctl
		 */
		memset(&iwr, 0, sizeof(iwr));
		strncpy(iwr.ifr_name, (char *)dev_name, IFNAMSIZ);
		iwr.u.data.pointer = (caddr_t)buf;
		iwr.u.data.length = buf_len;

		if (ioctl(cur_if->ioctl_sock, SIOCSIWGENIE, &iwr) < 0) {
			perror("ioctl[SIOCSIWGENIE]");
			ret = WPS_STATUS_FAIL;
			goto _exit_;
		} else {
			ret = WPS_STATUS_SUCCESS;
			goto _exit_;
		}
	}
#endif /* (WIRELESS_EXT >= 18) */

_exit_:
	LEAVE();
	return ret;
}

#if 1
int load_cred_info(struct mwu_iface_info *cur_if, bss_config_t *bss)
{
	int i, ret = WPS_STATUS_SUCCESS;
	struct CREDENTIAL_DATA *reg_cred = NULL, *enr_cred = NULL;
	struct MESSAGE_ENROLLEE_REGISTRAR *enr_reg = NULL,
					  *enrollee_info = NULL;
	u8 cipher = 0;

	ENTER();

	if (!bss) {
		mwu_printf(MSG_ERROR, "BSS info is not present");
		return WPS_STATUS_FAIL;
	}

	enr_reg = &cur_if->pwps_info->registrar;
	reg_cred = &enr_reg->cred_data[0];

	enrollee_info = &cur_if->pwps_info->enrollee;
	enr_cred = &enrollee_info->cred_data[0];

	memset(reg_cred, 0, sizeof(struct CREDENTIAL_DATA));
	if (bss->ssid.ssid_len) {
		memcpy(reg_cred->ssid, bss->ssid.ssid, bss->ssid.ssid_len);
		reg_cred->ssid_length = bss->ssid.ssid_len;
		memcpy(cur_if->pwps_info->wps_data.current_ssid.ssid,
		       bss->ssid.ssid, bss->ssid.ssid_len);
		cur_if->pwps_info->wps_data.current_ssid.ssid_len =
			bss->ssid.ssid_len;
		mwu_printf(DEBUG_INIT, "SSID '%s'",
			   cur_if->pwps_info->wps_data.current_ssid.ssid);
	}
	switch (bss->protocol) {
	case UAP_PROTO_OPEN:
		reg_cred->auth_type = AUTHENTICATION_TYPE_OPEN;
		reg_cred->encry_type = ENCRYPTION_TYPE_NONE;
		break;
	case UAP_PROTO_WEP_STATIC:
		reg_cred->encry_type = ENCRYPTION_TYPE_WEP;
		break;
	case UAP_PROTO_WPA:
		reg_cred->auth_type = AUTHENTICATION_TYPE_WPAPSK;
		break;
	case UAP_PROTO_WPA2:
		reg_cred->auth_type = AUTHENTICATION_TYPE_WPA2PSK;
		break;
	case UAP_PROTO_MIXED:
		reg_cred->auth_type = AUTHENTICATION_TYPE_WPA_MIXED;
		break;
	default:
		break;
	}

	if (cur_if->bss_role == BSS_TYPE_UAP) {
		if (bss->bandcfg.chanBand == BAND_5GHZ ||
		    bss->channel > MAX_CHANNELS) {
			reg_cred->rf_bands = RF_50_G;

			cur_if->pwps_info->enrollee.rf_bands = RF_50_G;
			cur_if->pwps_info->registrar.rf_bands = RF_50_G;
		} else {
			reg_cred->rf_bands = RF_24_G;

			cur_if->pwps_info->enrollee.rf_bands = RF_24_G;
			cur_if->pwps_info->registrar.rf_bands = RF_24_G;
		}
	}

	if (reg_cred->encry_type == ENCRYPTION_TYPE_WEP) {
		/* For WEP */
		for (i = 0; i < 4; i++) {
			if (bss->wep_cfg[i].is_default) {
				/* Get the current default key settings */
				memcpy(reg_cred->network_key,
				       bss->wep_cfg[i].key,
				       bss->wep_cfg[i].length);
				reg_cred->network_key_len =
					bss->wep_cfg[i].length;
				reg_cred->network_key_index =
					bss->wep_cfg[i].key_index + 1;
				break;
			}
		}
		mwu_printf(DEBUG_INIT, "NETWORK KEY INDEX = %d",
			   reg_cred->network_key_index - 1);
		mwu_hexdump(DEBUG_INIT, "NETWORK_KEY",
			    (const unsigned char *)reg_cred->network_key,
			    reg_cred->network_key_len);
		switch (bss->auth_mode) {
		case UAP_AUTH_MODE_OPEN:
			reg_cred->auth_type = AUTHENTICATION_TYPE_OPEN;
			break;
		case UAP_AUTH_MODE_SHARED:
			reg_cred->auth_type = AUTHENTICATION_TYPE_SHARED;
			break;
		default:
			break;
		}
	}
	mwu_printf(DEBUG_INIT, "AUTHENTICATION TYPE = 0x%04x",
		   enr_reg->cred_data[0].auth_type);
	if ((reg_cred->auth_type == AUTHENTICATION_TYPE_WPAPSK) ||
	    (reg_cred->auth_type == AUTHENTICATION_TYPE_WPA2PSK) ||
	    (reg_cred->auth_type == AUTHENTICATION_TYPE_WPA_MIXED) ||
	    (reg_cred->auth_type == AUTHENTICATION_TYPE_ALL)) {
		/* For WPA/WPA2 */

		if (reg_cred->auth_type == AUTHENTICATION_TYPE_WPAPSK)
			cipher = bss->wpa_cfg.pairwise_cipher_wpa;
		else
			cipher = bss->wpa_cfg.pairwise_cipher_wpa2;

		switch (cipher) {
		case UAP_CIPH_NONE:
			reg_cred->encry_type = ENCRYPTION_TYPE_NONE;
			break;
		case UAP_CIPH_TKIP:
			reg_cred->encry_type = ENCRYPTION_TYPE_TKIP;
			break;
		case UAP_CIPH_AES:
			reg_cred->encry_type = ENCRYPTION_TYPE_AES;
			break;
		case UAP_CIPH_AES_TKIP:
			reg_cred->encry_type = ENCRYPTION_TYPE_TKIP_AES_MIXED;
			break;
		default:
			break;
		}
		memcpy(reg_cred->network_key, bss->wpa_cfg.passphrase,
		       bss->wpa_cfg.length);
		if (reg_cred->encry_type != ENCRYPTION_TYPE_WEP) {
			reg_cred->network_key_len = bss->wpa_cfg.length;
			reg_cred->network_key_index = 1;
		}
		mwu_printf(DEBUG_INIT, "NETWORK_KEY '%s'",
			   enr_reg->cred_data[0].network_key);
	}
	mwu_printf(DEBUG_INIT, "ENCRYPTION TYPE = 0x%04x",
		   enr_reg->cred_data[0].encry_type);

	/* Both auth_type and encry_type can not be ZERO */
	if (!reg_cred->auth_type && !reg_cred->encry_type) {
		reg_cred->auth_type = AUTHENTICATION_TYPE_OPEN;
		mwu_printf(
			MSG_INFO,
			"Incorrect Auth encryption types, Using open security\n");
	}
	if (enr_reg->version >= WPS_VERSION_2DOT0) {
		/* Dont go ahead if current security configuration is prohibited
		 */
		switch (reg_cred->encry_type) {
		case ENCRYPTION_TYPE_TKIP:
			mwu_printf(MSG_ERROR,
				   "WPA-TKIP is deprecated from WSC2.0.\n");
			ret = WPS_STATUS_FAIL;
			break;
		case ENCRYPTION_TYPE_WEP:
			mwu_printf(MSG_ERROR, "WEP deprecated from WSC2.0.\n");
			ret = WPS_STATUS_FAIL;
			break;
		default:
			break;
		}
	}

	memcpy(enr_cred, reg_cred, sizeof(struct CREDENTIAL_DATA));
	LEAVE();
	return ret;
}

/**
 *  @brief Change AP configuration as per registrar credential structure
 *
 *  @param cur_if    Current interface
 *
 *  @return          WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
static int wlan_change_ap_configuration(struct mwu_iface_info *cur_if)
{
	struct MESSAGE_ENROLLEE_REGISTRAR *enr_reg = NULL;
	bss_config_t bss;
	int ret = WPS_STATUS_SUCCESS;
	struct CREDENTIAL_DATA *pCred = NULL;

	ENTER();

	mwu_apcmd_stop_bss(cur_if->ifname);

	enr_reg = &cur_if->pwps_info->registrar;
	pCred = &enr_reg->cred_data[0];
	mwu_printf(MSG_ERROR, "ap_conf: pCred: %p", pCred);

	memset(&bss, 0, sizeof(bss_config_t));

	/* Read current params for default values */
	if ((ret = mwu_apcmd_get_bss_config(cur_if->ifname, &bss)) !=
	    WPS_STATUS_SUCCESS) {
		goto done;
	}

	wps_cred_to_bss_config(&bss, pCred,
			       cur_if->pwps_info->config_load_by_oob);

	mwu_printf(DEBUG_WLAN, "====== new credentials ======\n");

	mwu_printf(
		DEBUG_WLAN,
		"SSID:%s proto:0x%x pair_cip_wpa:0x%x pair_cip_wpa2:0x%x group_cip:0x%x\n",
		bss.ssid.ssid, bss.protocol, bss.wpa_cfg.pairwise_cipher_wpa,
		bss.wpa_cfg.pairwise_cipher_wpa2, bss.wpa_cfg.group_cipher);

	mwu_hexdump(DEBUG_WLAN, "Net key(PSK)", (u8 *)bss.wpa_cfg.passphrase,
		    bss.wpa_cfg.length);

	/* Set updated params */
	ret = mwu_apcmd_set_bss_config(cur_if->ifname, &bss);

	mwu_apcmd_start_bss(cur_if->ifname);

done:
	LEAVE();
	return ret;
}
#if 0
int
wps_wlan_reset_ap_config(struct mwu_iface_info *cur_if)
{
    bss_config_t bss;

    ENTER();
    memset(&bss, 0, sizeof(bss_config_t));

    /* Read current params for default values */
    if (mwu_apcmd_get_bss_config(cur_if->ifname, &bss) != WPS_STATUS_SUCCESS) {
        LEAVE();
        return WPS_STATUS_FAIL;
    }

    if (cur_if->role == WPS_ENROLLEE
            && cur_if->pwps_info->read_ap_config_only == WPS_CANCEL) {

        mwu_printf(DEBUG_WPS_STATE,
                   "Delay loading new AP config and restart by 200 milli-seconds!");
        mwu_sleep(0, 200000);
        /* Update the new AP's settings received from the Registrar */
        wlan_change_ap_configuration(cur_if);

    } else if (cur_if->role == WPS_REGISTRAR
            && cur_if->pwps_info->wps_device_state == SC_NOT_CONFIGURED_STATE) {

        cur_if->pwps_info->config_load_by_oob = WPS_SET;

        mwu_printf(DEBUG_WPS_STATE,
                   "Delay loading new AP config and restart by 200 milli-seconds!");
        mwu_sleep(0, 200000);
        if (cur_if->pwps_info->reset_oob_mixed == WPS_SET) {
            mwu_printf(DEBUG_WLAN, "Settings already loaded!");
            mwu_apcmd_stop_bss(cur_if->ifname);
            mwu_apcmd_start_bss(cur_if->ifname);
            return WPS_STATUS_SUCCESS;
        }
        /* Update the new Randomly generated AP setting after OOB reset from
           the Registrar */
        wlan_change_ap_configuration(cur_if);
    } else {

        mwu_printf(DEBUG_WLAN, "AP configuration unchanged!");

        /* For UAP mode */
        cur_if->pwps_info->wps_data.current_ssid.mode = IEEE80211_MODE_INFRA;
        cur_if->pwps_info->mode = IEEE80211_MODE_INFRA;

        if (mwu_set_beacon_probe_resp_ie(cur_if, SET_WPS_AP_SESSION_INACTIVE_IE) != MWU_ERR_SUCCESS)
            return WPS_STATUS_FAIL;

        if (cur_if->pwps_info->registrar.version >= WPS_VERSION_2DOT0 ||
                cur_if->pwps_info->enrollee.version >= WPS_VERSION_2DOT0) {

            if (mwu_set_ie(cur_if->ifname, IE_CFG_AP_ASSOCRESP,
                    SET_WPS_AP_SESSION_INACTIVE_AR_IE) != MWU_ERR_SUCCESS)
                return WPS_STATUS_FAIL;
        }

        cur_if->pwps_info->pin_pbc_set = WPS_CANCEL;
        cur_if->pwps_info->read_ap_config_only = WPS_CANCEL;
    }

    LEAVE();
    return WPS_STATUS_SUCCESS;
}
#endif
int wps_load_uap_cred(struct mwu_iface_info *cur_if)
{
	int ret;
	bss_config_t bss;

	ENTER();

	memset(&bss, 0, sizeof(bss_config_t));

	if ((ret = mwu_apcmd_get_bss_config(cur_if->ifname, &bss)) !=
	    WPS_STATUS_SUCCESS) {
		goto done;
	}

	/* set credentials */
	ret = load_cred_info(cur_if, &bss);

done:
	LEAVE();
	return ret;
}

/**
 *  @brief Converts a string to hex value
 *
 *  @param str      A pointer to the string
 *  @param raw      A pointer to the raw data buffer
 *  @return         Number of bytes read
 */
int string2raw(char *str, unsigned char *raw)
{
	int len = (strlen(str) + 1) / 2;

	do {
		if (!isxdigit(*str)) {
			return -1;
		}
		*str = toupper(*str);
		*raw = CHAR2INT(*str) << 4;
		++str;
		*str = toupper(*str);
		if (*str == '\0')
			break;
		*raw |= CHAR2INT(*str);
		++raw;
	} while (*++str != '\0');
	return len;
}

/**
 *  @brief Copy Credential data into BSS configuration
 *  @param bss       A pointer to the bss_config_t structure
 *  @param pCred     A pointer to the CREDENTIAL_DATA structure
 *
 *  @return          WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_cred_to_bss_config(bss_config_t *bss, struct CREDENTIAL_DATA *pCred,
			   u8 load_by_oob)
{
	/* Convert cred info to bss-config_t */
	int ret = WPS_STATUS_SUCCESS;
	unsigned char network_key[MAX_WEP_KEY_LEN];

	ENTER();

	/* Copy SSID */
	memset(bss->ssid.ssid, 0, MAX_SSID_LEN);
	bss->ssid.ssid_len = pCred->ssid_length;
	memcpy(bss->ssid.ssid, pCred->ssid, pCred->ssid_length);
	/* Enable SSID bcast ctrl */
	bss->bcast_ssid_ctl = 1;

	/* Copy Auth Type */
	switch (pCred->auth_type) {
	case AUTHENTICATION_TYPE_OPEN:
		bss->protocol = UAP_PROTO_OPEN;
		bss->auth_mode = UAP_AUTH_MODE_OPEN;
		break;
	case AUTHENTICATION_TYPE_WPAPSK:
		bss->protocol = UAP_PROTO_WPA;
		bss->auth_mode = UAP_AUTH_MODE_OPEN;
		break;
	case AUTHENTICATION_TYPE_SHARED:
		bss->protocol = UAP_PROTO_WEP_STATIC;
		bss->auth_mode = UAP_AUTH_MODE_SHARED;
		break;
	case AUTHENTICATION_TYPE_WPA:
		bss->protocol = UAP_PROTO_WPA;
		bss->auth_mode = UAP_AUTH_MODE_OPEN;
		break;
	case AUTHENTICATION_TYPE_WPA2:
		bss->protocol = UAP_PROTO_WPA2;
		bss->auth_mode = UAP_AUTH_MODE_OPEN;
		break;
	case AUTHENTICATION_TYPE_WPA2PSK:
		bss->protocol = UAP_PROTO_WPA2;
		bss->auth_mode = UAP_AUTH_MODE_OPEN;
		break;
	case AUTHENTICATION_TYPE_WPA_MIXED:
		bss->protocol = UAP_PROTO_MIXED;
		bss->auth_mode = UAP_AUTH_MODE_OPEN;
		break;
	default:
		if (pCred->auth_type == AUTHENTICATION_TYPE_WPA_MIXED &&
		    load_by_oob == WPS_SET) {
			bss->protocol = UAP_PROTO_MIXED;
			bss->auth_mode = UAP_AUTH_MODE_OPEN;
		} else {
			mwu_printf(MSG_ERROR, " Unsupported AUTH type: 0x%x\n",
				   pCred->auth_type);
			ret = WPS_STATUS_FAIL;
		}
		break;
	}
	if (pCred->encry_type == ENCRYPTION_TYPE_WEP) {
		bss->protocol = UAP_PROTO_WEP_STATIC;
		bss->wep_cfg[pCred->network_key_index - 1].is_default = WPS_SET;

		if (pCred->network_key_len == 10 ||
		    pCred->network_key_len == 5) {
			bss->wep_cfg[pCred->network_key_index - 1].length = 5;
		} else if (pCred->network_key_len == 13 ||
			   pCred->network_key_len == 26) {
			bss->wep_cfg[pCred->network_key_index - 1].length = 13;
		}

		bss->wep_cfg[pCred->network_key_index - 1].key_index =
			pCred->network_key_index - 1;

		if (pCred->network_key_len == 26 ||
		    pCred->network_key_len == 10) {
			memset(network_key, 0, MAX_WEP_KEY_LEN);
			string2raw((char *)pCred->network_key, network_key);
		} else {
			memcpy(network_key, pCred->network_key,
			       pCred->network_key_len);
		}

		memcpy(bss->wep_cfg[pCred->network_key_index - 1].key,
		       network_key, pCred->network_key_len);
	} else {
		if (bss->protocol == UAP_PROTO_WPA) {
			switch (pCred->encry_type) {
			case ENCRYPTION_TYPE_TKIP:
				bss->wpa_cfg.group_cipher = UAP_CIPH_TKIP;
				bss->wpa_cfg.pairwise_cipher_wpa =
					UAP_CIPH_TKIP;
				break;
			case ENCRYPTION_TYPE_AES:
				bss->wpa_cfg.group_cipher = UAP_CIPH_AES;
				bss->wpa_cfg.pairwise_cipher_wpa = UAP_CIPH_AES;
				break;
			default:
				mwu_printf(
					MSG_WARNING,
					"Invalid Encryption type for Auth type WPA!!");
				return WPS_STATUS_FAIL;
			}
		} else if (bss->protocol == UAP_PROTO_WPA2) {
			switch (pCred->encry_type) {
			case ENCRYPTION_TYPE_TKIP:
				bss->wpa_cfg.group_cipher = UAP_CIPH_TKIP;
				bss->wpa_cfg.pairwise_cipher_wpa2 =
					UAP_CIPH_TKIP;
				break;
			case ENCRYPTION_TYPE_AES:
				bss->wpa_cfg.group_cipher = UAP_CIPH_AES;
				bss->wpa_cfg.pairwise_cipher_wpa2 =
					UAP_CIPH_AES;
				break;
			default:
				mwu_printf(
					MSG_WARNING,
					"Invalid Encryption type for Auth type WPA2!!");
				return WPS_STATUS_FAIL;
			}
		} else if (bss->protocol == UAP_PROTO_MIXED) {
			bss->wpa_cfg.group_cipher = UAP_CIPH_TKIP;
			bss->wpa_cfg.pairwise_cipher_wpa2 = UAP_CIPH_AES;
			bss->wpa_cfg.pairwise_cipher_wpa = UAP_CIPH_TKIP;
		}

		/* Copy Network Key */
		bss->wpa_cfg.length = pCred->network_key_len;
		memcpy(bss->wpa_cfg.passphrase, pCred->network_key,
		       pCred->network_key_len);
	}

	LEAVE();
	return ret;
}

int wps_load_wsta_registrar_cred(struct mwu_iface_info *cur_if)
{
	struct MESSAGE_ENROLLEE_REGISTRAR *enr_reg = NULL;
	bss_config_t bss;
	int ret = WPS_STATUS_SUCCESS;
	struct CREDENTIAL_DATA *pCred = NULL;

	ENTER();
	enr_reg = &cur_if->pwps_info->registrar;
	pCred = &enr_reg->cred_data[0];
	memset(&bss, 0, sizeof(bss_config_t));

	wps_cred_to_bss_config(&bss, pCred,
			       cur_if->pwps_info->config_load_by_oob);

	/* set credentials */
	ret = load_cred_info(cur_if, &bss);
	LEAVE();

	return ret;
}
#if 0
void
wlan_wifidir_generate_psk(struct mwu_iface_info *cur_if)
{
    int i, len = 0;
    u8 temp[32];
    struct CREDENTIAL_DATA *reg_cred = NULL;

    ENTER();
    reg_cred = &cur_if->pwps_info->registrar.cred_data[0];

    /*
     * For WIFIDIR, we need to convert the passphrase to PSK.
     */
    pbkdf2_sha1((char *) reg_cred->network_key, (char *) reg_cred->ssid,
                reg_cred->ssid_length, 4096, temp, 32);
    for (i = 0; i < 32; ++i)
        len += sprintf((char *) reg_cred->network_key + len, "%02x",
                       (temp[i] & 0xff));
    reg_cred->network_key_len = 64;

    mwu_hexdump(DEBUG_WIFIDIR_DISCOVERY, "NETWORK KEY(PSK)",
                (u8 *) cur_if->pwps_info->registrar.cred_data[0].network_key, 64);

    LEAVE();
}
#endif
int wps_handle_reset_oob(struct mwu_iface_info *cur_if)
{
	bss_config_t bss;
	int exitcode = WPS_STATUS_SUCCESS;

	ENTER();

	if ((exitcode = mwu_get_bss_role(cur_if->ifname, &cur_if->bss_role)) !=
	    MWU_ERR_SUCCESS) {
		ERR("Failed to get BSS role.");
		return exitcode;
	}

	if (cur_if->bss_role == BSS_TYPE_UAP &&
	    cur_if->pwpa_info->wps_reset_oob == WPS_SET) {
		mwu_printf(DEBUG_INIT, "Current SSID:%s \n", bss.ssid.ssid);
		exitcode = wps_wlan_create_ap_config_after_OOB(cur_if);
		if (exitcode == WPS_STATUS_SUCCESS) {
			INFO("Reset OOB successful");
		} else {
			ERR("Failed to reset AP to OOB");
		}
	}

	LEAVE();
	return exitcode;
}

/**
 *  @brief  Generates OOB SSID string from device mac address and stroes it
 *
 *  @param my_mac_addr      A pointer to device MAC address array
 *  @return                 A pointer to OOB SSID.
 */
void wps_wlan_get_OOB_ap_ssid(char *wps_oob_ssid, u8 *my_mac_addr)
{
	u8 mac_str[10];

	ENTER();

	memset(mac_str, 0, 10);
	snprintf((char *)mac_str, 5, "%02x%02x", my_mac_addr[4],
		 my_mac_addr[5]);
	mac_str[4] = '\0';
	strncpy(wps_oob_ssid, "NXPMicroAP_", 15);
	strncat(wps_oob_ssid, (char *)mac_str, 5);
	LEAVE();
	return;
}

static char *wps_wlan_proto_to_security_string(int proto)
{
	switch (proto) {
	case UAP_PROTO_OPEN:
		return "Open Security";
		break;
	case UAP_PROTO_WEP_STATIC:
		return "Static WEP";
		break;
	case UAP_PROTO_WPA:
		return "WPA";
		break;
	case UAP_PROTO_WPA2:
		return "WPA2";
		break;
	case UAP_PROTO_MIXED:
		return "WPA/WPA2 Mixed mode";
		break;
	}
	return "Unknown Security";
}

int wps_wlan_reset_ap_config_to_OOB(struct mwu_iface_info *cur_if)
{
	struct iwreq iwr;
	int ret = WPS_STATUS_SUCCESS;
	bss_config_t *bss;
	bss_config_t temp_bss;
	u8 wps_oob_ssid[MAX_SSID_LEN + 1];
	ENTER();

#if 0
    // TODO: Get Current associated STA list and send deauth.
    /* Deauthenticate all connected stations */
    if (cur_if->pwps_info->wps_data.current_ssid.mode == IEEE80211_MODE_INFRA)
        mwu_set_deauth(cur_if->ifname);
#endif
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, cur_if->ifname, IFNAMSIZ);

	if (cur_if->bss_role == BSS_TYPE_UAP) {
		if ((ret = mwu_apcmd_get_bss_config(cur_if->ifname,
						    &temp_bss)) !=
		    WPS_STATUS_SUCCESS) {
			LEAVE();
			return ret;
		}
	}

	memset(wps_oob_ssid, 0, sizeof(wps_oob_ssid));
	wps_wlan_get_OOB_ap_ssid((char *)wps_oob_ssid, temp_bss.mac_addr);

	cur_if->pwpa_info->oob_ssid_len = strlen((char *)wps_oob_ssid);
	memcpy(cur_if->pwpa_info->oob_ssid, wps_oob_ssid,
	       cur_if->pwpa_info->oob_ssid_len);

	bss = (bss_config_t *)malloc(sizeof(bss_config_t));
	memset(bss, 0, sizeof(bss_config_t));
	/* SSID update to credentials & Driver, FW. */

	mwu_apcmd_stop_bss(cur_if->ifname);

	/* Read current params for default values */
	if ((ret = mwu_apcmd_get_bss_config(cur_if->ifname, bss)) !=
	    WPS_STATUS_SUCCESS) {
		goto done;
	}

	bss->ssid.ssid_len = cur_if->pwpa_info->oob_ssid_len;
	memcpy(&bss->ssid.ssid, cur_if->pwpa_info->oob_ssid,
	       bss->ssid.ssid_len);
	bss->protocol = UAP_PROTO_OPEN;

	mwu_printf(DEBUG_WLAN, "====== WPS OOB credentials ======\n");

	mwu_printf(DEBUG_WLAN, "SSID:%s \nSecurity:%s \n", bss->ssid.ssid,
		   wps_wlan_proto_to_security_string(bss->protocol));

	/* Set updated params */
	ret = mwu_apcmd_set_bss_config(cur_if->ifname, bss);
	mwu_apcmd_start_bss(cur_if->ifname);

done:
	FREE(bss);
	LEAVE();

	return ret;
}

/**
 *  @brief Generate Random PSK
 *
 *  @param dest      A pointer to the destination character array
 *  @param length    Destination string legth.
 *  @return          None.
 *  */
void wps_wlan_generate_random_psk(char *dest, unsigned short len)
{
	int i;
	char charset[] = "0123456789ABCDEFabcdef";

	ENTER();
	srand(time(0));
	for (i = 0; i < len; ++i) {
		dest[i] = charset[rand() % (sizeof(charset) - 1)];
	}
	LEAVE();
	return;
}

/**
 *  @brief Creates new AP configuration after AP has been reset to OOB settings
 *
 *  @param cur_if      Current interface
 *  @return            WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_wlan_create_ap_config_after_OOB(struct mwu_iface_info *cur_if)
{
	int ret = WPS_STATUS_SUCCESS;
	bss_config_t *bss = NULL;
	ENTER();

	bss = (bss_config_t *)malloc(sizeof(bss_config_t));
	memset(bss, 0, sizeof(bss_config_t));
	/*
	 * SSID update to credentials & Driver, FW.
	 */

	mwu_apcmd_stop_bss(cur_if->ifname);

	/* reset bss */
	ret = mwu_apcmd_sys_reset(cur_if);

	if (ret != MWU_ERR_SUCCESS) {
		ERR("Failed to reset AP bss");
		goto done;
	}

	/* Read current params for default values */
	if ((ret = mwu_apcmd_get_bss_config(cur_if->ifname, bss)) !=
	    WPS_STATUS_SUCCESS) {
		ERR("Failed to get AP bss config");
		goto done;
	}

	memset(cur_if->pwpa_info->oob_ssid, 0,
	       sizeof(cur_if->pwpa_info->oob_ssid));
	memset(cur_if->pwpa_info->oob_psk, 0,
	       sizeof(cur_if->pwpa_info->oob_psk));
	wps_wlan_get_OOB_ap_ssid((char *)cur_if->pwpa_info->oob_ssid,
				 bss->mac_addr);
	bss->ssid.ssid_len = cur_if->pwpa_info->oob_ssid_len =
		strlen((char *)cur_if->pwpa_info->oob_ssid);
	if (bss->ssid.ssid_len <= MAX_SSID_LEN) {
		memcpy(&bss->ssid.ssid, cur_if->pwpa_info->oob_ssid,
		       bss->ssid.ssid_len);
	} else {
		ERR("SSID length is invalid");
		ret = WPS_STATUS_FAIL;
		goto done;
	}

	bss->auth_mode = UAP_AUTH_MODE_OPEN;
	bss->protocol = UAP_PROTO_WPA2;

	bss->wpa_cfg.length = 63;
	wps_wlan_generate_random_psk((char *)bss->wpa_cfg.passphrase,
				     bss->wpa_cfg.length);

	memcpy(cur_if->pwpa_info->oob_psk, bss->wpa_cfg.passphrase,
	       bss->wpa_cfg.length);
	bss->wpa_cfg.pairwise_cipher_wpa2 = UAP_CIPH_AES;
	bss->wpa_cfg.group_cipher = UAP_CIPH_AES;

	mwu_printf(DEBUG_WLAN, "====== WPS OOB credentials ======\n");

	mwu_printf(
		DEBUG_WLAN,
		"SSID: %s proto: 0x%x pair_cip_wpa: 0x%x pair_cip_wpa2: 0x%x group_cip: 0x%x\n",
		bss->ssid.ssid, bss->protocol, bss->wpa_cfg.pairwise_cipher_wpa,
		bss->wpa_cfg.pairwise_cipher_wpa2, bss->wpa_cfg.group_cipher);
	mwu_hexdump(DEBUG_WLAN,
		    "Random Generated Passphrse: ", bss->wpa_cfg.passphrase,
		    bss->wpa_cfg.length);

	/* Set updated params */
	ret = mwu_apcmd_set_bss_config(cur_if->ifname, bss);
	if (ret != MWU_ERR_SUCCESS) {
		/*@TODO: DO a sys_reset */
		ERR("Failed to set BSS CONFIG. Reset BSS");
		mwu_apcmd_sys_reset(cur_if);
		goto done;
	}

done:
	FREE(bss);
	LEAVE();

	return ret;
}

/**
 *  @brief Checks if APs configuration is same as default OOB settings
 *
 *  @param bss        A pointer to bss_config_t structure.
 *  @return            WPS_STATUS_SUCCESS--success, WPS_STATUS_FAIL--fail
 */
int wps_check_for_default_oob_settings(bss_config_t *bss)
{
	int ret = WPS_STATUS_FAIL;
	u8 wps_oob_ssid[33];

	ENTER();
	memset(wps_oob_ssid, 0, sizeof(wps_oob_ssid));
	wps_wlan_get_OOB_ap_ssid((char *)wps_oob_ssid, bss->mac_addr);

	if (strlen((char *)wps_oob_ssid) == bss->ssid.ssid_len) {
		if ((memcmp(wps_oob_ssid, bss->ssid.ssid, bss->ssid.ssid_len) ==
		     0)) {
			LEAVE();
			return WPS_STATUS_SUCCESS;
		}
	}
	LEAVE();
	return ret;
}
#if 0
int wps_bss_sta_deauth_handler(char *ifname, u8 *sta_mac_addr)
{
    int ret = WPS_STATUS_SUCCESS;
    struct mwu_iface_info *cur_if = NULL;

    ENTER();

    cur_if = mwu_get_interface(ifname, MWPSMOD_ID);
    if (!cur_if) {
        ERR("Cannot find interface info for %s.", ifname);
        return WPS_STATUS_FAIL;
    }

    if (!cur_if->pwps_info->registration_in_progress)
            return ret;

    if ((cur_if->role == WPS_REGISTRAR &&
         !memcmp(sta_mac_addr, cur_if->pwps_info->enrollee.mac_address, ETH_ALEN)) ||
        (cur_if->role == WPS_ENROLLEE &&
         !memcmp(sta_mac_addr, cur_if->pwps_info->registrar.mac_address, ETH_ALEN))
       ) {

            mwu_printf(MSG_WARNING, "WPS Registration Failed!! Received deauth from STA during WPS registration!");

            cur_if->pwps_info->registration_fail = WPS_SET;
            cur_if->pwps_info->registration_in_progress = WPS_CANCEL;
            cur_if->pwps_info->auto_enrollee_in_progress = WPS_CANCEL;

            /* Cancel packet Tx timer */
            if (cur_if->pwps_info->set_timer == WPS_SET) {
                    wps_cancel_timer(wps_txTimer_handler, cur_if);
                    cur_if->pwps_info->set_timer = WPS_CANCEL;
                    cur_if->pwps_info->wps_msg_resent_count = 0;
            }

            cur_if->pwps_info->state = WPS_STATE_A;
            mwu_printf(DEBUG_WPS_STATE ,"Cancelling registration timer!");
            wps_cancel_timer(wps_registration_time_handler, cur_if);
            /* Reset Public keys and E-Hash, R-Hash*/
            wps_reset_wps_state(cur_if);

            if (mwu_set_beacon_probe_resp_ie(cur_if, SET_WPS_AP_SESSION_INACTIVE_IE) != MWU_ERR_SUCCESS)
                return WPS_STATUS_FAIL;

            if (cur_if->pwps_info->registrar.version >= WPS_VERSION_2DOT0 ||
                cur_if->pwps_info->enrollee.version >= WPS_VERSION_2DOT0) {

                if (mwu_set_ie(cur_if->ifname, IE_CFG_AP_ASSOCRESP,
                        SET_WPS_AP_SESSION_INACTIVE_AR_IE) != MWU_ERR_SUCCESS)
                    return WPS_STATUS_FAIL;
            }

        cur_if->pwps_info->pin_pbc_set = WPS_CANCEL;
    }
    LEAVE();
    return ret;
}
#endif
static inline int is_zero_mac(const u8 *a)
{
	return !(a[0] | a[1] | a[2] | a[3] | a[4] | a[5]);
}

/* Event IDs for Netlink socket events.  They are borrowed from mlanevent.c.
 * Note that these are not all WIFIDIR events, so they should probably be
 * defined elsewhere.
 */
#define EV_ID_UAP_EV_WMM_STATUS_CHANGE 0x00000017
#define EV_ID_UAP_EV_ID_STA_DEAUTH 0x0000002c
#define EV_ID_UAP_EV_ID_STA_ASSOC 0x0000002d
#define EV_ID_UAP_EV_ID_BSS_START 0x0000002e
#define EV_ID_UAP_EV_ID_DEBUG 0x00000036
#define EV_ID_UAP_EV_BSS_IDLE 0x00000043
#define EV_ID_UAP_EV_BSS_ACTIVE 0x00000044
#define EV_ID_WIFIDIR_GENERIC 0x00000049
#define EV_ID_WIFIDIR_SERVICE_DISCOVERY 0x0000004a
#define EV_ID_UAP_EV_RSN_CONNECT 0x00000051
#define EV_ID_STA_EV_SCAN_COMPLETE 0x80000008
#define EV_ID_STA_EV_SCAN_COMPLETE_GSPI 0x80000009

static inline char *nl_event_id_to_str(int id)
{
	switch (id) {
		PRINT_CASE(EV_ID_UAP_EV_WMM_STATUS_CHANGE);
		PRINT_CASE(EV_ID_UAP_EV_ID_STA_DEAUTH);
		PRINT_CASE(EV_ID_UAP_EV_ID_STA_ASSOC);
		PRINT_CASE(EV_ID_UAP_EV_ID_BSS_START);
		PRINT_CASE(EV_ID_UAP_EV_ID_DEBUG);
		PRINT_CASE(EV_ID_UAP_EV_BSS_IDLE);
		PRINT_CASE(EV_ID_UAP_EV_BSS_ACTIVE);
		PRINT_CASE(EV_ID_WIFIDIR_GENERIC);
		PRINT_CASE(EV_ID_WIFIDIR_SERVICE_DISCOVERY);
		PRINT_CASE(EV_ID_UAP_EV_RSN_CONNECT);
	}
	return "UNKNOWN";
}
/**
 *  @brief  Wlan event parser for FW events
 *  @param context    Pointer to Context
 *  @param wps_s      Pointer to WPS global structure
 *  @param evt_buffer Pointer to Event buffer
 *  @param evt_len    Event Length
 *
 *  @return           None
 */
void wps_wlan_event_parser(void *context, char *if_name, char *evt_buffer,
			   int evt_len)
{
	event_header *event = NULL;
	u32 event_id = 0;
	eventbuf_rsn_connect *evt_rsn_connect;
	/* Event to pass to state machine */
	struct event *e = NULL;
	struct mwu_iface_info *cur_if = NULL;
	struct mwu_iface_info *temp_if = NULL;

	/* Interface name in the event is must for supporting events on multiple
	 * interaces.*/
	if (if_name == NULL) {
		ERR("Dropping event on (NULL) interface");
		LEAVE();
		return;
	}

	ENTER();

	/* Allocate the event - We send the iface the event came from as the
	 * payload of this event to make the state machine aware of the
	 * interface event arrieved from. */

	e = malloc(sizeof(struct event) + IFNAMSIZ + 1);
	if (!e) {
		ERR("Failed to allocate memory for event");
		return;
	}

	if (strncmp(evt_buffer, "EVENT=AP_CONNECTED",
		    strlen("EVENT=AP_CONNECTED")) == 0) {
		if (is_zero_mac((const u8 *)(evt_buffer +
					     strlen("EVENT=AP_CONNECTED")))) {
			INFO("%s: Connection lost", if_name);
			e->type = WLAN_STA_KERNEL_EVENT_LINK_LOST;
			e->len = IFNAMSIZ;
			strncpy(e->val, if_name, e->len);
			// mwpamod_sta_kernel_event(e);
		} else {
			cur_if = mwu_get_interface(if_name, GENERIC_MODULE_ID);

			INFO("%s: Associated with " UTIL_MACSTR, if_name,
			     UTIL_MAC2STR(evt_buffer +
					  strlen("EVENT=AP_CONNECTED")));
			INFO("Update AP MAC Address ****");

			temp_if = mwu_get_interface("nan0", NAN_ID);
			if (temp_if && temp_if->pnan_info) {
				if (strcmp(if_name, "nan0") != 0) {
					temp_if->pnan_info->nan_other_if_active =
						TRUE;
					INFO("Interface other than nan0 is active");
				}
			}

			if (cur_if && cur_if->pwpa_info) {
				/* Update AP Mac address */
				memcpy(&cur_if->pwpa_info->sta_info.ap_address,
				       (evt_buffer +
					strlen("EVENT=AP_CONNECTED")),
				       ETH_ALEN);
			}
			e->type = WLAN_STA_KERNEL_EVENT_ASSOCIATED;
			e->len = IFNAMSIZ;
			strncpy(e->val, if_name, e->len);
			// mwpamod_sta_kernel_event(e);
		}
		goto done;
	} else if (strncmp(evt_buffer, "EVENT=PORT_RELEASE",
			   strlen("EVENT=PORT_RELEASE")) == 0) {
		INFO("%s: PORT_RELEASE event received. Authenticated!",
		     if_name);
		e->type = WLAN_STA_KERNEL_EVENT_AUTHENTICATED;
		e->len = IFNAMSIZ;
		strncpy(e->val, if_name, e->len);
		// mwpamod_sta_kernel_event(e);
		goto done;
	}

	/* Event should always have interface name prepended with it. */
	event = (event_header *)(evt_buffer);

	memcpy(&event_id, evt_buffer, sizeof(event_id));

	switch (event->event_id) {
#if 0
    case EV_ID_WIFIDIR_GENERIC:
        /* Please note wifidirect event handler expects the event data without
         * the event header */
        wifidir_driver_event(if_name, (u8 *)event->event_data,
                              evt_len - sizeof(event->event_id));
        break;
    case EV_ID_WIFIDIR_SERVICE_DISCOVERY:
        wifidir_service_discovery_driver_event(if_name, (u8*)event->event_data,
                              evt_len - sizeof(event->event_id));
        break;
    case EV_ID_UAP_EV_ID_STA_DEAUTH:
        wps_bss_sta_deauth_handler(if_name, (u8*)event->event_data+2);
        mwpamod_ap_driver_event(if_name, (u8 *)event, evt_len);
        wifidisplay_update_session_info(if_name, (u8*)event->event_data+2, WIFIDISPLAY_SESSION_DISCONNECT);
        break;

    case EV_ID_UAP_EV_WMM_STATUS_CHANGE:
    case EV_ID_UAP_EV_ID_STA_ASSOC:
    case EV_ID_UAP_EV_ID_BSS_START:
    case EV_ID_UAP_EV_ID_DEBUG:
    case EV_ID_UAP_EV_BSS_IDLE:
    case EV_ID_UAP_EV_BSS_ACTIVE:
   /* case EV_ID_WIFIDIR_SERVICE_DISCOVERY:
        mwpamod_ap_driver_event(if_name, (u8 *)event, evt_len);
        break;*/
    case EV_ID_UAP_EV_RSN_CONNECT:
        evt_rsn_connect = (eventbuf_rsn_connect *)event->event_data;

        if (EV_ID_UAP_EV_RSN_CONNECT == event->event_id)
            wifidisplay_update_session_info(if_name, evt_rsn_connect->sta_mac_address, WIFIDISPLAY_SESSION_CONNECT);

        mwpamod_ap_driver_event(if_name, (u8 *)event, evt_len);
        break;
    case EV_ID_UAP_EV_PROBE_REQUEST:
#ifdef CONFIG_HOTSPOT
        if (hotspot_is_init() != 0  && hotspot_is_ifname(if_name) != 0)
            hotspot_driver_event(if_name, (u8 *)event, evt_len);
        else
#endif
        mwpsmod_ap_driver_event(if_name, (u8 *)event, evt_len);
        break;

    case EV_ID_STA_EV_SCAN_COMPLETE:
    case EV_ID_STA_EV_SCAN_COMPLETE_GSPI:
        wps_wlan_scan_completion_handler(if_name);
        break;
#endif
	case EV_ID_NAN_GENERIC_EVENT:
		ERR("++==> NAN Generic event recvd");

		nan_driver_event(if_name, (u8 *)event->event_data,
				 evt_len - sizeof(event->event_id));
		break;

	case EV_ID_MLOCATION_COMPLETE_EVENT:
		mwu_printf(MSG_INFO,
			   "FTM_EVENT : event %s (%d) on interface %s\n",
			   nl_event_id_to_str(event->event_id), event->event_id,
			   if_name);

		mlocation_driver_event(if_name, (u8 *)event->event_data,
				       evt_len - sizeof(event->event_id));
		break;

	default:
		mwu_printf(MSG_INFO,
			   "Unhandled event %s (%d) on interface %s\n",
			   nl_event_id_to_str(event->event_id), event->event_id,
			   if_name);
		break;
	}
done:
	FREE(e);
	LEAVE();
}
#if 0
void wps_wlan_scan_completion_handler(char *ifname)
{
    struct mwu_iface_info *cur_if = NULL;
    int ret = WPS_STATUS_SUCCESS;

    ENTER();
    if (!ifname) {
        ERR("Invalid parameter.");
        LEAVE();
        return;
    }

    cur_if = mwu_get_interface(ifname, GENERIC_MODULE_ID);
    if (!cur_if) {
        ERR("Cannot find interface info for %s. Dropping scan completion event.", ifname);
        LEAVE();
        return;
    }

    if (cur_if->pmlocation_info) {
        if (cur_if->pmlocation_info->internal_scan) {
            mlocation_process_scan(cur_if);
        }

    }

    /* Check if we are in wifidirect find state */
    if (cur_if->pwifidirect_info) {
        if (cur_if->pwifidirect_info->cur_state == WIFIDIR_STATE_FIND) {
            wifidir_process_periodic_scan_results(cur_if);
            LEAVE();
            return;
        }
    }

    if (cur_if->pwps_info) {
        if(cur_if->pwps_info->scan_5G_band) {
            ret = wps_wlan_set_scan(cur_if, RF_50_G, KEEP_PREV_RESULT);
            cur_if->pwps_info->scan_5G_band = WPS_CANCEL;
            cur_if->pwps_info->scan_result_ready = WPS_CANCEL;

            if(ret == WPS_STATUS_SUCCESS) {
                LEAVE();
                return;
            } else {
                WARN("Failed to scan 5GHz band");
            }
        } else {
            cur_if->pwps_info->scan_result_ready = WPS_SET;
        }

        if(cur_if->pwps_info->dump_peers_request) {
            mwpsmod_prepare_scan_done_event(cur_if);
            cur_if->pwps_info->dump_peers_request = WPS_CANCEL;
            LEAVE();
            return;
        }

        if(cur_if->pwps_info->handle_scan_done_event == WPS_SET)
            wps_scan_timeout_handler(cur_if);
    }

    LEAVE();
}
#endif
#if 0
int
wps_wlan_set_user_scan(struct mwu_iface_info *cur_if, char *go_ssid,
        int operation)
{
    int op_channel;
    struct wifidir_peer *peer = NULL;
    int all_chan_scan_list[MWU_IOCTL_USER_SCAN_CHAN_MAX];
    int i = 0, j = 0, k = 0;

    ENTER();

    if(cur_if->pwifidirect_info->cur_peer == NULL) {
        ERR("Current Peer is not yet updated\n");
        LEAVE();
        return WPS_STATUS_FAIL;
    }

    memset(all_chan_scan_list, 0, sizeof(int)*MWU_IOCTL_USER_SCAN_CHAN_MAX);

    switch(operation) {
        case SCAN_OP_CHANNEL:
            op_channel = cur_if->pwifidirect_info->cur_peer->op_channel;
            if(op_channel) {
                /* Scan on the OP channel */
                all_chan_scan_list[0] = op_channel;
            } else {
                LEAVE();
                return WPS_STATUS_FAIL;
            }
            break;

        case SCAN_COMMON_CHANNEL_LIST:
            peer = cur_if->pwifidirect_info->cur_peer;
            if(peer->peer_channel_list.num_of_chan &&
                    cur_if->pwifidirect_info->self_channel_list.num_of_chan) {
                /* Scan on the channels that are common between peer and self */

                for(i=0; i < cur_if->pwifidirect_info->self_channel_list.num_of_chan; i++) {
                    for(j=0; j < peer->peer_channel_list.num_of_chan; j++) {
                        if(cur_if->pwifidirect_info->self_channel_list.chan[i] ==
                                peer->peer_channel_list.chan[j]) {

                            all_chan_scan_list[k] = cur_if->pwifidirect_info->self_channel_list.chan[i];
                            k++;
                        }
                    }
                }
            } else {
                LEAVE();
                return WPS_STATUS_FAIL;
            }
            break;

        default:
            ERR("Invalid option!!");
            LEAVE();
            return WPS_STATUS_FAIL;
    }

    INFO("Scanning channels - ");
    for (i = 0; i < MWU_IOCTL_USER_SCAN_CHAN_MAX; i++) {
        if (all_chan_scan_list[i] == 0)
            break;
        mwu_printf(MSG_INFO, "%d ", all_chan_scan_list[i]);
    }
    mwu_set_user_scan(cur_if->ifname, all_chan_scan_list, go_ssid);
    LEAVE();
    return WPS_STATUS_SUCCESS;
}
#endif
#endif

int wps_wlan_scan(struct mwu_iface_info *cur_if)
{
	int ret = WPS_STATUS_SUCCESS;
	int retry = 5;
	wlan_ioctl_scan_cfg scan_cfg;

	ENTER();

	/* Configure Scan Parameters */
	memset(&scan_cfg, 0, sizeof(scan_cfg));

	scan_cfg.scan_type = ACTIVE_SCAN; /* Active Scan */

	ret = wps_wlan_scan_cfg(cur_if, &scan_cfg);

	if (ret != WPS_STATUS_SUCCESS) {
		mwu_printf(MSG_WARNING, "Failed to set Active scan");
		ret = WPS_STATUS_SUCCESS;
	}

	/* Append the WPS IE in Probe Request */
	wps_wlan_set_fgscan_wps_ie(cur_if);

	/* Send the Scan IOCTL Command */
	do {
		if (cur_if->pwps_info->enrollee.dual_band_scan) {
			ret = wps_wlan_set_scan(cur_if, RF_24_G,
						FLUSH_PREV_RESULT);

			if (ret != WPS_STATUS_SUCCESS)
				goto done;

			cur_if->pwps_info->scan_5G_band = WPS_SET;
		} else {
			ret = wps_wlan_set_scan(
				cur_if, cur_if->pwps_info->enrollee.rf_bands,
				FLUSH_PREV_RESULT);
			cur_if->pwps_info->scan_5G_band = WPS_CANCEL;

			if (ret != WPS_STATUS_SUCCESS)
				goto done;
		}
		retry--;
	} while (retry > 0 && ret == WPS_STATUS_FAIL);

done:
	/* Clear the WPS IE in Probe Request */
	wps_wlan_reset_wps_ie(cur_if);

	LEAVE();
	return ret;
}

#if 0
/**
 *  @brief mapping RF band by channel
 *  @param freq     frequency value
 *  @return         channel number
 */
int wps_wlan_chan_to_band(int chan)
{
    int i, table;

    table = sizeof(channel_freq_UN_BG) / sizeof(CHANNEL_FREQ_ENTRY);
    for (i = 0; i < table; i++) {
        if (chan == channel_freq_UN_BG[i].Channel) {
            return RF_24_G;
        }
    }

    table = sizeof(channel_freq_UN_AJ) / sizeof(CHANNEL_FREQ_ENTRY);
    for (i = 0; i < table; i++) {
        if (chan == channel_freq_UN_AJ[i].Channel) {
            return RF_50_G;
        }
    }

    return RF_24_G;
}
/**
 *  @brief mapping RF band by frequency
 *
 *  @param freq     frequency value
 *  @return         channel number
 */
int
wps_wlan_freq_to_band(int freq)
{
    int i, table;

    table = sizeof(channel_freq_UN_BG) / sizeof(CHANNEL_FREQ_ENTRY);
    for (i = 0; i < table; i++) {
        if (freq == channel_freq_UN_BG[i].Freq) {
            return RF_24_G;
        }
    }

    table = sizeof(channel_freq_UN_AJ) / sizeof(CHANNEL_FREQ_ENTRY);
    for (i = 0; i < table; i++) {
        if (freq == channel_freq_UN_AJ[i].Freq) {
            return RF_50_G;
        }
    }

    return RF_24_G;
}

int os_get_random(unsigned char *buf, size_t len)
{
       FILE *f;
       size_t rc;

       f = fopen("/dev/urandom", "rb");
       if (f == NULL) {
               printf("Could not open /dev/urandom.\n");
               return -1;
       }

       rc = fread(buf, 1, len, f);
       fclose(f);

       return rc != len ? -1 : 0;
}
#endif

/**
 * inc_byte_array - Increment arbitrary length byte array by one
 * @counter: Pointer to byte array
 * @len: Length of the counter in bytes
 *
 * This function increments the last byte of the counter by one and continues
 * rolling over to more significant bytes if the byte was incremented from
 * 0xff to 0x00.
 */
void inc_byte_array(u8 *counter, size_t len)
{
	int pos = len - 1;
	while (pos >= 0) {
		counter[pos]++;
		if (counter[pos] != 0)
			break;
		pos--;
	}
}
