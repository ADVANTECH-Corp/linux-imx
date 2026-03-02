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
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h> /* For close */
#include <net/if_arp.h> /* For ARPHRD_ETHER */
#if defined(ANDROID) && !defined(HAVE_GLIBC)
#include <linux/wireless.h>
#include <linux/if.h>
#else
#include "wireless_copy.h"
#endif
#define UTIL_LOG_TAG "MWU_IOCTL"
#include "util.h"
#include "mwu_log.h"
#include "wps_msg.h"
#include "wlan_hostcmd.h"
#include "wlan_wifidir.h"
//#include "wifidir_lib.h"
#include "mwu_ioctl.h"
#include "wps_def.h"
#include "mwu_internal.h"
#include "mwu_if_manager.h"

#include "nan_lib.h"

#define DEFAULT_SSID "DIRECT-*"

/**
 *  Return ioctl_sock of interface
 */
static int get_ioctl_sock(const char *ifname)
{
	struct mwu_iface_info *found = NULL;

	if (!ifname)
		return -1;

	found = mwu_get_interface(ifname, GENERIC_MODULE_ID);
	if (!found) {
		ERR("Cannot find interface info for %s.", ifname);
		return -1;
	}

	return found->ioctl_sock;
}

struct iw_priv_args *Priv_args = NULL;

static int send_iwreq_ioctl(const char *ifname, struct iwreq *iwr,
			    int ioctl_val)
{
	int ret = MWU_ERR_SUCCESS;
	int ioctl_sock = get_ioctl_sock(ifname);

	if (!ifname || !iwr || ioctl_sock < 0)
		return MWU_ERR_INVAL;

	strncpy(iwr->ifr_name, (char *)ifname, IFNAMSIZ);
	if (ioctl(ioctl_sock, ioctl_val, iwr) < 0)
		ret = MWU_ERR_COM;

	return ret;
}

int send_ifreq_ioctl(const char *ifname, struct ifreq *ifr, int ioctl_val)
{
	int ret = MWU_ERR_SUCCESS;
	int ioctl_sock = get_ioctl_sock(ifname);

	if (!ifname || !ifr || ioctl_sock < 0)
		return MWU_ERR_INVAL;

	strncpy(ifr->ifr_name, (char *)ifname, IFNAMSIZ);
	if (ioctl(ioctl_sock, ioctl_val, ifr) < 0)
		ret = MWU_ERR_COM;

	return ret;
}

int mwu_get_private_info(char *iface)
{
	/* This function sends the SIOCGIWPRIV command which is
	 * handled by the kernel. and gets the total number of
	 * private ioctl's available in the host driver.
	 */
	struct iwreq iwr;
	int ret = MWU_ERR_SUCCESS;
	struct iw_priv_args *pPriv = NULL;
	struct iw_priv_args *newPriv;
	int result = 0;
	size_t size = IW_INIT_PRIV_NUM;
	int ioctl_sock = get_ioctl_sock(iface);

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, (char *)iface, IFNAMSIZ);

	do {
		/* (Re)allocate the buffer */
		newPriv = realloc(pPriv, size * sizeof(pPriv[0]));
		if (newPriv == NULL) {
			ERR("Error: Buffer allocation failed\n");
			/* this func() returns priv_cnt, returning an element
			 * from MWU_ERR enum is risky here since the enum value
			 * can be misinterpreted as priv_cnt by caller, so we
			 * daviate a bit from the convention */
			ret = -1;
			break;
		}
		pPriv = newPriv;

		iwr.u.data.pointer = (caddr_t)pPriv;
		iwr.u.data.length = size;
		iwr.u.data.flags = 0;

		if (ioctl(ioctl_sock, SIOCGIWPRIV, &iwr) < 0) {
			ERR("==> ioctl_sock: %d, errno: %d", ioctl_sock, errno);
			result = errno;
			ret = -1;
			if (result == E2BIG) {
				/* We need a bigger buffer. Check if kernel gave
				 * us any hints. */
				if (iwr.u.data.length > size) {
					/* Kernel provided required size */
					size = iwr.u.data.length;
				} else {
					/* No hint from kernel, double the
					 * buffer size */
					size *= 2;
				}
			} else {
				/* ioctl error */
				perror("ioctl[SIOCGIWPRIV]");
				break;
			}
		} else {
			/* Success. Return the number of private ioctls */
			Priv_args = pPriv;
			ret = iwr.u.data.length;
			break;
		}
	} while (size <= IW_MAX_PRIV_NUM);

	if ((ret == -1) && (pPriv))
		FREE(pPriv);

	return ret;
}

int mwu_get_subioctl_no(int i, int priv_cnt, int *ioctl_val, int *subioctl_val)
{
	int j;

	if (Priv_args[i].cmd >= SIOCDEVPRIVATE) {
		*ioctl_val = Priv_args[i].cmd;
		*subioctl_val = 0;
		ERR("Not a private ioctl.\n");
		return MWU_ERR_SUCCESS;
	}

	j = -1;

	/* Find the matching *real* ioctl */

	while ((++j < priv_cnt) &&
	       ((Priv_args[j].name[0] != '\0') ||
		(Priv_args[j].set_args != Priv_args[i].set_args) ||
		(Priv_args[j].get_args != Priv_args[i].get_args))) {
	}

	/* If not found... */
	if (j == priv_cnt) {
		INFO("Invalid private ioctl definition for: 0x%x\n",
		     Priv_args[i].cmd);
		return MWU_ERR_COM;
	}

	/* Save ioctl numbers */
	*ioctl_val = Priv_args[j].cmd;
	*subioctl_val = Priv_args[i].cmd;

	return MWU_ERR_SUCCESS;
}

int mwu_get_ioctl_no(char *iface, const char *priv_cmd, int *ioctl_val,
		     int *subioctl_val)
{
	int i;
	int priv_cnt;
	int ret = MWU_ERR_COM;

	priv_cnt = mwu_get_private_info(iface);

	/* Are there any private ioctls? */
	if (priv_cnt <= 0 || priv_cnt > IW_MAX_PRIV_NUM) {
		/* Could skip this message ? */
		ERR("%-8.8s  no private ioctls.\n", iface);
	} else {
		for (i = 0; i < priv_cnt; i++) {
			if (Priv_args[i].name[0] &&
			    !strcmp((char *)Priv_args[i].name,
				    (char *)priv_cmd)) {
				ret = mwu_get_subioctl_no(
					i, priv_cnt, ioctl_val, subioctl_val);
				break;
			}
		}
	}

	FREE(Priv_args);

	return ret;
}

int mwu_get_priv_ioctl(char *iface, char *ioctl_name, int *ioctl_val,
		       int *subioctl_val)
{
	int retval;

	retval = mwu_get_ioctl_no(iface, ioctl_name, ioctl_val, subioctl_val);

	/* Debug print discovered IOCTL values */
	INFO("ioctl %s: %x, %x\n", ioctl_name, *ioctl_val, *subioctl_val);

	return retval;
}

int mwu_get_ssid(char *ifname, unsigned char *ssid)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	iwr.u.essid.pointer = (caddr_t)ssid;
	iwr.u.essid.length = 32;

	return (send_iwreq_ioctl(ifname, &iwr, SIOCGIWESSID) ==
		MWU_ERR_SUCCESS) ?
		       iwr.u.essid.length :
		       MWU_ERR_COM;
}

int mwu_set_ssid(char *ifname, const unsigned char *ssid, unsigned int ssid_len,
		 int skip_scan)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *temp = NULL;
	u8 buf[MRVDRV_SIZE_OF_CMD_BUFFER];
	u16 mrvl_header_len = 0;
	struct mwreq *mwr;
	char buffer[33];

	if (ssid_len > 32) {
		ERR("Invalid SSID. SSID should be <= 32 ASCII");
		return MWU_ERR_INVAL;
	}

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_SET_ESSID);
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
	strncpy((char *)temp, PRIV_CMD_SET_ESSID, strlen(PRIV_CMD_SET_ESSID));
	temp += (strlen(PRIV_CMD_SET_ESSID));

	mwr = (struct mwreq *)temp;
	memset(mwr, 0, sizeof(struct mwreq));

	/* flags: 1 = ESSID is active, 0 = Any (Promiscuous) */
	if (!skip_scan)
		mwr->u.essid.flags = (ssid_len != 0);
	else
		mwr->u.essid.flags = 0xFFFF;
	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, ssid, ssid_len);
	mwr->u.essid.pointer = (u8 *)buffer;
	mwr->u.essid.length = ssid_len ? ssid_len : 0;
	INFO("SSID length %d", mwr->u.essid.length);
	return mwu_privcmd(ifname, (u8 *)mrvl_cmd);
}

int mwu_set_authentication(char *ifname, int auth_mode)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *temp = NULL;
	u8 buf[MRVDRV_SIZE_OF_CMD_BUFFER];
	u16 mrvl_header_len = 0;
	struct mwreq *mwr;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_SET_AUTH);
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
	strncpy((char *)temp, PRIV_CMD_SET_AUTH, strlen(PRIV_CMD_SET_AUTH));
	temp += (strlen(PRIV_CMD_SET_AUTH));

	mwr = (struct mwreq *)temp;
	memset(mwr, 0, sizeof(struct mwreq));
	mwr->u.param.flags = MW_AUTH_80211_AUTH_ALG;

	if (auth_mode == AUTHENTICATION_TYPE_SHARED) {
		mwr->u.param.value = MW_AUTH_ALG_SHARED_KEY;
	} else {
		mwr->u.param.value = MW_AUTH_ALG_OPEN_SYSTEM;
	}

	return mwu_privcmd(ifname, (u8 *)mrvl_cmd);
}

int mwu_get_wap(char *ifname, unsigned char *bssid)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *temp = NULL;
	u8 buf[MRVDRV_SIZE_OF_CMD_BUFFER];
	u16 mrvl_header_len = 0;
	struct mwreq *mwr;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_GET_AP);
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
	strncpy((char *)temp, PRIV_CMD_GET_AP, strlen(PRIV_CMD_GET_AP));
	temp += (strlen(PRIV_CMD_GET_AP));

	mwr = (struct mwreq *)temp;

	if (mwu_privcmd(ifname, (u8 *)mrvl_cmd) == MWU_ERR_SUCCESS &&
	    mwr->u.ap_addr.sa_family == ARPHRD_ETHER) {
		memcpy(bssid, mwr->u.ap_addr.sa_data, ETH_ALEN);
		return MWU_ERR_SUCCESS;
	}

	return MWU_ERR_COM;
}

int mwu_set_wap(char *ifname, const unsigned char *bssid)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *temp = NULL;
	u8 buf[MRVDRV_SIZE_OF_CMD_BUFFER];
	u16 mrvl_header_len = 0;
	struct mwreq *mwr;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_SET_AP);
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
	strncpy((char *)temp, PRIV_CMD_SET_AP, strlen(PRIV_CMD_SET_AP));
	temp += (strlen(PRIV_CMD_SET_AP));

	mwr = (struct mwreq *)temp;
	memset(mwr, 0, sizeof(struct mwreq));
	mwr->u.ap_addr.sa_family = ARPHRD_ETHER;
	memcpy(mwr->u.ap_addr.sa_data, bssid, ETH_ALEN);

	return mwu_privcmd(ifname, (u8 *)mrvl_cmd);
}

int mwu_set_mode(char *ifname, int mode)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *temp = NULL;
	u8 buf[MRVDRV_SIZE_OF_CMD_BUFFER];
	u16 mrvl_header_len = 0;
	struct mwreq *mwr;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_SET_BSS_MODE);
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
	strncpy((char *)temp, PRIV_CMD_SET_BSS_MODE,
		strlen(PRIV_CMD_SET_BSS_MODE));
	temp += (strlen(PRIV_CMD_SET_BSS_MODE));

	mwr = (struct mwreq *)temp;
	memset(mwr, 0, sizeof(struct mwreq));
	mwr->u.mode = mode ? MW_MODE_ADHOC : MW_MODE_INFRA;

	return mwu_privcmd(ifname, (u8 *)mrvl_cmd);
}

