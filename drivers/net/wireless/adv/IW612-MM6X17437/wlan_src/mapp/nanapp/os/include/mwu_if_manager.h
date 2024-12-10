/** @file mwu_if_manager.h
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

#ifndef __MWU_IF_MANAGER_H__
#define __MWU_IF_MANAGER_H__

#if !defined(ANDROID) || defined(HAVE_GLIBC)
#include <sys/socket.h>
#endif

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/time.h>

#include "mwu_defs.h"
#include "wps_def.h"
#include "queue.h"

#include "mwu_timer.h"

#include "nan.h"
#include "nan_eapol.h"

#include "mlocation.h"

/** Maximum number of supported interfaces */
#define MAX_IFACE 16

/** Max enrollee for PBC overlap detection */
#define MAX_PBC_ENROLLEE 8

/** Array index of each IE index */
#define IE_CFG_WIFIDIR 0
#define IE_CFG_WIFIDIR_WPS 1
#define IE_CFG_AP_BCN 2
#define IE_CFG_PROBE_RESP 3
#define IE_CFG_AP_ASSOCRESP 4
#define IE_CFG_PROBE 5
#define IE_CFG_ASSOCREQ 6
#define IE_CFG_WIFIDISPLAY 7
#define IE_CFG_MAX 8

#define EAP_FRAG_BUF_SIZE (1500)
#define EAP_FLAGS_MORE_FRAG (0x01)
#define EAP_FLAGS_LEN_FIELD (0x02)
#define EAP_FRAG_THRESHOLD_DEF (1200)
#define EAP_FRAG_THRESHOLD_MAX (2300)
#define EAP_FRAG_THRESHOLD_PF (100)

#define WPS_PUB_KEY_LEN (192)
#define WPS_PRIV_KEY_LEN (192)
#define WPS_AGREE_KEY_LEN (192)

#define MODEL_NUMBER_LEN (32)
#define SERIAL_NUMBER_LEN (32)
#define VENDOR_EXT_LEN (1024)

/** Max WPS_IE length */
#define MAX_WPS_IE_LEN 256
/** Max SSID WPA_IE length */
#define SSID_MAX_WPA_IE_LEN 40
/** Max WIFIDIR_IE length */
#define MAX_WIFIDIR_IE_LEN 256
/** Max. no of CREDENTIAL DATA */
#define MAX_NUM_CRDENTIAL 5

/** Max Extended Cap len */
#define MAX_EXTENDED_CAP 10

/** WPA role STA */
#define WPA_ROLE_STA 1
/** WPA role AP */
#define WPA_ROLE_AP 2

/** Max clients supported by AP */
#define MAX_AP_CLIENTS 32

/** Max active miracast client supported by GO */
#define WIFIDISPLAY_MAX_ACTIVE_CLIENTS 16

/** Maximum no of channels in channel list */
#define MAX_CHANNELS 165

#ifdef CONFIG_WPS_UPNP_AP
#define WPS_UPNP_NONE 0
#define WPS_UPNP_UAP_NONE 1
#define WPS_UPNP_ENROLLEE 3
#define WPS_UPNP_PROXY 5
#endif

/* struct peer_list: a list of wifidir_peers
 *
 * This SLIST_HEAD macro defines the head of a linked list of wifidir_peer
 * structs.  The struct peer_list can be be manipulated using the SLIST_*
 * macros defined in queue.h.  Note that the "list_item" member is the
 * SLIST_ENTRY that is required to navigate the peer list.  See the
 * wifidir_peer_* functions in wifidir.c for examples of using the peer list.
 */
SLIST_HEAD(peer_list, wifidir_peer);

/** Module Identifier */
#define MWPSMOD_ID 0x1
#define MWPAMOD_ID (1 << 1)
#define WIFIDIR_ID (1 << 2)
#define MNFCMOD_ID (1 << 3)
#define NAN_ID (1 << 4)
#define MLOCATION_ID (1 << 5)
#define GENERIC_MODULE_ID 0XFFFF

/* mwpamod_net_info: network info specifying encryption, ssid, etc.
 *
 * ssid: The null-terminated SSID of the network.
 *
 * key: The actual encryption key
 *
 * key_len: length of the key.  If len is 64, key is a null-terminated 32-byte
 * PSK repreasented as 64 hex chars for a WPA2 network.  If len is less than
 * 64, key is a null-terminated ascii passphrase for a WPA2 network.  Note that
 * key_len never includes the null-termination byte.  If len is 0, key is
 * ignored and the net_info represents an open network.
 *//* MWPAMOD_MAX_SSID: maximum ssid length.
 */
#define MWPAMOD_MAX_SSID 32
#define MWPAMOD_MAX_KEY 64
#define MWPAMOD_MIN_KEY 8

