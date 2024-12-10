/*
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

#include <sys/socket.h>
#include <unistd.h>

#include "mwu_if_manager.h"
#include "mwu.h"
#ifdef UTIL_LOG_TAG
#undef UTIL_LOG_TAG
#endif
#define UTIL_LOG_TAG "MWU IF MANAGER"
#include "mwu_log.h"
#include "mwu_defs.h"
#include "wps_def.h"
#include "wps_msg.h"
#include "wps_wlan.h"
#include "mwu_ioctl.h"
#include "wps_os.h"
#include "util.h"

// #include "mwpamod.h"
// #include "mwpsmod.h"
// #include "wifidir.h"

/** Interface list structure */
SLIST_HEAD(mwu_iface_list, mwu_iface_info);

/** We maintain a list of mwu_iface_info items that are currently in use */
static struct mwu_iface_list mwu_ifaces;
static int iface_num = 0;

/************************************
 *        static functions          *
 ************************************
 */

const char *module_name(int module_id)
{
	ERR("Module id: %d", module_id);
	switch (module_id) {
	case MWPSMOD_ID:
		return "WPS Module";

	case MWPAMOD_ID:
		return "WPA Module";

	case WIFIDIR_ID:
		return "Wifidir Module";

	case NAN_ID:
		return "NAN Module";

	case MLOCATION_ID:
		return "MLOCATION Module";

	default:
		return "Unknown Module";
	}
}

/************************************
 *               APIs               *
 ************************************
 */

struct mwu_iface_info *mwu_iface_init(char *ifname, int module_id)
{
	struct mwu_iface_info *new_if = NULL;
	struct mwu_iface_info *cur = NULL;
	int i = 0;
	int update_if_list = 0;

	ENTER();

	if (!ifname) {
		LEAVE();
		return NULL;
	}

	INFO("Initialize interface instance for %s interface and %s", ifname,
	     module_name(module_id));

	/** Check whether the interface is already in the list */
	SLIST_FOREACH(cur, &mwu_ifaces, list_item)
	{
		if (!strncmp(cur->ifname, ifname, IFNAMSIZ)) {
			new_if = cur;
			break;
		}
	}

	if (new_if) {
		INFO("Found interface instance for %s", ifname);

		if (new_if->module_id & module_id) {
			/* Interface instance is present */
			INFO("Interface instance for %s is already initialized",
			     module_name(module_id));
			/* Interface instance is already in place for this
			 *module No need to increment reference count
			 **/
			LEAVE();
			return new_if;
		}
	} else {
		/* Interface instance is not present
		 * Create a new interface instance
		 **/
		new_if = (struct mwu_iface_info *)malloc(
			sizeof(struct mwu_iface_info));
		if (!new_if) {
			ERR("Failed to allocate memory for new interface.");
			LEAVE();
			return NULL;
		}
		memset(new_if, 0, sizeof(struct mwu_iface_info));
		update_if_list = 1;

		/** ie index array */
		for (i = 0; i < IE_CFG_MAX; ++i)
			new_if->ie_idx[i] = -1;

		/** Initialize each member */
		/** name */
		strncpy(new_if->ifname, ifname, IFNAMSIZ);
		/** ioctl socket */
		if ((new_if->ioctl_sock = socket(AF_INET, SOCK_STREAM, 0)) <
		    0) {
			ERR("Cannot open socket.");
			goto fail;
		}
	}

	/* Increment reference count and register the module */
	new_if->reference_count++;
	new_if->module_id |= module_id;

	INFO("Initializing interface instance for %s", module_name(module_id));

