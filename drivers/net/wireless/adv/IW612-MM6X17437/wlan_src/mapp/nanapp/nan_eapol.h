#ifndef __NAN_EAPOL_H__
#define __NAN_EAPOL_H__

#include "nan_security_api.h"

void nan_alloc_key_desc_buf(eapol_key_frame **eapol);

void nan_free_key_desc_buf(eapol_key_frame **eapol);

/*
 * If M1 and M2 are  successfully transmitted and received, replay_counter
 * values of M1 and M2 should be same. Replay counter values of M3 and M4 should
 * be same return 0 if replay_counter values are same, indicating successful M1
 * tx and M2 rx return 0 if replay_counter values are same, indicating
 * successful M3 tx and M4 rx
 */
int check_replay_counter_values(eapol_key_frame *rcvd_eapol);

/*
 * Create pointer to m1_eapol and allocate memory before calling
 * nan_generate_key_desc_m1() as follows: eapol_key_frame * m1_eapol;
 * key_descriptor_init(&m1_eapol);
 * nan_generate_key_desc_m1(&m1_eapol,...)
 * Parameters::
 * @m1_eapol : pointer  to EAPOL M1 frame
 */
void nan_generate_key_desc_m1(eapol_key_frame *m1_eapol);

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
			  const u8 *RAddr, u8 *INonce, u8 *RNonce, u8 *ip_ptk);

/*
 * Create pointer to m2_eapol and allocate memory before calling
 * nan_generate_key_desc_m2() as follows: eapol_key_frame * m2_eapol;
 * key_descriptor_init(&m2_eapol);
 * nan_generate_key_desc_m2(&m2_eapol,...)
 * Parameters::
 * @m2_eapol :      pointer  to EAPOL M2 frame
 * @rsna_key_desc :  pointer  to EAPOL desc in entire frame
 * @rcvd_m1_eapol : pointer  to received EAPOL M1 frame
 * @m2_body :       pointer to body of M2 in order to generate MIC
 * @m2_len:         Length of body of M2
 * kck :           pointer to kck
 */
void nan_generate_key_desc_m2(eapol_key_frame *m2_eapol,
			      eapol_key_frame *rsna_key_desc,
			      eapol_key_frame *rcvd_m1_eapol, u8 *m2_body,
			      u16 m2_len, u8 *kck);

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
 * @rsna_key_desc :  pointer  to EAPOL desc in entire frame
 * @m1_body :   pointer to body of M1 in ordre to generate MIC
 * @m1_len:     Length of body of M1
 * @m3_body :   pointer to body of M3 in ordre to generate MIC
 * @m3_len:     Length of body of M3
 * @kck :       pointer to Key Confirmation Key which is used to generate MIC
 */

void nan_generate_key_desc_m3(eapol_key_frame *m3_eapol,
			      eapol_key_frame *rsna_key_desc, u8 *m1_body,
			      u16 m1_len, u8 *m3_body, u16 m3_len, u8 *kck);

/*
 * Create pointer to m4_eapol and allocate memory before calling
 * nan_generate_key_desc_m4() as follows: eapol_key_frame * m4_eapol;
 * key_descriptor_init(&m4_eapol);
 * nan_generate_key_desc_m4(&m4_eapol,...)
 *
 * Parameters::
 * @m4_eapol :      pointer  to EAPOL M4 frame
 * @rsna_key_desc :  pointer  to EAPOL desc in entire frame
 * @rcvd_m3_eapol : pointer  to received EAPOL M3 frame
 * @m4_body :       pointer to body of M4 in ordre to generate MIC
 * @m4_len:         Length of body of M4
 * @kck :           pointer to Key Confirmation Key which is used to generate
 * MIC
 */

void nan_generate_key_desc_m4(eapol_key_frame *m4_eapol,
			      eapol_key_frame *rsna_key_desc,
			      eapol_key_frame *rcvd_m3_eapol, u8 *m4_body,
			      u16 m4_len, u8 *kck);

void nan_validate_m3_mic(u8 *mic, u8 *m1_body, u16 m1_len, u8 *m3_body,
			 u16 m3_len, u8 *kck);

int eapol_key_mic(const u8 *key, size_t key_len, const u8 *buf, size_t len,
		  u8 *mic);

#endif
