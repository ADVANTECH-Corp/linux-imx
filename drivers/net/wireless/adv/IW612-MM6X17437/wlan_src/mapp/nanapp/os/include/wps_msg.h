/** @file wps_msg.h
 *  @brief This file contains definition for WPS Messages.
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

#ifndef WPS_MSG_H
#define WPS_MSG_H

#include <stdio.h>
#include <net/if.h> /* IFNAMSIZ */
#include <sys/time.h> /* struct timeval */

#include "wps_def.h"
//#include "wifidir/wifidir.h" /* for struct wifidir_cfg */
#include "mwu_if_manager.h"

#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__ __attribute__((packed))
#endif

/** enum : WPS attribute type */
typedef enum {
	SC_AP_Channel = 0x1001,
	SC_Association_State = 0x1002,
	SC_Authentication_Type = 0x1003,
	SC_Authentication_Type_Flags = 0x1004,
	SC_Authenticator = 0x1005,
	SC_Config_Methods = 0x1008,
	SC_Configuration_Error = 0x1009,
	SC_Confirmation_URL4 = 0x100A,
	SC_Confirmation_URL6 = 0x100B,
	SC_Connection_Type = 0x100C,
	SC_Connection_Type_Flags = 0x100D,
	SC_Credential = 0x100E,
	SC_Device_Name = 0x1011,
	SC_Device_Password_ID = 0x1012,
	SC_E_Hash1 = 0x1014,
	SC_E_Hash2 = 0x1015,
	SC_E_SNonce1 = 0x1016,
	SC_E_SNonce2 = 0x1017,
	SC_Encrypted_Settings = 0x1018,
	SC_Encryption_Type = 0X100F,
	SC_Encryption_Type_Flags = 0x1010,
	SC_Enrollee_Nonce = 0x101A,
	SC_Feature_ID = 0x101B,
	SC_Identity = 0X101C,
	SC_Identity_Proof = 0X101D,
	SC_Key_Wrap_Authenticator = 0X101E,
	SC_Key_Identifier = 0X101F,
	SC_MAC_Address = 0x1020,
	SC_Manufacturer = 0x1021,
	SC_Message_Type = 0x1022,
	SC_Model_Name = 0x1023,
	SC_Model_Number = 0x1024,
	SC_Network_Index = 0x1026,
	SC_Network_Key = 0x1027,
	SC_Network_Key_Index = 0x1028,
	SC_New_Device_Name = 0x1029,
	SC_New_Password = 0x102A,
	SC_OOB_Device_Password = 0X102C,
	SC_OS_Version = 0X102D,
	SC_Power_Level = 0X102F,
	SC_PSK_Current = 0x1030,
	SC_PSK_Max = 0x1031,
	SC_Public_Key = 0x1032,
	SC_Radio_Enabled = 0x1033,
	SC_Reboot = 0x1034,
	SC_Registrar_Current = 0x1035,
	SC_Registrar_Established = 0x1036,
	SC_Registrar_List = 0x1037,
	SC_Registrar_Max = 0x1038,
	SC_Registrar_Nonce = 0x1039,
	SC_Request_Type = 0x103A,
	SC_Response_Type = 0x103B,
	SC_RF_Band = 0X103C,
	SC_R_Hash1 = 0X103D,
	SC_R_Hash2 = 0X103E,
	SC_R_SNonce1 = 0X103F,
	SC_R_SNonce2 = 0x1040,
	SC_Selected_Registrar = 0x1041,
	SC_Serial_Number = 0x1042,
	SC_Simple_Config_State = 0x1044,
	SC_SSID = 0x1045,
	SC_Total_Networks = 0x1046,
	SC_UUID_E = 0x1047,
	SC_UUID_R = 0x1048,
	SC_Vendor_Extension = 0x1049,
	SC_Version = 0x104A,
	SC_X_509_Certificate_Request = 0x104B,
	SC_X_509_Certificate = 0x104C,
	SC_EAP_Identity = 0x104D,
	SC_Message_Counter = 0x104E,
	SC_Public_Key_Hash = 0x104F,
	SC_Rekey_Key = 0x1050,
	SC_Key_Lifetime = 0x1051,
	SC_Permitted_Config_Methods = 0x1052,
	SC_SelectedRegistrarConfigMethods = 0x1053,
	SC_Primary_Device_Type = 0x1054,
	SC_Secondary_Device_Type_List = 0x1055,
	SC_Portable_Device = 0x1056,
	SC_AP_Setup_Locked = 0x1057,
	SC_Application_List = 0x1058,
	SC_EAP_Type = 0x1059,
	SC_Initialization_Vector = 0x1060,
	SC_Key_Provided_Auto = 0x1061,
	SC_8021x_Enabled = 0x1062,
	SC_App_Session_key = 0x1063,
	SC_WEP_Transmit_Key = 0x1064,
	SC_SMPBC_Entry_Acceptable = 0x106D,
	SC_SMPBC_Registration_Ready = 0x106E,
	SC_Dummy_Attribute = 0x2345,
	SC_IPv4_Config_Methods = 0x1073,
	/** New attributes for IPv4 WSC Auto assignment */
	SC_Registrar_Ipv4_Address = 0x106F,
	SC_Ipv4_Subnet_Mask = 0x1070,
	SC_Enrollee_Ipv4_Address = 0x1071,
	SC_Available_Ipv4_Submask_List = 0x1072,
} SIMPLE_CONFIG_ATTRIBUTE;