	if (module_id & WIFIDIR_ID) {
		INFO("Allocated wifidirect info structure");
		new_if->pwifidirect_info = (struct WIFIDIRECT_INFO *)malloc(
			sizeof(struct WIFIDIRECT_INFO));
		if (!new_if->pwifidirect_info) {
			ERR("Failed to allocate memory for WIFIDIRECT_INFO.");
			goto fail;
		}
		/** WIFIDIRECT info structure */
		memset(new_if->pwifidirect_info, 0,
		       sizeof(struct WIFIDIRECT_INFO));
	} else if (module_id & MWPSMOD_ID) {
		INFO("Allocated wps info structure");
		new_if->pwps_info =
			(struct WPS_INFO *)malloc(sizeof(struct WPS_INFO));
		if (!new_if->pwps_info) {
			ERR("Failed to allocate memory for WPS_INFO.");
			goto fail;
		}
		/** WPS info structure */
		memset(new_if->pwps_info, 0, sizeof(struct WPS_INFO));
	} else if (module_id & MWPAMOD_ID) {
		INFO("Allocated wpa info structure");
		new_if->pwpa_info =
			(struct WPA_INFO *)malloc(sizeof(struct WPA_INFO));
		if (!new_if->pwpa_info) {
			ERR("Failed to allocate memory for WPA_INFO.");
			goto fail;
		}
		/** WPA info structure */
		memset(new_if->pwpa_info, 0, sizeof(struct WPA_INFO));
	} else if (module_id & NAN_ID) {
		INFO("Allocated NAN info structure");
		new_if->pnan_info =
			(struct NAN_INFO *)malloc(sizeof(struct NAN_INFO));
		if (!new_if->pnan_info) {
			ERR("Failed to allocate memory for NAN_INFO.");
			goto fail;
		}
		/** WPA info structure */
		memset(new_if->pnan_info, 0, sizeof(struct NAN_INFO));
		INFO("cur_if is %p", new_if);
		INFO("pnan_info is %p", new_if->pnan_info);
	} else if (module_id & MLOCATION_ID) {
		INFO("Allocated mlocation info structure");
		new_if->pmlocation_info = (struct MLOCATION_INFO *)malloc(
			sizeof(struct MLOCATION_INFO));
		if (!new_if->pmlocation_info) {
			ERR("Failed to allocate memory for MLOCATION_INFO.");
			goto fail;
		}
		/** MLOCATION info structure */
		memset(new_if->pmlocation_info, 0,
		       sizeof(struct MLOCATION_INFO));
	}

	/* Insert new interface instance in the list */
	if (update_if_list) {
		/** Check whether the list is empty or full */
		if (iface_num == 0) {
			SLIST_INIT(&mwu_ifaces);
		} else if (iface_num == MAX_IFACE) {
			/** If reach MAX_IFACE, remove one item before
			 * insertion. */
			struct mwu_iface_info *temp = NULL;

			temp = mwu_ifaces.slh_first;
			SLIST_REMOVE_HEAD(&mwu_ifaces, list_item);

			close(temp->ioctl_sock);
			FREE(temp->pwifidirect_info);
			FREE(temp->pwps_info->wps_data.scan_results);
			FREE(temp->pwps_info);
			close(temp->pwpa_info->sta_info.rth.fd);
			FREE(temp->pwpa_info);
			FREE(temp->pnan_info);
			FREE(temp->pmlocation_info);
			FREE(temp);

			--iface_num;
		}

		/** Add new interface into list */
		SLIST_INSERT_HEAD(&mwu_ifaces, new_if, list_item);
		++iface_num;
	}

	LEAVE();
	return new_if;

fail:
	FREE(new_if->pwifidirect_info);
	FREE(new_if->pwps_info->wps_data.scan_results);
	FREE(new_if->pwps_info);
	FREE(new_if->pwpa_info);
	FREE(new_if->pnan_info);
	FREE(new_if->pmlocation_info);
	FREE(new_if);
	LEAVE();
	return NULL;
}

