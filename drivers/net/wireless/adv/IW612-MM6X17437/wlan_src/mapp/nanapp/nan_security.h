#ifndef __NAN_SECURITY_H__
#define __NAN_SECURITY_H__

#include "mwu_defs.h"
#include "mwu_log.h"
#include "string.h"
#include <openssl/opensslv.h>
#include <openssl/err.h>
#include <openssl/des.h>
#include <openssl/aes.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/dh.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#if 0
typedef unsigned char UINT8;
typedef UINT8 u8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#endif

#define os_memcpy(d, s, n) memcpy((d), (s), (n))
#define os_memset(s, c, n) memset(s, c, n)
#define os_strlen(s) strlen(s)

#define NAN_PARAMS_KEY_MATERIAL_CMD "hostcmd"
#define HostCmd_CMD_NAN_KEY_MATERIAL 0x005e

#define SHA256_MAC_LEN 32
#define Secure_service_ID_LEN 32
#define AUTH_TOKEN_HASH_LEN 32
#define HASH_LEN 32
#define AUTH_TOKEN_LEN 16
#define SERVICE_ID_LEN 6

/*
 * TODO :: check length of all these #defs
 * */
#define NAN_PMKID_LEN 32
#define PMK_LEN 32
#define PMK_STR_LEN 36
#define NONCE_LEN 32
#define BIT(x) (1U << (x))
#define KEY_MGMT_IEEE8021X_SUITE_B_192 BIT(17)
#define KCK_LEN 16
#define KEK_LEN 16
#define TK_LEN 16

#define ETH_ALEN 6
#define WLAN_EID_VENDOR_SPECIFIC 221
#define PMKID_LEN 16
#define RSN_SELECTOR_LEN 4
#define NONCE_LEN 32
#define REPLAY_COUNTER_LEN 8
#define NONCE_LEN 32
#define KEY_RSC_LEN 8
#define MIC_LEN 16
#define KEY_IV_LEN 16

#define KEY_INFO_TYPE_AES_128_CMAC 3
#define KEY_INFO_KEY_TYPE BIT(3) /* 1 = Pairwise, 0 = Group key */
#define KEY_INFO_INSTALL BIT(6)
#define KEY_INFO_ACK BIT(7)
#define KEY_INFO_MIC BIT(8)
#define KEY_INFO_SECURE BIT(9)

#define PUT_BE16(a, val)                                                       \
	do {                                                                   \
		(a)[0] = ((u16)(val)) >> 8;                                    \
		(a)[1] = ((u16)(val)) & 0xff;                                  \
	} while (0)
#define RSN_SELECTOR(a, b, c, d)                                               \
	((((u32)(a)) << 24) | (((u32)(b)) << 16) | (((u32)(c)) << 8) | (u32)(d))
#define RSN_KEY_DATA_PMKID RSN_SELECTOR(0x00, 0x0f, 0xac, 4)

#define RSN_SELECTOR_PUT(a, val) PUT_BE32((u8 *)(a), (val))

enum {
	EAPOL_KEY_TYPE_RC4 = 1,
	EAPOL_KEY_TYPE_RSN = 2,
	EAPOL_KEY_TYPE_WPA = 254
};

typedef struct _eapol_key_frame {
	u8 type;
	/* Note: key_info, key_length, and key_data_length are unaligned */
	u8 key_info[2]; /* big endian */
	u8 key_length[2]; /* big endian */
	u8 replay_counter[REPLAY_COUNTER_LEN];
	u8 key_nonce[NONCE_LEN];
	u8 key_iv[KEY_IV_LEN];
	u8 key_rsc[KEY_RSC_LEN];
	u8 key_id[8]; /* Reserved in IEEE 802.11i/RSN */
	u8 key_mic[MIC_LEN];
	u8 key_data_length[2]; /* big endian */
	/* followed by key_data_length bytes of key_data */
} __attribute__((packed)) eapol_key_frame;
/*length of eapol_key_frame is 95 bytes*/

typedef struct CMAC_CTX_st CMAC_CTX;

/*
 * struct nan_ptk - NAN Pairwise Transient Key
 */
typedef struct _nan_ptk {
	u8 kck[KCK_LEN]; /* EAPOL-Key Key Confirmation Key (KCK) */
	u8 kek[KEK_LEN]; /* EAPOL-Key Key Encryption Key (KEK) */
	u8 tk[TK_LEN]; /* Temporal Key (TK) */
	size_t kck_len;
	size_t kek_len;
	size_t tk_len;
} nan_ptk;

struct nan_sec_buf {
	int size;
	u8 buf[0];
};

typedef struct _nan_security_info {
	nan_ptk ptk_buf;
	u8 pmk[PMK_LEN];
	u8 pmk_len;
	u8 local_key_nonce[NONCE_LEN];
	u8 peer_key_nonce[NONCE_LEN];
	u8 tk[TK_LEN];
	u8 nan_pmkid[PMKID_LEN];
	struct nan_sec_buf *m1; /*Points to the copy of Rx NDP resp*/
	struct nan_sec_buf *m2; /*Points to the copy of Rx NDP resp*/
	struct nan_sec_buf *m3; /*Points to the copy of Rx NDP resp*/
	struct nan_sec_buf *m4;
	u8 wrong_mic_test : 1; /* Test Params only TBR after PF */
	u8 m4_reject_test : 1;
} nan_security_info;

/*typedef struct CMAC_CTX_st CMAC_CTX;*/
CMAC_CTX *CMAC_CTX_new(void);
void CMAC_CTX_cleanup(CMAC_CTX *ctx);
void CMAC_CTX_free(CMAC_CTX *ctx);
EVP_CIPHER_CTX *CMAC_CTX_get0_cipher_ctx(CMAC_CTX *ctx);
int CMAC_CTX_copy(CMAC_CTX *out, const CMAC_CTX *in);
int CMAC_Init(CMAC_CTX *ctx, const void *key, size_t keylen,
	      const EVP_CIPHER *cipher, ENGINE *impl);
int CMAC_Update(CMAC_CTX *ctx, const void *data, size_t dlen);
int CMAC_Final(CMAC_CTX *ctx, unsigned char *out, size_t *poutlen);
int CMAC_resume(CMAC_CTX *ctx);

#endif
