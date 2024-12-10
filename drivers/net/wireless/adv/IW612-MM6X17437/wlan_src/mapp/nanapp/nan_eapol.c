#include "nan_eapol.h"
#include "wps_wlan.h"
#include "sha1.h"

#define POOL_WORDS 32
#define POOL_WORDS_MASK (POOL_WORDS - 1)
#define POOL_TAP1 26
#define POOL_TAP2 20
#define POOL_TAP3 14
#define POOL_TAP4 7
#define POOL_TAP5 1
#define EXTRACT_LEN 16
#define SHA1_MAC_LEN 20
/*
 * To store replay counter values of M1 and M3 so that it can be
 * compared with values received from M2 and M4 resp.
 */
u8 replay_counter_backup[REPLAY_COUNTER_LEN];
static unsigned int entropy = 0;
static unsigned int input_rotate = 0;
static unsigned int pool_pos = 0;
static u32 pool[POOL_WORDS];
static u8 dummy_key[20];
/**
 * eapol_key_mic - Calculate EAPOL-Key MIC
 * @key: EAPOL-Key Key Confirmation Key (KCK)
 * @buf: Pointer to the beginning of the EAPOL header (version field)
 * @len: Length of the EAPOL frame (from EAPOL header to the end of the frame)
 * @mic: Pointer to the buffer to which the EAPOL-Key MIC is written
 * Returns: 0 on success, -1 on failure
 *
 * Calculate EAPOL-Key MIC for an EAPOL-Key packet. The EAPOL-Key MIC field has
 * to be cleared (all zeroes) when calling this function.
 *
 * Note: 'IEEE Std 802.11i-2004 - 8.5.2 EAPOL-Key frames' has an error in the
 * description of the Key MIC calculation. It includes packet data from the
 * beginning of the EAPOL-Key header, not EAPOL header. This incorrect change
 * happened during final editing of the standard and the correct behavior is
 * defined in the last draft (IEEE 802.11i/D10).
 */
int eapol_key_mic(const u8 *key, size_t key_len, const u8 *buf, size_t len,
		  u8 *mic)
{
	int ret = NAN_ERR_SUCCESS;
	u8 hash[SHA256_MAC_LEN];

	ERR("NAN2 DBG len %lu", len);

	mwu_hexdump(MSG_INFO, "NAN2 DBG Input buffer for MIC", buf, len);
	mwu_hexdump(MSG_INFO, "NAN2 DBG key", key, 16);

	nan_hmac_sha256(key, key_len, buf, len, hash);
	os_memcpy(mic, hash, MIC_LEN);
	return ret;
}
/*
static u8 * nan_add_kde(u8 *pos, u32 kde, const u8 *data, unsigned int data_len)
{
	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = RSN_SELECTOR_LEN + data_len;
	RSN_SELECTOR_PUT(pos, kde);
	pos += RSN_SELECTOR_LEN;
	os_memcpy(pos, data, data_len);
	pos += data_len;
	return pos;
}
*/

/*
 *  This function allocates memory for eapol key descriptor frame
 *  parameters :
 *  eapol_frame : pointer to eapol_key_frame structure
 */
void key_descriptor_init(eapol_key_frame *eapol_frame)
{
	unsigned int kde_len;
	u8 *mbuf;
	kde_len = 0; // 2 + RSN_SELECTOR_LEN + PMKID_LEN;

	mbuf = (u8 *)malloc(sizeof(eapol_key_frame) + kde_len);
	if (mbuf == NULL)
		return;
	else
		os_memset(mbuf, 0, sizeof(eapol_key_frame) + kde_len);

	eapol_frame = (eapol_key_frame *)mbuf;
}

static u32 __ROL32(u32 x, u32 y)
{
	return (x << (y & 31)) | (x >> (32 - (y & 31)));
}

static void random_mix_pool(const void *buf, size_t len)
{
	static const u32 twist[8] = {0x00000000, 0x3b6e20c8, 0x76dc4190,
				     0x4db26158, 0xedb88320, 0xd6d6a3e8,
				     0x9b64c2b0, 0xa00ae278};
	const u8 *pos = buf;
	u32 w;

	mwu_hexdump(MSG_INFO, "random_mix_pool", buf, len);

	while (len--) {
		w = __ROL32(*pos++, input_rotate & 31);
		input_rotate += pool_pos ? 7 : 14;
		pool_pos = (pool_pos - 1) & POOL_WORDS_MASK;
		w ^= pool[pool_pos];
		w ^= pool[(pool_pos + POOL_TAP1) & POOL_WORDS_MASK];
		w ^= pool[(pool_pos + POOL_TAP2) & POOL_WORDS_MASK];
		w ^= pool[(pool_pos + POOL_TAP3) & POOL_WORDS_MASK];
		w ^= pool[(pool_pos + POOL_TAP4) & POOL_WORDS_MASK];
		w ^= pool[(pool_pos + POOL_TAP5) & POOL_WORDS_MASK];
		pool[pool_pos] = (w >> 3) ^ twist[w & 7];
	}
}

