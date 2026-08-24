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
/* mwu.c: marvell wireless utility message channel
 *
 * This is the implementation of the mwu APIs defined in mwu.h and
 * mwu_internal.h.  It implements the actual socket communication between the
 * control app (client) and the daemon (server).  It uses a unix domain socket
 * for IPC.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "wps_msg.h"
#include "wps_os.h"
#include "mwu.h"

#ifdef UTIL_LOG_TAG
#undef UTIL_LOG_TAG
#endif
#define UTIL_LOG_TAG "MWU"
#include "util.h"
#include "mwu_internal.h"

/* TODO: We need various utility functions (e.g., mwu_printf()) from these
 * include files.  Such functionality should be migrated out of the
 * wps-specific headers and into generic ones.
 */
#include "mwu_log.h"
#include "wps_wlan.h"

#ifdef ANDROID
#define MWU_MSG_FIFO "/tmp/mwu.fifo"
#define MWU_EVENT_FIFO "/tmp/mwu-event.fifo"
#else
#include <unistd.h> /* for close() read() */
#define MWU_MSG_FIFO "/var/run/mwu.fifo"
#define MWU_EVENT_FIFO "/var/run/mwu-event.fifo"
#endif

#ifdef MWU_IPC_UDP
#define MWU_MSG_PORT 9096
#define MWU_EVENT_PORT 9097
#ifndef S_LEN
#define S_LEN(su) (sizeof(*(su)))
#endif
#else
#ifndef S_LEN
#define S_LEN(su)                                                              \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif
#endif

#if 0
/*
 * Create an AF_INET Address:
 *
 * args
 * 1. ap        Ptr to area  where address is to be placed.
 * 2. addr      The input string format hostname, and port.
 *              Hostname and port are expected to be strictly numeric. Eg. - 192.168.10.30:27018
 *              port name must be a positive integer less than 32768
 * 3. protocol  The input string indicating the protocol being used. NULL is tcp.
 *
 * return
 *  0 Success.
 * -1 Bad host part.
 * -2 Bad port part.
 */
static int make_addr(struct sockaddr_in *ap, char *addr_in, char *protocol) {
    char *host, *port;
    char *addr = strdup(addr_in);
    host = strtok(addr, ":" );
    port = strtok(NULL, "\n" );
    int pt;

    /* Initialize address structure */
    memset(ap,0, sizeof(struct sockaddr_in));
    ap->sin_family = AF_INET;
    ap->sin_port = 0;
    ap->sin_addr.s_addr = INADDR_ANY;

    /* Fill in Numeric host IP address */
        ap->sin_addr.s_addr = inet_addr(host);
        if ( !inet_aton(host,&ap->sin_addr) ) {
            return -1;
        }
    /* Process an optional Numeric port  */
        pt = atoi(port);
        if ( pt < 0L || pt >= 32768 ) {
            return -2;
        }
        ap->sin_port = htons( (short)pt);

    free(addr);
    return 0;
}
#endif

struct mwu_module *registered_modules[MWU_MAX_MODULES];

#ifdef MWU_IPC_UDP
static int new_udp_listener(short port)
{
	struct sockaddr_in adr_srv;
	int ret, len_srvr, srv_socket;
	int so_broadcast = 1;
	static char *e_addr_str = "127.0.0.1:9097",
		    *m_addr_str = "127.0.0.1:9096";

	/* form server address */
	ret = make_addr(&adr_srv,
			(port == MWU_EVENT_PORT) ? e_addr_str : m_addr_str,
			"udp");
	len_srvr = sizeof(struct sockaddr_in);

	/* create a udp socket for use */
	srv_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (srv_socket == -1) {
		ERR("Failed to create mwu event listener.\n");
		ret = -1;
		goto fail;
	}

	/* allow broadcast */
	if (port == MWU_EVENT_PORT) {
		ret = setsockopt(srv_socket, SOL_SOCKET, SO_BROADCAST,
				 &so_broadcast, sizeof so_broadcast);
		if (ret != 0) {
			ERR("Failed to set socket options.\n");
			ret = -1;
			goto fail;
		}
	}

	/* bind */
	ret = bind(srv_socket, (struct sockaddr *)&adr_srv, len_srvr);
	if (ret != 0) {
		ERR("Failed to bind: %s\n.", strerror(errno));
		ret = -1;
		goto fail;
	}

	return srv_socket;
fail:
	if (srv_socket >= 0)
		close(srv_socket);
	return ret;
}
#endif