struct mwpamod_net_info {
	char ssid[MWPAMOD_MAX_SSID + 1];
	char key[MWPAMOD_MAX_KEY + 1];
	unsigned short auth;
	unsigned short encrypt;
	int key_len;
};

/** chan_list data structure */
struct CHANNEL_LIST {
	/** Number of channel */
	int num_of_chan;
	/** Channel number*/
	u16 chan[MAX_CHANNELS];
};

struct wifidir_device_config {
	/** Device Name */
	char device_name[32];
	/** Manufacturer length */
	u16 device_name_length;
	/** Manufacturer */
	char manufacturer[64];
	/** Manufacturer length */
	u16 manufacture_length;
	/** Model name */
	char model_name[32];
	/** Model name length */
	u16 model_name_length;
	/** Model number */
	char model_number[32];
	/** Model number length */
	u16 model_number_length;
	/** Serial number */
	char serial_number[32];
	/** Serial number length */
	u16 serial_number_length;
	/** RF Bands */
	u8 rf_bands;
	/** UUID */
	unsigned char UUID[UUID_MAX_LEN];
};

#ifdef CONFIG_NFC

// SHRUTI TODO: Remove duplication of #defines and structures.
#define NFC_PUBKEY_HASH_LEN 20
#define NFC_DEVICE_PASSWORD_LEN 32
#define WFD_MAX_SECONDARY_DEVICE_LEN 32

/** wifidir_device_info: wifi direct device parameteres */
struct wifidir_dev_info {
	/** WIFIDIR device address */
	u8 dev_address[ETH_ALEN];
	/** WPS config methods */
	u16 config_methods;
	/** Primary device type : category */
	u16 primary_category;
	/** Primary device type : OUI */
	u8 primary_oui[4];
	/** Primary device type : sub-category */
	u16 primary_subcategory;
	/** Secondary Device Count */
	u8 secondary_dev_count;
	/** Secondary Device Info */
	u8 secondary_dev_info[WFD_MAX_SECONDARY_DEVICE_LEN];
	/** WPS Device Name Tag */
	u16 device_name_type;
	/** WPS Device Name Length */
	u16 device_name_len;
	/** Device name */
	char device_name[32 + 1];
};

/** wifidir_oob_goneg_chan: wifi direct device parameters */
struct wifidir_oob_goneg_channel {
	/** WIFIDIR country string */
	u8 country_string[3];
	/** WIFIDIR operating class */
	u8 operating_class;
	/** WIFIDIR channel number */
	u8 channel_number;
	/** WIFIDIR Role indication */
	u8 role_indication;
};

typedef struct wps_nfc_password_s {
	u8 pubkey_hash[NFC_PUBKEY_HASH_LEN];
	u16 oob_dev_pw_id;
	u8 dev_password[NFC_DEVICE_PASSWORD_LEN];
	size_t dev_password_len;
} wps_nfc_password_t;

// SHRUTI TODO: If possible, Integrate with wifidir_nfc_credentials
struct nfc_params {
	u8 nfc_ho_role;

	/* WPS Parameters */
	/** Public key */
	u8 public_key[WPS_PUB_KEY_LEN];
	/** Private key */
	u8 private_key[WPS_PRIV_KEY_LEN];
	/** Public key hash */
	u8 pubkey_hash[NFC_PUBKEY_HASH_LEN];
	/** Device Passowrd Id */
	u16 dev_pwd_id;
	/** Device Passowrd (binary form) */
	u8 dev_pwd[NFC_DEVICE_PASSWORD_LEN];
	/** Device Passowrd Len (binary form) */
	size_t dev_pwd_len;

	/* P2P Paramters */
	/* P2P Device Capability */
	u8 dev_capability;
	/* P2P Group Capability */
	u8 grp_capability;
	/* P2P Device Info */
	struct wifidir_dev_info p2p_dev_info;
	/* P2P OOB GO Negotiation Channel */
	struct wifidir_oob_goneg_channel oob_go_neg_chan;
};
#endif

struct WIFIDIRECT_INFO {
	/** Current state in wifidir state machine */
	u32 cur_state;
	/** Peer which we are currently working with */
	struct wifidir_peer *cur_peer;
	/** Number of find results */
	s32 num_find_results;
	/** Current peer index */
	s32 dev_index;
	/** Self channel list */
	struct CHANNEL_LIST self_channel_list;
	/** Wi-Fi Direct Interface MAC address */
	u8 interface_mac_addr[ETH_ALEN];
	/** Wi-Fi Direct Intended MAC address*/
	u8 intended_mac_addr[ETH_ALEN];
	/** Wi-Fi Direct Invitation procedure state variable */
	u8 invite_state;

	/** Wi-Fi Direct Device Configuration*/
	struct wifidir_device_config dev_cfg;

