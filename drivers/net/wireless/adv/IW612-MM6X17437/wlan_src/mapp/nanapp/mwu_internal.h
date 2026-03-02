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
/* mwu_internal.h: internal interface for mwu
 *
 * This is the internal interface that wireless modules use to communicate with
 * control programs via the mwu interface.  To receive messages sent via
 * mwu_send_message(), a module must register it's name with a suitable handler
 * by calling mwu_internal_register_module().  To send messages that will go to
 * control programs listening via mwu_recv_message(), a module must populate a
 * buffer with the desired message, then send it using mwu_internal_send().
 *
 * Before this messaging scheme can be used, mwu must be initialized by calling
 * mwu_internal_init() by the daemon.  When the daemon wishes to stop using the
 * mwu control interface, it must call mwu_internal_deinit().  It is up to the
 * daemon to ensure that mwu_internal_init() is called before any modules
 * attempt to register handlers or send events.
 *
 * Module developers adding new mwu modules should consider the following
 * guidelines:
 *
 * -- Keep in mind that your module will coexist with others.  So if you emit
 *    any log messages, you may wish to prepend your module name.
 *
 * -- You must document the message scheme used by your module.  The sensible
 *    place to do this is in a header file name mymodule.h, not in this header
 *    file or in mwu.h.  See mwu_test.h for an example.  Note how the internal
 *    documentation (e.g., how to initialize and tear down the module) is
 *    separated from the documentation meant for controller application
 *    developers (e.g., messaging format and scheme).
 */

#ifndef __MWU_INTERNAL__
#define __MWU_INTERNAL__

#include "mwu.h"

/* MWU_MAX_MODULES: maximum number of modules supported by mwu
 */
#define MWU_MAX_MODULES 8

/* mwu_internal_init: initialize mwu communications
 *
 * returns: MWU_ERR_SUCCESS on success.  Otherwise:
 *
 * MWU_ERR_COM: Failed to establist listening socket, etc.
 */
enum mwu_error mwu_internal_init(void);

/* mwu_internal_deinit: shut down the mwu communications
 *
 */
void mwu_internal_deinit(void);

/* msg_callback: callback registered by modules to handle mwu messages
 *
 * This callback is registered by a module to handle messages that are sent
 * from control programs via mwu_send_message.
 *
 * msg: a pointer to a the mwu_msg to be handled.
 *
 * response: If the module wishes to send a response message, this callback
 * must set *response = message.  Otherwise, it must set *response = NULL.
 *
 * returns: MWU_ERR_SUCCESS to indicate that the message was handled
 * successfully and the response is valid.  If the response is not NULL, mwu
 * will call the msg_freer callback (or free()) to free the response.
 * Any other response means that the message was not properly handled.  In this
 * case, the msg_freer will not be called.
 *
 * NOTE: Module developers may wish to use mwu_kv_parse() and mwu_kv_get_next()
 * to interpret the received message.
 *
 * NOTE: Response messages are not validated in any way.  It is up to the
 * module developer to ensure that the response contains the key-value pairs as
 * expected by the user and in the proper format.
 *
 * NOTE: callbacks SHOULD NOT BLOCK extensively.  If your module must perform
 * some background work (e.g., gather scan results), consider just launching
 * the background work in the callback and later emitting a message via
 * mwu_internal_send() when the work completes.
 */
typedef enum mwu_error (*msg_callback)(struct mwu_msg *msg,
				       struct mwu_msg **response);

/* msg_freer: callback registered by modules to free message that they
 * allocate.  For example, if mwu_internal_send() is called with a non-NULL
 * resp argument, the module's msg_freer callback will be called to free the
 * message after mwu is done processing it.
 */
typedef void (*msg_freer)(struct mwu_msg *);

/* struct mwu_module: a NXP wireless utility module
 *
 * name: name of the module
 *
 * msg_cb: msg_callback that will be called to handle message
 *
 * msg_free: routine used to free message allocated by this module.  If this is
 * NULL, free() will be called.
 */
struct mwu_module {
	char *name;
	msg_callback msg_cb;
	msg_freer msg_free;
};

/* mwu_internal_register_module: register a module command callback
 *
 * mod: The module to be registered.  The name and msg_cb must members must not
 * be NULL.  If msg_free is NULL, then any messages will be freed using the
 * free() call.
 *
 * returns:
 * MWU_ERR_SUCCESS on success
 * MWU_ERR_BUSY if the maximum number of modules has already been registered.
 * MWU_ERR_INVAL if mod was not populated as expected.
 */
enum mwu_error mwu_internal_register_module(struct mwu_module *mod);

/* mwu_internal_unregister_module: unregister a module
 *
 * mod: mod struct that was passed to mwu_internal_register_module at
 * registration time.
 */
void mwu_internal_unregister_module(struct mwu_module *mod);

/* mwu_internal_send: send the specified message to any listening control
 * programs.
 *
 * msg: the message to send.
 *
 * returns:
 * MWU_ERR_SUCCESS: msg was successfully sent
 *
 * MWU_ERR_COM: message could not be sent, or no client is listening
 *
 * NOTE: Messages are not validated in any way.  It is up to the module
 * developer to ensure that the message contains the module=<module> key-value
 * pair first, and that the rest of the message is in the format expected by
 * the user.
 */