/** enum : WPS WFA Vendor Ext Subelement type */
typedef enum {
	SC_Version2 = 0x00,
	SC_AuthorizedMACs = 0x01,
} SIMPLE_CONFIG_WFA_VENDOR_SUBELE;

/** enum : WPS message type */
typedef enum _WPS_MESSAGE_TYPE {
	WPS_BEACON = 0x01,
	WPS_REQUEST,
	WPS_RESPONSE,
	WPS_M1,
	WPS_M2,
	WPS_M2D,
	WPS_M3,
	WPS_M4,
	WPS_M5,
	WPS_M6,
	WPS_M7,
	WPS_M8,
	WPS_ACK,
	WPS_NACK,
	WPS_DONE
} WPS_MESSAGE_TYPE;

/** enum : WPS op code */
typedef enum {
	WPS_Start = 0x1,
	WPS_Ack,
	WPS_Nack,
	WPS_Msg,
	WPS_Done,
	WPS_Frag_Ack
} WPS_OP_CODE;

/** enum : WPS state */
typedef enum {
	WPS_STATE_A = 0,
	WPS_STATE_B,
	WPS_STATE_C,
	WPS_STATE_D,
	WPS_STATE_E,
	WPS_STATE_F,
	WPS_STATE_G,
	WPS_STATE_H,
	WPS_STATE_I
} WPS_STATE;

typedef struct _EAP_FRAME_HEADER {
	/** code */
	u8 code; /* 1:request 2:response */
	/** identifier */
	u8 identifier;
	/** length */
	u16 length;
	/** type */
	u8 type; /* 254 for WPS */
	/** vendor id */
	u8 vendor_id[3]; /* 00 37 2A */
	/** vendor type */
	u8 vendor_type[4]; /* 00 00 00 01 */
	/** op code */
	u8 op_code;
	/** flags */
	u8 flags;
} __ATTRIB_PACK__ EAP_FRAME_HEADER, *PEAP_FRAME_HEADER;

typedef struct _EAPOL_FRAME_HEADER {
	/** Protocol version */
	u8 protocol_version;
	/** Packet type */
	u8 packet_type;
	/** Length */
	u16 length;
} __ATTRIB_PACK__ EAPOL_FRAME_HEADER, *PEAPOL_FRAME_HEADER;

typedef struct _TLV_DATA_HEADER {
	/** TLV type */
	u16 type;
	/** TLV length */
	u16 length;
} __ATTRIB_PACK__ TLV_DATA_HEADER, *PTLV_DATA_HEADER;
typedef struct _SUBELE_DATA_HEADER {
	/** SUBELE type */
	u8 type;
	/** SUBELE length */
	u8 length;
} __ATTRIB_PACK__ SUBELE_DATA_HEADER, *PSUBELE_DATA_HEADER;