int mwu_iface_deinit(char *ifname, int module_id)
{
	struct mwu_iface_info *cur = NULL, *found = NULL;
	// int i = 0;

	ENTER();

	if (!ifname) {
		LEAVE();
		return MWU_ERR_INVAL;
	}

	SLIST_FOREACH(cur, &mwu_ifaces, list_item)
	{
		if (!strncmp(cur->ifname, ifname, IFNAMSIZ)) {
			found = cur;
			break;
		}
	}

	if (found == NULL) {
		ERR("Interface %s is not in the list.", ifname);
		LEAVE();
		return MWU_ERR_INVAL;
	}

	if (!(found->module_id & module_id)) {
		ERR("Module %s already de-initialized", module_name(module_id));
		LEAVE();
		return MWU_ERR_INVAL;
	}

	switch (module_id) {
#if 0
        case MWPSMOD_ID:
            FREE(found->pwps_info->wps_data.scan_results);
            FREE(found->pwps_info);
            break;

        case MWPAMOD_ID:
            if (found->pwpa_info->sta_info.rth.fd)
                close(found->pwpa_info->sta_info.rth.fd);
            FREE(found->pwpa_info);
            break;

        case WIFIDIR_ID:
            FREE(found->pwifidirect_info);
            break;
#endif
	case NAN_ID:
		FREE(found->pnan_info);
		break;

	case MLOCATION_ID:
		FREE(found->pmlocation_info);
		break;

	default:
		ERR("Invalid module ID: %d", module_id);
		LEAVE();
		return MWU_ERR_INVAL;
	}

	ERR("MODULE ID before deinit: %d", found->module_id);
	found->module_id &= ~module_id;
	ERR("MODULE ID after deinit: %d", found->module_id);

	if (found->module_id == 0) {
#if 0 // ToDO : check if this is needed
          /** Clear all IEs */
        for (i = 0; i < IE_CFG_MAX; ++i)
            mwu_clear_ie(ifname, i);
#endif
		/* Remove the interface info for the found interface */
		SLIST_REMOVE(&mwu_ifaces, found, mwu_iface_info, list_item);
		--iface_num;

		if (found->ioctl_sock)
			close(found->ioctl_sock);

		FREE(found);
	}
	LEAVE();
	return MWU_ERR_SUCCESS;
}

int mwu_iface_deinit_all(void)
{
	struct mwu_iface_info *cur = NULL;
#if 0
    struct module mwpamod_mod, mwpsmod_mod, wifidir_mod;
#endif
	struct module nan_mod;
	char *iface;

	ENTER();

	SLIST_FOREACH(cur, &mwu_ifaces, list_item)
	{
		iface = cur->ifname;
#if 0
        if (cur->module_id & MWPAMOD_ID) {
            INFO("Found initialized WPA module, de-init it!");
            strncpy(mwpamod_mod.iface, iface, IFNAMSIZ);
            mwpamod_mod.cb = NULL;
            mwpamod_mod.cbpriv = NULL;
            mwpamod_sta_deinit(&mwpamod_mod);
            mwpamod_ap_deinit(&mwpamod_mod);
        }
        if (cur->module_id & MWPSMOD_ID) {
            INFO("Found initialized WPS module, de-init it!");
            strncpy(mwpsmod_mod.iface, iface, IFNAMSIZ);
            mwpsmod_mod.cb = NULL;
            mwpsmod_mod.cbpriv = NULL;
            mwpsmod_registrar_deinit(&mwpsmod_mod);
            mwpsmod_enrollee_deinit(&mwpsmod_mod);
        }
        if (cur->module_id & WIFIDIR_ID) {
            INFO("Found initialized WIFIDIR module, de-init it!");
            strncpy(wifidir_mod.iface, iface, IFNAMSIZ);
            wifidir_mod.cb = NULL;
            wifidir_mod.cbpriv = NULL;
            wifidir_deinit(&wifidir_mod);
        }
#endif
		if (cur->module_id & NAN_ID) {
			INFO("Found initialized NAN module, de-init it!");
			strncpy(nan_mod.iface, iface, IFNAMSIZ);
			nan_mod.cb = NULL;
			nan_mod.cbpriv = NULL;
			nan_iface_close();
			nan_deinit(&nan_mod);
		}
	}
	LEAVE();
	return MWU_ERR_SUCCESS;
}

struct mwu_iface_info *mwu_get_interface(const char *ifname, int module_id)
{
	struct mwu_iface_info *cur = NULL;

	if (!ifname)
		return NULL;