int mwu_get_ifflags(char *ifname, int *flags)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(ifr));
	if ((ret = send_ifreq_ioctl(ifname, &ifr, SIOCGIFFLAGS)) ==
	    MWU_ERR_SUCCESS) {
		*flags = ifr.ifr_flags & 0xffff;
	}

	return ret;
}

int mwu_set_ifflags(char *ifname, int flags)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = flags & 0xffff;

	return send_ifreq_ioctl(ifname, &ifr, SIOCSIFFLAGS);
}

int mwu_bring_if_up(char *ifname)
{
	int flag = 0;

	if (mwu_get_ifflags(ifname, &flag) != MWU_ERR_SUCCESS ||
	    mwu_set_ifflags(ifname, flag | IFF_UP) != MWU_ERR_SUCCESS)
		return MWU_ERR_COM;

	return MWU_ERR_SUCCESS;
}

int mwu_set_deauth(char *ifname)
{
	unsigned char *buf = NULL, *temp = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	int ret = MWU_ERR_SUCCESS, mrvl_header_len = 0;

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL)
		return MWU_ERR_NOMEM;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_DEAUTH);
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
	strncpy((char *)temp, PRIV_CMD_DEAUTH, strlen(PRIV_CMD_DEAUTH));
	temp += (strlen(PRIV_CMD_DEAUTH));

	/* Perform IOCTL */
	ret = mwu_privcmd(ifname, (u8 *)mrvl_cmd);

	if (buf)
		free(buf);
	return ret;
}
#if 0
int mwu_ie_config(char *ifname, int flag, short *pie_index, int ie_type)
{
    mrvl_priv_cmd *mrvl_cmd = NULL;
    unsigned char *buf = NULL, *pos = NULL, *temp = NULL;
    IEEEtypes_Header_t *ptlv_header = NULL;
    struct ifreq ifr;
    int i, ret = MWU_ERR_SUCCESS;
    unsigned short ie_len = 0, mgmt_subtype_mask = 0;
    tlvbuf_custom_ie *tlv = NULL;
    custom_ie *ie_ptr = NULL;
    unsigned char action = 0x00, type = 0x00;
    u16 mrvl_header_len = 0;
    struct mwu_iface_info *cur_if = NULL;

    cur_if = mwu_get_interface(ifname, GENERIC_MODULE_ID);
    if (!cur_if) {
        ERR("Cannot find interface info for %s.", ifname);
        return MWU_ERR_INVAL;
    }

    type = flag & 0x0f;
    action = flag & 0xf0;

    buf = (unsigned char *) malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
    if (buf == NULL)
        return MWU_ERR_NOMEM;

    memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

    mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_CUSTOMIE);
    /* Fill up buffer */
    mrvl_cmd = (mrvl_priv_cmd *) buf;
    mrvl_cmd->buf = buf + sizeof(mrvl_priv_cmd);
    mrvl_cmd->used_len = 0;
    mrvl_cmd->total_len = MRVDRV_SIZE_OF_CMD_BUFFER - mrvl_header_len - BUF_HEADER_SIZE;
    /* Copy NXP command string */
    temp = mrvl_cmd->buf;
    strncpy((char *)temp, CMD_NXP, strlen(CMD_NXP));
    temp += (strlen(CMD_NXP));
    /* Insert command string*/
    strncpy((char *)temp, PRIV_CMD_CUSTOMIE, strlen(PRIV_CMD_CUSTOMIE));

    tlv = (tlvbuf_custom_ie *) (mrvl_cmd->buf + mrvl_header_len);
    tlv->tag = MRVL_MGMT_IE_LIST_TLV_ID;

    /* Locate headers */
    ie_ptr = (custom_ie *) (tlv->ie_data);
    /* Set TLV fields */

    if (action == SET_WPS_IE) {

        /* Set WPS IE */
        pos = ie_ptr->ie_buffer;
        ptlv_header = (IEEEtypes_Header_t *) pos;
        pos += sizeof(IEEEtypes_Header_t);

        switch (ie_type) {

        case IE_CFG_AP_BCN:
            if (type == WPS_AP_SESSION_INACTIVE) {
                ie_len = wps_ap_beacon_prepare(cur_if, WPS_END_REG_DISCOVERY_PHASE, pos);
                mgmt_subtype_mask = MGMT_MASK_BEACON;
            } else if (type == WPS_AP_SESSION_ACTIVE) {
                ie_len = wps_ap_beacon_prepare(cur_if, WPS_START_REG_DISCOVERY_PHASE, pos);
                mgmt_subtype_mask = MGMT_MASK_BEACON;
            } else {
                ERR("Unknown action %d type for %s", action, mwu_ie_type_to_str(ie_type));
                ret = MWU_ERR_INVAL;
                goto _exit_;
            }
            break;

        case IE_CFG_AP_ASSOCRESP:
            if (type == WPS_AP_SESSION_ACTIVE_AR) {
                ie_len =  wps_ap_assoc_response_prepare(cur_if, WPS_START_REG_DISCOVERY_PHASE, pos);
                mgmt_subtype_mask = MGMT_MASK_ASSOC_RESP | MGMT_MASK_REASSOC_RESP;
            } else if (type == WPS_AP_SESSION_INACTIVE_AR) {
                ie_len = wps_ap_assoc_response_prepare(cur_if, WPS_END_REG_DISCOVERY_PHASE, pos);
                mgmt_subtype_mask = MGMT_MASK_ASSOC_RESP | MGMT_MASK_REASSOC_RESP;
            } else {
                ERR("Unknown action %d type for %s", action, mwu_ie_type_to_str(ie_type));
                ret = MWU_ERR_INVAL;
                goto _exit_;
            }
            break;

        case IE_CFG_PROBE_RESP:
            if (type == WPS_AP_SESSION_INACTIVE) {
                ie_len = wps_ap_probe_response_prepare(cur_if, WPS_END_REG_DISCOVERY_PHASE, pos);
                mgmt_subtype_mask = MGMT_MASK_PROBE_RESP;
            } else if (type == WPS_AP_SESSION_ACTIVE) {
                ie_len = wps_ap_probe_response_prepare(cur_if, WPS_START_REG_DISCOVERY_PHASE, pos);
                mgmt_subtype_mask = MGMT_MASK_PROBE_RESP;
            } else {
                ERR("Unknown action %d type for %s", action, mwu_ie_type_to_str(ie_type));
                ret = MWU_ERR_INVAL;
                goto _exit_;
            }
            break;

        case IE_CFG_ASSOCREQ:
            if (type == WPS_STA_SESSION_ACTIVE) {
                ie_len = wps_sta_assoc_request_prepare(cur_if, pos);
                mgmt_subtype_mask = MGMT_MASK_ASSOC_REQ | MGMT_MASK_REASSOC_REQ;
            } else {
                ERR("Unknown action %d type for %s", action, mwu_ie_type_to_str(ie_type));
                ret = MWU_ERR_INVAL;
                goto _exit_;
            }

        case IE_CFG_PROBE:
            if (type == WPS_STA_SESSION_ACTIVE) {
                ie_len = wps_sta_probe_request_prepare(cur_if, pos);
                mgmt_subtype_mask = MGMT_MASK_ASSOC_REQ | MGMT_MASK_REASSOC_REQ;
            } else {
                ERR("Unknown action %d type for %s", action, mwu_ie_type_to_str(ie_type));
                ret = MWU_ERR_INVAL;
                goto _exit_;
            }

        default:
            ERR("ie_config: unknown SET type!\n");
            ret = MWU_ERR_INVAL;
            goto _exit_;
        }

        ptlv_header->Type = WPS_IE;
        ptlv_header->Len = ie_len;
        ie_ptr->mgmt_subtype_mask = mgmt_subtype_mask;
        tlv->length = sizeof(custom_ie) + sizeof(IEEEtypes_Header_t) + ie_len;
        ie_ptr->ie_length = sizeof(IEEEtypes_Header_t) + ie_len;
        ie_ptr->ie_index = *pie_index;

      } else if (action == CLEAR_WPS_IE) {
        /* Clear WPS IE */
        pos = ie_ptr->ie_buffer;
        ie_ptr->mgmt_subtype_mask = MGMT_MASK_CLEAR;
        ie_ptr->ie_length = 0;
        ie_ptr->ie_index = *pie_index;
        tlv->length = sizeof(custom_ie);

    } else {
        /* Get WPS IE */
        tlv->length = 0;
    }

    /* Perform IOCTL */
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifru.ifru_data = (void *) buf;
    strncpy(ifr.ifr_ifrn.ifrn_name, (char *) ifname, strlen(ifname));
    if ((ret = send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD)) != MWU_ERR_SUCCESS) {
        ERR("Failed to set/get/clear the IE buffer");
        goto _exit_;
    }

    if (action == GET_WPS_IE) {
        /* Get the free IE buffer index number */
        tlv = (tlvbuf_custom_ie *) (buf + sizeof(mrvl_priv_cmd) + mrvl_header_len);
        *pie_index = -1;
        ie_ptr = (custom_ie *) (tlv->ie_data);
        for (i = 0; i < MAX_MGMT_IE_INDEX; i++) {
            if (ie_ptr->mgmt_subtype_mask == 0) {
                *pie_index = ie_ptr->ie_index;
                break;
            }
            ie_ptr =
                (custom_ie *) ((u8 *) ie_ptr + sizeof(custom_ie) +
                        ie_ptr->ie_length);
        }
        if (*pie_index == -1 || *pie_index > (MAX_MGMT_IE_INDEX - 1)) {
            ERR("No free IE buffer available");
            ret = MWU_ERR_COM;
        }
    }
_exit_:
    FREE(buf);

    return ret;
}