/** size of PBC enrollee data */
#define SZ_WPS_PBC_ENROLLEE_INFO sizeof(struct WPS_PBC_ENROLLEE_INFO)

/** External Registrsar PIN faliure count */
#define ER_DEF_PIN_FAILURE_COUNT 3
/** External Registrsar PIN faliure count */
#define ER_MIN_PIN_FAILURE_COUNT 1
/** External Registrsar PIN faliure count */
#define ER_MAX_PIN_FAILURE_COUNT 10
/** AP setup locked default timeout */
#define WPS_AP_SETUP_LOCKED_DEF_TIMEOUT 1
/** AP setup locked maximum timeout */
#define WPS_AP_SETUP_LOCKED_MAX_TIMEOUT 1440

/** Max NACK error count */
#define MAX_NACK_ERROR_COUNT 10

/** R-hash faliure count */
#define MAX_R_HASH_FAILURE_COUNT 3
/** R-hash timeout */
#define WPS_R_HASH_TIMEOUT 60
/** AP setup locked timeout */
#define WPS_AP_SETUP_LOCKED_TIMEOUT 600

/** Max authorized enrollee count */
#define MAX_AUTH_ENROLLEE_COUNT 4

/** Max NACK error count */
#define MAX_NACK_ERROR_COUNT 10

typedef struct _SIMPLE_CONFIG_TLV {
	/** Attribute Type */
	u16 AttributeType;
	/** Data Length */
	u16 DataLength;
	/** Data */
	u8 DATA[1];
} __ATTRIB_PACK__ SIMPLE_CONFIG_TLV, *PSIMPLE_CONFIG_TLV;

typedef struct _PRIMARY_DEVICE_TYPE_MSG {
	/** Type */
	u16 type;
	/** Length */
	u16 length;
	/** Catagory Id */
	u16 category_id;
	/** OUI Id */
	u8 oui_id[4];
	/** Subcategory Id */
	u16 sub_category_id;
} __ATTRIB_PACK__ PRIMARY_DEVICE_TYPE_MSG, *PPRIMARY_DEVICE_TYPE_MSG;

/** enum: WPS message sent */
typedef enum {
	WPS_EAPOL_MSG_START = 0x00,
	WPS_EAP_MSG_REQUEST_IDENTITY,
	WPS_EAP_MSG_RESPONSE_IDENTITY,
	WPS_EAP_START,
	WPS_EAP_M1,
	WPS_EAP_M2,
	WPS_EAP_M2D,
	WPS_EAP_M3,
	WPS_EAP_M4,
	WPS_EAP_M5,
	WPS_EAP_M6,
	WPS_EAP_M7,
	WPS_EAP_M8,
	WPS_MSG_ACK,
	WPS_MSG_NACK,
	WPS_MSG_DONE,
	WPS_EAP_MSG_FAIL,
	WPS_MSG_FRAG_ACK,
	WPS_EAP_UNKNOWN
} WPS_MESSAGE_SENT;

/** EAP Type: Identity */
#define EAP_TYPE_IDENTITY 1
/** EAP Type: WPS */
#define EAP_TYPE_WPS 254
/** EAP Packet */
#define EAP_PACKET 0
/** EAP Request */
#define EAP_REQUEST 1
/** EAP Response */
#define EAP_RESPONSE 2
/** EAP Success */
#define EAP_SUCCESS 3
/** EAP Failure */
#define EAP_FAILURE 4
/** EAP WPS Frame header size */
#define SZ_EAP_WPS_FRAME_HEADER sizeof(EAP_FRAME_HEADER)
/** TLV header size */
#define SZ_TLV_HEADER sizeof(TLV_DATA_HEADER)
/** SUBELE header size */
#define SZ_SUBELE_HEADER sizeof(SUBELE_DATA_HEADER)
/** EAP message length size */
#define SZ_EAP_MESSAGE_LENGTH 2
/** WPS version */
#define WPS_VERSION_1DOT0 (0x10)
#define WPS_VERSION_2DOT0 (0x20)
#define WPS_VERSION WPS_VERSION_2DOT0
/** WPS Enrollee */
#define WPS_ENROLLEE 1
/** WPS Registrar */
#define WPS_REGISTRAR 2
/** WPS Extensions */
#define WPS_ADHOC_EXTENTION 3
/** WFD Extension */
#define WIFIDIR_ROLE 4