static struct mwu_module *lookup_module(char *name)
{
	struct mwu_module *m = NULL;
	int i;
	ENTER();
	for (i = 0; i < MWU_MAX_MODULES; i++) {
		m = registered_modules[i];
		if (m != NULL && m->name[0] != 0 &&
		    strcmp(m->name, name) == 0) {
			return m;
			LEAVE();
		}
	}
	LEAVE();
	return NULL;
}

enum mwu_error mwu_internal_register_module(struct mwu_module *mod)
{
	struct mwu_module *m;
	int i;
	ENTER();

	if (mod == NULL || mod->name == NULL || mod->name[0] == 0 ||
	    mod->msg_cb == NULL) {
		ERR("Can't register module without valid name\n");
		LEAVE();
		return MWU_ERR_INVAL;
	}

	INFO("Registering module %s\n", mod->name);

	/* don't allow modules with identical names */
	m = lookup_module(mod->name);
	if (m != NULL) {
		ERR("Module named %s already registered\n", m->name);
		LEAVE();
		return MWU_ERR_INVAL;
	}

	for (i = 0; i < MWU_MAX_MODULES; i++) {
		if (registered_modules[i] == NULL) {
			registered_modules[i] = mod;
			LEAVE();
			return MWU_ERR_SUCCESS;
		}
	}

	INFO("Registration failed.  Too many modules.\n");
	LEAVE();
	return MWU_ERR_BUSY;
}

void mwu_internal_unregister_module(struct mwu_module *mod)
{
	int i;

	ENTER();
	INFO("Unregistering module %s\n", mod->name);

	if (mod == NULL || mod->name == NULL || mod->name[0] == 0) {
		/* surely, we did not register this module */
		LEAVE();
		return;
	}

	for (i = 0; i < MWU_MAX_MODULES; i++) {
		if (registered_modules[i] == mod)
			registered_modules[i] = NULL;
	}
	LEAVE();
}

/* sockets for messages and events */
static int mclient = -1;
static int mlistener = -1;
static int eclient = -1;
static int elistener = -1;

#ifdef MWU_IPC_UDP
/* addresses for messages and events */
struct sockaddr_in *e_client_addr;
struct sockaddr_in *m_client_addr;
#endif

/* The client that we are handling here not only expects us to receive a
 * message, but also expects us to send a response.  If we don't send that
 * response, the client will block forever.  If we can't allocate memory or
 * something else fatal, there's not much we can do.  But for error cases that
 * we can handle, we should send back a status.
 */