int mwu_assoc_request_ie_config(char *ifname, int flag,
                                short *pie_index)
{
    mrvl_priv_cmd *mrvl_cmd = NULL;
    u8 *buf = NULL, *pos = NULL, *temp = NULL;
    IEEEtypes_Header_t *ptlv_header = NULL;
    struct ifreq ifr;
    int i, ret = MWU_ERR_SUCCESS;
    u16 ie_len = 0, mrvl_header_len = 0, mgmt_subtype_mask = 0;
    tlvbuf_custom_ie *tlv = NULL;
    custom_ie *ie_ptr = NULL;
    u8 action = 0x00, type = 0x00;
    struct mwu_iface_info *cur_if = NULL;

    cur_if = mwu_get_interface(ifname, GENERIC_MODULE_ID);
    if (!cur_if) {
        ERR("Cannot find interface info for %s.", ifname);
        return MWU_ERR_INVAL;
    }

    type = flag & 0x0f;
    action = flag & 0xf0;

    INFO("Type is 0x%x", type);

    INFO("Type is 0x%x", type);

    buf = (u8 *) malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
    if (buf == NULL)
        return MWU_ERR_NOMEM;

    memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

    mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_CUSTOMIE);
    /* Fill up buffer */
    mrvl_cmd = (mrvl_priv_cmd *)buf;
    mrvl_cmd->buf = buf + sizeof(mrvl_priv_cmd);
    mrvl_cmd->used_len = 0;
    mrvl_cmd->total_len = MRVDRV_SIZE_OF_CMD_BUFFER - mrvl_header_len - BUF_HEADER_SIZE;
    /* Copy NXP command string */
    temp = mrvl_cmd->buf;
    strncpy((char *)temp, CMD_NXP, strlen(CMD_NXP));
    temp += (strlen(CMD_NXP));
    /* Insert command string*/
    strncpy((char *)temp, PRIV_CMD_CUSTOMIE, strlen(PRIV_CMD_CUSTOMIE));

    tlv = (tlvbuf_custom_ie *) (mrvl_cmd->buf + mrvl_header_len);
    tlv->tag = MRVL_MGMT_IE_LIST_TLV_ID;

    /* Locate headers */
    ie_ptr = (custom_ie *) (tlv->ie_data);
    /* Set TLV fields */

    if (action == SET_WPS_IE) {
        /* Set WPS IE */
        pos = ie_ptr->ie_buffer;
        ptlv_header = (IEEEtypes_Header_t *) pos;
        pos += sizeof(IEEEtypes_Header_t);
        ie_len = wps_sta_assoc_request_prepare(cur_if, pos);
        mgmt_subtype_mask = MGMT_MASK_ASSOC_REQ | MGMT_MASK_REASSOC_REQ;

        ptlv_header->Type = WPS_IE;
        ptlv_header->Len = ie_len;
        ie_ptr->mgmt_subtype_mask = mgmt_subtype_mask;
        tlv->length += sizeof(custom_ie) + sizeof(IEEEtypes_Header_t) + ie_len;
        ie_ptr->ie_length = sizeof(IEEEtypes_Header_t) + ie_len;
        ie_ptr->ie_index = *pie_index;

    } else if (action == CLEAR_WPS_IE) {
        /* Clear WPS IE */
        pos = ie_ptr->ie_buffer;
        ie_ptr->mgmt_subtype_mask = MGMT_MASK_CLEAR;
        ie_ptr->ie_length = 0;
        ie_ptr->ie_index = *pie_index;   /* index */
        tlv->length = sizeof(custom_ie);

    } else {
        /* Get WPS IE */
        tlv->length = 0;
    }

    mwu_hexdump(DEBUG_WLAN, "MGMT_IE", (const unsigned char *) buf,
                sizeof(mrvl_priv_cmd) + mrvl_header_len + tlv->length + TLV_HEADER_SIZE);

    /* Perform IOCTL */
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifru.ifru_data = (void *) buf;
    strncpy(ifr.ifr_ifrn.ifrn_name, cur_if->ifname, IFNAMSIZ);
    if ((ret = send_ifreq_ioctl(cur_if->ifname, &ifr, MRVLPRIVCMD)) != MWU_ERR_SUCCESS) {
        ERR("Failed to set/get/clear the IE buffer");
        goto _exit_;
    }

    if (flag == GET_WPS_IE) {
        /* Get the free IE buffer index number */
        tlv = (tlvbuf_custom_ie *) (buf + sizeof(mrvl_priv_cmd) + mrvl_header_len);
        *pie_index = -1;
        ie_ptr = (custom_ie *) (tlv->ie_data);
        for (i = 0; i < MAX_MGMT_IE_INDEX; i++) {
            if (ie_ptr->mgmt_subtype_mask == 0) {
                *pie_index = ie_ptr->ie_index;
                break;
            }
            if (i < (MAX_MGMT_IE_INDEX - 1))
                ie_ptr =
                    (custom_ie *) ((u8 *) ie_ptr + sizeof(custom_ie) +
                                   ie_ptr->ie_length);
        }
        if (*pie_index == -1 || (*pie_index > (MAX_MGMT_IE_INDEX -1))) {
            /* check for > (MAX_MGMT_IE_INDEX -1) */
            mwu_printf(MSG_ERROR, "No free IE buffer available\n");
            ret = MWU_ERR_COM;
        }
    }
_exit_:
    FREE(buf);

    return ret;
}
#endif

#define WPS_VENDOR_EXT_ID_LEN (3)
static const char wfa_vendor_ext_id[WPS_VENDOR_EXT_ID_LEN] = {0x00, 0x37, 0x2A};

/** WPS OUI */
static u8 wps_wifi_oui[4] = {0x00, 0x50, 0xF2, 0x04};

/**
 *  @brief  Prepare for WPS2 Dummy Attribute
 *
 *  @param ptr          A pointer to buffer which store this attribute
 *  @return             Length of Attribute TLV
 */
static int wps_attribute_dummy_prepare(u8 *ptr)
{
	PTLV_DATA_HEADER ptlv;
	int offset = 0;
	ENTER();

	ptlv = (PTLV_DATA_HEADER)ptr;
	ptlv->type = mwu_htons(SC_Dummy_Attribute);
	ptlv->length = 1;
	ptr += SZ_TLV_HEADER;
	*ptr = 0xAA;
	ptr += ptlv->length;
	offset = SZ_TLV_HEADER + ptlv->length;
	ptlv->length = mwu_htons(ptlv->length);

	LEAVE();
	return offset;
}
/**
 *  @brief  Prepare for WPS Version Attribute
 *
 *  @param ptr          A pointer to buffer which store this attribute
 *  @return             Length of Attribute TLV
 */
static int wps_attribute_version_prepare(u8 *ptr)
{
	/* The Version attr is deprecated in WSC2.0, so hardcode a value of 0x10
	   for backward compatibility */
	u8 version = WPS_VERSION_1DOT0;
	PTLV_DATA_HEADER ptlv;
	int offset;
	ENTER();

	ptlv = (PTLV_DATA_HEADER)ptr;
	ptlv->type = mwu_htons(SC_Version);
	ptlv->length = 1;
	ptr += SZ_TLV_HEADER;

	memcpy(ptr, &version, ptlv->length);

	ptr += ptlv->length;
	offset = SZ_TLV_HEADER + ptlv->length;

	ptlv->length = mwu_htons(ptlv->length);

	LEAVE();
	return offset;
}

/**
 *  @brief  Prepare for WPS Version2 Attribute
 *
 *  @param cur_if       Current interface
 *  @param ptr          A pointer to buffer which store this attribute
 *  @return             Length of Attribute TLV
 */
static int wps_subele_version2_prepare(struct mwu_iface_info *cur_if, u8 *ptr)
{
	PSUBELE_DATA_HEADER pele;
	int offset;

	ENTER();

	pele = (PSUBELE_DATA_HEADER)ptr;
	pele->type = SC_Version2;
	pele->length = SZ_VERSION2;
	ptr += SZ_SUBELE_HEADER;

	if (cur_if->role == WPS_ENROLLEE || (IS_DISCOVERY_ENROLLEE(cur_if))) {
		mwu_printf(DEBUG_WLAN, "Sending Version2 as 0x%x!\n",
			   cur_if->pwps_info->enrollee.version);
		memcpy(ptr, &cur_if->pwps_info->enrollee.version, pele->length);
	} else if (cur_if->role == WPS_REGISTRAR ||
		   (IS_DISCOVERY_REGISTRAR(cur_if))) {
		memcpy(ptr, &cur_if->pwps_info->registrar.version,
		       pele->length);
	}

	offset = SZ_SUBELE_HEADER + pele->length;

	LEAVE();
	return offset;
}

/**
 *  @brief  Prepare for WPS UUID Attribute
 *
 *  @param cur_if       Current interface
 *  @param ptr          A pointer to buffer which store this attribute
 *  @return             Length of Attribute TLV
 */
static int wps_attribute_uuid_prepare(struct mwu_iface_info *cur_if, u8 *ptr)
{
	PTLV_DATA_HEADER ptlv;
	int offset;

	ENTER();

	ptlv = (PTLV_DATA_HEADER)ptr;

	if (cur_if->role == WPS_ENROLLEE || (IS_DISCOVERY_ENROLLEE(cur_if))) {
		ptlv->type = mwu_htons(SC_UUID_E);
		ptlv->length = cur_if->pwps_info->enrollee.wps_uuid_length;
		ptr += SZ_TLV_HEADER;
		memcpy(ptr, cur_if->pwps_info->enrollee.wps_uuid, ptlv->length);
	} else if (cur_if->role == WPS_REGISTRAR ||
		   (IS_DISCOVERY_REGISTRAR(cur_if))) {
		ptlv->type = mwu_htons(SC_UUID_R);
		ptlv->length = cur_if->pwps_info->registrar.wps_uuid_length;
		ptr += SZ_TLV_HEADER;
		memcpy(ptr, cur_if->pwps_info->registrar.wps_uuid,
		       ptlv->length);
	}

	offset = SZ_TLV_HEADER + ptlv->length;
	ptlv->length = mwu_htons(ptlv->length);

	LEAVE();
	return offset;
}
/**
 *  @brief  Prepare for WPS Primary Device Attribute
 *
 *  @param cur_if       Current interface
 *  @param ptr          A pointer to buffer which store this attribute
 *  @return             Length of Attribute TLV
 */
static int
wps_attribute_primary_device_type_prepare(struct mwu_iface_info *cur_if,
					  u8 *ptr)
{
	PRIMARY_DEVICE_TYPE_MSG *primary_dev;
	int offset;

	ENTER();

	primary_dev = (PRIMARY_DEVICE_TYPE_MSG *)ptr;
	primary_dev->type = mwu_htons(SC_Primary_Device_Type);
	primary_dev->length = SZ_PRIMARY_DEVICE_TYPE;
	primary_dev->category_id =
		mwu_htons(cur_if->pwps_info->primary_dev_category);
	primary_dev->sub_category_id =
		mwu_htons(cur_if->pwps_info->primary_dev_subcategory);
	memcpy(primary_dev->oui_id, wps_wifi_oui, sizeof(wps_wifi_oui));

	offset = SZ_TLV_HEADER + primary_dev->length;
	primary_dev->length = mwu_htons(primary_dev->length);

	LEAVE();
	return offset;
}
/**
 *  @brief  Prepare for WPS Vendor extension Attribute
 *
 *  @param ptr          A pointer to buffer which store this attribute
 *  @return             Length of Attribute TLV
 */
static int wps_attribute_vendor_ext_prepare(u8 *ptr)
{
	PTLV_DATA_HEADER ptlv;

	ENTER();

	ptlv = (PTLV_DATA_HEADER)ptr;
	ptlv->type = mwu_htons(SC_Vendor_Extension);
	ptlv->length = 0;
	ptr += SZ_TLV_HEADER;

	LEAVE();
	return (SZ_TLV_HEADER);
}
#define ACT_AS_ENROLLEE(cur_if)                                                \
	(((cur_if)->role == WIFIDIR_ROLE &&                                    \
	  (cur_if)->discovery_role == WPS_ENROLLEE) ||                         \
	 ((cur_if)->role == WPS_ENROLLEE))
/**
 *  @brief  Prepare for Attribute needed for probe request frame
 *
 *  @param cur_if   Current interface
 *  @param ptr      A pointer to buffer which store this attribute
 *  @return         Length of Attribute TLV
 */