/**Pin generator value for display PIN */
#define WIFIDIR_AUTO_DISPLAY_PIN 3
/**Pin generator value for Enter PIN */
#define WIFIDIR_AUTO_ENTER_PIN 4
/** EAPOL start */
#define EAPOL_START 1

/** Authentication Type: open */
#define AUTHENTICATION_TYPE_OPEN 0x0001
/** Authentication Type: WPAPSK */
#define AUTHENTICATION_TYPE_WPAPSK 0x0002
/** Authentication Type: shared */
#define AUTHENTICATION_TYPE_SHARED 0x0004
/** Authentication Type: WPA */
#define AUTHENTICATION_TYPE_WPA 0x0008
/** Authentication Type: WPA2 */
#define AUTHENTICATION_TYPE_WPA2 0x0010
/** Authentication Type: WPA2PSK */
#define AUTHENTICATION_TYPE_WPA2PSK 0x0020

/** Authentication Type: WPA Mixed Mode */
#define AUTHENTICATION_TYPE_WPA_MIXED                                          \
	(AUTHENTICATION_TYPE_WPAPSK | AUTHENTICATION_TYPE_WPA2PSK)

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

/** Connection Type: ESS */
#define CONNECTION_TYPE_ESS 0x01
/** Connection Type: IBSS */
#define CONNECTION_TYPE_IBSS 0x02
/** Connection Type: ALL */
#define CONNECTION_TYPE_ALL 0x03

/** Config error : NO ERROR */
#define CONFIG_ERROR_NO_ERROR 0x0
/** Config error : NO ERROR */
#define CONFIG_ERROR_AP_SETUP_LOCKED 0xf

/** Primary device catagory: COMPUTER */
#define PRIMARY_DEV_CATEGORY_COMPUTER 0x01
/** Primary device subcatagory: PC */
#define PRIMARY_DEV_SUB_CATEGORY_PC 0x01

/** RF: 2.4G */
#define RF_24_G 0x01
/** RF: 5.0G */
#define RF_50_G 0x02

/* BG BAND */
#define RF_BAND_BG 0x0
/* A BAND */
#define RF_BAND_A 0x1
/* Scan specific band */
#define SCAN_SPECIFIED_BAND 0x80

/** Response type : Registrar */
#define RESP_TYPE_REGISTRAR 0x02
/** Response type : AP */
#define RESP_TYPE_AP 0x03
/** Response type : Notifier */
#define RESP_TYPE_NOTIFIER 0x04

/** Request type : Enrollee */
#define REQ_TYPE_ENROLLEE 0x00
/** Request type : Registrar */
#define REQ_TYPE_REGISTRAR 0x02

/** Device password: PIN ID */
#define DEVICE_PASSWORD_ID_PIN 0x0000
/** Device password: Push Button */
#define DEVICE_PASSWORD_PUSH_BUTTON 0x0004
/** Device password: User Specified */
#define DEVICE_PASSWORD_USER_SPECIFIED 0x0001
/** Device password: Registrar Specified */
#define DEVICE_PASSWORD_REG_SPECIFIED 0x0005
/** Device password: SMPBC */
#define DEVICE_PASSWORD_SMPBC 0x0006
/** Device password: NFC Connection Handover */
#define DEVICE_PASSWORD_NFC_CONN_HANDOVER 0x0007
/** Device password: Out of Band */
#define DEVICE_PASSWORD_OOB 0x0011