static void handle_mclient(int sock, void *sock_ctx)
{
	int ret = 0;
	int status = MWU_ERR_SUCCESS;
	struct mwu_msg *msg = NULL, *resp = NULL;
	struct mwu_msg *tmp_buffer = NULL;
	char *modname = NULL;
	struct mwu_module *mod = NULL;
#ifdef MWU_IPC_UDP
	struct sockaddr_in client_addr;
	socklen_t len = sizeof(struct sockaddr);
#endif

	ENTER();
	if (sock != mclient) {
		ERR("Internal error.  Socket does not match expected socket.\n");
		LEAVE();
		return;
	}

	if (mclient == -1) {
		ERR("Got msg on closed socket!\n");
		LEAVE();
		return;
	}

	msg = malloc(sizeof(struct mwu_msg));
	if (!msg) {
		ERR("Failed to alloc message header.\n");
		goto done;
	}
#ifdef MWU_IPC_UDP
	ret = recvfrom(sock, msg, sizeof(struct mwu_msg), MSG_PEEK,
		       (struct sockaddr *)&client_addr, &len);
#else
	ret = recv(sock, msg, sizeof(struct mwu_msg), MSG_PEEK);
#endif

	if (ret == -1) {
		/* TODO: when the control program calls mwu_disconnect, we get
		 * ECONNRESET here.  But after that we get a zero-length
		 * response (i.e., ret=0).  Not sure why.  It would be nice to
		 * just get one shut-down event.
		 */
		ERR("Failed to recv msg from client: %s.\n", strerror(errno));
		goto done;
	}

	if (ret == 0) {
		/* an empty message means that the client wants to disconnect.
		 */
		INFO("Got empty message.  Disconnecting client.\n");
		wps_unregister_rdsock_handler(mclient);
		close(mclient);
		mclient = -1;
		goto done;
	}

	if (ret < (int)sizeof(struct mwu_msg)) {
		ERR("msg did not contain complete header (ret = %d).\n", ret);
		goto done;
	}

	tmp_buffer = realloc(msg, sizeof(struct mwu_msg) + msg->len);
	if (!tmp_buffer) {
		ERR("Failed to alloc space for msg body.\n");
		goto done;
	} else {
		msg = tmp_buffer;
		tmp_buffer = NULL;
	}
	ret = recv(sock, msg, sizeof(struct mwu_msg) + msg->len, 0);
	if (ret == -1) {
		ERR("Failed to recv msg body from client: %s\n",
		    strerror(errno));
		goto done;
	}

	/* Okay.  We have a message from a control client.  Check for the
	 * module=<module>, lookup the registered module, and pass the message.
	 */

	/* how long is the module name? */
	modname = strchr(msg->data, '\n');
	if (modname == NULL) {
		ERR("No valid key-value pairs in message.\n");
		status = MWU_ERR_INVAL;
		goto send_response;
	}
	modname = malloc(modname - msg->data + 1);
	if (modname == NULL) {
		ERR("Failed to allocate space for module name.\n");
		status = MWU_ERR_NOMEM;
		goto send_response;
	}
	ret = sscanf(msg->data, "module=%s\n", modname);
	if (ret != 1) {
		ERR("Failed to find module=<module> key-value pair in message\n");
		status = MWU_ERR_INVAL;
		goto send_response;
	}

	mod = lookup_module(modname);
	if (mod == NULL) {
		ERR("No registered module named %s\n", modname);
		status = MWU_ERR_NO_MODULE;
		goto send_response;
	}
	ret = mod->msg_cb(msg, &resp);
	if (ret != MWU_ERR_SUCCESS) {
		/* Don't bother cluttering the log with a message here.  The
		 * module should do that for us.
		 */
		resp = NULL;
		status = ret;
		goto send_response;
	}

	/* send the response */
send_response:

	if (resp == NULL) {
		msg->len = 0;
		msg->status = status;
#ifdef MWU_IPC_UDP
		ret = sendto(sock, msg, sizeof(struct mwu_msg), 0,
			     (struct sockaddr *)&client_addr,
			     sizeof(struct sockaddr));
#else
		ret = send(sock, msg, sizeof(struct mwu_msg), 0);
#endif
	} else {
		/* If the module actually provided a response, we have always
		 * succeeded.
		 */
		resp->status = status;
#ifdef MWU_IPC_UDP
		ret = sendto(sock, resp, sizeof(struct mwu_msg) + resp->len, 0,
			     (struct sockaddr *)&client_addr,
			     sizeof(struct sockaddr));
#else
		ret = send(sock, resp, sizeof(struct mwu_msg) + resp->len, 0);
#endif
	}

	if (ret == -1) {
		ERR("Failed to send response to client: %s\n", strerror(errno));
		goto done;
	}

done:
	FREE(msg);

	FREE(modname);

	if (resp) {
		if (mod && mod->msg_free)
			mod->msg_free(resp);
		else
			FREE(resp);
	}
	LEAVE();
}

#if 1
/* We should never _get_ events from the client.  We only send them.  So this
 * handler is really just a dummy handler that cleanly shuts down the socket
 * when it reads 0 bytes.
 */
