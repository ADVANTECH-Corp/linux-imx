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
/*
 * Create an AF_INET Address:
 *
 * args
 * 1. ap        Ptr to area  where address is to be placed.
 * 2. addr      The input string format hostname, and port.
 *              Hostname and port are expected to be strictly numeric. Eg. -
 * 192.168.10.30:27018 port name must be a positive integer less than 32768
 * 3. protocol  The input string indicating the protocol being used. NULL is
 * tcp.
 *
 * return
 *  0 Success.
 * -1 Bad host part.
 * -2 Bad port part.
 */
int make_addr(struct sockaddr_in *ap, char *addr_in, char *protocol)
{
	char *host, *port;
	char *addr = strdup(addr_in);
	host = strtok(addr, ":");
	port = strtok(NULL, "\n");
	int pt;

	/* Initialize address structure */
	memset(ap, 0, sizeof(struct sockaddr_in));
	ap->sin_family = AF_INET;
	ap->sin_port = 0;
	ap->sin_addr.s_addr = INADDR_ANY;

	/* Fill in Numeric host IP address */
	ap->sin_addr.s_addr = inet_addr(host);
	if (!inet_aton(host, &ap->sin_addr)) {
		return -1;
	}
	/* Process an optional Numeric port  */
	pt = atoi(port);
	if (pt < 0L || pt >= 32768) {
		return -2;
	}
	ap->sin_port = htons((short)pt);

	free(addr);

	return 0;
}

enum mwu_error mwu_connect(struct mwu *mwu)
{
	int ret = 0;
#ifdef MWU_IPC_UDP
	struct sockaddr_in m_addr, e_addr; /* AF_INET */
	int len_inet; /* length */
	static int so_reuseaddr = 1;
#else
	struct sockaddr_un addr;
#endif

	ret = pthread_mutex_init(&mwu->mfd_mutex, NULL);
	if (ret) {
		goto fail;
	}

	/* we need two sockets: one to send messages and receive message
	 * responses via mwu_send_message(), and the other to receive
	 * asynchronous message (i.e., events) that are passed via
	 * mwu_recv_message().  Set up the mwu_send_message() socket first.
	 */
#ifdef MWU_IPC_UDP
	mwu->mfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (mwu->mfd == -1) {
		return MWU_ERR_COM;
	}
	len_inet = sizeof m_addr;
#if 1
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sin_family = AF_INET;
	m_addr.sin_port = htons(MWU_MSG_PORT);
	m_addr.sin_addr.s_addr = htonl((127 << 24) | 1);
#endif

#if 1
	/* Allow multiple listeners on the broadcast address: */
	ret = setsockopt(mwu->mfd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr,
			 sizeof(so_reuseaddr));
	if (ret == -1) {
		ERR("Failed to set socket options");
		ret = MWU_ERR_COM;
		goto fail;
	}
#endif
	ret = make_addr(&m_addr, /* Returned address */
			"127.0.0.1:9096", /* Input string addr */
			"udp"); /* UDP protocol */

#if 1
	/* Bind our socket to this address: */
	ret = bind(mwu->mfd, (struct sockaddr *)&m_addr, len_inet);
	if (ret == -1) {
		printf("Failed to bind() msg sock: %s.\n", strerror(errno));
		ret = MWU_ERR_COM;
		goto fail;
	}
#endif
	ret = connect(mwu->mfd, (struct sockaddr *)&m_addr,
		      sizeof(struct sockaddr));
#else
	mwu->mfd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (mwu->mfd == -1) {
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
	if (mwu->efd == -1) {
		printf("Failed to create event socket: %s\n", strerror(errno));
		return MWU_ERR_COM;
	}
	len_inet = sizeof e_addr;
#if 1
	memset(&m_addr, 0, sizeof(e_addr));
	e_addr.sin_family = AF_INET;
	e_addr.sin_port = htons(MWU_EVENT_PORT);
	e_addr.sin_addr.s_addr = htonl((127 << 24) | 1);
#endif
	ret = make_addr(&e_addr, "127.255.255.255:9097", "udp");

	/* Allow multiple listeners on the broadcast address: */
	ret = setsockopt(mwu->efd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr,
			 sizeof(so_reuseaddr));
	if (ret == -1) {
		printf("Failed to set socket options on event socket: %s\n. ",
		       strerror(errno));
		ret = MWU_ERR_COM;
		goto fail;
	}

	/* Bind our socket to the broadcast address: */
	ret = bind(mwu->efd, (struct sockaddr *)&e_addr, len_inet);
	if (ret == -1) {
		printf("Failed to bind() event sock: %s.\n", strerror(errno));
		ret = MWU_ERR_COM;
		goto fail;
	}
	ret = 0;
#else
	mwu->efd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (mwu->efd == -1) {
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

void mwu_disconnect(struct mwu *mwu)
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

enum mwu_error mwu_send_message(struct mwu *mwu, struct mwu_msg *msg,
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
	ret = sendto(mwu->mfd, msg, msg->len + sizeof(struct mwu_msg), 0, NULL,
		     0);
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

void mwu_free_msg(struct mwu_msg *msg)
{
	FREE(msg);
}

enum mwu_error mwu_recv_message(struct mwu *mwu, struct mwu_msg **msg)
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