static void random_extract(u8 *out)
{
	unsigned int i;
	u8 hash[SHA1_MAC_LEN];
	u32 *hash_ptr;
	u32 buf[POOL_WORDS / 2];

	/* First, add hash back to pool to make backtracking more difficult. */
	hmac_sha1(dummy_key, sizeof(dummy_key), (const u8 *)pool, sizeof(pool),
		  hash);
	random_mix_pool(hash, sizeof(hash));
	/* Hash half the pool to extra data */
	for (i = 0; i < POOL_WORDS / 2; i++)
		buf[i] = pool[(pool_pos - i) & POOL_WORDS_MASK];
	hmac_sha1(dummy_key, sizeof(dummy_key), (const u8 *)buf, sizeof(buf),
		  hash);
	/*
	 *      * Fold the hash to further reduce any potential output pattern.
	 *           * Though, compromise this to reduce CPU use for the most
	 * common output
	 *                * length (32) and return 16 bytes from instead of only
	 * half.
	 *                     */
	hash_ptr = (u32 *)hash;
	hash_ptr[0] ^= hash_ptr[4];
	os_memcpy(out, hash, EXTRACT_LEN);
}

int nan_os_get_random(unsigned char *buf, size_t len)
{
	u8 *bytes = buf;
	size_t left;
	FILE *f;
	size_t rc;

	/* Start with assumed strong randomness from OS */
	f = fopen("/dev/urandom", "rb");
	if (f == NULL) {
		printf("Could not open /dev/urandom.\n");
		return -1;
	}
	rc = fread(buf, 1, len, f);
	fclose(f);

	mwu_hexdump(MSG_INFO, "random from os_get_random", buf, len);

	if (rc != len) {
		return -1;
	}

	/* Mix in additional entropy extracted from the internal pool */
	left = len;
	while (left) {
		size_t siz, i;
		u8 tmp[EXTRACT_LEN];
		random_extract(tmp);
		mwu_hexdump(MSG_INFO, "random from internal pool", tmp,
			    sizeof(tmp));
		siz = left > EXTRACT_LEN ? EXTRACT_LEN : left;
		for (i = 0; i < siz; i++)
			*bytes++ ^= tmp[i];
		left -= siz;
	}
#ifdef CONFIG_FIPS
	/* Mix in additional entropy from the crypto module */
	bytes = buf;
	left = len;
	while (left) {
		size_t siz, i;
		u8 tmp[EXTRACT_LEN];
		if (crypto_get_random(tmp, sizeof(tmp)) < 0) {
			printf(MSG_ERROR, "random: No entropy available "
					  "for generating strong random bytes");
			return -1;
		}
		mwu_hexdump(MSG_INFO, "random from crypto module", tmp,
			    sizeof(tmp));
		siz = left > EXTRACT_LEN ? EXTRACT_LEN : left;
		for (i = 0; i < siz; i++)
			*bytes++ ^= tmp[i];
		left -= siz;
	}
#endif /* CONFIG_FIPS */

	mwu_hexdump(MSG_INFO, "mixed random", buf, len);

	if (entropy < len)
		entropy = 0;
	else
		entropy -= len;

	return 0;
}
/*
 * parameters:
 * eapol_frame : pointer to updated key descriptor
 */
static void key_descriptor_update(eapol_key_frame *eapol_frame)
{
	eapol_frame->type = EAPOL_KEY_TYPE_RSN;

	/* Do not set nonce to 0 because in case of M2/M3/M4, it will be updated
	 * before calling this function
	 * */

	PUT_BE16(eapol_frame->key_length, 0);
	os_memset(eapol_frame->replay_counter, 0, REPLAY_COUNTER_LEN);
	os_memset(eapol_frame->key_iv, 0x0, KEY_IV_LEN);
	os_memset(eapol_frame->key_rsc, 0x0, KEY_RSC_LEN);
	os_memset(eapol_frame->key_id, 0x0, 8);
	os_memset(eapol_frame->key_mic, 0x0, MIC_LEN);

	PUT_BE16(eapol_frame->key_data_length, 0);
}