static void handle_eclient(int sock, void *sock_ctx)
{
	int ret;
	char buf[4];

	ENTER();

	if (sock != eclient) {
		ERR("Internal error.  Socket does not match expected event socket.\n");
		LEAVE();
		return;
	}

	ret = read(eclient, buf, sizeof(buf));
	if (ret < 0) {
		ERR("Error reading from event socket: %s\n", strerror(errno));
	} else if (ret > 0) {
		ERR("Unexpected data from event client.\n");
	} else {
		INFO("Closing event client.\n");
		wps_unregister_rdsock_handler(eclient);
		close(eclient);
		eclient = -1;
	}
	LEAVE();
	return;
}
#endif

#ifndef MWU_IPC_UDP
/* accept a connection from listening sock and return client sock.  Register
 * the handler to handle the client sock.
 */
static int accept_connection(int sock,
			     void (*handler)(int sock, void *sock_ctx),
			     void *data)
{
	struct sockaddr_un name;
	socklen_t len;
	int ret, client;

	ENTER();

	len = sizeof(name);
	client = accept(sock, (struct sockaddr *)&name, &len);
	if (client == -1) {
		ERR("Failed to accept connection: %s\n", strerror(errno));
		LEAVE();
		return -1;
	}
#if 1
	ret = wps_register_rdsock_handler(client, handler, data);
	if (ret != WPS_STATUS_SUCCESS) {
		ERR("Failed to register new connection\n");
		LEAVE();
		return -1;
	}
#endif

	LEAVE();
	return client;
}
#endif

void mwu_mlistener(int sock, void *sock_ctx)
{
	ENTER();
#ifndef MWU_IPC_UDP
	if (mclient != -1) {
		ERR("Failed to accept connection: busy. %d\n", mclient);
		LEAVE();
		return;
	}
#endif
#ifdef MWU_IPC_UDP
	mclient = sock;
	handle_mclient(sock, NULL);
#else
	mclient = accept_connection(sock, handle_mclient, NULL);
#endif
	if (mclient == -1) {
		ERR("Failed to accept control connection.\n");
		LEAVE();
		return;
	}
}

#if 1
static void mwu_elistener(int sock, void *sock_ctx)
{
	ENTER();
	if (eclient != -1) {
		ERR("Failed to accept event connection: busy.\n");
		LEAVE();
		return;
	}

#ifdef MWU_IPC_UDP
	eclient = sock;
	handle_eclient(sock, NULL);
#else
	eclient = accept_connection(sock, handle_eclient, NULL);
#endif
	if (eclient == -1) {
		ERR("Failed to accept event connection.\n");
		LEAVE();
		return;
	}
}
#endif

#ifndef MWU_IPC_UDP
/* create a listening socket at path */
static int new_un_listener(char *path)
{
	int s, ret;
	struct sockaddr_un addr;

	ENTER();
	/* Create a socket that listens for for connections */
	s = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (s == -1) {
		ERR("Failed to create listening socket: %s\n", strerror(errno));
		LEAVE();
		return -1;
	}

	/* unlink any stale hangers on.  The caller is responsible for ensuring
	 * that there are no other instances of the UI server running.
	 */
	unlink(path);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));
	ret = bind(s, (struct sockaddr *)&addr, S_LEN(&addr));
	if (ret == -1) {
		ERR("Failed to bind listening socket: %s\n", strerror(errno));
		LEAVE();
		return -1;
	}

	/* Listen for connections. */
	ret = listen(s, 5);
	if (ret == -1) {
		ERR("Failed to listen on socket: %s\n", strerror(errno));
		LEAVE();
		return -1;
	}
	LEAVE();
	return s;
}
#endif

/* we store the last few events so we can replay them through the
 * "get_next_event" facility.  All it does is send the next cached event from
 * our fifo.  This is really only used for test purposes.  A real system should
 * not depend on this capability for event fetching.
 */

struct cached_event {
	struct mwu_msg *msg;
	STAILQ_ENTRY(cached_event) list_item;
};
STAILQ_HEAD(event_list, cached_event);
static struct event_list events;

#define MAX_CACHED_EVENTS 300

static int num_events;

