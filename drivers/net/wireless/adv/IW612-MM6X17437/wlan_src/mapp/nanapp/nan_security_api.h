#ifndef __NAN_SECURITY_API_H__
#define __NAN_SECURITY_API_H__

#include "nan_security.h"

int nan_omac1_aes_vector(const u8 *key, size_t key_len, size_t num_elem,
			 const u8 *addr[], const size_t *len, u8 *mac);

int nan_omac1_aes_128_vector(const u8 *key, size_t num_elem, const u8 *addr[],
			     const size_t *len, u8 *mac);

int nan_omac1_aes_128(const u8 *key, const u8 *data, size_t data_len, u8 *mac);

int nan_hmac_sha256_vector(const u8 *key, size_t key_len, size_t num_elem,
			   const u8 *addr[], const size_t *len, u8 *mac);

int nan_hmac_sha256(const u8 *key, size_t key_len, const u8 *data,
		    size_t data_len, u8 *mac);

int nan_sha256_vector(size_t num_elem, const u8 *addr[], const size_t *len,
		      u8 *mac);

void nan_sha256_prf(const u8 *key, size_t key_len, const char *label,
		    const u8 *data, size_t data_len, u8 *buf, size_t buf_len);

/*
SHA256(Authentication token data(body of M1))
M1 body : Authentication token data
M1 length : Auhtentication token data length
Auth_token :  output pointer of auth_token used in MIC computation of M3
*/
void nan_compute_authentication_token_for_m3(const u8 *M1_body, size_t M1_len,
					     u8 *Auth_token);
/*
 * IEEE Std 802.11i-2004 - 8.5.1.2 Pairwise key hierarchy
 * PMKID = SHA256(PMK, "NAN PMK Name" , IAddr || RAddr || ServiceID)
 * 256 bits (32 bytes)
 */
void nan_generate_pmkid(const u8 *pmk, size_t pmk_len, const u8 *IAddr,
			const u8 *RAddr, u8 *ServiceID, u8 *nan_pmkid);
/*
 * secure service ID = HMAC_SHA256(NAN Group key, Service name || (TSF &
 * 0xFFFFFFFFFF800000)) secure service ID = HMAC_SHA256(NAN Group key, Service
 * name || TSF1)
 */
void nan_compute_secure_service_id(const u8 *nan_gk, size_t nan_gk_len,
				   u8 *Service_Name, u8 *TSF1,
				   u8 *secure_service_id);

/*
 *
 * PTK = SHA256(PMK, "NAN PMK Pairwise key expansion", || IAddr || RAddr ||
 * INonce || RNonce)
 * */
void nan_generate_ptk(const u8 *pmk, size_t pmk_len, const u8 *IAddr,
		      const u8 *RAddr, u8 *INonce, u8 *RNonce, u8 *ptk,
		      int key_mgmt);

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
			     u8 *Service_ID);

#endif
