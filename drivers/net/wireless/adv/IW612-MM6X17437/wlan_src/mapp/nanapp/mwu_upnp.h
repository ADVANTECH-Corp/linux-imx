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

#ifndef __MWU_UPNP_H__
#define __MWU_UPNP_H__
#include "mwpsmod/upnp/wps/wps_upnp.h"
#include "mwpsmod/mwpsmod.h"

#ifndef WPS_SAVE_PKT
#define WPS_SAVE_PKT(i, f, l)                                                  \
	do {                                                                   \
		memcpy(i->pwps_info->last_message.message, f, l);              \
		i->pwps_info->last_message.length = l;                         \
	} while (0)
#endif

#ifndef WPS_EAPOL_TX_AND_SAVE
#define WPS_EAPOL_TX_AND_SAVE(r, i, b, s)                                      \
	do {                                                                   \
		if (i->pwps_info->frag_thres < s) {                            \
			r = wps_setup_tx_frag(i, b, s);                        \
		} else {                                                       \
			r = wps_eapol_txPacket(i, (u8 *)b, s);                 \
			if (r >= 0) {                                          \
				WPS_SAVE_PKT(                                  \
					i,                                     \
					(u8 *)(((PEAP_FRAME_HEADER)(b)) + 1),  \
					s - SZ_EAP_WPS_FRAME_HEADER);          \
			}                                                      \
		}                                                              \
	} while (0)
#endif

#define WPS_UPNP_SET_MODE(cur_if, mode)                                        \
	do {                                                                   \
		(cur_if)->pwps_info->upnp_info.g_wps_upnp_mode.wps_upnp_mode = \
			(mode);                                                \
	} while (0)

#define WPS_UPNP_CHECK_MODE(cur_if, mode)                                      \
	((cur_if)->pwps_info->upnp_info.g_wps_upnp_mode.wps_upnp_mode ==       \
			 (mode) ?                                              \
		 TRUE :                                                        \
		 FALSE)

/********************************************************
	WPS UPNP Variables and Functions
********************************************************/
extern struct WPS_INFO *gpwps_info;

extern struct wpabuf_global global_msg;

#define WPS_DEV_TYPE_LEN 8
#define MAX_WPS_VENDOR_EXTENSIONS 10
struct wps_device_data {
	u8 mac_addr[ETH_ALEN];
	char *device_name;
	char *manufacturer;
	char *model_name;
	char *model_number;
	char *serial_number;
	u8 pri_dev_type[WPS_DEV_TYPE_LEN];
#define WPS_SEC_DEVICE_TYPES 5
	u8 sec_dev_type[WPS_SEC_DEVICE_TYPES][WPS_DEV_TYPE_LEN];
	u8 num_sec_dev_types;
	u32 os_version;
	u8 rf_bands;
	u16 config_methods;
	struct wpabuf *vendor_ext[MAX_WPS_VENDOR_EXTENSIONS];

	int p2p;
};

/* Wi-Fi Protected Setup State */
enum wps_state { WPS_STATE_NOT_CONFIGURED = 1, WPS_STATE_CONFIGURED = 2 };

struct oob_conf_data {
	enum {
		OOB_METHOD_UNKNOWN = 0,
		OOB_METHOD_DEV_PWD_E,
		OOB_METHOD_DEV_PWD_R,
		OOB_METHOD_CRED,
	} oob_method;
	struct wpabuf *dev_password;
	struct wpabuf *pubkey_hash;
};

/**
 * struct wps_context - Long term WPS context data
 *
 * This data is stored at the higher layer Authenticator or Supplicant data
 * structures and it is maintained over multiple registration protocol runs.
 */
struct wps_context {
	/**
	 * ap - Whether the local end is an access point
	 */
	int ap;

	/**
	 * registrar - Pointer to WPS registrar data from wps_registrar_init()
	 */
	struct wps_registrar *registrar;

	/**
	 * wps_state - Current WPS state
	 */
	enum wps_state wps_state;

	/**
	 * ap_setup_locked - Whether AP setup is locked (only used at AP)
	 */
	int ap_setup_locked;

	/**
	 * uuid - Own UUID
	 */
	u8 uuid[16];

	/**
	 * ssid - SSID
	 *
	 * This SSID is used by the Registrar to fill in information for
	 * Credentials. In addition, AP uses it when acting as an Enrollee to
	 * notify Registrar of the current configuration.
	 */
	u8 ssid[32];

	/**
	 * ssid_len - Length of ssid in octets
	 */
	size_t ssid_len;

	/**
	 * dev - Own WPS device data
	 */
	struct wps_device_data dev;

	/**
	 * dh_ctx - Context data for Diffie-Hellman operation
	 */
	void *dh_ctx;

	/**
	 * dh_privkey - Diffie-Hellman private key
	 */
	struct wpabuf *dh_privkey;

	/**
	 * dh_pubkey_oob - Diffie-Hellman public key
	 */
	struct wpabuf *dh_pubkey;

	/**
	 * config_methods - Enabled configuration methods
	 *
	 * Bit field of WPS_CONFIG_*
	 */
	u16 config_methods;

	/**
	 * encr_types - Enabled encryption types (bit field of WPS_ENCR_*)
	 */
	u16 encr_types;

	/**
	 * auth_types - Authentication types (bit field of WPS_AUTH_*)
	 */
	u16 auth_types;