/* XXX: Actually we should do this for both Enrollee & Registrar */
int wps_sta_probe_request_prepare(struct mwu_iface_info *cur_if, u8 *ptr)
{
	PTLV_DATA_HEADER ptlv;
	u8 msg_value_char;
	u16 msg_value_short;
	u16 offset, message_length = 0;
	PTLV_DATA_HEADER wfa_ven_tlv;
	struct MESSAGE_ENROLLEE_REGISTRAR *en_reg = NULL;

	ENTER();

	en_reg = ((ACT_AS_ENROLLEE(cur_if)) ? &cur_if->pwps_info->enrollee :
					      &cur_if->pwps_info->registrar);
	offset = 0;

	/* OUI */
	ptr += offset;
	memcpy(ptr, wps_wifi_oui, sizeof(wps_wifi_oui));
	message_length += sizeof(wps_wifi_oui);
	ptr += message_length;

	/* Version */
	offset = wps_attribute_version_prepare(ptr);
	message_length += offset;
	ptr += offset;

	/* Request Type */
	ptlv = (PTLV_DATA_HEADER)ptr;
	ptlv->type = mwu_htons(SC_Request_Type);
	ptlv->length = 1;
	ptr += SZ_TLV_HEADER;
	// TODO: Change For WSTA Registrar
	msg_value_char = ((cur_if->role == WPS_REGISTRAR) ? REQ_TYPE_REGISTRAR :
							    REQ_TYPE_ENROLLEE);
	memcpy(ptr, &msg_value_char, 1);
	message_length += SZ_TLV_HEADER + ptlv->length;
	ptr += ptlv->length;
	ptlv->length = mwu_htons(ptlv->length);

	/* Config Methods */
	ptlv = (PTLV_DATA_HEADER)ptr;
	ptlv->type = mwu_htons(SC_Config_Methods);
	ptlv->length = 2;
	ptr += SZ_TLV_HEADER;
	msg_value_short = mwu_htons(en_reg->config_methods);
	memcpy(ptr, &msg_value_short, 2);
	message_length += SZ_TLV_HEADER + ptlv->length;
	ptr += ptlv->length;
	ptlv->length = mwu_htons(ptlv->length);

	/* UUID */
	offset = wps_attribute_uuid_prepare(cur_if, ptr);
	message_length += offset;
	ptr += offset;

	/* Primary Device Type */
	offset = wps_attribute_primary_device_type_prepare(cur_if, ptr);
	message_length += offset;
	ptr += offset;

	/* RF Bands */
	ptlv = (PTLV_DATA_HEADER)ptr;
	ptlv->type = mwu_htons(SC_RF_Band);
	ptlv->length = 1;
	ptr += SZ_TLV_HEADER;
	memcpy(ptr, &en_reg->rf_bands, ptlv->length);
	message_length += SZ_TLV_HEADER + ptlv->length;
	ptr += ptlv->length;
	ptlv->length = mwu_htons(ptlv->length);

	/* Association State */
	ptlv = (PTLV_DATA_HEADER)ptr;
	ptlv->type = mwu_htons(SC_Association_State);
	ptlv->length = 2;
	ptr += SZ_TLV_HEADER;
	msg_value_short = mwu_htons(0x0); /* Not Associated */
	memcpy(ptr, &msg_value_short, ptlv->length);
	message_length += SZ_TLV_HEADER + ptlv->length;
	ptr += ptlv->length;
	ptlv->length = mwu_htons(ptlv->length);

	/* Configuration Error */
	ptlv = (PTLV_DATA_HEADER)ptr;
	ptlv->type = mwu_htons(SC_Configuration_Error);
	ptlv->length = 2;
	ptr += SZ_TLV_HEADER;
	msg_value_short = mwu_htons(CONFIG_ERROR_NO_ERROR);
	memcpy(ptr, &msg_value_short, 2);
	message_length += (SZ_TLV_HEADER + ptlv->length);
	ptr += ptlv->length;
	ptlv->length = mwu_htons(ptlv->length);

	/* Device Password ID */
	ptlv = (PTLV_DATA_HEADER)ptr;
	ptlv->type = mwu_htons(SC_Device_Password_ID);
	ptlv->length = 2;
	ptr += SZ_TLV_HEADER;

	msg_value_short = mwu_htons(en_reg->updated_device_password_id);
	memcpy(ptr, &msg_value_short, 2);
	message_length += SZ_TLV_HEADER + ptlv->length;
	ptr += ptlv->length;
	ptlv->length = mwu_htons(ptlv->length);

	if (en_reg->version >= WPS_VERSION_2DOT0) {
		/* Manufacture */
		ptlv = (PTLV_DATA_HEADER)ptr;
		ptlv->type = mwu_htons(SC_Manufacturer);
		ptlv->length = en_reg->manufacture_length;
		ptr += SZ_TLV_HEADER;
		memcpy(ptr, en_reg->manufacture, ptlv->length);
		message_length += SZ_TLV_HEADER + ptlv->length;
		ptr += ptlv->length;
		ptlv->length = mwu_htons(ptlv->length);

		/* Model Name */
		ptlv = (PTLV_DATA_HEADER)ptr;
		ptlv->type = mwu_htons(SC_Model_Name);
		ptlv->length = en_reg->model_name_length;
		ptr += SZ_TLV_HEADER;
		memcpy(ptr, en_reg->model_name, ptlv->length);
		message_length += SZ_TLV_HEADER + ptlv->length;
		ptr += ptlv->length;
		ptlv->length = mwu_htons(ptlv->length);

		/* Model Number */
		ptlv = (PTLV_DATA_HEADER)ptr;
		ptlv->type = mwu_htons(SC_Model_Number);
		ptlv->length = en_reg->model_number_length;
		ptr += SZ_TLV_HEADER;
		memcpy(ptr, en_reg->model_number, ptlv->length);
		message_length += SZ_TLV_HEADER + ptlv->length;
		ptr += ptlv->length;
		ptlv->length = mwu_htons(ptlv->length);

		/* Device Name */
		ptlv = (PTLV_DATA_HEADER)ptr;
		ptlv->type = mwu_htons(SC_Device_Name);
		ptlv->length = en_reg->device_name_length;
		ptr += SZ_TLV_HEADER;
		memcpy(ptr, en_reg->device_name, ptlv->length);
		message_length += SZ_TLV_HEADER + ptlv->length;
		ptr += ptlv->length;
		ptlv->length = mwu_htons(ptlv->length);

		wfa_ven_tlv = (PTLV_DATA_HEADER)ptr;
		/* Vendor Extension */
		offset = wps_attribute_vendor_ext_prepare(ptr);
		ptr += offset;

		/* WFA Vendor Extension ID */
		memcpy(ptr, wfa_vendor_ext_id, WPS_VENDOR_EXT_ID_LEN);
		ptr += WPS_VENDOR_EXT_ID_LEN;

		/* Version2 sub-ele */
		offset = wps_subele_version2_prepare(cur_if, ptr);
		ptr += offset;

		/* Set Vendor Extension Attr len */
		wfa_ven_tlv->length = ptr - ((u8 *)wfa_ven_tlv + SZ_TLV_HEADER);
		message_length += SZ_TLV_HEADER + wfa_ven_tlv->length;
		wfa_ven_tlv->length = mwu_htons(wfa_ven_tlv->length);

		if (cur_if->pwps_info->proto_ext_test) {
			offset = wps_attribute_dummy_prepare(ptr);
			ptr += offset;
			message_length += offset;
		}
	}

	LEAVE();
	return message_length;
}

int mwu_prob_request_ie_config(char *ifname, int flag, short *pie_index)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	unsigned char *buf = NULL, *pos = NULL, *temp = NULL;
	IEEEtypes_Header_t *ptlv_header = NULL;
	struct ifreq ifr;
	int i, ret = MWU_ERR_SUCCESS;
	unsigned short ie_len = 0, mgmt_subtype_mask = 0;
	tlvbuf_custom_ie *tlv = NULL;
	custom_ie *ie_ptr = NULL;
	unsigned char action = 0x00;
	// unsigned char  type = 0x00;
	u16 mrvl_header_len = 0;
	struct mwu_iface_info *cur_if = NULL;

	cur_if = mwu_get_interface(ifname, GENERIC_MODULE_ID);
	if (!cur_if) {
		ERR("Cannot find interface info for %s.", ifname);
		return MWU_ERR_INVAL;
	}

	// type = flag & 0x0f;
	action = flag & 0xf0;

	// INFO("Type is 0x%x", type);

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL)
		return MWU_ERR_NOMEM;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_CUSTOMIE);
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
	strncpy((char *)temp, PRIV_CMD_CUSTOMIE, strlen(PRIV_CMD_CUSTOMIE));

	tlv = (tlvbuf_custom_ie *)(mrvl_cmd->buf + mrvl_header_len);
	tlv->tag = MRVL_MGMT_IE_LIST_TLV_ID;

	/* Locate headers */
	ie_ptr = (custom_ie *)(tlv->ie_data);
	/* Set TLV fields */

	if (action == SET_WPS_IE) {
		/* Set WPS IE */
		pos = ie_ptr->ie_buffer;
		ptlv_header = (IEEEtypes_Header_t *)pos;
		pos += sizeof(IEEEtypes_Header_t);

		ie_len = wps_sta_probe_request_prepare(cur_if, pos);
		mgmt_subtype_mask = MGMT_MASK_PROBE_REQ;

		ptlv_header->Type = WPS_IE;
		ptlv_header->Len = ie_len;
		ie_ptr->mgmt_subtype_mask = mgmt_subtype_mask;
		tlv->length =
			sizeof(custom_ie) + sizeof(IEEEtypes_Header_t) + ie_len;
		ie_ptr->ie_length = sizeof(IEEEtypes_Header_t) + ie_len;
		ie_ptr->ie_index = *pie_index;

	} else if (action == CLEAR_WPS_IE) {
		/* Clear WPS IE */
		pos = ie_ptr->ie_buffer;
		ie_ptr->mgmt_subtype_mask = MGMT_MASK_CLEAR;
		ie_ptr->ie_length = 0;
		ie_ptr->ie_index = *pie_index; /* index */
		tlv->length = sizeof(custom_ie);

	} else {
		/* Get WPS IE */
		tlv->length = 0;
	}

	/* Perform IOCTL */
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifru.ifru_data = (void *)buf;
	strncpy(ifr.ifr_ifrn.ifrn_name, ifname, IFNAMSIZ);
	if ((ret = send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD)) !=
	    MWU_ERR_SUCCESS) {
		ERR("Failed to set/get/clear the IE buffer");
		goto _exit_;
	}

	if (flag == GET_WPS_IE) {
		/* Get the free IE buffer index number */
		tlv = (tlvbuf_custom_ie *)(buf + sizeof(mrvl_priv_cmd) +
					   mrvl_header_len);
		*pie_index = -1;
		ie_ptr = (custom_ie *)(tlv->ie_data);
		for (i = 0; i < MAX_MGMT_IE_INDEX; i++) {
			if (ie_ptr->mgmt_subtype_mask == 0) {
				*pie_index = ie_ptr->ie_index;
				break;
			}
			if (i < (MAX_MGMT_IE_INDEX - 1))
				ie_ptr = (custom_ie *)((unsigned char *)ie_ptr +
						       sizeof(custom_ie) +
						       ie_ptr->ie_length);
		}
		if (*pie_index == -1 || *pie_index > (MAX_MGMT_IE_INDEX - 1)) {
			ERR("No free IE buffer available");
			ret = MWU_ERR_COM;
		}
	}

_exit_:
	FREE(buf);

	return ret;
}

int mwu_apcmd_get_bss_config(char *ifname, bss_config_t *bss)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *pos = NULL, *buffer = NULL;
	u16 mrvl_header_len = 0, cmd_len = 0;
	apcmdbuf_bss_configure *cmd_buf = NULL;
	bss_config_t *bss_temp = NULL;
	struct ifreq ifr;
	int ret = MWU_ERR_SUCCESS;

	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_BSS_CONFIG);
	/* Alloc buf for command */
	buffer = (u8 *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (!buffer) {
		ERR("Failed to allocate buffer\n");
		return MWU_ERR_COM;
	}
	memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	mrvl_cmd->buf = buffer + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len = MRVDRV_SIZE_OF_CMD_BUFFER - sizeof(mrvl_priv_cmd);

	pos = mrvl_cmd->buf;
	strncpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += strlen(CMD_NXP);
	strncpy((char *)pos, PRIV_CMD_BSS_CONFIG, strlen(PRIV_CMD_BSS_CONFIG));
	/* Locate headers */
	cmd_len = sizeof(apcmdbuf_bss_configure);
	cmd_buf = (apcmdbuf_bss_configure *)(mrvl_cmd->buf + mrvl_header_len);
	bss_temp = (bss_config_t *)((u8 *)cmd_buf + cmd_len);

	/* Fill the command buffer */
	cmd_buf->Action = ACTION_GET;

	/* Initialize the ifr structure */
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_data = (void *)mrvl_cmd;
	ret = send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD);
	if (ret == MWU_ERR_SUCCESS)
		memcpy(bss, bss_temp, sizeof(bss_config_t));

	FREE(buffer);
	return ret;
}

