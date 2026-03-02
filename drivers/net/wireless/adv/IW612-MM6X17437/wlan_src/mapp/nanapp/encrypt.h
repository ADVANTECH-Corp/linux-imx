/* @file  encrypt.h
 * @brief This file contains definition for encrypt library
 *
 * Copyright 2012-2020, 2024 NXP
 *
 * NXP CONFIDENTIAL
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by NXP or its
 * suppliers or licensors. Title to the Material remains with NXP
 * or its suppliers and licensors. The Material contains trade secrets and
 * proprietary and confidential information of NXP or its suppliers and
 * licensors. The Material is protected by worldwide copyright and trade secret
 * laws and treaty provisions. No part of the Material may be used, copied,
 * reproduced, modified, published, uploaded, posted, transmitted, distributed,
 * or disclosed in any way without NXP's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by NXP in writing.
 *
 */

#ifndef _ENCRYPT_H
#define _ENCRYPT_H

#include "mwu_defs.h"

/** Digest size	*/
#define SHA256_DIGEST_SIZE (256 / 8)
/** Block size	*/
#define SHA256_BLOCK_SIZE (512 / 8)

/**
 * Diffie-Hellman parameters.
 */
typedef struct {
	/** prime */
	unsigned char *prime;
	/** length of prime */
	unsigned int primeLen;
	/** generator */
	unsigned char *generator;
	/** length of generator */
	unsigned int generatorLen;
} DH_PG_PARAMS;

int Mrv_SHA256(u8 *message, u32 len, u8 *digest);
int MrvHMAC_SHA256(u8 *Key, u32 Key_size, u8 *Message, u32 Message_len, u8 *Mac,
		   u32 MacSize);

int MrvDH_Setup(u8 *public_key, u32 public_len, u8 *private_key,
		u32 private_len, DH_PG_PARAMS *dh_params);
int MrvDH_Compute(u8 *shared_key, u8 *public_key, u32 public_len,
		  u8 *private_key, u32 private_len, DH_PG_PARAMS *dh_params);

int MrvAES_Wrap(u8 *pPlainTxt, u32 PlainTxtLen, u8 *pCipTxt, u8 *pKEK,
		u32 KeyLen, u8 *IV);
int MrvAES_UnWrap(u8 *pCipTxt, u32 CipTxtLen, u8 *pPlainTxt, u8 *pKEK,
		  u32 KeyLen, u8 *IV);

int MrvKDF(u8 *Key, u32 Key_size, u32 TotalKeyLen, u8 *OutKey);
unsigned int N8_Digits(u8 *a, u32 digits);

#endif /* ENCRYPT_H */