	/** Wi-Fi Display parameters */
	u8 wifidisplay_enabled;
	u16 wifidisplay_device_info;
	u8 wifidisplay_client_list[WIFIDISPLAY_MAX_ACTIVE_CLIENTS][ETH_ALEN];

#ifdef CONFIG_NFC
	/* OOB NFC Negotiated Handover */
	u8 oob_wfd_neg_ho;
	/* OOB NFC Static Handover (Read from tag)*/
	u8 oob_wfd_read_static_ho;
	/* OOB NFC Static Handover (Write to tag)*/
	u8 oob_wfd_write_static_ho;

	/** Peer NFC paramteres */
	struct nfc_params peer_nfc_info;
	/** Self NFC paramteres */
	struct nfc_params self_nfc_info;
#endif
	/* We maintain a list of peers that we know about.  For this we use a
	 * BSD SLIST called peers.
	 */
	struct peer_list peers;
	/* we track the peer which expires next */
	struct wifidir_peer *next_expiring_peer;
};

struct CREDENTIAL_DATA {
	/** Network index */
	u8 network_index;
	/** SSID */
	u8 ssid[MAX_SSID_LEN];
	/** SSID length */
	u8 ssid_length;
	/** Authentication type */
	u16 auth_type;
	/** Encryption type */
	u16 encry_type;
	/** Network key index */
	u8 network_key_index;
	/** WEP tranmit key index */
	u8 wep_tx_key_index;
	/** Network key */
	u8 network_key[64];
	/** Network key len */
	u8 network_key_len;
	/** Mac address */
	u8 mac_address[ETH_ALEN];
	/** RF bands */
	u8 rf_bands;
	/** AP Channel */
	u16 ap_channel;
};

struct MESSAGE_ENROLLEE_REGISTRAR {
	/** version */
	u8 version;
	/** version */
	u8 version2;
	/** UUID */
	u8 wps_uuid[16];
	/** Length of UUID */
	u16 wps_uuid_length;
	/** Public key */
	u8 public_key[WPS_PUB_KEY_LEN];
	/** Private key */
	u8 private_key[WPS_PRIV_KEY_LEN];
	/** Agreed key */
	u8 agreed_key[WPS_AGREE_KEY_LEN];
	/** nonce */
	u8 nonce[16];
	/** Manufacture */
	u8 manufacture[64];
	/** Manufacture length */
	u16 manufacture_length;
	/** Model name */
	u8 model_name[32];
	/** Model name length */
	u16 model_name_length;
	/** Model number */
	u8 model_number[MODEL_NUMBER_LEN];
	/** Model number length */
	u16 model_number_length;
	/** Serial number */
	u8 serial_number[SERIAL_NUMBER_LEN];
	/** Serial number length */
	u16 serial_number_length;
	/** Device name */
	u8 device_name[33];
	/** Device name length */
	u16 device_name_length;
	/** Association state */
	u8 association_state;
	/** Configuration error */
	u8 configuration_error;
	/** OS Version */
	u32 os_version;
	/** e_hash1 */
	u8 e_hash1[32];
	/** e_hash2 */
	u8 e_hash2[32];
	/** r_hash1 */
	u8 r_hash1[32];
	/** r_hash2 */
	u8 r_hash2[32];
	/** e_s1 */
	u8 e_s1[16];
	/** e_s2 */
	u8 e_s2[16];
	/** r_s1 */
	u8 r_s1[16];
	/** r_s2 */
	u8 r_s2[16];
	/** IV */
	u8 IV[16];
	/** device password id */
	u16 device_password_id;
	/** updated device password id */
	u16 updated_device_password_id;
	/** Authentication type flag */
	u16 auth_type_flag;
	/** Encryption type flag */
	u16 encry_type_flag;
	/** Connection type flag */
	u8 connection_type_flag;
	/** Config methods */
	u16 config_methods;
	/** Config state */
	u8 simple_config_state;
	/** Primary device type */
	u8 primary_device_type[8];
	/** RF bands */
	u8 rf_bands;
	/** Authenticator */
	u8 authenticator[8];
	/** Encrypted data */
	u8 encrypted_data[0x800];
	/** Encrypted data length */
	u16 encrypted_data_len;
	/** WRAP raw data */
	u8 wrap_raw_data[0x500];
	/** MAC address */
	u8 mac_address[6];
	/** GO ssid */
	char go_ssid[MAX_SSID_LEN + 1];
	/** Credential data */
	struct CREDENTIAL_DATA cred_data[MAX_NUM_CRDENTIAL];
	/* Dual Band Scan */
	u8 dual_band_scan;
	/* Current RF Band */
	u8 current_rf_band;
};

struct MESSAGE_BUFFER {
	/** Message length */
	u16 length;
	/** Message */
	u8 message[2048];
};

