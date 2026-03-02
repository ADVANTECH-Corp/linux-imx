/** @file wps_def.h
 *  @brief This file contains definitions for WPS global information.
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

#ifndef _WPS_DEF_H_
#define _WPS_DEF_H_

#include <stdint.h>
#include "mwu_defs.h"

#ifndef MIN
/** Macro to get minimun number */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif /* MIN */

#ifndef MAX
/** Macro to get maximun number */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef NELEMENTS
/** Number of elements in aray x */
#define NELEMENTS(x) (sizeof(x) / sizeof(x[0]))
#endif

/** Configuration file for initialization */
#define FILE_WPSINIT_CONFIG_NAME "./config/wps_init.conf"
/** Configuration file for multi-instance checking */
#define FILE_WPSRUN_CONFIG_NAME "/tmp/wpsapp.pid"

/** Size: 1 Byte */
#define SIZE_1_BYTE 1
/** Size: 2 Byte */
#define SIZE_2_BYTES 2
/** Size: 4 Byte */
#define SIZE_4_BYTES 4
/** Size: 6 Byte */
#define SIZE_6_BYTES 6
/** Size: 8 Byte */
#define SIZE_8_BYTES 8
/** Size: 16 Byte */
#define SIZE_16_BYTES 16
/** Size: 20 Byte */
#define SIZE_20_BYTES 20
/** Size: 32 Byte */
#define SIZE_32_BYTES 32
/** Size: 64 Byte */
#define SIZE_64_BYTES 64
/** Size: 80 Byte */
#define SIZE_80_BYTES 80
/** Size: 128 Byte */
#define SIZE_128_BYTES 128
/** Size: 192 Byte */
#define SIZE_192_BYTES 192
/** Size: 256 Byte */
#define SIZE_256_BYTES 256
/** Size: 512 Byte */
#define SIZE_512_BYTES 512

/** IEEE 802.11 mode : infra */
#define IEEE80211_MODE_INFRA 0
/** IEEE 802.11 mode : IBSS */
#define IEEE80211_MODE_IBSS 1

/** IEEE 802.11 capability : ESS */
#define IEEE80211_CAP_ESS 0x0001
/** IEEE 802.11 capability : IBSS */
#define IEEE80211_CAP_IBSS 0x0002
/** IEEE 802.11 capability : PRIVACY */
#define IEEE80211_CAP_PRIVACY 0x0010

/** Scan AP limit */
#define SCAN_AP_LIMIT 128

/** Return type: success */
#define WPS_STATUS_SUCCESS (0)
/** Return type: failure */
#define WPS_STATUS_FAIL (-1)

/** WPS Set */
#define WPS_SET 1
/** WPS Cancel */
#define WPS_CANCEL 0

/** Discovery phase : Start */
#define WPS_START_REG_DISCOVERY_PHASE 1
/** Discovery phase : End */
#define WPS_END_REG_DISCOVERY_PHASE 0

/** BSS type : STA */
#define BSS_TYPE_STA 0
/** BSS type : UAP */
#define BSS_TYPE_UAP 1

/* Indicates that wild card MAC address is found in APs Authorized MAC list */
#define WPS_BROADCAST_MAC 1
/* Indicates that enrollee's MAC address is found in APs Authorized MAC list */
#define WPS_AUTH_MAC 2

/* Auto PIN: PIN Discovery should be performed against top 4 APs */
#define AUTO_PIN_MAX_AP_COUNT 4

/** Skip scan and start adhoc network */
#define WPS_SKIP_SCAN_ADHOC_START 1
/** scan and join/start adhoc network */
#define WPS_SCAN_ADHOC_JOIN 0

/** Maximum event buffer size */
#define EVENT_MAX_BUF_SIZE 1024

/** Role enrollee after discovery */
#define IS_DISCOVERY_ENROLLEE(cur_if)                                          \
	(((cur_if)->role == WPS_ADHOC_EXTENTION ||                             \
	  (cur_if)->role == WIFIDIR_ROLE) &&                                   \
	 (cur_if)->discovery_role == WPS_ENROLLEE)
/** Role registrar after discovery */
#define IS_DISCOVERY_REGISTRAR(cur_if)                                         \
	(((cur_if)->role == WPS_ADHOC_EXTENTION ||                             \
	  (cur_if)->role == WIFIDIR_ROLE) &&                                   \
	 (cur_if)->discovery_role == WPS_REGISTRAR)

/** Maximum WIFIDIR devices in Find phase */
#define WIFIDIR_MAX_FIND_DEVICES 16
/** Maximum WIFIDIR device name length */
#define MAX_DEVICE_NAME_LEN 32
/** Maximum WIFIDIR manufacturer length */
#define MAX_MANUFACTURER_LEN 64
/** Maximum WIFIDIR model name length */
#define MAX_MODEL_NAME_LEN 32
/** Maximum WIFIDIR model number length */
#define MAX_MODEL_NUMBER_LEN 32
/** Maximum WIFIDIR serial number length */
#define MAX_SERIAL_NUMBER_LEN 32
/** Maximum WIFIDIR secondary device info length */
#define MAX_SECONDARY_DEVICE_LEN 32
/** Maximum WIFIDIR clients */
#define MAX_WIFI_DIRECT_CLIENT 8
/** Maximum WIFIDIR channel nos. in channel list */
#define MAX_CHANNEL_NUM 80

