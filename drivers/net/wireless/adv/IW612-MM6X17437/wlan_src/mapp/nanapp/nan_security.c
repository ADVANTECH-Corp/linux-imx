#include "nan_security_api.h"

int nan_omac1_aes_vector(const u8 *key, size_t key_len, size_t num_elem,
			 const u8 *addr[], const size_t *len, u8 *mac)
{
	CMAC_CTX *ctx;
	int ret = -1;
	size_t outlen, i;

	ctx = CMAC_CTX_new();
	if (ctx == NULL)
		return -1;

	if (key_len == 32) {
		if (!CMAC_Init(ctx, key, 32, EVP_aes_256_cbc(), NULL))
			goto fail;
	} else if (key_len == 16) {
		if (!CMAC_Init(ctx, key, 16, EVP_aes_128_cbc(), NULL))
			goto fail;
	} else {
		goto fail;
	}
	for (i = 0; i < num_elem; i++) {
		if (!CMAC_Update(ctx, addr[i], len[i]))
			goto fail;
	}
	if (!CMAC_Final(ctx, mac, &outlen) || outlen != 16)
		goto fail;

	ret = 0;
fail:
	CMAC_CTX_free(ctx);
	return ret;
}

int nan_omac1_aes_128_vector(const u8 *key, size_t num_elem, const u8 *addr[],
			     const size_t *len, u8 *mac)
{
	return nan_omac1_aes_vector(key, KCK_LEN, num_elem, addr, len, mac);
}

int nan_omac1_aes_128(const u8 *key, const u8 *data, size_t data_len, u8 *mac)
{
	return nan_omac1_aes_128_vector(key, 1, &data, &data_len, mac);
}

int nan_openssl_hmac_vector(const EVP_MD *type, const u8 *key, size_t key_len,
			    size_t num_elem, const u8 *addr[],
			    const size_t *len, u8 *mac, size_t mdlen)
{
#if OPENSSL_VERSION_NUMBER < 0x30000000
	HMAC_CTX *ctx;
#else
	EVP_MAC_CTX *ctx;
	EVP_MAC *evp_mac;
#endif
	size_t i;
	int res;

#if OPENSSL_VERSION_NUMBER < 0x30000000
#if OPENSSL_VERSION_NUMBER < 0x10100000
	HMAC_CTX octx;
	ctx = &octx;
	HMAC_CTX_init(ctx);
#else
	ctx = HMAC_CTX_new();
	if (ctx == NULL) {
		printf("OpenSSL: HMAC_CTX_new failed\n");
		return -1;
	}
#endif
#else
	evp_mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	ctx = EVP_MAC_CTX_new(evp_mac);
#endif

#if OPENSSL_VERSION_NUMBER < 0x00909000
	HMAC_Init_ex(ctx, key, key_len, type, NULL);
#else
#if OPENSSL_VERSION_NUMBER < 0x30000000
	if (HMAC_Init_ex(ctx, key, key_len, type, NULL) != 1)
		return -1;
#else
	EVP_MAC_init(ctx, key, key_len, NULL);
#endif
#endif /* openssl < 0.9.9 */

	for (i = 0; i < num_elem; i++)
#if OPENSSL_VERSION_NUMBER < 0x30000000
		HMAC_Update(ctx, addr[i], len[i]);
#else
		EVP_MAC_update(ctx, addr[i], len[i]);
#endif

#if OPENSSL_VERSION_NUMBER < 0x00909000
	HMAC_Final(ctx, mac, &mdlen);
	res = 1;
#else
#if OPENSSL_VERSION_NUMBER < 0x30000000
	res = HMAC_Final(ctx, mac, &mdlen);
#else
	res = EVP_MAC_final(ctx, mac, &mdlen, mdlen);
#endif
#endif /* openssl < 0.9.9 */

#if OPENSSL_VERSION_NUMBER < 0x30000000
#if OPENSSL_VERSION_NUMBER < 0x10100000
	HMAC_CTX_cleanup(ctx);
#else
	HMAC_CTX_free(ctx);
#endif
#else
	EVP_MAC_CTX_free(ctx);
#endif

	return res == 1 ? 0 : -1;
}