struct WPS_PBC_ENROLLEE_INFO {
	/** Mac address of PBC Enrollee */
	u8 pbc_enrollee_mac_addr[ETH_ALEN];
	/** UUID of PBC Enrollee */
	u8 pbc_enrollee_uuid[UUID_MAX_LEN];
	/** Time value at which Probe request or M1 was received */
	struct timeval pbc_enrollee_time;
};

struct WPS_L2_INFO {
	/** Packet socket for EAPOL frames */
	int fd;
	/** Interface name */
	char ifname[IFNAMSIZ + 1];
	/** Interface index */
	int ifindex;
	/** MAC Address */
	u8 my_mac_addr[ETH_ALEN];
	/** callback handler */
	void (*rx_callback)(const u8 *src_addr, const u8 *buf, size_t len,
			    struct WPS_L2_INFO *l2);
	/** Flag to decide whether to include layer 2 (Ethernet) header data
	 * buffers */
	int l2_hdr;
};

struct WPS_SSID {
	/** SSID */
	u8 ssid[SSID_MAX_WPA_IE_LEN];
	/** SSID Length */
	size_t ssid_len;
	/** BSSID */
	u8 bssid[ETH_ALEN];

	/*
	 * mode - IEEE 802.11 operation mode (Infrastructure/IBSS)
	 * 0 = infrastructure (Managed) mode, i.e., associate with an AP.
	 * 1 = IBSS (ad-hoc, peer-to-peer)
	 */
	/** IEEE 802.11 operation mode (Infrastructure/IBSS) */
	int mode;
};

/**
 * struct wpa_scan_result - Scan results
 *
 * This structure is used as a generic format for scan results from the
 * driver. Each driver interface implementation is responsible for converting
 * the driver or OS specific scan results into this format.
 */
struct SCAN_RESULTS {
	/** BSSID */
	u8 bssid[ETH_ALEN];
	/** SSID */
	u8 ssid[32];
	/** SSID length */
	size_t ssid_len;
	/** WPA IE */
	u8 wpa_ie[SSID_MAX_WPA_IE_LEN];
	/** WPA IE length */
	size_t wpa_ie_len;
	/** RSN Ie */
	u8 rsn_ie[SSID_MAX_WPA_IE_LEN];
	/** RSN IE length */
	size_t rsn_ie_len;
	/** WPS IE */
	u8 wps_ie[MAX_WPS_IE_LEN];
	/** WPS IE length */
	size_t wps_ie_len;
	/** WIFIDIR IE */
	u8 wifidir_ie[MAX_WIFIDIR_IE_LEN];
	/** WIFIDIR IE length */
	size_t wifidir_ie_len;
	/** Frequency of the channel in MHz (e.g., 2412 = channel 1) */
	int freq;
	/** Capability information field in host byte order */
	u16 caps;
	/** Signal quality */
	int qual;
	/** Noise level */
	int noise;
	/** Signal level */
	int level;
	/** Maximum supported rate */
	int maxrate;
	/** Extended Cap */
	u8 extended_cap[MAX_EXTENDED_CAP];
};

typedef struct _wlan_get_scan_table_fixed {
	/** BSSID of this network */
	u8 bssid[ETH_ALEN];
	/** Channel this beacon/probe response was detected */
	u8 channel;
	/** RSSI for the received packet */
	u8 rssi;
	/** TSF value from the firmware at packet reception */
	u64 network_tsf;
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
	u32 fixed_field_length;

	/**
	 *  Length of the BSS Information (probe resp or beacon) that
	 *    follows after the fixed_field_length
	 */
	u32 bss_info_length;

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
	/* u8  bss_info_buffer[1];*/
} wlan_ioctl_get_scan_table_entry;

/**
 *  Sructure to retrieve the scan table
 */
typedef struct {
	/**
	 *  - Zero based scan entry to start retrieval in command request
	 *  - Number of scans entries returned in command response
	 */
	u32 scan_number;
	/**
	 * Buffer marker for multiple wlan_ioctl_get_scan_table_entry
	 * structures. Each struct is padded to the nearest 32 bit boundary.
	 */
	u8 scan_table_entry_buf[1];

} wlan_ioctl_get_scan_table_info;

struct EVENT_INFO {
	/** event Info */
	int evt_fd;
	/** IO vector */
	void *evt_iov;
	/** event netlink header */
	void *evt_nl_head;
	/** event messsage header */
	void *evt_msg_head;
	/** event destination address */
	void *evt_dest_addr;
};

struct WPS_DATA {
	/** Layer 2 Info */
	struct WPS_L2_INFO *l2;
	/** Interface name */
	char ifname[IFNAMSIZ + 1];
	/** Bridge name */
	char brname[IFNAMSIZ + 1];
	/** BSSID */
	u8 bssid[ETH_ALEN];
	/** WPS SSID */
	struct WPS_SSID current_ssid;
	/** Scan results */
	struct SCAN_RESULTS *scan_results;
	/** number of scan results */
	int num_scan_results;
	/** User abort flag */
	u8 userAbort;
	/** number of EAPOL packets received after the previous association
	 * event */
	int eapol_received;
};