static void mwu_internal_free_cached_events(void)
{
	struct cached_event *e;

	while (!STAILQ_EMPTY(&events)) {
		e = STAILQ_FIRST(&events);
		STAILQ_REMOVE_HEAD(&events, list_item);
		FREE(e->msg);
		FREE(e);
	}
	num_events = 0;
}

enum mwu_error handle_mwu_cmd(struct mwu_msg *msg, struct mwu_msg **resp)
{
	struct cached_event *e;
	*resp = NULL;

	ENTER();

	if (strcmp(msg->data, "module=mwu\ncmd=get_next_event\n") == 0) {
		/* If there's an event in the queue, pop it out and return it */
		if (num_events == 0)
			return MWU_ERR_SUCCESS;
		e = STAILQ_FIRST(&events);
		if (!e) {
			WARN("event queue unexpectedly empty!");
			LEAVE();
			return MWU_ERR_SUCCESS;
		}
		STAILQ_REMOVE_HEAD(&events, list_item);
		num_events--;
		*resp = e->msg;
		/* We no longer need the container.  msg will be freed by our
		 * caller. */
		FREE(e);

	} else if (strcmp(msg->data, "module=mwu\ncmd=clear_events\n") == 0) {
		mwu_internal_free_cached_events();
	} else {
		ERR("Ignoring unknown command: %s\n", msg->data);
	}
	LEAVE();
	return MWU_ERR_SUCCESS;
}

struct mwu_module mwu_mod = {
	.name = "mwu",
	.msg_cb = handle_mwu_cmd,
	.msg_free = NULL,
};

enum mwu_error mwu_internal_init(void)
{
	int ret;
	printf("mwu_internal_init");
	ENTER();
	if (mlistener != -1 || elistener != -1) {
		ERR("Can't launch mwu server.  Busy.\n");
		LEAVE();
		return MWU_ERR_BUSY;
	}

#ifdef MWU_IPC_UDP
	mlistener = new_udp_listener(MWU_MSG_PORT);
#else
	mlistener = new_un_listener(MWU_MSG_FIFO);
#endif
	if (mlistener == -1) {
		ERR("Failed to create mwu message listener.\n");
		ret = MWU_ERR_COM;
		goto fail;
	}
#if 1
	ret = wps_register_rdsock_handler(mlistener, mwu_mlistener, NULL);
	if (ret != 0) {
		ret = MWU_ERR_COM;
		goto fail;
	}
#endif

#ifdef MWU_IPC_UDP
	elistener = new_udp_listener(MWU_EVENT_PORT);
	eclient = elistener;
#else
	elistener = new_un_listener(MWU_EVENT_FIFO);
#endif
	if (elistener == -1) {
		ERR("Failed to create mwu event listener.\n");
		ret = MWU_ERR_COM;
		goto fail;
	}
#if 1
	ret = wps_register_rdsock_handler(elistener, mwu_elistener, NULL);
	if (ret != 0) {
		ret = MWU_ERR_COM;
		goto fail;
	}
#endif
	memset(registered_modules, 0, sizeof(registered_modules));

	/* add ourselves as a module */
	if (!STAILQ_EMPTY(&events))
		WARN("events queue is not empty!  This is a memory leak!\n");

	STAILQ_INIT(&events);
	mwu_internal_register_module(&mwu_mod);

	INFO("Launched mwu server.\n");
	LEAVE();
	return 0;

fail:
	mwu_internal_deinit();
	LEAVE();
	return ret;
}

void mwu_internal_deinit(void)
{
	ENTER();
#if 1
	if (mclient != -1) {
		wps_unregister_rdsock_handler(mclient);
		close(mclient);
		mclient = -1;
	}

	if (mlistener != -1) {
		wps_unregister_rdsock_handler(mlistener);
		close(mlistener);
		mlistener = -1;
	}

	if (eclient != -1) {
		wps_unregister_rdsock_handler(eclient);
		close(eclient);
		eclient = -1;
	}

	if (elistener != -1) {
		wps_unregister_rdsock_handler(elistener);
		close(elistener);
		elistener = -1;
	}
#endif
	remove(MWU_MSG_FIFO);
	remove(MWU_EVENT_FIFO);
	mwu_internal_free_cached_events();
	LEAVE();
}