	SLIST_FOREACH(cur, &mwu_ifaces, list_item)
	{
		if (!strncmp(cur->ifname, ifname, IFNAMSIZ)) {
			/* Return interface instance if it exists
			 * Skip the module id check
			 *
			 * Caution:
			 * pwps_info, pwpa_info and pwifidir_info could be NULL
			 */
			if (module_id == GENERIC_MODULE_ID)
				return cur;

			/* Return interface instance for specific module */
			if (cur->module_id & module_id)
				return cur;
			else
				return NULL;
		}
	}

	return NULL;
}

int mwu_nan_info_init(struct mwu_iface_info *cur_if)
{
	ENTER();

	if (!cur_if) {
		LEAVE();
		return MWU_ERR_INVAL;
	}

	cur_if->pnan_info->cur_state = 0;
	cur_if->pnan_info->cur_peer = NULL;
	/*Set default availability*/
	cur_if->pnan_info->self_avail_info.entry_potential[0].time_bitmap[0] =
		NAN_POTENTIAL_BITMAP;
	cur_if->pnan_info->self_avail_info.band_entry_potential = TRUE;
	cur_if->pnan_info->self_avail_info.potential_valid = (1 << 0);

	cur_if->pnan_info->self_avail_info.entry_conditional[0].time_bitmap[0] =
		NAN_DEFAULT_BITMAP;
	cur_if->pnan_info->self_avail_info.entry_conditional[0].op_class = 0;
	cur_if->pnan_info->self_avail_info.entry_conditional[0].channels[0] = 0;
	cur_if->pnan_info->self_avail_info.conditional_valid = (1 << 0);

	/*Set Default Ndl QoS*/
	cur_if->pnan_info->self_avail_info.qos.min_slots = NAN_QOS_MIN_SLOTS;
	cur_if->pnan_info->self_avail_info.qos.max_latency =
		NAN_QOS_MAX_LATENCY;

	LEAVE();
	return MWU_ERR_SUCCESS;
}

#if 0 // No WiFi Direct /WPS
int mwu_wifidirect_info_init(struct mwu_iface_info *cur_if)
{
    ENTER();

    if (!cur_if) {
        LEAVE();
        return MWU_ERR_INVAL;
    }

    cur_if->pwifidirect_info->cur_state = 0;
    cur_if->pwifidirect_info->cur_peer = NULL;
    cur_if->pwifidirect_info->dev_index = -1;
    cur_if->pwifidirect_info->invite_state = WIFIDIR_INVITE_STATE_IDLE;

    LEAVE();
    return MWU_ERR_SUCCESS;
}