/** Size: WPS_NONCE */
#define WPS_NONCE_SIZE 16
/** Size: DH_PRIME_1536 */
#define SZ_DH_PRIME_1536 192
/** Size: Public Key  */
#define SZ_PUBLIC_KEY 192
/** Size:  Private key */
#define SZ_PRIVATE_KEY 192
/** Size:  DHKEY_SIZE */
#define SZ_DHKEY_SIZE 32
/** Size:  KDK */
#define SZ_KDK 32
/** Size:  KDF_TOTAL_BYTE */
#define SZ_KDF_TOTAL_BYTE 80
/** Size:  AUTH_KEY */
#define SZ_AUTH_KEY 32
/** Size:  WRAP_KEY */
#define SZ_KEY_WRAP_KEY 16
/** Size: EMSK */
#define SZ_EMSK 32
/** Size: HASH_X  */
#define SZ_HASH_X 16
/** Size: E_S_X */
#define SZ_E_S_X 16
/** Size: PSK_X */
#define SZ_PSK_X 16
/** Size: AUTHENTICATOR */
#define SZ_AUTHENTICATOR 8
/** Size: KWA */
#define SZ_KWA 8
/** Size: MANUFACTURE */
#define SZ_MANUFACTURE 64
/** Size: MODEL_NAME */
#define SZ_MODEL_NAME 32
/** Size: MODEL_NUMBER */
#define SZ_MODEL_NUMBER 32
/** Size: SERIAL_NUMBER */
#define SZ_SERIAL_NUMBER 32
/** Size: PRIMARY_DEVICE_TYPE */
#define SZ_PRIMARY_DEVICE_TYPE 8
/** Size: DEVICE_NAME */
#define SZ_DEVICE_NAME 32
/** Size: OS_VERSION */
#define SZ_OS_VERSION 4
/** Size: 128BIT_IV */
#define SZ_128BIT_IV 16
/** Size: HASH */
#define SZ_HASH 32
/** Size: E_SX */
#define SZ_E_SX 16
/** Size: PSKX */
#define SZ_PSKX 16
/** Size: E_HASHX */
#define SZ_E_HASHX 32
/** Size: WPS_PSK1 */
#define WPS_PSK1 1
/** Size: WPS_PSK2 */
#define WPS_PSK2 2
/** Size: 64_BITS */
#define SZ_64_BITS 8
/** Size: 128_BITS */
#define SZ_128_BITS 16

/** Size: Version2 */
#define SZ_VERSION2 1

/** Size: WPS_MSG04 */
#define WPS_MSG04 0x4
/** Size: WPS_MSG05 */
#define WPS_MSG05 0x5

/** PIN Walk time */
#define PIN_WALK_TIME 120 /* sec */
/** PBC Walk time */
#define PBC_WALK_TIME 120 /* sec */
/** WPS Registration Protocol Timeout */
#define WPS_REGISTRATION_TIME 120 /* sec */
/** Scan operation time out */
#define WPS_SCAN_TIMEOUT 10 /* sec */

/** EAP fail count */
#define MAX_EAP_FAIL_COUNT 2

/** WPS PBC/SMPBC Session Overlap Error */
#define WPS_SESSION_OVERLAP_ERROR 12

/** default WPS channel*/
#define WPS_DEFAULT_CHANNEL 6

