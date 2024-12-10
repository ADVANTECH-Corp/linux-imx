#ifndef __MWU_KEY_MATERIAL_H__
#define __MWU_KEY_MATERIAL_H__

typedef unsigned short t_u16;
typedef unsigned char t_u8;
typedef unsigned int t_u32;

/*Declarations required for support of 5E command in MWU*/
#define NAN_PARAMS_KEY_MATERIAL_CMD "hostcmd"
#define HostCmd_CMD_NAN_KEY_MATERIAL 0x005e

/** Key Info flag for multicast key */
#define KEY_INFO_MCAST_KEY 0x01
/** Key Info flag for unicast key */
#define KEY_INFO_UCAST_KEY 0x02
/** Key Info flag for enable key */
#define KEY_INFO_ENABLE_KEY 0x04
/** Key Info flag for default key */
#define KEY_INFO_DEFAULT_KEY 0x08
/** Key Info flag for TX key */
#define KEY_INFO_TX_KEY 0x10
/** Key Info flag for RX key */
#define KEY_INFO_RX_KEY 0x20

//#ifdef ENABLE_802_11W
#define KEY_INFO_CMAC_AES_KEY 0x400
//#endif

//#ifdef ENABLE_802_11W
/** PN size for PMF IGTK */
#define IGTK_PN_SIZE 8
//#endif
/** key params fix size */
#define KEY_PARAMS_FIXED_LEN 10
/** key index mask */
#define KEY_INDEX_MASK 0xf

/** packet number size */
#define PN_SIZE 16
#define MLAN_MAC_ADDR_LENGTH 6

#define TK_LEN 16

/** TLV type: key param v2 */
#define TLV_TYPE_KEY_PARAM_V2 (PROPRIETARY_TLV_BASE_ID + 0x9C) /* 0x019C */
/** max seq size of wpa/wpa2 key */
#define SEQ_MAX_SIZE 8
/** Maximum key length */
#define MLAN_MAX_KEY_LENGTH 32

#define IGTK_PN_SIZE 8
#define CMAC_AES_KEY_LEN 16

/** cmac_aes_param */
typedef struct {
	t_u8 ipn[IGTK_PN_SIZE];
	t_u16 key_len;
	t_u8 key[CMAC_AES_KEY_LEN];
} __attribute__((packed)) cmac_aes_param;

/** MrvlIEtype_KeyParamSet_t */
typedef struct {
	/** Type ID */
	t_u16 type;
	/** Length of Payload */
	t_u16 length;
	/** mac address */
	t_u8 mac_addr[MLAN_MAC_ADDR_LENGTH];

	/** key index */
	t_u8 key_idx;

	/** Type of Key: WEP=0, TKIP=1, AES=2, WAPI=3 AES_CMAC=4 */
	t_u8 key_type;
	/** Key Control Info specific to a key_type_id */
	t_u16 key_info;
	/** IGTK key param */
	cmac_aes_param cmac_aes;
} __attribute__((packed)) MrvlIEtype_KeyParamSetV2_t;

/** HostCmd_DS_802_11_KEY_MATERIAL */
typedef struct {
	/** Action */
	t_u16 action;
	//#ifdef KEY_PARAM_SET_V2
	/** Key parameter set */
	MrvlIEtype_KeyParamSetV2_t key_param_set;
	//#else
	/** Key parameter set */
	//              MrvlIEtype_KeyParamSet_t  key_param_set;
	//#endif
} __attribute__((packed)) KEY_MATERIAL;

/** KEY_INFO_AES*/
typedef enum _KEY_INFO_AES {
	KEY_INFO_AES_MCAST = 0x01,
	KEY_INFO_AES_UNICAST = 0x02,
	KEY_INFO_AES_ENABLED = 0x04,
	KEY_INFO_AES_MCAST_IGTK = 0x400,
} KEY_INFO_AES;

/** KEY_TYPE_ID */
typedef enum _KEY_TYPE_ID {
	/** Key type : WEP */
	KEY_TYPE_ID_WEP = 0,
	/** Key type : TKIP */
	KEY_TYPE_ID_TKIP = 1,
	/** Key type : AES */
	KEY_TYPE_ID_AES = 2,
	KEY_TYPE_ID_WAPI = 3,
	KEY_TYPE_ID_AES_CMAC = 4,
} KEY_TYPE_ID;

typedef struct _encrypt_key {
	/** Key disabled, all other fields will be
	 *  ignore when this flag set to MTRUE
	 */
	t_u32 key_disable;
	/** key removed flag, when this flag is set
	 *  to MTRUE, only key_index will be check
	 */
	t_u32 key_remove;
	/** Key index, used as current tx key index
	 *  when is_current_wep_key is set to MTRUE
	 */
	t_u32 key_index;
	/** Current Tx key flag */
	t_u32 is_current_wep_key;
	/** Key length */
	t_u32 key_len;
	/** Key */
	t_u8 key[MLAN_MAX_KEY_LENGTH];
	/** mac address */
	t_u8 mac_addr[MLAN_MAC_ADDR_LENGTH];
	/** wapi key flag */
	t_u32 is_wapi_key;
	/** Initial packet number */
	t_u8 pn[PN_SIZE];
	/** key flags */
	t_u32 key_flags;
} encrypt_key, *pencrypt_key;

/*
 * TODO :
 * Currently this function sets key of type AES ; it should be set of type
 * CMAC_AES. Parameters :: cur_if :     Pointer to current interface key_mat :
 * pointer to key material structure which will be updated and sent to ffirmware
 * pdata_buf:   pointer to buffer which contains tk, tk_len ,peer MAC address
 * and key_index, pdata_buf is used to update key_mat
 */
int mwu_key_material(struct mwu_iface_info *cur_if, KEY_MATERIAL *key_mat,
		     encrypt_key *pdata_buf);
int mwu_add_nan_peer(struct mwu_iface_info *cur_if);

#endif
