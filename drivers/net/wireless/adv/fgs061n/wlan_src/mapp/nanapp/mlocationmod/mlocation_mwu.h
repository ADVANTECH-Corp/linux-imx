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

/* mwu module "mlocation"
 *
 * The mlocation module is meant as a model module to demonstrate the internal
 * module API for developers wishing to add wireless utilities to the suite.
 *
 * Any message sent via mwu_send_message() with "module=mlocation" will be
 * routed to the mlocation module and must have "cmd=<cmd>" as it's next
 * key-value pair.  Any message received via mwu_recv_message() with
 * module=mlocation is an event from the mlocation module and will have
 * event=<event> as its second key-value pair.
 *
 * <cmd> can be one of the following:
 *
 * ping: This is simply a mlocation command that the control program can use to
 * ensure that it is connected to mwu.  The mlocation module always returns a
 * single key-value pair in the response message: status=<status>.  <status> is
 * the base-ten ascii representation of one of the status codes defined below.
 * If <status> is MLOCATION_SUCCESS, the user can expect a pong event (see
 * below).
 *
 * <event> will be one of the following:
 *
 * pong: Somebody sent a message with "cmd=ping".  This is the response.
 */

#ifndef __MWU_MLOCATION__
#define __MWU_MLOCATION__

/* enum mlocation_status
 *
 * MLOCATION_SUCCESS: The operation was successful
 *
 * MLOCATION_UNSUPPORTED: The command specified by the cmd key-value pair is not
 * supported.
 *
 * MLOCATION_INVALID: The command message was missing required fields, or is
 * otherwise invalid.
 *
 * MLOCATION_INTERNAL: An unexpected internal error occured (e.g.,
 * out-of-memory). See log output for details.
 */
enum mlocation_status {
	MLOCATION_SUCCESS = 0,
	MLOCATION_ERR_UNSUPPORTED_CMD,
	MLOCATION_ERR_INVALID,
	MLOCATION_ERR_INTERNAL,
};

enum mwu_error mlocation_mwu_launch(void);
void mlocation_mwu_halt(void);
void mlocationmod_halt(void);

#endif