enum mwu_error mwu_internal_send(struct mwu_msg *msg)
{
	int ret = 0;
	struct cached_event *e;
#ifdef MWU_IPC_UDP
	struct sockaddr_in sock_bc;
	static char *bcast = "127.255.255.255:9097";
#endif

	ENTER();
	if (msg == NULL) {
		ERR("Can't send invalid message.\n");
		LEAVE();
		return MWU_ERR_INVAL;
	}

	/* record the event in our event cache */
	if (num_events == MAX_CACHED_EVENTS) {
		WARN("Non fatal: Event cache is full.  Refusing to cache event\n");
	} else {
		e = malloc(sizeof(struct cached_event));
		if (!e) {
			WARN("Failed to put event into cache (no mem)\n");
		} else {
			e->msg = malloc(sizeof(struct mwu_msg) + msg->len);
			if (!e->msg) {
				WARN("Failed to copy event (no mem)\n");
				FREE(e);
			} else {
				memcpy(e->msg, msg,
				       sizeof(struct mwu_msg) + msg->len);
				STAILQ_INSERT_TAIL(&events, e, list_item);
				num_events++;
			}
		}
	}
#ifdef MWU_IPC_UDP
	/* In case of UDP IPC, we always broadcast events, no matter any client
	 * is connected or not. This allows us to have multiple clients
	 * listening to the events, its clients responsibility to consume the
	 * event depending upon the module, and iface parmas in the sent event.
	 */
#else
	if (eclient == -1) {
		ERR("Can't send event.  No client connected.\n");
		LEAVE();
		return MWU_ERR_COM;
	}
#endif
#ifdef MWU_IPC_UDP
	ret = make_addr(&sock_bc, /* Returned address */
			bcast, /* Input string addr */
			"udp"); /* UDP protocol */

	INFO("make_addr called. RET=%d", ret);

	ret = sendto(eclient, msg, sizeof(struct mwu_msg) + msg->len, 0,
		     (struct sockaddr *)&sock_bc, sizeof(struct sockaddr));
#else
	ret = sendto(eclient, msg, sizeof(struct mwu_msg) + msg->len, 0, NULL,
		     0);
#endif
	if (ret != (int)sizeof(struct mwu_msg) + msg->len) {
		ERR("Failed to send event to client: %s\n", strerror(errno));
		ret = MWU_ERR_COM;
		goto fail;
	}

	INFO("Sent event to client - \n");
	INFO("%s\n", msg->data);
	LEAVE();
	return MWU_ERR_SUCCESS;

fail:
	LEAVE();
	return ret;
}

#if 0
static enum mwu_error mwu_connect(struct mwu *mwu)
{

    int ret = 0;
#ifdef MWU_IPC_UDP
    struct sockaddr_in m_addr, e_addr;  /* AF_INET */
    int len_inet;            /* length */
    static int so_reuseaddr = 1;
#else
    struct sockaddr_un addr;
#endif


    ret = pthread_mutex_init(&mwu->mfd_mutex, NULL);
    if (ret) {
        goto fail;
    }

    /* we need two sockets: one to send messages and receive message responses
     * via mwu_send_message(), and the other to receive asynchronous message
     * (i.e., events) that are passed via mwu_recv_message().  Set up the
     * mwu_send_message() socket first.
     */
#ifdef MWU_IPC_UDP
    mwu->mfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(mwu->mfd == -1) {
        return MWU_ERR_COM;
    }
    len_inet = sizeof m_addr;
#if 0
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(MWU_MSG_PORT);
    m_addr.sin_addr.s_addr = htonl((127 << 24) | 1);
#endif

#if 0
   /* Allow multiple listeners on the broadcast address: */
    ret = setsockopt(mwu->mfd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr));
    if ( ret == -1 ) {
        ERR("Failed to set socket options");
        ret = MWU_ERR_COM;
        goto fail;
    }
#endif
    ret = make_addr(
	 		  &m_addr,  /* Returned address */
			  "127.0.0.1:9096",    /* Input string addr */
			  "udp");     /* UDP protocol */