/** WPS Device Type Length */
#define WPS_DEVICE_TYPE_LEN 8
/** Maximum Bit Map bit length */
#define MAX_BITMAP_LEN 16
/** Maximum Action length */
#define MAX_ACTION_LEN 8
#ifdef CONFIG_WPS_UPNP_ER
/** Maximum UUID length */
#define MAX_STRING_UUID_LEN 36
#endif

/** UPNP Manufacturer URL length */
#define MAX_MANUFACTURER_URL_LEN 100
/** UPNP Model URL length */
#define MAX_MODEL_URL_LEN 150
/** UPNP Model description length */
#define MAX_MODEL_DESC_LEN 100
/** UPNP Product code length */
#define MAX_PRODUCT_CODE_LEN 32

/** Config methods mask (bit 8:0) */
#define CFG_METHODS_MASK 0x01FF

/** Action SET */
#define ACTION_GET (0)
/** Action GET */
#define ACTION_SET (1)

/** Maximum Persistent peers possible in a group */
#define WIFIDIR_MAX_PERSISTENT_PEERS (8)

/** Max Persistent records */
#define WIFIDIR_MAX_PERSISTENT_RECORDS (2)

/** Maximum Peers in invitation list [maintained at the FW] */
#define WIFIDIR_MAX_INVITATION_LIST_ENTRIES 2

/** PSK max len */
#define WIFIDIR_PSK_LEN_MAX 64

/** Not configured state */
#define SC_NOT_CONFIGURED_STATE 0x01
/** Configured state */
#define SC_CONFIGURED_STATE 0x02

/** enum : WPS input state */
typedef enum {
	WPS_INPUT_STATE_NO_INPUT = 0,
	WPS_INPUT_STATE_METHOD,
	WPS_INPUT_STATE_READ_PIN,
	WPS_INPUT_STATE_CONNECT_AP_INDEX,
	WPS_INPUT_STATE_QUIT_CONNECT
} WPS_INPUT_STATE;

/** WPS Message Default Timeout */
#define WPS_MSG_DEFAULT_TIMEOUT 5000 /* ms */
/** WPS Message Short Timeout */
#define WPS_MSG_SHORT_TIMEOUT 3000 /* ms */
/** WPS Message Long Timeout */
#define WPS_MSG_LONG_TIMEOUT 10000 /* ms */
/** WPS Message Maximum Resent */
#define WPS_MSG_RETRY_LIMIT 3
/** WPS Message Maximum Resent */
#define WPS_MSG_MAX_RESENT 30

/** Authentication Type: ALL */
#define AUTHENTICATION_TYPE_ALL 0x003F

/** Encryption Type: None */
#define ENCRYPTION_TYPE_NONE 0x0001
/** Encryption Type: WEP */
#define ENCRYPTION_TYPE_WEP 0x0002
/** Encryption Type: TKIP */
#define ENCRYPTION_TYPE_TKIP 0x0004
/** Encryption Type: AES */
#define ENCRYPTION_TYPE_AES 0x0008

/** Encryption Type: AES */
#define ENCRYPTION_TYPE_TKIP_AES_MIXED                                         \
	(ENCRYPTION_TYPE_TKIP | ENCRYPTION_TYPE_AES)

/** Encryption Type: ALL */
#define ENCRYPTION_TYPE_ALL 0x000F

/** Config Method: USBA */
#define CONFIG_METHOD_USBA 0x0001
/** Config Method: ETHERNET */
#define CONFIG_METHOD_ETHERNET 0x0002
/** Config Method: LABEL */
#define CONFIG_METHOD_LABEL 0x0004
/** Config Method: DISPLAY */
#define CONFIG_METHOD_DISPLAY 0x0008
/** Config Method: Physical DISPLAY */
#define CONFIG_METHOD_PHY_DISPLAY 0x4008
/** Config Method: Virtual DISPLAY */
#define CONFIG_METHOD_VIR_DISPLAY 0x2008
/** Config Method: EXTERNAL_NFC */
#define CONFIG_METHOD_EXTERNAL_NFC 0x0010
/** Config Method: INTEGRATED_NFC */
#define CONFIG_METHOD_INTEGRATED_NFC 0x0020
/** Config Method: NFC_INTERFACE */
#define CONFIG_METHOD_NFC_INTERFACE 0x0040
/** Config Method: PUSHBUTTON */
#define CONFIG_METHOD_PUSHBUTTON 0x0080
/** Config Method: KEYPAD */
#define CONFIG_METHOD_KEYPAD 0x0100
/** Config Method: SMPBC */
#define CONFIG_METHOD_SMPBC 0x0880

#endif /* _WPS_DEF_H_ */