#if defined(CONFIG_WPS_UPNP_AP) || defined(CONFIG_NFC)
struct wpabuf {
	size_t size; /* total size of the allocated buffer */
	size_t used; /* length of data in the buffer */
	u8 *ext_data; /* pointer to external data; NULL if data follows
		       * struct wpabuf */
	/* optionally followed by the allocated buffer */
};

extern struct wpabuf *wpabuf_alloc(size_t len);
extern void wpabuf_free(struct wpabuf *buf);
extern void *wpabuf_put(struct wpabuf *buf, size_t len);
extern struct wpabuf *wpabuf_alloc_copy(const void *data, size_t len);

static inline const void *wpabuf_head(const struct wpabuf *buf)
{
	if (buf->ext_data)
		return buf->ext_data;
	return buf + 1;
}

static inline const u8 *wpabuf_head_u8(const struct wpabuf *buf)
{
	return wpabuf_head(buf);
}

static inline size_t wpabuf_len(const struct wpabuf *buf)
{
	return buf->used;
}
#endif

#ifdef CONFIG_WPS_UPNP_AP
struct wpabuf_global {
	u32 length;
	u8 nonce[16];
	u8 ext_data[968];
	u16 wps_stage;
};

struct wpabuf_proxy_event {
	u32 length;
	u8 ext_data[968];
};

struct wps_upnp_datas {
	u8 nonce_m1[16];
	u8 pubkey_m1[200];
	u8 authkey_m2[32];
	u8 keywrapkey_m2[16];
	u8 emsk_m2[32];
	u8 snonce_m3[32];
};

struct wps_upnp_mode {
	u8 wps_upnp_mode;
};

struct wps_upnp_info {
	struct wpabuf_global global_msg;
	struct wpabuf_proxy_event upnp_proxy_data;
	struct wps_upnp_datas global_datas;
	struct wps_upnp_mode g_wps_upnp_mode;
};
#endif /* CONFIG_WPS_UPNP_AP */

struct WPS_INFO {
	/** current Tx msg buffer */
	u8 buffer[2048];
	/** EAP packet ID of peer station */
	u8 peer_id;
	/** EAP packet ID */
	u8 id;
#ifndef CONFIG_NFC
	/** PIN */
	u8 PIN[32];
	/** PIN length */
	u16 PINLen;
#else
	u8 PIN[65]; // To take care of NFC PIN in uppercase ASCII format
	/** PIN length */
	u16 PINLen;
#endif
	/** Static PIN setting */
	u8 static_pin;
	/** Key wrap authenticator */
	u8 key_wrap_authenticator[8];
	/** Authentication key */
	u8 auth_key[32];
	/** Key wrap key */
	u8 key_wrap_key[16];
	/** EMSK */
	u8 emsk[32];
	/** Enrollee private data */
	struct MESSAGE_ENROLLEE_REGISTRAR enrollee;
	/** Registrar private data */
	struct MESSAGE_ENROLLEE_REGISTRAR registrar;
	void *dh;
	/** Last message */
	struct MESSAGE_BUFFER last_message;
	/** Last Tx message, as opposited to the current Tx msg buffer*/
	struct MESSAGE_BUFFER last_tx_message;
	/** Role Switch */
	u8 role_switched;
	/** PIN Generator - Enrollee or Registrar */
	u8 pin_generator;
	/** PIN genrator status*/
	u8 wifidir_pin_generated;
	/** WIFIDIR Intended Address */
	u8 wifidir_intended_addr[ETH_ALEN];
	/** Mode for BSS or IBSS */
	u8 mode;
	/** WPS current state */
	u8 state;
	/** WPS input state */
	u8 input_state;
	/** Whether PIN is entered or method PBC is selected */
	u8 pin_pbc_set;
	/** EAP message sent flag */
	u8 eap_msg_sent;
	/** EAP Identity count */
	u8 eap_identity_count;
	/** WPS message resent count */
	u8 wps_msg_resent_count;
	/** WPS message max retry */
	u8 wps_msg_max_retry;
	/** WPS message timeout */
	u32 wps_msg_timeout;
	/** Flag for timer set */
	u8 set_timer;
	/** Flag for registration completed */
	u8 register_completed;
	/** Power save state of WLAN driver */
	int ps_saved;
	/** Flag for restart loop by M2D received */
	u8 restart_by_M2D;
	/** Flag for PBC Auto */
	u8 pbc_auto;
	/** Flag for PIN Auto */
	u8 pin_auto;
	/** Used during Auto PIN function */
	u8 auto_pin_index;
	/** Flag for registration in progress */
	u8 registration_in_progress;
	/** Flag for registration fail */
	u8 registration_fail;
	/** Flag for restart for link lost*/
	u8 restart_link_lost;
	/** Primary Device category*/
	u16 primary_dev_category;
	/** Primary Device subcategory*/
	u16 primary_dev_subcategory;
	/** Read AP config only */
	u8 read_ap_config_only;
	/** Continue Async scan operation */
	u8 keep_scanning;
	/** Enable Scan Completion Handler */
	u8 handle_scan_done_event;
	/** Scan 5G Band */
	u8 scan_5G_band;
	/** Flag that indicates the scan result status */
	u8 scan_result_ready;
	/** Received dump_peers command */
	u8 dump_peers_request;
	/** Indicates if AP/GO found in scan result */
	s32 dev_found;