/*Key info field for M1*/
void set_key_info_m1(eapol_key_frame *eapol_frame)
{
	u16 key_info = 0, ver = 0;
	ver = 0;
	PUT_BE16(eapol_frame->key_info, key_info);

	key_info = ver | KEY_INFO_KEY_TYPE | KEY_INFO_ACK;
	PUT_BE16(eapol_frame->key_info, key_info);
}

/*Key info field for M2*/
void set_key_info_m2(eapol_key_frame *eapol_frame)
{
	u16 key_info = 0, ver = 0;
	ver = 0;
	PUT_BE16(eapol_frame->key_info, key_info);

	key_info = ver | KEY_INFO_KEY_TYPE | KEY_INFO_MIC;
	PUT_BE16(eapol_frame->key_info, key_info);
}

/*Key info field for M3*/
void set_key_info_m3(eapol_key_frame *eapol_frame)
{
	u16 key_info = 0, ver = 0;
	// ver = KEY_INFO_TYPE_AES_128_CMAC;
	PUT_BE16(eapol_frame->key_info, key_info);

	key_info = ver | KEY_INFO_KEY_TYPE | KEY_INFO_MIC | KEY_INFO_ACK |
		   KEY_INFO_INSTALL | KEY_INFO_SECURE;
	PUT_BE16(eapol_frame->key_info, key_info);
}

/*Key info field for M4, Install bit should be set for M4*/
void set_key_info_m4(eapol_key_frame *eapol_frame)
{
	u16 key_info = 0, ver = 0;
	// ver = KEY_INFO_TYPE_AES_128_CMAC;
	PUT_BE16(eapol_frame->key_info, key_info);

	key_info = ver | KEY_INFO_KEY_TYPE | KEY_INFO_INSTALL | KEY_INFO_MIC |
		   KEY_INFO_SECURE;
	PUT_BE16(eapol_frame->key_info, key_info);
}

static void save_replay_counter(eapol_key_frame *eapol_frame)
{
	u8 *ptr;
	ptr = (u8 *)&replay_counter_backup;
	os_memcpy(ptr, eapol_frame->replay_counter, REPLAY_COUNTER_LEN);
}

/*
 * M2 should contain same replay_counter value that was sent by M1
 * M4 should contain same replay_counter value that was sent by M3
 */
void update_replay_counter(eapol_key_frame *rcvd_eapol,
			   eapol_key_frame *eapol_frame)
{
	os_memcpy(eapol_frame->replay_counter, rcvd_eapol->replay_counter,
		  REPLAY_COUNTER_LEN);
}

/*
 * If M1 and M2 are  successfully transmitted and received, replay_counter
 * values of M1 and M2 should be same. Replay counter values of M3 and M4 should
 * be same return 0 if replay_counter values are same, indicating successful M1
 * tx and M2 rx return 0 if replay_counter values are same, indicating
 * successful M3 tx and M4 rx
 */
int check_replay_counter_values(eapol_key_frame *rcvd_eapol)
{
	u8 *ptr;
	ptr = (u8 *)&replay_counter_backup;

	return (memcmp(rcvd_eapol->replay_counter, ptr, REPLAY_COUNTER_LEN));
}

/*Allocate local key_desc buffer*/

void nan_alloc_key_desc_buf(eapol_key_frame **eapol)
{
	*eapol = (eapol_key_frame *)malloc(sizeof(eapol_key_frame));
	if (*eapol == NULL)
		return;
	else
		os_memset(*eapol, 0, sizeof(eapol_key_frame));
}

void nan_free_key_desc_buf(eapol_key_frame **eapol)
{
	free(*eapol);
}
/*
 * Create pointer to m1_eapol and allocate memory before calling
 * nan_generate_key_desc_m1() as follows: eapol_key_frame * m1_eapol;
 * key_descriptor_init(&m1_eapol);
 * nan_generate_key_desc_m1(&m1_eapol,...)
 * Parameters::
 * @m1_eapol : pointer  to EAPOL M1 frame
 */