int mwu_apcmd_set_bss_config(char *ifname, bss_config_t *bss)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *pos = NULL, *buffer = NULL;
	u16 mrvl_header_len = 0, cmd_len = 0;
	apcmdbuf_bss_configure *cmd_buf = NULL;
	bss_config_t *bss_temp = NULL;
	struct ifreq ifr;
	int ret = MWU_ERR_SUCCESS;

	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_BSS_CONFIG);
	/* Alloc buf for command */
	buffer = (u8 *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (!buffer) {
		ERR("Failed to allocate buffer\n");
		return MWU_ERR_COM;
	}
	memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	mrvl_cmd->buf = buffer + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len = MRVDRV_SIZE_OF_CMD_BUFFER - sizeof(mrvl_priv_cmd);

	pos = mrvl_cmd->buf;
	strncpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += strlen(CMD_NXP);
	strncpy((char *)pos, PRIV_CMD_BSS_CONFIG, strlen(PRIV_CMD_BSS_CONFIG));
	/* Locate headers */
	cmd_len = sizeof(apcmdbuf_bss_configure);
	cmd_buf = (apcmdbuf_bss_configure *)(mrvl_cmd->buf + mrvl_header_len);
	bss_temp = (bss_config_t *)((u8 *)cmd_buf + cmd_len);
	memcpy(bss_temp, bss, sizeof(bss_config_t));

	/* Fill the command buffer */
	cmd_buf->Action = ACTION_SET;

	/* Initialize the ifr structure */
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_data = (void *)mrvl_cmd;
	ret = send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD);

	FREE(buffer);
	return ret;
}

int mwu_apcmd_get_countrycode(char *ifname, u8 *countrycode)
{
	struct ifreq ifr;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *pos = NULL, *buffer = NULL;
	u16 mrvl_header_len = 0, cmd_len = 0;
	apcmdbuf_cfg_80211d *cmd = NULL;
	int ret = MWU_ERR_SUCCESS;

	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_HOSTCMD);
	/* Alloc buf for command */
	buffer = (u8 *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (!buffer) {
		ERR("Failed to allocate buffer\n");
		return MWU_ERR_COM;
	}
	memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	mrvl_cmd->buf = buffer + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len = MRVDRV_SIZE_OF_CMD_BUFFER - sizeof(mrvl_priv_cmd);

	pos = mrvl_cmd->buf;
	strncpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += strlen(CMD_NXP);
	strncpy((char *)pos, PRIV_CMD_HOSTCMD, strlen(PRIV_CMD_HOSTCMD));

	cmd_len = sizeof(apcmdbuf_cfg_80211d) - sizeof(domain_param_t);
	cmd = (apcmdbuf_cfg_80211d *)((mrvl_cmd->buf) + mrvl_header_len);
	cmd->cmd_code = HostCmd_CMD_802_11D_DOMAIN_INFO;
	cmd->seq_num = 0;
	cmd->result = 0;
	cmd->action = ACTION_GET;
	cmd->size = cmd_len - BUF_HEADER_SIZE;

	/* Initialize the ifr structure */
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_data = (void *)mrvl_cmd;
	ret = send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD);

	if (ret == MWU_ERR_SUCCESS) {
		if (cmd->domain.country_code[0] ||
		    cmd->domain.country_code[1] ||
		    cmd->domain.country_code[2]) {
			memcpy(countrycode, cmd->domain.country_code, 3);
			countrycode[2] = '\0';
		}
	}

	FREE(buffer);
	return ret;
}

int mwu_check_bss_config(char *ifname)
{
	bss_config_t bss_config;
	u8 secondary_ch_set = 0;
	u8 countrycode[3];
	int ret = MWU_ERR_SUCCESS;

	memset(&bss_config, 0, sizeof(bss_config_t));
	ret = mwu_apcmd_get_countrycode(ifname, countrycode);
	if (ret != MWU_ERR_SUCCESS) {
		ERR("Failed to get countrycode\n");
		return ret;
	}
	ret = mwu_apcmd_get_bss_config(ifname, &bss_config);
	if (ret != MWU_ERR_SUCCESS) {
		ERR("Failed to get BSS config\n");
		return ret;
	}

	secondary_ch_set = bss_config.bandcfg.chan2Offset;
	if (bss_config.bandcfg.chanBand != BAND_5GHZ) { /* Not 5G band */
		if (secondary_ch_set == SEC_CHAN_BELOW) { /* second channel
							     below */
			if (!strncmp((char *)countrycode, "US", 2)) {
				if (bss_config.num_of_chan > 11) {
					printf("ERR: Only channels 5-11 are allowed with secondary channel below for the US\n");
					return MWU_ERR_INVAL;
				}
			} else if (strncmp((char *)countrycode, "JP", 2)) {
				if (bss_config.num_of_chan > 13) {
					printf("ERR: Only channels 5-13 are allowed with secondary channel below for the non-JAPAN countries!\n");
					return MWU_ERR_INVAL;
				}
			}
		}
		if (secondary_ch_set == SEC_CHAN_ABOVE) { /* second channel
							     above */
			if (!strncmp((char *)countrycode, "US", 2)) {
				if (bss_config.num_of_chan > 7) {
					printf("ERR: Only channels 1-7 are allowed with secondary channel above for the US\n");
					return MWU_ERR_INVAL;
				}
			} else if (strncmp((char *)countrycode, "JP", 2)) {
				if (bss_config.num_of_chan > 9) {
					printf("ERR: Only channels 1-9 are allowed with secondary channel below for the non-JAPAN countries!\n");
					return MWU_ERR_INVAL;
				}
			}
		}
	}
	return ret;
}

int mwu_apcmd_start_bss(char *ifname)
{
	struct ifreq ifr;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *buffer = NULL, *pos = NULL;
	u16 len = 0, mrvl_header_len = 0;
	int ret = MWU_ERR_SUCCESS;

	ret = mwu_check_bss_config(ifname);
	if (ret != MWU_ERR_SUCCESS) {
		ERR("Invalid BSS config!\n");
		return ret;
	}
	/* Do not send CMD_NXP string to use android command AP_BSS_START in the
	 * driver */
	mrvl_header_len = strlen(PRIV_CMD_AP_START);
	len = sizeof(mrvl_priv_cmd) + mrvl_header_len;
	buffer = (u8 *)malloc(len);
	if (!buffer) {
		ERR("Failed to allocate buffer\n");
		return MWU_ERR_COM;
	}
	memset(buffer, 0, len);
	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	mrvl_cmd->buf = buffer + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len = len - sizeof(mrvl_priv_cmd);

	pos = mrvl_cmd->buf;
	strncpy((char *)pos, PRIV_CMD_AP_START, strlen(PRIV_CMD_AP_START));

	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_ifru.ifru_data = (void *)mrvl_cmd;
	ret = send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD);

	FREE(buffer);
	return ret;
}

int mwu_apcmd_stop_bss(char *ifname)
{
	struct ifreq ifr;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *buffer = NULL, *pos = NULL;
	u16 len = 0, mrvl_header_len = 0;
	int ret = MWU_ERR_SUCCESS;

	/* Do not send CMD_NXP string to use android command AP_BSS_STOP in the
	 * driver */
	mrvl_header_len = strlen(PRIV_CMD_AP_STOP);
	len = sizeof(mrvl_priv_cmd) + mrvl_header_len;
	buffer = (u8 *)malloc(len);
	if (!buffer) {
		ERR("Failed to allocate buffer\n");
		return MWU_ERR_COM;
	}
	memset(buffer, 0, len);
	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	mrvl_cmd->buf = buffer + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len = len - sizeof(mrvl_priv_cmd);

	pos = mrvl_cmd->buf;
	strncpy((char *)pos, PRIV_CMD_AP_STOP, strlen(PRIV_CMD_AP_STOP));

	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_ifru.ifru_data = (void *)mrvl_cmd;
	ret = send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD);

	FREE(buffer);
	return ret;
}

int mwu_set_intended_mac_addr(char *ifname, unsigned char *mac)
{
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	memcpy(ifr.ifr_hwaddr.sa_data, mac, ETH_ALEN);

	return send_ifreq_ioctl(ifname, &ifr, SIOCSIFHWADDR);
}

int mwu_get_mac_addr(char *ifname, unsigned char *mac)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));

	if (send_ifreq_ioctl(ifname, &ifr, SIOCGIFHWADDR) == MWU_ERR_SUCCESS) {
		memcpy(mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
		return MWU_ERR_SUCCESS;
	}

	return MWU_ERR_COM;
}

int mwu_custom_ie_config(char *ifname, unsigned char *buf)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifru.ifru_data = (void *)buf;
	strncpy(ifr.ifr_ifrn.ifrn_name, ifname, IFNAMSIZ);
	return send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD);
}

int mwu_privcmd(char *ifname, unsigned char *buf)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifru.ifru_data = (void *)buf;

	return send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD);
}

int mwu_set_bss_role(char *ifname, int bss_role)
{
	unsigned char *buf = NULL, *temp = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	int ret = MWU_ERR_SUCCESS, mrvl_header_len = 0;

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL)
		return MWU_ERR_NOMEM;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_BSSROLE);
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
	strncpy((char *)temp, PRIV_CMD_BSSROLE, strlen(PRIV_CMD_BSSROLE));
	temp += (strlen(PRIV_CMD_BSSROLE));
	if (bss_role == BSS_TYPE_UAP || bss_role == BSS_TYPE_STA) {
		sprintf((char *)temp, "%d", bss_role);
	}

	/* Perform IOCTL */
	ret = mwu_privcmd(ifname, (u8 *)mrvl_cmd);
	if (buf)
		free(buf);
	return ret;
}

int mwu_get_bss_role(char *ifname, int *bss_role)
{
	unsigned char *buf = NULL, *temp = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	int ret = MWU_ERR_SUCCESS, mrvl_header_len = 0;

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL)
		return MWU_ERR_NOMEM;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_BSSROLE);
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
	strncpy((char *)temp, PRIV_CMD_BSSROLE, strlen(PRIV_CMD_BSSROLE));
	temp += (strlen(PRIV_CMD_BSSROLE));

	/* Perform IOCTL */
	if (mwu_privcmd(ifname, (u8 *)mrvl_cmd) != MWU_ERR_SUCCESS) {
		if (!strncmp(ifname, "uap", 3)) {
			*bss_role = BSS_TYPE_UAP;
		} else {
			*bss_role = BSS_TYPE_STA;
		}
		ret = MWU_ERR_SUCCESS;
		goto done;
	}
	memcpy((u8 *)bss_role, mrvl_cmd->buf, sizeof(int));
done:
	if (buf)
		free(buf);
	return ret;
}

int mwu_set_deepsleep(char *ifname, u16 enable)
{
	unsigned char *buf = NULL, *temp = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	int ret = MWU_ERR_SUCCESS, mrvl_header_len = 0;

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL)
		return MWU_ERR_NOMEM;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_DEEPSLEEP);
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
	strncpy((char *)temp, PRIV_CMD_DEEPSLEEP, strlen(PRIV_CMD_DEEPSLEEP));
	temp += (strlen(PRIV_CMD_DEEPSLEEP));
	sprintf((char *)temp, "%d", enable);

	/* Perform IOCTL */
	ret = mwu_privcmd(ifname, (u8 *)mrvl_cmd);

	if (buf)
		free(buf);
	return ret;
}