int nan_openssl_digest_vector(const EVP_MD *type, size_t num_elem,
			      const u8 *addr[], const size_t *len, u8 *mac)
{
	EVP_MD_CTX *ctx;
	size_t i;
	unsigned int mac_len;
#if OPENSSL_VERSION_NUMBER < 0x10100000
	EVP_MD_CTX mctx;
	ctx = &mctx;
#else
	ctx = EVP_MD_CTX_new();
	if (ctx == NULL) {
		printf("OpenSSL: EVP_MD_CTX_new failed\n");
		return -1;
	}
#endif

	EVP_MD_CTX_init(ctx);
	if (!EVP_DigestInit_ex(ctx, type, NULL)) {
		printf("OpenSSL: EVP_DigestInit_ex failed: %s",
		       ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}
	for (i = 0; i < num_elem; i++) {
		if (!EVP_DigestUpdate(ctx, addr[i], len[i])) {
			printf("OpenSSL: EVP_DigestUpdate "
			       "failed: %s",
			       ERR_error_string(ERR_get_error(), NULL));
			return -1;
		}
	}
	if (!EVP_DigestFinal(ctx, mac, &mac_len)) {
		printf("OpenSSL: EVP_DigestFinal failed: %s",
		       ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}
#if OPENSSL_VERSION_NUMBER < 0x10100000
	EVP_MD_CTX_cleanup(ctx);
#else
	EVP_MD_CTX_free(ctx);
#endif
	return 0;
}
int nan_hmac_sha256_vector(const u8 *key, size_t key_len, size_t num_elem,
			   const u8 *addr[], const size_t *len, u8 *mac)
{
	return nan_openssl_hmac_vector(EVP_sha256(), key, key_len, num_elem,
				       addr, len, mac, 32);
}

/**
 *  * hmac_sha256_vector - HMAC-SHA256 over data vector
 *   * @key: Key for HMAC operations
 *    * @key_len: Length of the key in bytes
 *      * @data: Pointers to the data areas
 *       * @data_len: Lengths of the data blocks
 *       *        * @mac: Buffer for the hash
 *        *         */
int nan_hmac_sha256(const u8 *key, size_t key_len, const u8 *data,
		    size_t data_len, u8 *mac)
{
	return nan_hmac_sha256_vector(key, key_len, 1, &data, &data_len, mac);
}

int nan_sha256_vector(size_t num_elem, const u8 *addr[], const size_t *len,
		      u8 *mac)
{
	return nan_openssl_digest_vector(EVP_sha256(), num_elem, addr, len,
					 mac);
}

void PUT_LE16(u8 *a, u16 val)
{
	a[1] = val >> 8;
	a[0] = val & 0xff;
}

/**
 * sha256_prf_bits - IEEE Std 802.11-2012, 11.6.1.7.2 Key derivation function
 * @key: Key for KDF
 * @key_len: Length of the key in bytes
 * @label: A unique label for each purpose of the PRF
 * @data: Extra data to bind into the key
 * @data_len: Length of the data
 * @buf: Buffer for the generated pseudo-random key
 * @buf_len: Number of bits of key to generate
 *
 * This function is used to derive new, cryptographically separate keys from a
 * given key. If the requested buf_len is not divisible by eight, the least
 * significant 1-7 bits of the last octet in the output are not part of the
 * requested output.
 */
void nan_sha256_prf_bits(const u8 *key, size_t key_len, const char *label,
			 const u8 *data, size_t data_len, u8 *buf,
			 size_t buf_len_bits)
{
	u16 counter = 1;
	size_t pos, plen;
	u8 hash[SHA256_MAC_LEN];
	const u8 *addr[4];
	size_t len[4];
	u8 counter_le[2], length_le[2];
	size_t buf_len = (buf_len_bits + 7) / 8;

	addr[0] = counter_le;
	len[0] = 2;
	addr[1] = (u8 *)label;
	len[1] = os_strlen(label);
	addr[2] = data;
	len[2] = data_len;
	addr[3] = length_le;
	len[3] = sizeof(length_le);

	PUT_LE16(length_le, buf_len_bits);
	pos = 0;
	while (pos < buf_len) {
		plen = buf_len - pos;
		PUT_LE16(counter_le, counter);
		if (plen >= SHA256_MAC_LEN) {
			nan_hmac_sha256_vector(key, key_len, 4, addr, len,
					       &buf[pos]);
			pos += SHA256_MAC_LEN;
		} else {
			nan_hmac_sha256_vector(key, key_len, 4, addr, len,
					       hash);
			os_memcpy(&buf[pos], hash, plen);
			pos += plen;
			break;
		}
		counter++;
	}

	/*
	 * Mask out unused bits in the last octet if it does not use all the
	 * bits.
	 */
	if (buf_len_bits % 8) {
		u8 mask = 0xff << (8 - buf_len_bits % 8);
		buf[pos - 1] &= mask;
	}

	os_memset(hash, 0, sizeof(hash));
}

/**
 * sha256_prf - SHA256-based Pseudo-Random Function (IEEE 802.11r, 8.5.1.5.2)
 * @key: Key for PRF
 * @key_len: Length of the key in bytes
 * @label: A unique label for each purpose of the PRF
 * @data: Extra data to bind into the key
 * @data_len: Length of the data
 * @buf: Buffer for the generated pseudo-random key
 * @buf_len: Number of bytes of key to generate
 *
 * This function is used to derive new, cryptographically separate keys from a
 * given key.
 */
void nan_sha256_prf(const u8 *key, size_t key_len, const char *label,
		    const u8 *data, size_t data_len, u8 *buf, size_t buf_len)
{
	nan_sha256_prf_bits(key, key_len, label, data, data_len, buf,
			    buf_len * 8);
}

unsigned int compute_kck_len(int akmp)
{
	if (akmp == KEY_MGMT_IEEE8021X_SUITE_B_192)
		return 24;
	return 16;
}

unsigned int compute_kek_len(int akmp)
{
	if (akmp == KEY_MGMT_IEEE8021X_SUITE_B_192)
		return 32;
	return 16;
}
/*
 *
 * PTK = SHA256(PMK, "NAN PMK Pairwise key expansion", IAddr || RAddr || INonce
 * || RNonce)
 * *
 */
/*
 * Parameters :
 * pmk : pointer to PMK
 * pmk_len : length of PMK
 * IAddr : pointer to initiator MAC address
 * RAddr : pointer to responder MAC address
 * INonce : pointer to initiator nonce
 * RNonce : pointer to responder nonce
 * ip_ptk : pointer to buffer in which generated PTK will be stored
 * akmp : parameter to decide length of kck and kek
 */
void nan_generate_ptk(const u8 *pmk, size_t pmk_len, const u8 *IAddr,
		      const u8 *RAddr, u8 *INonce, u8 *RNonce, u8 *ip_ptk,
		      int akmp)
{
	mwu_hexdump(MSG_INFO, "pmk", pmk, PMK_LEN);
	mwu_hexdump(MSG_INFO, "Peer Addr", IAddr, ETH_ALEN);
	mwu_hexdump(MSG_INFO, "Dev Addr", RAddr, ETH_ALEN);
	mwu_hexdump(MSG_INFO, "Key nonce", INonce, NONCE_LEN);
	mwu_hexdump(MSG_INFO, "local nonce", RNonce, NONCE_LEN);

	nan_ptk *ptk = (nan_ptk *)ip_ptk;
	u8 buf[2 * NONCE_LEN + 2 * ETH_ALEN];
	u8 *pos;
	u8 tmp[KCK_LEN + KEK_LEN + TK_LEN];
	size_t ptk_len;

	pos = buf;
	os_memcpy(pos, IAddr, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, RAddr, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, INonce, NONCE_LEN);
	pos += NONCE_LEN;
	os_memcpy(pos, RNonce, NONCE_LEN);
	pos += NONCE_LEN;

	ptk->kck_len = compute_kck_len(akmp);
	ptk->kek_len = compute_kek_len(akmp);
	/**
	 * TODO :: verify size of cipherkey
	 * */
	ptk->tk_len = 16; /*based on CCMP 128 not sure */
	ptk_len = ptk->kck_len + ptk->kek_len + ptk->tk_len;

	nan_sha256_prf(pmk, pmk_len, "NAN Pairwise key expansion", buf,
		       pos - buf, tmp, ptk_len);

	os_memcpy(ptk->kck, tmp, ptk->kck_len);
	os_memcpy(ptk->kek, tmp + ptk->kck_len, ptk->kek_len);
	os_memcpy(ptk->tk, tmp + ptk->kck_len + ptk->kek_len, ptk->tk_len);
	mwu_hexdump(MSG_INFO, "KCK", ptk->kck, ptk->kck_len);
	mwu_hexdump(MSG_INFO, "KEK", ptk->kek, ptk->kek_len);
	mwu_hexdump(MSG_INFO, "TK", ptk->tk, ptk->tk_len);
}
/*
 * The first 48 bits of the SHA-256 hash of the Service Name.  A lower case
 * representation of the Service Name shall be used to calculate the Service ID.
 */
/*
 *   SHA256(service name)
 *   service_name : service_name in lower case(string from 1 to 255 bytes in
 * length) service_name_len : length of service_name Service_ID :  output
 * pointer of generated Service_ID
 *           */

void nan_generate_service_id(const u8 *service_name, size_t service_name_len,
			     u8 *Service_ID)
{
	u8 hash[HASH_LEN];
	nan_sha256_vector(1, &service_name, &service_name_len, hash);
	os_memcpy(Service_ID, hash, SERVICE_ID_LEN);
	/*Take 0 to 47 bits */
}
/*
 * IEEE Std 802.11i-2004 - 8.5.1.2 Pairwise key hierarchy
 * PMKID = L(PRF(PMK, "NAN PMK Name" , IAddr || RAddr || ServiceID),0,128)
 * PMKID = SHA256(PMK, "NAN PMK Name" , IAddr || RAddr || ServiceID)
 * 256 bits (32 bytes)
 */
/*TODO:
 * SEVICE_ID/secure service Id is same or not
 */
/*
 * Parameters :
 * pmk : pointer to PMK
 * pmk_len : length of PMK
 * IAddr : pointer to initiator MAC address
 * RAddr : pointer to responder MAC address
 * ServiceID : pointer to ServiceID
 * nan_pmkid : pointer to buffer in which generated PMKID will be stored
 */
void nan_generate_pmkid(const u8 *pmk, size_t pmk_len, const u8 *IAddr,
			const u8 *RAddr, u8 *ServiceID, u8 *nan_pmkid)
{
	const u8 *addr[4];
	size_t len[4];

	mwu_hexdump(MSG_INFO, "pmk", pmk, PMK_LEN);
	mwu_hexdump(MSG_INFO, "Init Addr", IAddr, ETH_ALEN);
	mwu_hexdump(MSG_INFO, "Reponder Addr", RAddr, ETH_ALEN);
	mwu_hexdump(MSG_INFO, "ServiceID", ServiceID, SERVICE_ID_LEN);

	u8 hash[AUTH_TOKEN_HASH_LEN];
#if 0
    u8 buf[SERVICE_ID_LEN + 2 * ETH_ALEN];
	u8 *pos;
    pos = buf;
    os_memcpy(pos, IAddr, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, RAddr, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, ServiceID, SERVICE_ID_LEN);
	pos += SERVICE_ID_LEN;
#endif

	addr[0] = (u8 *)"NAN PMK Name";
	len[0] = 12;
	addr[1] = IAddr;
	len[1] = ETH_ALEN;
	addr[2] = RAddr;
	len[2] = ETH_ALEN;
	addr[3] = ServiceID;
	len[3] = SERVICE_ID_LEN;

	nan_hmac_sha256_vector(pmk, pmk_len, 4, addr, len, hash);
	mwu_hexdump(MSG_INFO, "pmkid", hash, 32);

	os_memcpy(nan_pmkid, hash, PMKID_LEN);
#if 0
    nan_sha256_prf(pmk, pmk_len, "NAN PMK Name", buf, pos - buf, hash, 32);
    os_memcpy(nan_pmkid, hash, PMKID_LEN );
    mwu_hexdump(MSG_INFO, "pmkid",hash, PMKID_LEN);
    /*truncate nan_pmkid to 128 bits*/
#endif
}

/*
SHA256(Authentication token data(body of M1))
M1 body : Authentication token data
M1 length : Auhtentication token data length
Auth_token :  output pointer of auth_token used in MIC computation of M3
*/
void nan_compute_authentication_token_for_m3(const u8 *M1_body, size_t M1_len,
					     u8 *Auth_token)
{
	u8 hash[AUTH_TOKEN_HASH_LEN] = {0};
	mwu_hexdump(MSG_INFO, "NAN2 DBG m1 body", M1_body, M1_len);
	INFO("NAN2 DBG: m1 len %lu", M1_len);

	nan_sha256_vector(1, &M1_body, &M1_len, hash);

	os_memcpy(Auth_token, hash, AUTH_TOKEN_LEN);
	/*Take 0 to 127 bits */
}

/*
 * secure service ID = HMAC_SHA256(NAN Group key, Service name || (TSF &
 * 0xFFFFFFFFFF800000)) secure service ID = HMAC_SHA256(NAN Group key, Service
 * name || TSF1)
 */
void nan_compute_secure_service_id(const u8 *nan_gk, size_t nan_gk_len,
				   u8 *Service_Name, u8 *TSF1,
				   u8 *secure_service_id)
{
	const u8 *addr[2];
	/*
	 * TODO :: UPDATE LEN
	 * */
	const size_t len[2] = {32 /*size of Service_Name*/,
			       32 /*size of TSF1*/};
	unsigned char hash[SHA256_MAC_LEN];

	addr[0] = Service_Name;
	addr[1] = TSF1;

	nan_hmac_sha256_vector(nan_gk, nan_gk_len, 2, addr, len, hash);

	os_memcpy(secure_service_id, hash, Secure_service_ID_LEN);
}