void nan_generate_key_desc_m1(eapol_key_frame *m1_eapol)
{
	os_memset(m1_eapol, 0x0, sizeof(eapol_key_frame));
	os_memset(m1_eapol->key_nonce, 0, NONCE_LEN);

	key_descriptor_update(m1_eapol);

	if (nan_os_get_random(m1_eapol->key_nonce, NONCE_LEN)) {
		return;
	}

	/* Save replay_counter value so that it can be compared with
	 * replay_counter values received in M2
	 */
	save_replay_counter(m1_eapol);
	set_key_info_m1(m1_eapol);
}
/*
 * Generate nonce for M2 and PTK .
 *
 * @pmk : pointer  to pmk
 * @pmk_len : length of pmk
 * IAddr : pointer to initiator MAC address
 * RAddr : pointer to responder MAC address
 * INonce : pointer to initiator nonce
 * RNonce : pointer to responder nonce
 * ip_ptk : pointer to buffer in which generated PTK will be stored
 */
void nan_gen_ptk_nonce_m2(const u8 *pmk, size_t pmk_len, const u8 *IAddr,
			  const u8 *RAddr, u8 *INonce, u8 *RNonce, u8 *ip_ptk)
{
	int akmp = BIT(16);

	if (nan_os_get_random(RNonce, NONCE_LEN)) {
		return;
	}

	mwu_hexdump(MSG_INFO, "M2 Nonce", RNonce, NONCE_LEN);

	nan_generate_ptk(pmk, pmk_len, IAddr, RAddr, INonce, RNonce, ip_ptk,
			 akmp);
}

/*
 * Create pointer to m2_eapol and allocate memory before calling
 * nan_generate_key_desc_m2() as follows: eapol_key_frame * m2_eapol;
 * key_descriptor_init(&m2_eapol);
 * nan_generate_key_desc_m2(&m2_eapol,...)
 * Parameters::
 * @m2_eapol :      pointer  to EAPOL M2 frame
 * @rcvd_m1_eapol : pointer  to received EAPOL M1 frame
 * @m2_body :       pointer to body of M2 in order to generate MIC
 * @m2_len:         Length of body of M2
 * kck :           pointer to kck
 */
void nan_generate_key_desc_m2(eapol_key_frame *m2_eapol,
			      eapol_key_frame *rsna_key_desc,
			      eapol_key_frame *rcvd_m1_eapol, u8 *m2_body,
			      u16 m2_len, u8 *kck)
{
	key_descriptor_update(m2_eapol);

	/* M2 should contain same replay_counter value that was sent by M1*/
	update_replay_counter(rcvd_m1_eapol, m2_eapol);
	set_key_info_m2(m2_eapol);

	mwu_hexdump(MSG_INFO, "NAN2 DBG M2 Body before key desc updation\n",
		    m2_body, m2_len);

	memcpy(rsna_key_desc, m2_eapol, sizeof(eapol_key_frame));

	mwu_hexdump(MSG_INFO,
		    "NAN2 DBG M2 Body after key desc updation, MIC=0\n",
		    m2_body, m2_len);

	eapol_key_mic(kck, 16, m2_body, m2_len, m2_eapol->key_mic);
	mwu_hexdump(MSG_INFO, "NAN2 DBG M2 MIC \n", m2_eapol->key_mic, MIC_LEN);
}

/* Before generating M3 check if replay counter values of M1 and M2 are same.
 * If sucess, Create pointer to m3_eapol and allocate memory before calling
 nan_generate_key_desc_m3().
 * if (check_replay_counter_values(eapol_key_frame * rcvd_m2_eapol) == 0)
 * {
 * eapol_key_frame * m3_eapol;
 * key_descriptor_init(&m3_eapol);
 * nan_generate_key_desc_m3(&m3_eapol,...)
 * }
 * else
 * printf("M2 not received successfully\n");

 * Parameters::
 * @m3_eapol :  pointer  to EAPOL M3 frame
 * @m1_body :   pointer to body of M1 in ordre to generate MIC
 * @m1_len:     Length of body of M1
 * @m3_body :   pointer to body of M3 in ordre to generate MIC
 * @m3_len:     Length of body of M3
 * @kck :       pointer to Key Confirmation Key which is used to generate MIC
 */