int mwu_session_control(char *ifname, int enable)
{
	unsigned char *buf = NULL, *temp = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	int ret = MWU_ERR_SUCCESS, mrvl_header_len = 0;

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL)
		return MWU_ERR_NOMEM;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_WPSSESSION);

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
	strncpy((char *)temp, PRIV_CMD_WPSSESSION, strlen(PRIV_CMD_WPSSESSION));
	temp += (strlen(PRIV_CMD_WPSSESSION));
	if (enable == WPS_SESSION_ON || enable == WPS_SESSION_OFF) {
		sprintf((char *)temp, "%d", enable);
	} else {
		ret = MWU_ERR_INVAL;
		goto done;
	}
	/* Perform IOCTL */
	ret = mwu_privcmd(ifname, (u8 *)mrvl_cmd);

done:
	if (buf)
		free(buf);
	return ret;
}

int mwu_get_power_mode(char *ifname, int *enable)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *temp = NULL;
	u8 buf[MRVDRV_SIZE_OF_CMD_BUFFER];
	u16 mrvl_header_len = 0;
	struct mwreq *mwr;
	int ret = MWU_ERR_SUCCESS;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_GET_POWER);
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
	strncpy((char *)temp, PRIV_CMD_GET_POWER, strlen(PRIV_CMD_GET_POWER));
	temp += (strlen(PRIV_CMD_GET_POWER));

	mwr = (struct mwreq *)temp;
	memset(mwr, 0, sizeof(struct mwreq));
	mwr->u.power.flags = 0;

	if ((ret = mwu_privcmd(ifname, (u8 *)mrvl_cmd)) != MWU_ERR_SUCCESS)
		return ret;

	if (mwr->u.power.disabled == 1)
		*enable = 0;
	else
		*enable = 1;

	INFO("Get PS Mode : %d", *enable);
	return ret;
}

int mwu_set_power_mode(char *ifname, int enable)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *temp = NULL;
	u8 buf[MRVDRV_SIZE_OF_CMD_BUFFER];
	u16 mrvl_header_len = 0;
	struct mwreq *mwr;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_SET_POWER);
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
	strncpy((char *)temp, PRIV_CMD_SET_POWER, strlen(PRIV_CMD_SET_POWER));
	temp += (strlen(PRIV_CMD_SET_POWER));

	mwr = (struct mwreq *)temp;
	memset(mwr, 0, sizeof(struct mwreq));
	INFO("Set PS Mode : %d", enable);
	mwr->u.power.flags = 0;
	if (enable == 1)
		mwr->u.power.disabled = 0;
	else
		mwr->u.power.disabled = 1;

	return mwu_privcmd(ifname, (u8 *)mrvl_cmd);
}

int mwu_set_passphrase(char *ifname, char *buffer, int buf_len)
{
	unsigned char *buf = NULL, *temp = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	int ret = MWU_ERR_SUCCESS, mrvl_header_len = 0;

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL)
		return MWU_ERR_NOMEM;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_PASSPHRASE);
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
	strncpy((char *)temp, PRIV_CMD_PASSPHRASE, strlen(PRIV_CMD_PASSPHRASE));
	temp += (strlen(PRIV_CMD_PASSPHRASE));
	memcpy(temp, buffer, buf_len);

	/* Perform IOCTL */
	ret = mwu_privcmd(ifname, (u8 *)mrvl_cmd);

	if (buf)
		free(buf);
	return ret;
}

// Neeraj: Added to fix wifidir_ioctl not defined issue, but is this really
// needed?
extern int wifidir_ioctl(char *ifname, mrvl_priv_cmd *cmd, u16 *size,
			 u16 buf_size, u16 mrvl_header_size);

/* borrowed from mlanconfig.c and bg_scan_wifidir.conf.  This function is hard
 * coded for our WIFIDIR set up and may not be suitable for all applications.
 */
int mwu_mlanconfig_bgscan(char *ifname)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	unsigned char *buf = NULL, *pos = NULL;
	mrvl_cmd_head_buf *cmd = NULL;
	int ret = MWU_ERR_SUCCESS;
	mlanconfig_bgscfg_cmd_hdr *hdr;
	ssid_header_tlv *ssid_tlv;
	probe_header_tlv *probe_tlv;
	channel_header_tlv *chan_tlv;
	snr_header_tlv *snr_tlv;
	start_later_header_tlv *start_later_tlv;
	u16 cmd_len = 0, buf_len = 0, mrvl_header_len = 0;

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL) {
		ERR("allocate memory for hostcmd failed");
		return MWU_ERR_NOMEM;
	}

	/* prepare the bgscan command */
	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len += strlen(CMD_NXP) + strlen(PRIV_CMD_HOSTCMD);
	mrvl_cmd = (mrvl_priv_cmd *)buf;
	mrvl_cmd->buf = buf + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len =
		MRVDRV_SIZE_OF_CMD_BUFFER - mrvl_header_len - BUF_HEADER_SIZE;
	pos = mrvl_cmd->buf;
	/* Copy NXP command string */
	strncpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += (strlen(CMD_NXP));
	/* Insert command string*/
	strncpy((char *)pos, PRIV_CMD_HOSTCMD, strlen(PRIV_CMD_HOSTCMD));

	cmd_len = sizeof(mrvl_cmd_head_buf);
	cmd = (mrvl_cmd_head_buf *)((mrvl_cmd->buf) + mrvl_header_len);
	cmd->cmd_code = HostCmd_CMD_BGSCAN_CFG;
	cmd->seq_num = 0;
	cmd->result = 0;

	/* These are all magic numbers from the bg_scan_wifidir.conf file
	 * supplied by NXP
	 */
	hdr = (mlanconfig_bgscfg_cmd_hdr *)(cmd->cmd_data);
	hdr->Action = 1;
	hdr->Enable = 1;
	hdr->BssType = 3;
	hdr->ChannelsPerScan = 3;
	hdr->ScanInterval = wlan_cpu_to_le32(1000);
	hdr->StoreCondition = wlan_cpu_to_le32(1);
	hdr->ReportConditions = wlan_cpu_to_le32(1);
	cmd_len += sizeof(mlanconfig_bgscfg_cmd_hdr);

	ssid_tlv =
		(ssid_header_tlv *)(mrvl_cmd->buf + mrvl_header_len + cmd_len);
	ssid_tlv->type = MLANCONFIG_SSID_HEADER_TYPE;
	ssid_tlv->len = wlan_cpu_to_le16(SIZEOF_VALUE(ssid_header_tlv));
	ssid_tlv->MaxSSIDLen = wlan_cpu_to_le16(0);
	memcpy(ssid_tlv->ssid, "DIRECT-", sizeof(ssid_tlv->ssid));
	cmd_len += sizeof(tlv) + ssid_tlv->len;

	probe_tlv =
		(probe_header_tlv *)(mrvl_cmd->buf + mrvl_header_len + cmd_len);
	probe_tlv->type = MLANCONFIG_PROBE_HEADER_TYPE;
	probe_tlv->len = wlan_cpu_to_le16(SIZEOF_VALUE(probe_header_tlv));
	probe_tlv->NumProbes = wlan_cpu_to_le16(2);
	cmd_len += sizeof(tlv) + probe_tlv->len;

	chan_tlv = (channel_header_tlv *)(mrvl_cmd->buf + mrvl_header_len +
					  cmd_len);
	chan_tlv->type = MLANCONFIG_CHANNEL_HEADER_TYPE;
	chan_tlv->len = wlan_cpu_to_le16(SIZEOF_VALUE(channel_header_tlv));
	memset(&(chan_tlv->Chan1_bandcfg), 0, sizeof(chan_tlv->Chan1_bandcfg));
	chan_tlv->Chan1_ChanNumber = 1;
	chan_tlv->Chan1_ScanType = 2;
	chan_tlv->Chan1_MinScanTime = wlan_cpu_to_le16(10);
	chan_tlv->Chan1_ScanTime = wlan_cpu_to_le16(20);
	memset(&(chan_tlv->Chan2_bandcfg), 0, sizeof(chan_tlv->Chan2_bandcfg));
	chan_tlv->Chan2_ChanNumber = 6;
	chan_tlv->Chan2_ScanType = 2;
	chan_tlv->Chan2_MinScanTime = wlan_cpu_to_le16(10);
	chan_tlv->Chan2_ScanTime = wlan_cpu_to_le16(20);
	memset(&(chan_tlv->Chan3_bandcfg), 0, sizeof(chan_tlv->Chan3_bandcfg));
	chan_tlv->Chan3_ChanNumber = 11;
	chan_tlv->Chan3_ScanType = 2;
	chan_tlv->Chan3_MinScanTime = wlan_cpu_to_le16(10);
	chan_tlv->Chan3_ScanTime = wlan_cpu_to_le16(20);
	cmd_len += sizeof(tlv) + chan_tlv->len;

	snr_tlv = (snr_header_tlv *)(mrvl_cmd->buf + mrvl_header_len + cmd_len);
	snr_tlv->type = MLANCONFIG_SNR_HEADER_TYPE;
	snr_tlv->len = wlan_cpu_to_le16(SIZEOF_VALUE(snr_header_tlv));
	snr_tlv->SNRValue = 40;
	cmd_len += sizeof(tlv) + snr_tlv->len;

	start_later_tlv = (start_later_header_tlv *)(mrvl_cmd->buf +
						     mrvl_header_len + cmd_len);
	start_later_tlv->type = MLANCONFIG_START_LATER_HEADER_TYPE;
	start_later_tlv->len =
		wlan_cpu_to_le16(SIZEOF_VALUE(start_later_header_tlv));
	start_later_tlv->StartLaterValue = 0;
	cmd_len += sizeof(tlv) + start_later_tlv->len;

	buf_len = MRVDRV_SIZE_OF_CMD_BUFFER;
	cmd->size = cmd_len - BUF_HEADER_SIZE;

	/* Send collective command */
	wifidir_ioctl(ifname, mrvl_cmd, &cmd_len, buf_len, mrvl_header_len);

	/* check the response */
	cmd->cmd_code = wlan_le16_to_cpu(cmd->cmd_code);
	cmd->size = wlan_le16_to_cpu(cmd->size);
	cmd->cmd_code &= ~WIFIDIRCMD_RESP_CHECK;
	ret = wlan_le16_to_cpu(cmd->result);

	FREE(buf);
	return ret;
}

/* mwu_set_user_scan
 *
 * ifname       - interface name
 * chan_list    - list of scan channel, pls note that caller should init the
 *                unused array elements to 0
 * ssid         - ssid to direct probe req (e.g. WC ssid like DIRECT-*, etc)
 *
 * return       MWU_ERR_SUCCESS if success
 *              MWU_ERR_COM if fail
 */