	/**
	 * network_key - The current Network Key (PSK) or %NULL to generate new
	 *
	 * If %NULL, Registrar will generate per-device PSK. In addition, AP
	 * uses this when acting as an Enrollee to notify Registrar of the
	 * current configuration.
	 *
	 * When using WPA/WPA2-Person, this key can be either the ASCII
	 * passphrase (8..63 characters) or the 32-octet PSK (64 hex
	 * characters). When this is set to the ASCII passphrase, the PSK can
	 * be provided in the psk buffer and used per-Enrollee to control which
	 * key type is included in the Credential (e.g., to reduce calculation
	 * need on low-powered devices by provisioning PSK while still allowing
	 * other devices to get the passphrase).
	 */
	u8 *network_key;

	/**
	 * network_key_len - Length of network_key in octets
	 */
	size_t network_key_len;

	/**
	 * psk - The current network PSK
	 *
	 * This optional value can be used to provide the current PSK if
	 * network_key is set to the ASCII passphrase.
	 */
	u8 psk[32];

	/**
	 * psk_set - Whether psk value is set
	 */
	int psk_set;

	/**
	 * ap_settings - AP Settings override for M7 (only used at AP)
	 *
	 * If %NULL, AP Settings attributes will be generated based on the
	 * current network configuration.
	 */
	u8 *ap_settings;

	/**
	 * ap_settings_len - Length of ap_settings in octets
	 */
	size_t ap_settings_len;

	/**
	 * friendly_name - Friendly Name (required for UPnP)
	 */
	char *friendly_name;

	/**
	 * manufacturer_url - Manufacturer URL (optional for UPnP)
	 */
	char *manufacturer_url;

	/**
	 * model_description - Model Description (recommended for UPnP)
	 */
	char *model_description;

	/**
	 * model_url - Model URL (optional for UPnP)
	 */
	char *model_url;

	/**
	 * upc - Universal Product Code (optional for UPnP)
	 */
	char *upc;

	/**
	 * cred_cb - Callback to notify that new Credentials were received
	 * @ctx: Higher layer context data (cb_ctx)
	 * @cred: The received Credential
	 * Return: 0 on success, -1 on failure
	 */
	int (*cred_cb)(void *ctx, const void *cred);

	/**
	 * event_cb - Event callback (state information about progress)
	 * @ctx: Higher layer context data (cb_ctx)
	 * @event: Event type
	 * @data: Event data
	 */
	void (*event_cb)(void *ctx, int event, void *data);

	/**
	 * cb_ctx: Higher layer context data for callbacks
	 */
	void *cb_ctx;

	struct upnp_wps_device_sm *wps_upnp;

	/* Pending messages from UPnP PutWLANResponse */
	struct upnp_pending_message *upnp_msgs;

	u16 ap_nfc_dev_pw_id;
	struct wpabuf *ap_nfc_dh_pubkey;
	struct wpabuf *ap_nfc_dh_privkey;
	struct wpabuf *ap_nfc_dev_pw;

	u16 er_nfc_dev_pw_id;
	struct wpabuf *er_nfc_dh_pubkey;
	struct wpabuf *er_nfc_dh_privkey;
	struct wpabuf *er_nfc_dev_pw;
	u16 er_nfc_dev_pw_len;
};

extern int wps_upnp_start;

#ifdef CONFIG_WPS_UPNP_AP
int wps_upnp_send_wlan_event(struct mwu_iface_info *);
int wps_upnp_send_probe_request_event(struct mwu_iface_info *pwps_if,
				      const u8 *address);
int wps_eapol_txPacket(struct mwu_iface_info *, u8 *buf, size_t len);
int wps_eap_message_header_prepare(struct mwu_iface_info *wps_info, u8 code,
				   u8 id, u8 wps_msg);
void wps_debug_print_msgtype(const char *str, int msg);
int wps_registrar_state_transition(struct mwu_iface_info *pwps_info,
				   u16 msg_type);
void wps_upnp_destroy(void);

int wps_uap_session_complete_handler(struct mwu_iface_info *pwps_info);

enum mwpsmod_error mwpsmod_upnp_set_ap_pin(const char *iface,
					   const char *ap_pin);

enum mwpsmod_error mwpsmod_upnp_init(struct mwu_iface_info *cur_if);
enum mwpsmod_error mwpsmod_upnp_deinit(struct mwu_iface_info *cur_if);
#endif

#ifdef CONFIG_WPS_UPNP_ER
enum mwpsmod_error mwpsmod_er_init(struct module *mod);
enum mwpsmod_error mwpsmod_er_deinit(struct module *mod);

enum mwpsmod_error mwpsmod_er_start(char *iface, const char *ipFilter);

enum mwpsmod_error mwpsmod_wps_er_stop(char *iface);

enum mwpsmod_error mwpsmod_er_add_pin(char *iface, const u8 *addr,
				      const char *uuid, const char *pin);

enum mwpsmod_error mwpsmod_er_add_pbc(char *iface, const char *uuid);

enum mwpsmod_error mwpsmod_er_learn(char *iface, const char *uuid,
				    const char *pin);

enum mwpsmod_error mwpsmod_er_config(char *iface, char *uuid, char *pin,
				     struct mwpsmod_registrar_config *cfg);

#ifdef CONFIG_NFC

enum mwpsmod_error mwpsmod_er_add_nfc_pwd_token(char *iface, const u8 *addr,
						const char *uuid);

enum mwpsmod_error mwpsmod_er_add_nfc_config_token(char *iface, const u8 *addr,
						   const char *uuid);

enum mwpsmod_error mwpsmod_er_add_nfc_handover(char *iface, const u8 *addr,
					       const char *uuid);

#endif /* CONFIG_NFC */

#endif /* CONFIG_WPS_UPNP_ER */

#endif