	u32 frag_thres;
	u8 frag_buf[EAP_FRAG_BUF_SIZE];
	u32 frag_rx_in_prog;
	u32 frag_tx_in_prog;
	u32 frag_msg_tot_len;
	u32 frag_msg_cur_len;

	u32 proto_ext_test;
	u32 tx_frag_test;
	/** WPS state*/
	u8 wps_device_state;
	/** Last received WPS message*/
	s32 last_recv_wps_msg;
	/** AP Configuration loading after OOB*/
	u8 config_load_by_oob;
	/** AP Setup locked*/
	u8 wps_ap_setup_locked;
	/** R-Hash failure counter*/
	u8 wps_r_hash_failure_count;
	/** MAC Addresses of Authorized Enrollees */
	u8 auth_enrollee_mac_addr[4 * ETH_ALEN];
	/** Enrollee in Authorized Mac address list*/
	u8 enrollee_in_authorized_mac;
	/** is Device low UI*/
	u8 is_low_ui_device;
	/** NACK error count*/
	u8 nack_error_count;
	/** WPS PBC Enrollee data structure*/
	struct WPS_PBC_ENROLLEE_INFO wps_pbc_enrollee_info[MAX_PBC_ENROLLEE];
	/** Time value at which last Registrar PBC session overlap has occured
	 */
	struct timeval wps_last_overlap_timeval;

	/** Invalid Credential received value*/
	u8 invalid_credential;

	/* Our P2P Device capability */
	u8 self_p2p_dev_cap;
	/* Our P2P Group capability */
	u8 self_p2p_grp_cap;
	/* Custom reset OOB to immediately load WPA2 mixed mode */
	u8 reset_oob_mixed;
	/* Strictly transfer PSK instead of Passphrase in WPS handshake*/
	u8 use_psk;
	/* OUI */
	u8 oui[4];
	/* Vendor Extension */
	u8 vendor_ext[VENDOR_EXT_LEN];
	/* Vendor extension length */
	u16 vendor_ext_len;

	u8 wps_session_active;
	u8 auto_enrollee_in_progress;
	u8 seed_er_attacks;

	struct WPS_DATA wps_data;
	u16 setup_locked_timeout;
	u8 max_setup_locked_attempts;
	char init_cfg_file[100];
#ifdef CONFIG_NFC
	/* OOB NFC Password Token */
	u8 oob_wps_pt;
	/* OOB NFC Handover */
	u8 oob_wps_ct;
	/* OOB NFC Handover */
	u8 oob_wps_ho;
	/* OOB NFC self pubkey hash */
	u8 oob_self_pubkey_hash[20];
	/* OOB NFC peer pubkey hash */
	u8 oob_peer_pubkey_hash[20];
	/* OOB  NFC Hash Msimatch */
	u8 oob_wps_hash_mismatch;
#endif
#ifdef CONFIG_WPS_UPNP
	u8 enable_upnp;
	struct wps_context *wps;
	char upnp_iface[17];
#endif
#ifdef CONFIG_WPS_UPNP_AP
	u8 friendly_name[MAX_DEVICE_NAME_LEN + 1];
	u8 manufacturer_url[MAX_MANUFACTURER_URL_LEN + 1];
	u8 model_description[MAX_MODEL_DESC_LEN + 1];
	u8 model_url[MAX_MODEL_URL_LEN + 1];
	u8 upc[MAX_PRODUCT_CODE_LEN + 1];
	struct wps_upnp_info upnp_info;
#endif
#ifdef CONFIG_WPS_UPNP_ER
	struct wps_er *wps_er;
#endif
};

struct rtnl_handle {
	int fd;
	struct sockaddr_nl local;
	struct sockaddr_nl peer;
	u32 seq;
	u32 dump;
};

/** runtime parameters and stats for an STA */
struct STA_INFO {
	/** for events from wext */
	struct rtnl_handle rth;
	/** private state tracking variable */
	int sta_state;
	unsigned char ap_address[ETH_ALEN];
};

/** station stats */
typedef struct _sta_stats {
	unsigned long long last_rx_in_msec;
} sta_stats;