int mwu_set_user_scan(char *ifname, int chan_list[MWU_IOCTL_USER_SCAN_CHAN_MAX],
		      char *ssid)
{
	mwu_ioctl_user_scan_cfg scan_req;
	unsigned char *buf = NULL, *temp = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	int i = 0, ret = MWU_ERR_SUCCESS, mrvl_header_len = 0;

	memset(&scan_req, 0x00, sizeof(scan_req));
	for (i = 0; i < MWU_IOCTL_USER_SCAN_CHAN_MAX; ++i) {
		scan_req.chan_list[i].chan_number = chan_list[i];
		scan_req.chan_list[i].scan_type = 1;
		scan_req.chan_list[i].scan_time = 10;
		if (scan_req.chan_list[i].chan_number > 14)
			scan_req.chan_list[i].radio_type = 1; /* A Band */
		else
			scan_req.chan_list[i].radio_type = 0; /* BG Band */
	}
	if (ssid != NULL) {
		strncpy(scan_req.ssid_list[0].ssid,
			strlen(ssid) ? ssid : DEFAULT_SSID, MAX_SSID_LEN); /* SSID
									    */
		scan_req.ssid_list[0].max_len = strlen(ssid) ? 0x00 /* Specific
								       scan */
							       :
							       0xFF;
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
	ret = mwu_privcmd(ifname, (u8 *)mrvl_cmd);

	if (buf)
		free(buf);
	return ret;
}

int mwu_set_ap_deauth(char *ifname, unsigned char *sta_mac)
{
	mrvl_priv_cmd *mrvl_cmd = NULL;
	deauth_param *param;
	u8 *buffer = NULL, *pos = NULL;
	struct ifreq ifr;
	int ret = MWU_ERR_SUCCESS;
	u16 len = 0, mrvl_header_len = 0;

	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_AP_DEAUTH);
	len = sizeof(mrvl_priv_cmd) + mrvl_header_len + sizeof(deauth_param);

	buffer = (u8 *)malloc(len);
	if (!buffer) {
		ERR("Failed to allocate buffer\n");
		return MWU_ERR_COM;
	}
	memset(buffer, 0, len);
	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	mrvl_cmd->buf = buffer + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len = len - sizeof(mrvl_priv_cmd);

	pos = mrvl_cmd->buf;
	strncpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += strlen(CMD_NXP);
	strncpy((char *)pos, PRIV_CMD_AP_DEAUTH, strlen(PRIV_CMD_AP_DEAUTH));
	param = (deauth_param *)(buffer + sizeof(mrvl_priv_cmd) +
				 mrvl_header_len);

	memset(&ifr, 0, sizeof(ifr));

	/* Assign 0 (reserved) to IEEE reason code. */
	param->reason_code = 0;
	memcpy(&param->mac_addr, sta_mac, ETH_ALEN);

	ifr.ifr_data = (void *)mrvl_cmd;

	ret = send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD);

	FREE(buffer);
	return ret;
}

/* This function is required when the peer_mac_addr in connptr->romData in FW
   needs to be updated for the purpose of key mapping against the peer_mac in FW
*/
#if 0 // the host command 253 isn't yet supported
int mwu_add_nan_peer(struct mwu_iface_info *cur_if)
{
    mrvl_priv_cmd *mrvl_cmd = NULL;
    unsigned char *buf = NULL, *pos = NULL;
    mrvl_cmd_head_buf *cmd = NULL;
    int ret = MWU_ERR_SUCCESS;
    host_cmd_sta_add_cmd_hdr *hdr;
    u16 cmd_len = 0, buf_len = 0, mrvl_header_len = 0;

    buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
    if (buf == NULL) {
        ERR("allocate memory for hostcmd failed");
        return MWU_ERR_NOMEM;
    }


    /* prepare the bgscan command */
    memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
    mrvl_header_len += strlen(CMD_NXP) + strlen(PRIV_CMD_HOSTCMD);
    mrvl_cmd = (mrvl_priv_cmd *)buf;
    mrvl_cmd->buf = buf + sizeof(mrvl_priv_cmd);
    mrvl_cmd->used_len = 0;
    mrvl_cmd->total_len = MRVDRV_SIZE_OF_CMD_BUFFER - mrvl_header_len - BUF_HEADER_SIZE;
    pos = mrvl_cmd->buf;
    /* Copy NXP command string */
    strncpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
    pos += (strlen(CMD_NXP));
    /* Insert command string*/
    strncpy((char *)pos, PRIV_CMD_HOSTCMD, strlen(PRIV_CMD_HOSTCMD));

    cmd_len = sizeof(mrvl_cmd_head_buf);
    cmd = (mrvl_cmd_head_buf *) ((mrvl_cmd->buf) + mrvl_header_len);
    cmd->cmd_code = HostCmd_CMD_STA_ADD;
    cmd->seq_num = 0;
    cmd->result = 0;

    hdr = (host_cmd_sta_add_cmd_hdr*)(cmd->cmd_data);
    memset(hdr, 0, sizeof(host_cmd_sta_add_cmd_hdr));
    hdr->Action = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
    hdr->add = wlan_cpu_to_le32(HostCmd_ACT_GEN_SET);;

    memcpy(hdr->peerMacAddr,
           cur_if->pnan_info->ndc_info[0].ndl_info[0].ndp_info[0].peer_ndi,
           MAC_ADDR_LENGTH);

    cmd_len += sizeof(host_cmd_sta_add_cmd_hdr);

    buf_len = MRVDRV_SIZE_OF_CMD_BUFFER;
    cmd->size = cmd_len - BUF_HEADER_SIZE;

    /* Send collective command */
    if(WIFIDIR_ERR_SUCCESS != wifidir_ioctl(cur_if->ifname, mrvl_cmd, &cmd_len, buf_len, mrvl_header_len))
    {
        ret = WIFIDIR_ERR_COM;
        ERR("Security install : IOCTL ERR in sending peer_mac to firmware\n");
    }

    /* check the response */
    cmd->cmd_code = wlan_le16_to_cpu(cmd->cmd_code);
    cmd->size = wlan_le16_to_cpu(cmd->size);
    cmd->cmd_code &= ~WIFIDIRCMD_RESP_CHECK;
    ret = wlan_le16_to_cpu(cmd->result);

    ERR("Security install : added the NAN peer_mac to firmware\n");

    FREE(buf);
    return ret;
}
#endif

/*
 * TODO :
 * Currently this function sets key of type AES ; it should be set of type
 * CMAC_AES. Parameters :: cur_if :     Pointer to current interface key_mat :
 * pointer to key material structure which will be updated and sent to ffirmware
 * pdata_buf:   pointer to buffer which contains tk, tk_len ,peer MAC address
 * and key_index, pdata_buf is used to update key_mat
 */
int mwu_key_material(struct mwu_iface_info *cur_if, KEY_MATERIAL *key_mat,
		     encrypt_key *pdata_buf)
{
	int ret = 0;
	encrypt_key *pkey = (encrypt_key *)pdata_buf;
	mrvl_cmd_head_buf *cmd = NULL;
	unsigned char *buffer = NULL, *pos = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u16 mrvl_header_len =
		strlen(CMD_NXP) + strlen(NAN_PARAMS_KEY_MATERIAL_CMD);

	u16 len = 0, cmd_len = 0, buf_len = 0;

	len = sizeof(KEY_MATERIAL) + mrvl_header_len + sizeof(mrvl_priv_cmd) +
	      sizeof(mrvl_cmd_head_buf);

	buffer = (unsigned char *)malloc(len);
	if (buffer == NULL) {
		ERR("Failed to allocate buffer\n");
		return MWU_ERR_COM;
	}
	memset(buffer, 0, len);

	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	mrvl_cmd->buf = buffer + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len = len - sizeof(mrvl_priv_cmd);

	pos = mrvl_cmd->buf;
	strncpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += strlen(CMD_NXP);
	strncpy((char *)pos, PRIV_CMD_HOSTCMD, strlen(PRIV_CMD_HOSTCMD));
	pos += strlen(PRIV_CMD_HOSTCMD);

	cmd_len = sizeof(mrvl_cmd_head_buf);
	cmd = (mrvl_cmd_head_buf *)(mrvl_cmd->buf + mrvl_header_len);
	cmd->cmd_code = HostCmd_CMD_KEY_MATERIAL;
	cmd->seq_num = 0;
	cmd->result = 0;
	key_mat = (KEY_MATERIAL *)cmd->cmd_data;

	key_mat->action = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
	key_mat->key_param_set.type = wlan_cpu_to_le16(TLV_TYPE_KEY_PARAM_V2);
	memcpy(key_mat->key_param_set.mac_addr, pkey->mac_addr,
	       MLAN_MAC_ADDR_LENGTH);

	key_mat->key_param_set.key_idx = 0; ///*pkey->key_index &*/
					    ///KEY_INDEX_MASK;
	/* TODO how to get pkey->key_index */

	// key_mat->key_param_set.key_type = KEY_TYPE_ID_AES_CMAC;
	key_mat->key_param_set.key_type = KEY_TYPE_ID_AES;

	key_mat->key_param_set.key_info = KEY_INFO_ENABLE_KEY;
	/* TODO how to get pkey->key_info  */

	key_mat->key_param_set.key_info = KEY_INFO_CMAC_AES_KEY;
	memset(key_mat->key_param_set.cmac_aes.ipn, 0, SEQ_MAX_SIZE);

	/* TODO how to get aes.pin */
	key_mat->key_param_set.key_info = KEY_INFO_ENABLE_KEY;

	/*
	if(pkey->key_flags & KEY_FLAG_GROUP_KEY)
		pkey_material->key_param_set.key_info |= KEY_INFO_MCAST_KEY;
	else
		pkey_material->key_param_set.key_info |= KEY_INFO_UCAST_KEY;
	if(pkey->key_flags & KEY_FLAG_SET_TX_KEY)
		pkey_material->key_param_set.key_info |= KEY_INFO_TX_KEY |
    KEY_INFO_RX_KEY; else pkey_material->key_param_set.key_info |=
    KEY_INFO_RX_KEY;


    key_mat->key_param_set.key_info &= ~(wlan_cpu_to_le16(KEY_INFO_MCAST_KEY));
    key_mat->key_param_set.key_info |=
    wlan_cpu_to_le16(KEY_INFO_AES_MCAST_IGTK);
*/
	key_mat->key_param_set.key_info = 62;
	INFO("Set CMAC AES Key pkey->key_len %d\n", pkey->key_len);

	/* Copy encryption key from pkey in cmac_aes.key*/
	key_mat->key_param_set.cmac_aes.key_len =
		wlan_cpu_to_le16(pkey->key_len);
	memcpy(key_mat->key_param_set.cmac_aes.key, pkey->key, pkey->key_len);

	key_mat->key_param_set.length =
		wlan_cpu_to_le16(KEY_PARAMS_FIXED_LEN + sizeof(cmac_aes_param));
	cmd->size = wlan_cpu_to_le16(S_DS_GEN + sizeof(KEY_MATERIAL) +
				     sizeof(cmd->buf_size));

	INFO("Set CMAC AES Key\n");
	cmd_len += sizeof(KEY_MATERIAL);
	ret = nan_cmdbuf_send(cur_if, mrvl_cmd, mrvl_header_len);

	buf_len = cmd_len;
	cmd->size = cmd_len;

	mwu_hexdump(MSG_INFO, "CMD: ", (unsigned char *)mrvl_cmd, len);
	/* Send collective command */
	wifidir_ioctl(cur_if->ifname, mrvl_cmd, &cmd_len, buf_len,
		      mrvl_header_len);

	/* check the response */
	cmd->cmd_code = wlan_le16_to_cpu(cmd->cmd_code);
	cmd->size = wlan_le16_to_cpu(cmd->size);
	cmd->cmd_code &= ~WIFIDIRCMD_RESP_CHECK;
	ret = wlan_le16_to_cpu(cmd->result);

	FREE(buffer);
	return ret;
}

int mwu_get_ap_sta_list(char *ifname, struct AP_STA_LIST *list)
{
	struct ifreq ifr;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	u8 *buffer = NULL, *pos = NULL;
	u16 len = 0, mrvl_header_len = 0;
	int ret = MWU_ERR_SUCCESS;

	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_GET_STA_LIST);
	len = sizeof(mrvl_priv_cmd) + mrvl_header_len +
	      sizeof(struct AP_STA_LIST);
	buffer = (u8 *)malloc(len);
	if (!buffer) {
		ERR("Failed to allocate buffer\n");
		return MWU_ERR_COM;
	}
	memset(buffer, 0, len);
	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	mrvl_cmd->buf = buffer + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len = len - sizeof(mrvl_priv_cmd);

	pos = mrvl_cmd->buf;
	strncpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += strlen(CMD_NXP);
	strncpy((char *)pos, PRIV_CMD_GET_STA_LIST,
		strlen(PRIV_CMD_GET_STA_LIST));

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifru.ifru_data = (void *)mrvl_cmd;
	ret = send_ifreq_ioctl(ifname, &ifr, MRVLPRIVCMD);
	if (ret == MWU_ERR_SUCCESS)
		memcpy(list, buffer + sizeof(mrvl_priv_cmd) + mrvl_header_len,
		       sizeof(struct AP_STA_LIST));

	FREE(buffer);
	return ret;
}