enum mwu_error mwu_internal_send(struct mwu_msg *msg);

/* ALLOC_MSG_OR_FAIL: convenience macro to allocate and populate a message
 *
 * msg: pointer to a struct mwu_msg that will contain the allocated msg
 *
 * size: maximum length of the message.
 *
 * ...: format string (e.g., "module=foo\nkey1=%d\nkey2=%03x\n") and values to
 * populate format string.
 *
 * NOTE: To use this macro, the function calling it must declare a "fail:"
 * label to which the macro can jump if allocation fails.  It must also declare
 * an integer return value called "ret".  See mwu_test.c for examples of how
 * the macro is to be used.
 */
#define ALLOC_MSG_OR_FAIL(msg, size, ...)                                      \
	do {                                                                   \
		msg = malloc(sizeof(struct mwu_msg) + (size));                 \
		if ((msg) == NULL) {                                           \
			ERR("Failed to allocate message: %s: %d\n", __FILE__,  \
			    __LINE__);                                         \
			ret = MWU_ERR_NOMEM;                                   \
			goto fail;                                             \
		}                                                              \
		snprintf((msg)->data, size, __VA_ARGS__);                      \
		(msg)->len = strlen((msg)->data) + 1;                          \
	} while (0)

#endif

/* Convenience macros for mwu strings
 *
 * These macros can be used to conveniently declare format strings for printing
 * and scanning, and the max length of format strings.  Note that the macros
 * with leading underscores should not be invoked.  They are used just as
 * preprocessor magic to allow a macro to be passed as the maxlen argument.
 *
 * Note that the MWU_KV_MAX_* macros do not account for any trailing 0.  This
 * must be added as needed.  But they do account for the required '=' and '\n'
 *
 * Note that often scan fmt strings differ from print fmt strings.  Where a
 * MWU_KV_FMT_X and MWU_KV_SCAN_X both exist, be sure to use one for scanning
 * and the other for printing.
 */
#ifndef UTIL_MACSTR
#error "You must include util.h prior to including mwu_internal.h"
#endif

#define MWU_KV(key, val) key "=" val "\n"
#define MWU_KV_SZ(key, val) sizeof(key) + sizeof(val)

#define _MWU_KV_ONE_LINE_STR(key, maxlen) key "=%" #maxlen "[^\n]\n"
#define MWU_KV_ONE_LINE_STR(key, maxlen) _MWU_KV_ONE_LINE_STR(key, maxlen)
#define _MWU_KV_SCAN_STR(key, maxlen) key "=%" #maxlen "s\n"
#define MWU_KV_SCAN_STR(key, maxlen) _MWU_KV_SCAN_STR(key, maxlen)
#define _MWU_KV_FMT_STR(key, maxlen) key "=%." #maxlen "s\n"
#define MWU_KV_FMT_STR(key, maxlen) _MWU_KV_FMT_STR(key, maxlen)
#define MWU_KV_SZ_STR(key, maxlen) (sizeof(key) + (maxlen) + 1)

#define MWU_KV_FMT_MAC(key) key "=" UTIL_MACSTR "\n"
#define MWU_KV_SZ_MAC(key) (sizeof(key) + 17 + 1)
#define MWU_KV_SZ_NETWORK_KEY(key) (sizeof(key) + 64 + 1)

#define MWU_KV_FMT_IFACE_ID(key) key "=" UTIL_IFACEIDSTR "\n"
#define MWU_KV_SZ_IFACE_ID(key) (sizeof(key) + 19 + 1)

#define MWU_KV_FMT_HEX8(key) key "=%02hX\n"
#define MWU_KV_SZ_HEX8(key) sizeof(key) + 2 + 1

#define MWU_KV_FMT_HEX16(key) key "=%04hX\n"
#define MWU_KV_SZ_HEX16(key) sizeof(key) + 4 + 1

#define MWU_KV_FMT_DEC32(key) key "=%d\n"
#define MWU_KV_SZ_DEC32(key) sizeof(key) + sizeof("-2147483648") + 1

#define MWU_KV_FMT_FLOAT01(key) key "=%.01f\n"
#define MWU_KV_SZ_FLOAT01(key) sizeof(key) + sizeof("-2147483648") + 1

#define MWU_KV_FMT_FLOAT(key) key "=%f\n"
#define MWU_KV_SZ_FLOAT(key) sizeof(key) + sizeof("-2147483648") + 1

#define MWU_KV_FMT_DOUBLE(key) key "=%lf\n"
#define MWU_KV_FMT_U8(key) key "=%hhu\n"

/* Many message contain a single decimal status=<status> response. */
#define MWU_KV_FMT_STATUS MWU_KV_FMT_DEC32("status")
#define MWU_KV_SZ_STATUS MWU_KV_SZ_DEC32("status")
#define ALLOC_STATUS_MSG_OR_FAIL(msg, status)                                  \
	ALLOC_MSG_OR_FAIL(msg, MWU_KV_SZ_STATUS + 1, MWU_KV_FMT_STATUS, status)