#if 0
    /* Bind our socket to this address: */
    ret = bind(mwu->mfd, (struct sockaddr *)&m_addr, len_inet);
    if ( ret == -1 ) {
        printf("Failed to bind() msg sock: %s.\n", strerror(errno));
        ret = MWU_ERR_COM;
        goto fail;
    }
#endif
    ret = connect(mwu->mfd, (struct sockaddr *)&m_addr, sizeof(struct sockaddr));
#else
    mwu->mfd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
    if(mwu->mfd == -1) {
        printf("Failed to create event socket\n");
        ret = MWU_ERR_COM;
        goto fail;
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MWU_MSG_FIFO, sizeof(addr.sun_path));
    ret = connect(mwu->mfd, (struct sockaddr *)&addr, S_LEN(&addr));
#endif
    if (ret == -1) {
        printf("Failed to connect to mwu: %s\n", strerror(errno));
        printf("Daemon not running?  Permissions?\n");
        printf("Try again.\n");
        ret = MWU_ERR_COM;
        goto fail;
    }


    /* ...now set up the mwu_recv_message() socket */
#ifdef MWU_IPC_UDP
    mwu->efd = socket(AF_INET, SOCK_DGRAM, 0);
    if(mwu->efd == -1) {
        printf("Failed to create event socket: %s\n", strerror(errno));
        return MWU_ERR_COM;
    }
    len_inet = sizeof e_addr;
#if 0
    memset(&m_addr, 0, sizeof(e_addr));
    e_addr.sin_family = AF_INET;
    e_addr.sin_port = htons(MWU_EVENT_PORT);
    e_addr.sin_addr.s_addr = htonl((127 << 24) | 1);
#endif
    ret = make_addr(&e_addr, "127.255.255.255:9097", "udp");

    /* Allow multiple listeners on the broadcast address: */
    ret = setsockopt(mwu->efd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr));
    if ( ret == -1 ) {
        printf("Failed to set socket options on event socket: %s\n. ", strerror(errno));
        ret = MWU_ERR_COM;
        goto fail;
    }

    /* Bind our socket to the broadcast address: */
    ret = bind(mwu->efd, (struct sockaddr *)&e_addr, len_inet);
    if ( ret == -1 ) {
        printf("Failed to bind() event sock: %s.\n", strerror(errno));
        ret = MWU_ERR_COM;
        goto fail;
    }
    ret = 0;
#else
    mwu->efd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
    if(mwu->efd == -1) {
        printf("Failed to create event socket: %s\n", strerror(errno));
        printf("Leave: mwu_connect()");
        return MWU_ERR_COM;
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MWU_EVENT_FIFO, sizeof(addr.sun_path));
    ret = connect(mwu->efd, (struct sockaddr *)&addr, S_LEN(&addr));
#endif
    if (ret == -1) {
        printf("Failed to connect to mwu: %s\n", strerror(errno));
        printf("Daemon not running?  Permissions?\n");
        printf("Try again.\n");
        ret = MWU_ERR_COM;
        goto fail;
    }
    return MWU_ERR_SUCCESS;

fail:
    mwu_disconnect(mwu);
    return ret;
}

static void mwu_disconnect(struct mwu *mwu)
{
    INFO("Disconnecting from mwu\n");
    if (mwu->mfd != -1) {
        close(mwu->mfd);
        mwu->mfd = -1;
    }

    if (mwu->efd != -1) {
        close(mwu->efd);
        mwu->efd = -1;
    }

    pthread_mutex_destroy(&mwu->mfd_mutex);
}

static enum mwu_error mwu_send_message(struct mwu *mwu, struct mwu_msg *msg,
                                struct mwu_msg **resp)
{

    int ret = 0;
    char module[2];
    struct mwu_msg *tmp_buffer = NULL;

    *resp = NULL;

    // To avoid this function to be re-entry by another thread
    pthread_mutex_lock(&mwu->mfd_mutex);