/** associated station info */
struct AP_CLIENT_INFO {
	/** STA MAC address */
	unsigned char mac_address[ETH_ALEN];
	/** Power mfg status */
	unsigned char power_mfg_status;
	/** RSSI */
	char rssi;
	/*band mode*/
	unsigned char bandmode;
	/* station stats*/
	sta_stats stats;
};

/** sta_list structure */
struct AP_STA_LIST {
	/** station count */
	unsigned short sta_count;
	/** station list */
	struct AP_CLIENT_INFO client_info[MAX_AP_CLIENTS];
};

/** runtime parameters and stats for an AP */
struct AP_INFO {
	int basic_rate;
	int ap_state;
	struct AP_STA_LIST sta_list;
};

struct WPA_INFO {
	u8 wpa_role;
	u8 wps_reset_oob;
	/** Default OOB SSID */
	u8 oob_ssid[MAX_SSID_LEN];
	/** OOB SSID length */
	u8 oob_ssid_len;
	u8 oob_psk[MWPAMOD_MAX_KEY];
	char bridge_iface[IFNAMSIZ + 1];
	struct STA_INFO sta_info;
	struct AP_INFO ap_info;
	/* current net_info for module instance.*/
	struct mwpamod_net_info net_info;
};

struct MLOCATION_INFO {
	u8 internal_scan;
	u8 anqp_in_progress;
	mlocation_anqp_cfg anqp_cfg;
};

struct published_service {
	int publish_id;
	unsigned char service_hash[6];
	char service_name[255];
	int tx_filter_len;
	char matching_filter_tx[256];
	int rx_filter_len;
	char matching_filter_rx[256];
	char service_info[255];
	int publish_type;
	int discovery_range;
	int solicited_tx_type;
	int announcement_period;
	int time_to_live;
	int event_cond;
	int matching_filter_flag;
	int bloom_filter_index;
	char bloom_filter_tx[256];
};

struct subscribed_service {
	int subscribe_id;
	unsigned char service_hash[6];
	char service_name[255];
	int tx_filter_len;
	char matching_filter_rx[256];
	int rx_filter_len;
	char matching_filter_tx[256];
	char service_info[255];
	int subscribe_type;
	int discovery_range;
	int query_period;
	int time_to_live;
	int matching_filter_flag;
};

struct saved_nan_srf {
	u8 srf_ctrl;
	u8 num_mac;
	u8 mac_list[10][6];
};

#if 0
struct nan_params_p2p_fa {
    u8 dev_role;
    u8 mac[6];
    u8 map_id;
    u8 ctrl;
    u8 op_class;
    u8 op_chan;
    u32 availability_map;
};

struct fa_map_attribute {
    u8 id;
    u16 len;
    u8 map_id;
    struct available

}
#endif
/* struct neighbour_list: a list of nan_neighbour
 *
 * This SLIST_HEAD macro defines the head of a linked list of nan_neighbour
 * structs.  The struct neighbour_list can be be manipulated using the SLIST_*
 * macros defined in queue.h.  Note that the "list_item" member is the
 * SLIST_ENTRY that is required to navigate the peer list.
 * */
SLIST_HEAD(neighbour_list, nan_neighbour);
struct NAN_INFO {
	/** Current state in wifidir state machine */
	unsigned short cur_state;
	unsigned short cur_ndp_state;
	/** Peer which we are currently working with */
	struct nan_neighbour *cur_peer;
	struct published_service p_service;
	struct subscribed_service s_service;
	int last_publish_id;
	int last_subscribe_id;
	struct saved_nan_srf saved_srf;
	int include_srf;
	int instance_id;
	//    struct fa_map_attribute fa_map;
	int include_fa_attr;
	int process_fa_attr;

	struct nan_params_p2p_fa fa_p2p_attr;
	/* We maintain a list of neighbours that we know about.  For this we use
	 * a BSD SLIST called peers.
	 */
	struct neighbour_list neighbours;
	struct nan_schedule ndl_sched;
	struct nan_schedule counter_proposal;
	int awake_dw_interval;
	int data_path_type;
	int ranging_required;
	int nan_other_if_active;
	int ndp_id;
	u32 immutable_bitmap;
	char peer_mac[ETH_ALEN];
	struct nan_generic_buf *rx_ndp_req; /*Points to the copy of Rx NDP req*/
	struct nan_generic_buf *rx_ndp_resp; /*Points to the copy of Rx NDP
						resp*/
	struct nan_generic_buf *rx_ndp_conf; /*Points to the copy of Rx NDP
						conf*/
	peer_availability_info peer_avail_info; /*Peers Availability parsed from
						   NAF (NDP frames/ schedule
						   update etc)*/
	peer_availability_info peer_avail_info_published; /*Peer's Availability
							     parsed from
							     SDF's(Publish)*/
	peer_availability_info self_avail_info;

