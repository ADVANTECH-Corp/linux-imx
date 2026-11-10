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
/* mwu.h: API to interface with the NXP Wireless Utilities
 *
 * This file provides a unified command interface for communicating with the
 * various marvell wireless utilities such as WifiDirect (wfd) and Wifi
 * Protected Setup (wps).  After launching the mwu daemon (which is previously
 * called wfdd for legacy reasons), a control program can connect to the daemon
 * by calling mwu_connect().  Upon a successful return from this function, the
 * control program can send commands to and receive events from the various
 * utilities using the mwu_send_message() and mwu_recv_message() functions
 * respectively.  Finally, after it no longer wishes to use the utilities, the
 * control program must call mwu_disconnect().
 *
 * All messages, regardless of whether they are commands, responses to
 * commands, or events, are formatted as null-terminated ascii strings.  These
 * strings contain key-value pairs that are separated by an '=' char and
 * terminated with a '\n' char.  Further, all messages sent via
 * mwu_send_message() must have at least one key value pair, and the first
 * key-value pair must be module=<module>.  Similarly, all messages returned
 * via mwu_recv_message() are guaranteed to have at least one key-value pair,
 * and the first key-value pair is always module=<module>.  <module> must be
 * one of the supported modules described below under SUPPORTED MODULES.  The
 * remaining key-value pairs depend on the <module>.
 *
 * Developers who wish to add new SUPPORTED MODULES should read mwu_internal.h.
 * Users who wish to create custom control programs should study mwu_cli.c.
 */

#ifndef __MWU_H__
#define __MWU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

/* struct mwu: private structure initialized by mwu
 *
 * mfd: file descriptor used to send messages via mwu_send_message and receive
 * the responses to those messages.  This file descriptor is not for public
 * use.  It has nothing to do with messages received via mwu_recv_message().
 *
 * efd: file descriptor suitable for use with select() on *nix systems.  When
 * this file descriptor is unblocked, the next call to mwu_recv_message() will
 * not block.  Note that this file descriptor has nothing to do with messages
 * and message responses sent via mwu_send_message.
 *
 */
struct mwu {
	int mfd;
	int efd;
	pthread_mutex_t mfd_mutex;
};

/* mwu_msg: the actual data passed between the client and server
 *
 * len: total length of the data element, including the terminating 0.  If len
 * is 0, no terminating 0 is required.
 *
 * status: this is a private status value used by mwu to allow the passing of
 * an mwu (i.e., not module) status in a command response.  This status should
 * not be altered or used outside of the mwu internals.
 *
 * data: key-value pair string as described throughout this document.
 *
 */
struct mwu_msg {
	int len;
	int status;
	char data[0];
};

/* enum mwu_error
 *
 * These are the return codes that can be returned by mwu functions.
 *
 * MWU_ERR_SUCCESS: The operation succeeded.
 *
 * MWU_ERR_BUSY: mwu is busy and cannot perform the desired command.
 *
 * MWU_ERR_INVAL: The specified message, data, or config is invalid.
 *
 * MWU_ERR_NOMEM: Memory allocation failed.
 *
 * MWU_ERR_COM: A system-dependent communication failure has occurred (e.g.,
 * permission denied, failed to create socket, etc.)
 *
 * MWU_ERR_NO_MODULE: Module specified as message target is not known.
 */
enum mwu_error {
	MWU_ERR_SUCCESS = 0,
	MWU_ERR_BUSY,
	MWU_ERR_INVAL,
	MWU_ERR_NOMEM,
	MWU_ERR_COM,
	MWU_ERR_NO_MODULE,
};

/* mwu_connect: connect to the marvell wireless utility daemon
 *
 * mwu: pointer to an unitialized struct mwu
 *
 * returns: On success, this function returns MWU_ERR_SUCCESS.  If the maximum
 * number of connections to the mwu daemon are exceeded, MWU_ERR_BUSY is
 * returned.  If the connection fails for system-dependent reasons, MWU_ERR_COM
 * is returned.
 */
enum mwu_error mwu_connect(struct mwu *mwu);

/* mwu_send_message: send the specified msg.  Expect any module-dependent
 * responses to passed via the resp argument.
 *
 * mwu: private struct initialized by mwu_connect()
 *
 * msg: The msg to send.  msg->data is the null-terminated ascii message
 * containing key-value pairs to be dispatched to one of the modules.  The
 * first key-value pair must be module=<module> where <module> is a supported
 * module (see SUPPORTED MODULES below).
 *
 * resp: If the module returns a response message, *resp will point to the
 * response message or to NULL if no response message was received.
 *
 * returns:
 *
 * MWU_ERR_SUCCESS: msg points to the module-dependent response message and the
 * caller MUST call mwu_free_msg(*resp) after it is done processing the
 * response.  If *resp is NULL, no action is necessary.
 *
 * MWU_ERR_COM: failed to send message to daemon.
 *
 * MWU_ERR_INVAL: msg did not contain all required key-value pairs, or was
 * otherwise unparsable.
 *
 * MWU_ERR_NO_MODULE: the module specified by the "module=<module>" is unknown.
 *
 * MWU_ERR_NOMEM: The message was successfully sent, but allocating memory for
 * the response failed.
 */
enum mwu_error mwu_send_message(struct mwu *m, struct mwu_msg *msg,
				struct mwu_msg **resp);

/* mwu_recv_message: get the next message to process
 *
 * mwu: private struct initialized by mwu_connect()
 *
 * msg: After a successful call, *msg will point to the message received from
 * mwu.
 *
 * returns:
 *
 * MWU_ERR_SUCCESS: msg points to the non-null module-dependent message and the
 * caller MUST call mwu_free_msg(*msg) after it is done processing msg.
 *
 * MWU_ERR_COM: failed to receive message.
 *
 * MWU_ERR_NOMEM: a msg was received but the system failed to allocate memory
 * to hold it.
 *
 * NOTE: This function normally blocks until the next message is available.
 * Systems that require non-blocking behavior should block on select() using
 * the efd member of the struct mwu.
 */
enum mwu_error mwu_recv_message(struct mwu *m, struct mwu_msg **msg);

/* mwu_free_msg: free a message allocated by mwu
 */
void mwu_free_msg(struct mwu_msg *msg);

/* mwu_disconnect: close connection to mwu
 *
 * mwu: private struct initialized by mwu_connect()
 */
void mwu_disconnect(struct mwu *mwu);

/* SUPPORTED MODULES */

/* more to come */

#ifdef __cplusplus
}
#endif

#endif