void nan_generate_key_desc_m3(eapol_key_frame *m3_eapol,
			      eapol_key_frame *rsna_key_desc, u8 *m1_body,
			      u16 m1_len, u8 *m3_body, u16 m3_len, u8 *kck)
{
	u8 *buf;
	u8 Auth_token[AUTH_TOKEN_LEN];
	u8 *ptr_auth_token;

	u16 len = AUTH_TOKEN_LEN + m3_len;
	buf = (u8 *)malloc(len);

	if (buf)
		memset(buf, 0x0, len);

	ptr_auth_token = (u8 *)&Auth_token;

	key_descriptor_update(m3_eapol);
	/*
	 * M2 contains same replay counter value as that of M1
	 * So, increment replay_counter for M3.
	 */

	inc_byte_array(m3_eapol->replay_counter, REPLAY_COUNTER_LEN);
	save_replay_counter(m3_eapol);

	set_key_info_m3(m3_eapol);

	memcpy(rsna_key_desc, m3_eapol, sizeof(eapol_key_frame));

	nan_compute_authentication_token_for_m3(m1_body, m1_len,
						ptr_auth_token);

	/*MIC(KCK, Authentication Token || Body of M3 ) */

	os_memcpy(buf, ptr_auth_token, AUTH_TOKEN_LEN);
	os_memcpy(buf + AUTH_TOKEN_LEN, m3_body, m3_len);

	/*
	 * MIC is computed over concatenation of Auth_token and body of M3
	 */
	mwu_hexdump(MSG_INFO, "NAN2 DBG buf used for M3 MIC calulation\n", buf,
		    len);

	eapol_key_mic(kck, 16, buf, len, m3_eapol->key_mic);
	mwu_hexdump(MSG_INFO, "NAN2 DBG M3 MIC\n", m3_eapol->key_mic, MIC_LEN);
}
/*
 * Create pointer to m4_eapol and allocate memory before calling
 * nan_generate_key_desc_m4() as follows: eapol_key_frame * m4_eapol;
 * key_descriptor_init(&m4_eapol);
 * nan_generate_key_desc_m4(&m4_eapol,...)
 *
 * Parameters::
 * @m4_eapol :      pointer  to EAPOL M4 frame
 * @rcvd_m3_eapol : pointer  to received EAPOL M3 frame
 * @m4_body :       pointer to body of M4 in ordre to generate MIC
 * @m4_len:         Length of body of M4
 * @kck :           pointer to Key Confirmation Key which is used to generate
 * MIC
 */

void nan_generate_key_desc_m4(eapol_key_frame *m4_eapol,
			      eapol_key_frame *rsna_key_desc,
			      eapol_key_frame *rcvd_m3_eapol, u8 *m4_body,
			      u16 m4_len, u8 *kck)
{
	key_descriptor_update(m4_eapol);

	set_key_info_m4(m4_eapol);

	update_replay_counter(rcvd_m3_eapol, m4_eapol);

	mwu_hexdump(MSG_INFO, "NAN2 DBG M4 Body before key desc updation\n",
		    m4_body, m4_len);

	memcpy(rsna_key_desc, m4_eapol, sizeof(eapol_key_frame));
	/*
	 * MIC is computed over body of M4
	 */
	eapol_key_mic(kck, 16, m4_body, m4_len, m4_eapol->key_mic);

	mwu_hexdump(MSG_INFO, "NAN2 DBG M4 MIC\n", m4_eapol->key_mic, MIC_LEN);
	mwu_hexdump(MSG_INFO, "NAN2 DBG M4 Body after key desc updation\n",
		    m4_body, m4_len);
}

/*
 * After M3 tx and M4 rx, check if replay_counter values are same. If yes
 * increment replay counter before proceding further.
 */

/*Generate MIC for M3 when M3 is received*/
void nan_validate_m3_mic(u8 *temp_mic, u8 *m1_body, u16 m1_len, u8 *m3_body,
			 u16 m3_len, u8 *kck)
{
	u8 *buf;
	u8 Auth_token[AUTH_TOKEN_LEN] = {0};
	u8 *ptr_auth_token;
	u16 len;

	len = AUTH_TOKEN_LEN + m3_len;
	buf = (void *)malloc(len);

	if (buf)
		memset(buf, 0x0, len);

	ptr_auth_token = (u8 *)&Auth_token;

	nan_compute_authentication_token_for_m3(m1_body, m1_len,
						ptr_auth_token);

	/* buf = Authentication Token || Body of M3 */
	os_memcpy(buf, ptr_auth_token, AUTH_TOKEN_LEN);
	os_memcpy(buf + AUTH_TOKEN_LEN, m3_body, m3_len);

	/*MIC(KCK, Authentication Token || Body of M3 ) */
	/*
	 * MIC is computed over concatenation of Auth_token and body of M3
	 */
	eapol_key_mic(kck, 16, buf, len, temp_mic);

	mwu_hexdump(MSG_INFO, "NAN2 DBG MIC genertaed for rcvd M3\n", temp_mic,
		    MIC_LEN);
}