	struct nan_user_attr avail_attr;
	struct nan_user_attr ndc_attr;
	struct nan_user_attr ndl_attr;
	NDC_INFO ndc_info[MAX_SUPPORTED_NDC];

	/*Add all the bitfields here*/
	int data_path_needed : 1;
	int counter_proposal_needed : 1;
	int schedule_update_needed : 1;
	int confirm_required : 1;
	int ndc_proposed : 1;
	int a_band : 1;
	int immutable_sched_required : 1;
	int dialog_token;

	int security_required : 1;
	int pmf_required : 1;
	int dual_map : 1;
	int qos_enabled : 1;
	int invalid_sched_required : 1;
	int set_apple_dut_test : 1;
	int is_nan_ranging_responder : 1;
	int ndpe_attr_supported;
	int ndpe_not_present;
	int ndpe_attr_protocol;
	int ndpe_attr_iface_identifier;
	int ndpe_attr_trans_port;
	int ndpe_attr_negative;
	int ndp_attr_present;
	u8 peer_ndp_attr[50];
	nan_security_info nan_security;
	NAN_FTM_PARAMS *nan_ftm_params;
};

/** Interface structure */
struct mwu_iface_info {
	char ifname[IFNAMSIZ + 1]; /** interface name */
	int ioctl_sock; /** ioctl socket */
	short ie_idx[IE_CFG_MAX]; /** IE index array */
	unsigned char device_mac_addr[ETH_ALEN]; /** Device MAC address */
	int role; /** Wifidirect, Enrollee, Registrar */
	int bss_role; /** BSS Role */
	int we_version_compiled; /** Wireless Extension version info */
	int reference_count; /** Reference count for the interface instance */
	int module_id; /** Bitmap to track modules those initialize
			   the structure */
	int discovery_role; /** Discovery Role Enrollee or Registrar */
	struct WIFIDIRECT_INFO *pwifidirect_info; /** wifidirect information */
	struct WPS_INFO *pwps_info; /** WPS information */
	struct WPA_INFO *pwpa_info; /** WPA information */
	struct NAN_INFO *pnan_info; /** NAN information */
	struct MLOCATION_INFO *pmlocation_info; /** MLOCATION information */
	SLIST_ENTRY(mwu_iface_info) list_item;
};

/**
 *  Initialize interface and add it into list
 *
 *  params
 *      ifname       A pointer to interface name
 *      module_id    Module ID to be registered
 *
 *  return
 *      Interface structure. In case of error, return NULL.
 *
 */
struct mwu_iface_info *mwu_iface_init(char *ifname, int module_id);

/**
 *  De-initialize interface and remove it from list
 *
 *  params
 *      ifname       A pointer to interface name
 *      module_id    Module Identifier
 *
 *  return
 *      MWU_ERR_SUCCESS -- success
 *      MWU_ERR_INVAL   -- parameter is invalid or interface is not in list
 */
int mwu_iface_deinit(char *ifname, int module_id);

/**
 *  De-initialize all interfaces and clear list
 *
 *  params
 *      None
 *
 *  return
 *      MWU_ERR_SUCCESS -- success
 *      Otherwise       -- fail
 */
int mwu_iface_deinit_all(void);

/**
 *  Get interface structure
 *
 *  params
 *      ifname       A pointer to interface name
 *
 *  return
 *      Interface structure. If the interface is not in list, return NULL.
 */
struct mwu_iface_info *mwu_get_interface(const char *ifname, int module_id);

/**
 *  Initialize wifidirect_info
 *
 *  params
 *      cur_if       Current Interface
 *
 *  return
 *      MWU_ERR_SUCCESS -- success
 *      MWU_ERR_INVAL   -- parameter is invalid
 */
int mwu_wifidirect_info_init(struct mwu_iface_info *cur_if);

/**
 *  Initialize wps_info
 *
 *  params
 *      cur_if          Current Interface
 *      init_cfg_file   Configuration file name
 *
 *  return
 *      MWU_ERR_SUCCESS -- success
 *      MWU_ERR_INVAL   -- parameter is invalid
 */
int mwu_wps_info_init(struct mwu_iface_info *cur_if, char *init_cfg_file);

/**
 *  Initialize nan_info
 *
 *  params
 *      cur_if          Current Interface
 *
 *  return
 *      MWU_ERR_SUCCESS -- success
 *      MWU_ERR_INVAL   -- parameter is invalid
 */
int mwu_nan_info_init(struct mwu_iface_info *cur_if);

/**
 *  Convert IE index type to string
 *
 *  params
 *      ie_type         Type of IE index
 *
 *  return
 *      The string of corresponding IE index type. "UNKNOWN" if ie_type not
 * valid.
 */
const char *mwu_ie_type_to_str(int ie_type);

#endif