int mwu_wps_info_init(struct mwu_iface_info *cur_if, char *init_cfg_file)
{
    ENTER();

    if (!cur_if) {
        LEAVE();
        return MWU_ERR_INVAL;
    }

    cur_if->pwps_info->wps_data.userAbort = WPS_CANCEL;

    /** Store file name into pwps_info */
    memset(cur_if->pwps_info->init_cfg_file, 0,
            sizeof(cur_if->pwps_info->init_cfg_file));
    strncpy((char *)cur_if->pwps_info->init_cfg_file, init_cfg_file,
            sizeof(cur_if->pwps_info->init_cfg_file));

    /* Initial variable used in state machine */
    cur_if->pwps_info->register_completed = WPS_CANCEL;
    cur_if->pwps_info->registration_fail = WPS_CANCEL;
    cur_if->pwps_info->registration_in_progress = WPS_CANCEL;
    cur_if->pwps_info->auto_enrollee_in_progress = WPS_CANCEL;
    cur_if->pwps_info->id = 1;
    cur_if->pwps_info->eap_msg_sent = 0;
    cur_if->pwps_info->last_recv_wps_msg = -1;
    cur_if->pwps_info->wps_msg_max_retry = WPS_MSG_RETRY_LIMIT;
    cur_if->pwps_info->restart_link_lost = WPS_CANCEL;
    cur_if->pwps_info->read_ap_config_only = WPS_CANCEL;
    cur_if->pwps_info->pin_pbc_set = WPS_CANCEL;
    cur_if->pwps_info->input_state = WPS_INPUT_STATE_METHOD;
    cur_if->pwps_info->wps_device_state = SC_NOT_CONFIGURED_STATE;
    cur_if->pwps_info->config_load_by_oob = WPS_CANCEL;
    cur_if->pwps_info->wps_ap_setup_locked = WPS_CANCEL;
    cur_if->pwps_info->seed_er_attacks = 1;
    cur_if->pwps_info->enrollee_in_authorized_mac = WPS_SET;
    cur_if->pwps_info->handle_scan_done_event = WPS_CANCEL;
    cur_if->pwps_info->scan_5G_band = WPS_CANCEL;
    cur_if->pwps_info->scan_result_ready = WPS_CANCEL;
    cur_if->pwps_info->dump_peers_request = WPS_CANCEL;
    cur_if->pwps_info->use_psk = WPS_CANCEL;
    cur_if->pwps_info->dev_found = -1;

    if (cur_if->bss_role == BSS_TYPE_STA)
        cur_if->pwps_info->frag_thres = EAP_FRAG_THRESHOLD_DEF;
    else
        cur_if->pwps_info->frag_thres = EAP_FRAG_THRESHOLD_MAX;

    cur_if->pwps_info->proto_ext_test = 0;
    cur_if->pwps_info->tx_frag_test = 0;

    if (cur_if->pwps_info->tx_frag_test) {
        /* TX Frag Test - override Default value */
        cur_if->pwps_info->frag_thres = EAP_FRAG_THRESHOLD_PF;
    }

    /* Reset ssid here, to read from file or driver */
    memset(&cur_if->pwps_info->wps_data.current_ssid, 0,
            sizeof(cur_if->pwps_info->wps_data.current_ssid));

    memset(cur_if->pwps_info->enrollee.wps_uuid, 0, UUID_MAX_LEN);
    memset(cur_if->pwps_info->registrar.wps_uuid, 0, UUID_MAX_LEN);
    cur_if->pwps_info->registrar.wps_uuid_length = UUID_MAX_LEN;
    cur_if->pwps_info->enrollee.wps_uuid_length = UUID_MAX_LEN;

    cur_if->pwps_info->registrar.auth_type_flag = AUTHENTICATION_TYPE_ALL;
    cur_if->pwps_info->registrar.encry_type_flag = ENCRYPTION_TYPE_ALL;
    cur_if->pwps_info->enrollee.auth_type_flag = AUTHENTICATION_TYPE_ALL;
    cur_if->pwps_info->enrollee.encry_type_flag = ENCRYPTION_TYPE_ALL;

    /* Read Device info from config file & generate UUID */
    if (wps_update_device_info(cur_if) != WPS_STATUS_SUCCESS) {
        LEAVE();
        return MWU_ERR_INVAL;
    }

    mwu_printf(DEBUG_INIT, "Device state SC_NOT_CONFIGURED_STATE !\n");

    if (cur_if->pwps_info->registrar.config_methods & CONFIG_METHOD_KEYPAD) {
        mwu_printf(DEBUG_INIT, "Device is Rich UI device.");
        cur_if->pwps_info->is_low_ui_device = WPS_CANCEL;
    } else {
        mwu_printf(DEBUG_INIT, "Device is Low UI device.");
        cur_if->pwps_info->is_low_ui_device = WPS_SET;
    }

    if (cur_if->bss_role == BSS_TYPE_UAP) {
        /* Get current uAP BSS configuration - for both Enrollee/Registrar
           modes */
        if (!cur_if->pwps_info->wps_data.current_ssid.ssid_len) {
            if (wps_load_uap_cred(cur_if) != WPS_STATUS_SUCCESS) {
                printf("Incorrect Network configuration. Quitting!\n");
                LEAVE();
                return MWU_ERR_INVAL;
            }
        }
    } else if (cur_if->role == WPS_REGISTRAR) {
        /* Wireless STA Registrar */
        if (wps_load_wsta_registrar_cred(cur_if) != WPS_STATUS_SUCCESS) {
            mwu_printf(DEBUG_INIT, "Incorrect Network configuration. Quitting!\n");
            LEAVE();
            return MWU_ERR_INVAL;
        }
    }

    if (cur_if->role == WPS_ENROLLEE) {
        mwu_printf(MSG_INFO, "Role : WPS_ENROLLEE\n");
        cur_if->pwps_info->state = WPS_STATE_A;

        /* Enrollee MAC Address */
        memcpy(cur_if->pwps_info->enrollee.mac_address,
               cur_if->device_mac_addr, ETH_ALEN);

        /* Association State */
        cur_if->pwps_info->enrollee.association_state = 0x01;

        /* Random Number */
        wps_generate_nonce_16B(cur_if->pwps_info->enrollee.nonce);      /* Nonce */
        wps_generate_nonce_16B(cur_if->pwps_info->enrollee.e_s1);       /* E-S1 */
        wps_generate_nonce_16B(cur_if->pwps_info->enrollee.e_s2);       /* E-S1 */
        wps_generate_nonce_16B(cur_if->pwps_info->enrollee.IV); /* IV */
    } else if (cur_if->role == WPS_REGISTRAR) {
        mwu_printf(MSG_INFO, "Role : WPS_REGISTRAR\n");
        /* Registrar MAC Address */
        memcpy(cur_if->pwps_info->registrar.mac_address,
               cur_if->device_mac_addr, ETH_ALEN);

        /* Association State */
        cur_if->pwps_info->registrar.association_state = 0x01;

        /* Random Number */
        wps_generate_nonce_16B(cur_if->pwps_info->registrar.nonce);     /* Nonce */
        wps_generate_nonce_16B(cur_if->pwps_info->registrar.r_s1);      /* R-S1 */
        wps_generate_nonce_16B(cur_if->pwps_info->registrar.r_s2);      /* R-S2 */
        wps_generate_nonce_16B(cur_if->pwps_info->registrar.IV);        /* IV */
    } else if (cur_if->role == WIFIDIR_ROLE) {
        mwu_printf(MSG_INFO, "Role : WIFIDIRECT\n");

        memcpy(cur_if->pwps_info->enrollee.mac_address,
               cur_if->pwifidirect_info->intended_mac_addr, ETH_ALEN);
        memcpy(cur_if->pwps_info->registrar.mac_address,
               cur_if->pwifidirect_info->intended_mac_addr, ETH_ALEN);

        /* Association State */
        cur_if->pwps_info->enrollee.association_state = 0x01;
        cur_if->pwps_info->registrar.association_state = 0x01;

        /* Random Number */
        wps_generate_nonce_16B(cur_if->pwps_info->enrollee.nonce);      /* Nonce */
        wps_generate_nonce_16B(cur_if->pwps_info->enrollee.e_s1);       /* E-S1 */
        wps_generate_nonce_16B(cur_if->pwps_info->enrollee.e_s2);       /* E-S1 */
        wps_generate_nonce_16B(cur_if->pwps_info->enrollee.IV); /* IV */
        wps_generate_nonce_16B(cur_if->pwps_info->registrar.nonce);     /* Nonce */
        wps_generate_nonce_16B(cur_if->pwps_info->registrar.r_s1);      /* R-S1 */
        wps_generate_nonce_16B(cur_if->pwps_info->registrar.r_s2);      /* R-S2 */
        wps_generate_nonce_16B(cur_if->pwps_info->registrar.IV);        /* IV */
    }

#ifdef CONFIG_WPS_UPNP
    cur_if->pwps_info->enable_upnp = FALSE;
#endif

    LEAVE();
    return MWU_ERR_SUCCESS;
}
#endif
const char *mwu_ie_type_to_str(int ie_type)
{
	switch (ie_type) {
	case IE_CFG_WIFIDIR:
		return "Wi-Fi Direct";
	case IE_CFG_PROBE_RESP:
		return "Probe resp";
	case IE_CFG_WIFIDIR_WPS:
		return "Wifidir WPS";
	case IE_CFG_AP_BCN:
		return "Beacon";
	case IE_CFG_AP_ASSOCRESP:
		return "Associate Response";
	case IE_CFG_PROBE:
		return "Probe Request";
	case IE_CFG_ASSOCREQ:
		return "Associate Request";
	case IE_CFG_WIFIDISPLAY:
		return "Wi-Fi Display";
	default:
		return "UNKNOWN";
	}
}