    ret = sscanf(msg->data, "module=%1s\n", module);
    if (ret != 1) {
        ERR("Failed to find module=<module> key-value pair in message\n");
        ret = MWU_ERR_INVAL;
        goto done;
    }
    ret = sendto(mwu->mfd, msg, msg->len + sizeof(struct mwu_msg), 0, NULL, 0);
    if (ret != (int)sizeof(struct mwu_msg) + msg->len) {
        ERR("Failed to send message. %s.\n", strerror(errno));
        ret = MWU_ERR_COM;
        goto done;
    }

    /* get the result */
    *resp = malloc(sizeof(struct mwu_msg));
    if (!*resp) {
        ERR("Failed to alloc response message.\n");
        ret = MWU_ERR_NOMEM;
        goto done;
    }

    /* TODO: this should timeout instead of blocking forever. */

    /* recv the header */
    ret = recv(mwu->mfd, *resp, sizeof(struct mwu_msg), MSG_PEEK);
    if (ret == -1) {
        ERR("Failed to recv response.\n");
        ret = MWU_ERR_COM;
        goto done;
    }

    if (ret < (int)sizeof(struct mwu_msg)) {
        ERR("Failed recv complete message header (ret=%d).\n", ret);
        ret = MWU_ERR_COM;
        goto done;
    }

    /* if we're done, flush the socket and quit early */
    if ((*resp)->len == 0) {
        recv(mwu->mfd, *resp, sizeof(struct mwu_msg), 0);
        ret = (*resp)->status;
        FREE(*resp);
        goto done;
    }

    tmp_buffer = realloc(*resp, sizeof(struct mwu_msg) + (*resp)->len);
    if (!tmp_buffer) {
        ERR("Failed to alloc space for command response data.\n");
        ret = MWU_ERR_NOMEM;
        goto done;
    } else {
        *resp = tmp_buffer;
        tmp_buffer = NULL;
    }

    ret = recv(mwu->mfd, *resp, sizeof(struct mwu_msg) + (*resp)->len, 0);
    if (ret == -1) {
        ERR("Failed to recv resp data.\n");
        ret = MWU_ERR_COM;
        goto done;
    }
    ret = MWU_ERR_SUCCESS;

done:
    pthread_mutex_unlock(&mwu->mfd_mutex);

    if (ret != MWU_ERR_SUCCESS && *resp) {
        FREE(*resp);
    }

    return ret;
}

static void mwu_free_msg(struct mwu_msg *msg)
{
    FREE(msg);
}

static enum mwu_error mwu_recv_message(struct mwu *mwu, struct mwu_msg **msg)
{
    int ret = MWU_ERR_SUCCESS;
    struct mwu_msg *tmp_buffer;

    *msg = malloc(sizeof(struct mwu_msg));
    if (!*msg) {
        ERR("Failed to alloc message.\n");
        ret = MWU_ERR_NOMEM;
        goto done;
    }

    /* recv the header */
    ret = recv(mwu->efd, *msg, sizeof(struct mwu_msg), MSG_PEEK);
    if (ret == -1) {
        ERR("Failed to recv message: %s\n", strerror(errno));
        ret = MWU_ERR_COM;
        goto done;
    } else if (ret == 0) {
        WARN("Recv EOF from mwu eclient: mwu is terminated");
        ret = MWU_ERR_COM;
        goto done;
    }

    tmp_buffer = realloc(*msg, sizeof(struct mwu_msg) + (*msg)->len);
    if (!tmp_buffer) {
        ERR("Failed to alloc space for message body.\n");
        ret = MWU_ERR_NOMEM;
        goto done;
    } else {
        *msg = tmp_buffer;
        tmp_buffer = NULL;
    }
    ret = recv(mwu->efd, *msg, sizeof(struct mwu_msg) + (*msg)->len, 0);
    if (ret == -1) {
        ERR("Failed to recv msg data: %s\n", strerror(errno));
        ret = MWU_ERR_COM;
        goto done;
    }
    ret = MWU_ERR_SUCCESS;

done:
    if (ret != MWU_ERR_SUCCESS && *msg) {
        FREE(*msg);
    }
    return ret;
}
#endif