int wps_eap_M1_frame_prepare(struct mwu_iface_info *cur_if);
int wps_eap_M1_frame_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_eap_M2_frame_prepare(struct mwu_iface_info *cur_if);
int wps_eap_M2_frame_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_eap_M2D_frame_prepare(struct mwu_iface_info *cur_if);
int wps_eap_M2D_frame_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_eap_M3_frame_prepare(struct mwu_iface_info *cur_if);
int wps_eap_M3_frame_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_eap_M4_frame_prepare(struct mwu_iface_info *cur_if);
int wps_eap_M4_frame_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_eap_M5_frame_prepare(struct mwu_iface_info *cur_if);
int wps_eap_M5_frame_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_eap_M6_frame_prepare(struct mwu_iface_info *cur_if);
int wps_eap_M6_frame_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_eap_M7_frame_prepare(struct mwu_iface_info *cur_if);
int wps_eap_M7_frame_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_eap_M8_frame_prepare(struct mwu_iface_info *cur_if);
int wps_eap_M8_frame_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_eap_response_identity_prepare(struct mwu_iface_info *cur_if);
int wps_eap_response_identity_process(struct mwu_iface_info *cur_if, u8 *, u16);
void wps_message_handler(struct mwu_iface_info *cur_if, u8 *, u8 *);
void wps_eapol_start_handler(struct mwu_iface_info *cur_if);
int wps_ack_message_prepare(struct mwu_iface_info *cur_if);
int wps_eap_fail_frame_prepare(struct mwu_iface_info *cur_if);
int wps_eap_frag_ack_frame_prepare(struct mwu_iface_info *cur_if);
int wps_send_next_fragment(struct mwu_iface_info *cur_if);
int wps_done_message_prepare(struct mwu_iface_info *cur_if);
int wps_done_message_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_eap_request_start_prepare(struct mwu_iface_info *cur_if);
int wps_eap_request_start_process(struct mwu_iface_info *cur_if, u8 *, u16);
u32 get_wps_pin(struct mwu_iface_info *cur_if);
int wps_eap_request_identity_prepare(struct mwu_iface_info *cur_if);
void wps_txTimer_handler(void *);
int wps_eapol_start_prepare(struct mwu_iface_info *cur_if);
int wps_nack_message_remap(struct mwu_iface_info *cur_if);
void wps_start_pbc_monitor_timer(struct mwu_iface_info *cur_if);
u16 wps_probe_request_device_password_id_parser(u8 *message, size_t size);
int wps_check_for_registrar_pbc_session_overlap(struct mwu_iface_info *cur_if);
void wps_start_registration_timer(struct mwu_iface_info *cur_if);
void wps_registration_pbc_overlap_handler(void *user_data);
int wps_nack_message_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_ack_message_process(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_dummy(struct mwu_iface_info *cur_if, u8 *, u16);
int wps_ap_beacon_prepare(struct mwu_iface_info *cur_if, u8 selectedRegistrar,
			  u8 *ptr);
int wps_ap_probe_response_prepare(struct mwu_iface_info *cur_if,
				  u8 selectedRegistrar, u8 *ptr);
int wps_ap_assoc_response_prepare(struct mwu_iface_info *cur_if,
				  u8 selectedRegistrar, u8 *ptr);
int wps_sta_assoc_request_prepare(struct mwu_iface_info *cur_if, u8 *ptr);
u16 wps_probe_response_device_password_id_parser(u8 *, size_t);
u16 wps_probe_request_device_password_id_parser(u8 *message, size_t size);
int wps_probe_response_uuid_parser(u8 *message, size_t size, u8 *uuid_e);
int wps_probe_response_conf_state_parser(u8 *message, size_t size,
					 u8 *conf_state);
int wps_probe_response_selected_registrar_config_method_parser(
	u8 *message, size_t size, u16 *config_method);
int wps_probe_response_selected_registrar_parser(u8 *message, size_t size,
						 int *is_true);
int wps_probe_response_resp_type_parser(u8 *message, size_t size,
					u8 *resp_type);
int wps_probe_response_wsc_version_parser(u8 *message, u8 size);
char hexc2bin(char chr);
u32 a2hex(char *s);
int hex2num(s8 c);

/**
 *  @brief Generate the PSK from passphrase and ssid generated.
 *  @param cur_if  Current interface
 *
 *  @return   None
 */
void wlan_wifidir_generate_psk(struct mwu_iface_info *cur_if);

int wps_sta_probe_request_prepare(struct mwu_iface_info *cur_if, u8 *ptr);
void wps_ap_setup_locked_timer_handler(void *user_data);
void wps_r_hash_failure_count_timer_handler(void *user_data);
int wps_probe_response_authorized_enrollee_mac_parser(
	struct mwu_iface_info *cur_if, u8 *message, u8 size);
int wps_state_machine_start(struct mwu_iface_info *cur_if);
void wps_registration_time_handler(void *user_data);
int wps_write_to_config_file(struct mwu_iface_info *cur_if);
int wps_update_device_info(struct mwu_iface_info *cur_if);
int do_wps_associate(struct mwu_iface_info *cur_if);
void wps_reset_wps_state(struct mwu_iface_info *cur_if);
int initiate_registration(struct mwu_iface_info *cur_if);
void wps_scan_timeout_handler(void *user_data);
void wps_walk_time_handler(void *user_data);

/**
 *  @brief Process WPS Enrollee PIN mode and PBC user selection operations
 *
 *  @param cur_if       Current interface
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_enrollee_start(struct mwu_iface_info *cur_if);

/**
 *  @brief Process WPS Enrollee Auti PIN detection and connection operations
 *
 *  @param cur_if       Current interface
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_enrollee_start_PINAuto(struct mwu_iface_info *cur_if);

/**
 *  @brief Process WPS Registrar operations
 *
 *  @param cur_if       Current interface
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_registrar_start(struct mwu_iface_info *cur_if);
void wps_registrar_stop(struct mwu_iface_info *cur_if);
void wps_enable_probe_request_event(struct mwu_iface_info *cur_if);
#ifdef CONFIG_WPS_UPNP_AP
void wps_disable_probe_request_event(struct mwu_iface_info *cur_if);
#endif
/**
 *  @brief Process WPS Enrollee Auto PBC detection and connection operations
 *
 *  @param cur_if       Current interface
 *  @return             WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_enrollee_start_PBCAuto(struct mwu_iface_info *cur_if);

/**
 * TODO: Following functions are declared here temporarily to eliminate
 * "extern abuse". Later they should be distributed to more appropriate headers.
 */
void send_enrollee_fail_event(struct mwu_iface_info *cur_if, int status);
void send_registrar_done_event(struct mwu_iface_info *cur_if, int status);
void send_credential_event(struct mwu_iface_info *cur_if);
void send_registrar_fail_event(struct mwu_iface_info *cur_if, int status);
void send_wps_progress_event(struct mwu_iface_info *cur_if, int progress);
void mwpsmod_ap_driver_event(char *ifname, u8 *buffer, u16 size);
void mwpamod_sta_kernel_event(struct event *e);
void mwpamod_ap_driver_event(char *ifname, u8 *buffer, u16 size);
void wifidir_driver_event(char *ifname, u8 *buffer, u16 size);
void send_ap_setup_locked_event(struct mwu_iface_info *cur_if, int locked_time);
int wps_clear_running_instance(void);
void wps_uap_pbc_monitor_timer_handler(void *user_data);
void mwpsmod_reset_pin(struct mwu_iface_info *cur_if);
void send_ap_setup_locked_event(struct mwu_iface_info *cur_if, int locked_time);

#ifdef CONFIG_NFC
int oob_generate_random_bytes(u8 *block, u32 block_len);
int oob_generate_public_key_hash(u8 *public_key, u8 *private_key,
				 u8 *public_key_hash);

int wps_build_NFC_pwd_token(struct mwu_iface_info *cur_if, u8 *buf, u16 *len);
int wps_build_NFC_config_token(struct mwu_iface_info *cur_if, u8 *buf,
			       u16 *len);
int wps_build_NFC_handover(struct mwu_iface_info *cur_if, u8 *buf, u16 *len,
			   u8 err_pub_key);

int wps_set_NFC_pwd_token(struct mwu_iface_info *cur_if, u8 *buf, u16 len);
int wps_set_NFC_config_token(struct mwu_iface_info *cur_if, u8 *buf, u16 len);
int wps_set_NFC_handover(struct mwu_iface_info *cur_if, u8 *buf, u16 len);

int wps_set_P2P_NFC_handover(struct mwu_iface_info *cur_if);
#endif

#ifdef CONFIG_WPS_UPNP_AP
extern int wps_eap_frame_proxy_process(struct mwu_iface_info *, u8 *, u16);
extern int wps_eap_done_frame_proxy_process(struct mwu_iface_info *, u8 *, u16);
#endif

#endif