int mwu_get_ie_index(char *ifname, int type, short *ie_idx)
{
	struct mwu_iface_info *found = NULL;

	if (!ifname || !ie_idx)
		return MWU_ERR_INVAL;

	found = mwu_get_interface(ifname, GENERIC_MODULE_ID);
	if (!found) {
		ERR("Cannot find interface info for %s.", ifname);
		return MWU_ERR_INVAL;
	}

	*ie_idx = found->ie_idx[type];

	return MWU_ERR_SUCCESS;
}

int mwu_set_ie_index(char *ifname, int type, short ie_idx)
{
	struct mwu_iface_info *found = NULL;

	if (!ifname)
		return MWU_ERR_INVAL;

	found = mwu_get_interface(ifname, GENERIC_MODULE_ID);
	if (!found) {
		ERR("Cannot find interface info for %s.", ifname);
		return MWU_ERR_INVAL;
	}

	found->ie_idx[type] = ie_idx;

	return MWU_ERR_SUCCESS;
}
#if 0
int mwu_clear_ie(char *ifname, int type)
{
    short ie_idx = -1;

    if (mwu_get_ie_index(ifname, type, &ie_idx) != MWU_ERR_SUCCESS)
        return MWU_ERR_INVAL;

    ERR ("CLEAR IE Type: %d Index %d", type, ie_idx);

    if (ie_idx != -1) {
        mwu_ie_config(ifname, CLEAR_WPS_IE, (short int *)&ie_idx, type);
        ie_idx = -1;
        if (mwu_set_ie_index(ifname, type, ie_idx) != MWU_ERR_SUCCESS)
            return MWU_ERR_INVAL;
    }

    return MWU_ERR_SUCCESS;
}

int mwu_set_ie(char *ifname, int ie_type, int flag)
{
    short ie_idx = -1;
    const char *ie_type_str = mwu_ie_type_to_str(ie_type);

    if (mwu_get_ie_index(ifname, ie_type, &ie_idx) != MWU_ERR_SUCCESS) {
        ERR("Failed to get %s IE index.", ie_type_str);
        return MWU_ERR_INVAL;
    }
    if (ie_idx == -1) {
        if (mwu_ie_config(ifname, GET_WPS_IE, &ie_idx, ie_type) != MWU_ERR_SUCCESS) {
            ERR("Failed to get %s IE.", ie_type_str);
            return MWU_ERR_COM;
        }
        if (mwu_set_ie_index(ifname, ie_type, ie_idx) != MWU_ERR_SUCCESS) {
            ERR("Failed to set %s IE index.\n", ie_type_str);
            return MWU_ERR_INVAL;
        }
    }
    INFO("Setting %s IE at index %d", ie_type_str, ie_idx);
    if (mwu_ie_config(ifname, flag, &ie_idx, ie_type) != MWU_ERR_SUCCESS) {
        ERR("Failed to set %s IE.", ie_type_str);
        return MWU_ERR_COM;
    }

    return MWU_ERR_SUCCESS;
}
#endif
#ifdef WEXT_SUPPORT
int mwu_get_range(char *ifname)
{
	struct iw_range *range = NULL;
	struct iwreq iwr;
	int buflen = 0;
	int we_version_compiled = -1;

	buflen = sizeof(struct iw_range) + 500;
	range = (struct iw_range *)malloc(buflen);
	if (!range)
		return we_version_compiled;
	memset(range, 0, buflen);

	memset(&iwr, 0, sizeof(iwr));
	iwr.u.data.pointer = (caddr_t)range;
	iwr.u.data.length = buflen;

	if (send_iwreq_ioctl(ifname, &iwr, SIOCGIWRANGE) == MWU_ERR_SUCCESS)
		we_version_compiled = range->we_version_compiled;

	FREE(range);

	return we_version_compiled;
}
#endif

int mwu_get_bandcfg(char *ifname, int *rf_bands)
{
	unsigned char *buf = NULL, *temp = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;
	struct mrvl_priv_cmd_bandcfg *bandcfg = NULL;
	int ret = MWU_ERR_SUCCESS, mrvl_header_len = 0;

	buf = (unsigned char *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	if (buf == NULL)
		return MWU_ERR_NOMEM;

	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	mrvl_header_len = strlen(CMD_NXP) + strlen(PRIV_CMD_BANDCFG);
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
	strncpy((char *)temp, PRIV_CMD_BANDCFG, strlen(PRIV_CMD_BANDCFG));
	temp += (strlen(PRIV_CMD_BANDCFG));

	/* Perform IOCTL */
	ret = mwu_privcmd(ifname, (u8 *)mrvl_cmd);

	/* Process result */
	bandcfg = (struct mrvl_priv_cmd_bandcfg *)(mrvl_cmd->buf);

#define BAND_A (1U << 2)
#define BAND_AN (1U << 4)

	INFO("band cfg %d", bandcfg->config_bands);
	if (((bandcfg->config_bands & BAND_A) ||
	     (bandcfg->config_bands & BAND_AN)))
		*rf_bands = 3;
	else
		*rf_bands = 1;

	if (buf)
		free(buf);
	return ret;
}
#if 0
/* convinience function to set both beacon and probe response IEs to session
 * active and inactive */
int mwu_set_beacon_probe_resp_ie(struct mwu_iface_info *cur_if, int flag)
{
     if (mwu_set_ie(cur_if->ifname, IE_CFG_AP_BCN, flag) != MWU_ERR_SUCCESS) {
         return MWU_ERR_COM;
     }

     if (mwu_set_ie(cur_if->ifname, IE_CFG_PROBE_RESP, flag) != MWU_ERR_SUCCESS) {
         return MWU_ERR_COM;
     }
     return MWU_ERR_SUCCESS;
}

/* convinience function to clear both beacon and probe response IEs to session
 * active and inactive */
int mwu_clear_beacon_probe_resp_ie(struct mwu_iface_info *cur_if)
{
     if (mwu_clear_ie(cur_if->ifname, IE_CFG_AP_BCN) != MWU_ERR_SUCCESS) {
         return MWU_ERR_COM;
     }

     if (mwu_clear_ie(cur_if->ifname, IE_CFG_PROBE_RESP) != MWU_ERR_SUCCESS) {
         return MWU_ERR_COM;
     }
     return MWU_ERR_SUCCESS;
}
#endif
/* convinience funciton to prepare the MRVL_CMD - Prepends MRVL_CMD and cmd str
 * to buffer, populates the command header length*/
int prepare_buffer(u8 *buffer, char *cmd, u16 *mrvl_header_len)
{
	u8 *pos = NULL;
	mrvl_priv_cmd *mrvl_cmd = NULL;

	memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	/* Fill up buffer */
	*mrvl_header_len = strlen(CMD_NXP) + strlen(cmd);
	mrvl_cmd = (mrvl_priv_cmd *)buffer;
	mrvl_cmd->buf = buffer + sizeof(mrvl_priv_cmd);
	mrvl_cmd->used_len = 0;
	mrvl_cmd->total_len =
		MRVDRV_SIZE_OF_CMD_BUFFER - *mrvl_header_len - BUF_HEADER_SIZE;

	pos = mrvl_cmd->buf;
	strncpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += (strlen(CMD_NXP));

	/* Insert command */
	strncpy((char *)pos, (char *)cmd, strlen(cmd));
	pos += (strlen(cmd));

	return MWU_ERR_SUCCESS;
}

#if 0
/* Convinience function to set firmware managed IEs (P2P and WPS) for P2P
 * operation
 *
 * params
 * cur_if           pointer to mwu_iface_info struct
 * ie_type          type of IE to set (can be IE_CFG_WIFIDIR or IE_CFG_WIFIDIR_WPS)
 * buf              pointer to buffer data
 * buf_len          length of buffer to set
 * mrvl_header_len  length of header (usually MRVL_CMD + <command str>)
 *
 * return
 * MWU_ERR_SUCCESS  success
 * MWU_ERR_COM      fail
 *
 * */
int mwu_set_fw_managed_ie(struct mwu_iface_info *cur_if, int ie_type, u8* buf,
                          u16 buf_len, u16 mrvl_header_len)
{
    u16 complete_buf_len = 0, ie_cmd_header_len = 0;
    short ie_index = -1;
    const char *ie_type_str = mwu_ie_type_to_str(ie_type);
    int ret = 0;

    ie_cmd_header_len = sizeof(mrvl_priv_cmd) + mrvl_header_len
                        + sizeof(tlvbuf_custom_ie) + sizeof(custom_ie);

    complete_buf_len = buf_len + ie_cmd_header_len;
    INFO("Configure IE %s", ie_type_str);
    mwu_hexdump(DEBUG_WLAN, "Download IE", buf, complete_buf_len);

    if (mwu_get_ie_index(cur_if->ifname, ie_type, &ie_index)
        != MWU_ERR_SUCCESS) {
        ERR("ERR:Failed to get %s IE index\n", ie_type_str);
        return ret;
    }
    if (ie_index == -1) {
        ret = mwu_ie_config(cur_if->ifname, GET_WPS_IE, &ie_index, ie_type);
        if (ret != WPS_STATUS_SUCCESS) {
            ERR("ERR:Could not get free IE buffer\n");
            return ret;
        }
        if (mwu_set_ie_index(cur_if->ifname, ie_type, ie_index)
            != MWU_ERR_SUCCESS) {
            ERR("ERR:Failed to set %s IE index.\n", ie_type_str);
            return ret;
        }
    }

    ret = wifidir_ie_config(cur_if, (u16 *)&ie_index, mrvl_header_len,
            buf_len, buf);

    if (ret != WPS_STATUS_SUCCESS) {
        ERR("ERR:Could not set IE parameters\n");
        return MWU_ERR_COM;
    }
    return MWU_ERR_SUCCESS;
}
#endif
/**
 *  @brief Creates get_fw_info request and send to driver
 *
 *
 *  @param pfw_info Pointer to FW information structure
 *  @return         0--success, otherwise fail
 */
int mwu_get_fw_info(char *ifname, fw_info *pfw_info)
{
	struct ifreq ifr;
	s32 sockfd;

#define UAP_FW_INFO 4
	memset(pfw_info, 0, sizeof(fw_info));
	pfw_info->subcmd = UAP_FW_INFO;
	pfw_info->action = 0;
	/* Open socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ERR("ERR:Cannot open socket\n");
		return MWU_ERR_COM;
	}
	/* Initialize the ifr structure */
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_ifrn.ifrn_name, ifname, IFNAMSIZ - 1);
	ifr.ifr_ifru.ifru_data = (void *)pfw_info;
	/* Perform ioctl */
	if (ioctl(sockfd, UAPPRIVCMD, &ifr)) {
		ERR("ERR: get fw info failed\n");
		close(sockfd);
		return MWU_ERR_COM;
	}
	/* Close socket */
	close(sockfd);
	return MWU_ERR_SUCCESS;
}

int mwu_apcmd_sys_reset(struct mwu_iface_info *cur_if)
{
	struct ifreq ifr;
	s32 sockfd;
#define UAP_BSS_RESET 2
#define UAP_BSS_CTRL (SIOCDEVPRIVATE + 4)
	u32 data = (u32)UAP_BSS_RESET;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ERR("Can not open socket");
		return MWU_ERR_COM;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_ifrn.ifrn_name, cur_if->ifname, IFNAMSIZ - 1);
	ifr.ifr_ifru.ifru_data = (void *)&data;

	errno = 0;
	if (ioctl(sockfd, UAP_BSS_CTRL, &ifr)) {
		close(sockfd);
		ERR("Could not RESET AP BSS");
		return MWU_ERR_COM;
	}
	close(sockfd);
	return MWU_ERR_SUCCESS;
}
