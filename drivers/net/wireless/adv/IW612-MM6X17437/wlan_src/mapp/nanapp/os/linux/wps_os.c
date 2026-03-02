/** @file wps_os.c
 *  @brief This file contains timer and socket read functions.
 *
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <netinet/in.h>
#if defined(ANDROID) && !defined(HAVE_GLIBC)
#include <linux/if.h>
#else
#include <unistd.h>
#endif
#include <linux/netlink.h>
#include <arpa/inet.h>

#ifdef ANDROID
#include "wps_msg.h"
#endif
#include "wps_def.h"
#include "mwu_log.h"
#include "util.h"
#include "wps_l2.h"
#include "wps_eapol.h"
#include "wps_os.h"
#include "wps_wlan.h"
#include "mwu_defs.h"

#ifdef CONFIG_WPS_UPNP
#include "mwu_upnp.h"
#endif

/** Netlink protocol number */
#define NETLINK_NXP (MAX_LINKS - 1)
/** Netlink multicast group number */
#define NL_MULTICAST_GROUP 1
/** Netlink maximum payload size */
#define NL_MAX_PAYLOAD 1024

/********************************************************
	Local Variables
********************************************************/
#ifdef CONFIG_WPS_UPNP
#define WPS_UPNP_ALL_CTX (void *)-1
typedef enum {
	EVENT_TYPE_READ = 0,
	EVENT_TYPE_WRITE,
} eloop_event_type;
typedef void (*eloop_sock_handler)(int sock, void *eloop_ctx, void *sock_ctx);
struct eloop_sock {
	int sock;
	void *eloop_data;
	void *user_data;
	eloop_sock_handler handler;
};
struct eloop_sock_table {
	int count;
	struct eloop_sock *table;
	int changed;
};

struct upnp_dl_list {
	struct upnp_dl_list *next;
	struct upnp_dl_list *prev;
};

int upnp_dl_list_empty(struct upnp_dl_list *list)
{
	return list->next == list;
}
/*
int os_get_time(struct os_time *t)
{
    int res;
    struct timeval tv;
    res = gettimeofday(&tv, NULL);
    t->sec = tv.tv_sec;
    t->usec = tv.tv_usec;
    return res;
}
*/
#define upnp_offsetof(type, member) ((long)&((type *)0)->member)

#define upnp_dl_list_entry(item, type, member)                                 \
	((type *)((char *)item - upnp_offsetof(type, member)))

#define upnp_dl_list_for_each(item, list, type, member)                        \
	for (item = upnp_dl_list_entry((list)->next, type, member);            \
	     &item->member != (list);                                          \
	     item = upnp_dl_list_entry(item->member.next, type, member))

#define upnp_dl_list_first(list, type, member)                                 \
	(upnp_dl_list_empty((list)) ?                                          \
		 NULL :                                                        \
		 upnp_dl_list_entry((list)->next, type, member))

void upnp_dl_list_add(struct upnp_dl_list *list, struct upnp_dl_list *item)
{
	item->next = list->next;
	item->prev = list;
	list->next->prev = item;
	list->next = item;
}

#define upnp_dl_list_for_each_safe(item, n, list, type, member)                \
	for (item = upnp_dl_list_entry((list)->next, type, member),            \
	    n = upnp_dl_list_entry(item->member.next, type, member);           \
	     &item->member != (list);                                          \
	     item = n, n = upnp_dl_list_entry(n->member.next, type, member))

void upnp_dl_list_add_tail(struct upnp_dl_list *list, struct upnp_dl_list *item)
{
	upnp_dl_list_add(list->prev, item);
}

typedef void (*eloop_timeout_handler)(void *eloop_data, void *user_ctx);

typedef long os_time_t;

struct os_time {
	os_time_t sec;
	os_time_t usec;
};

struct eloop_timeout {
	struct upnp_dl_list list;
	struct os_time time;
	void *eloop_data;
	void *user_data;
	eloop_timeout_handler handler;
	/*
	    WPA_TRACE_REF(eloop);
	    WPA_TRACE_REF(user);
	    WPA_TRACE_INFO
	*/
};

#define upnp_time_before(a, b)                                                 \
	((a)->sec < (b)->sec || ((a)->sec == (b)->sec && (a)->usec < (b)->usec))

#define upnp_time_sub(a, b, res)                                               \
	do {                                                                   \
		(res)->sec = (a)->sec - (b)->sec;                              \
		(res)->usec = (a)->usec - (b)->usec;                           \
		if ((res)->usec < 0) {                                         \
			(res)->sec--;                                          \
			(res)->usec += 1000000;                                \
		}                                                              \
	} while (0)

void *upnp_zalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr)
		memset(ptr, 0, size);
	return ptr;
}

int upnp_get_time(struct os_time *t)
{
	int res;
	struct timeval tv;
	res = gettimeofday(&tv, NULL);
	t->sec = tv.tv_sec;
	t->usec = tv.tv_usec;
	return res;
}

static void upnp_dl_list_del(struct upnp_dl_list *item)
{
	item->next->prev = item->prev;
	item->prev->next = item->next;
	item->next = NULL;
	item->prev = NULL;
}

static void upnp_dl_list_init(struct upnp_dl_list *list)
{
	list->next = list;
	list->prev = list;
}
struct eloop_sock_table *wps_upnp_get_sock_table(eloop_event_type type);
int wps_upnp_sock_table_add_sock(struct eloop_sock_table *table, int sock,
				 eloop_sock_handler handler, void *eloop_data,
				 void *user_data);
void wps_upnp_sock_table_remove_sock(struct eloop_sock_table *table, int sock);
int wps_upnp_register_sock(eloop_event_type type, int sock,
			   eloop_sock_handler handler, void *eloop_data,
			   void *user_data);
int wps_upnp_register_timeout(unsigned int secs, unsigned int usecs,
			      eloop_timeout_handler handler, void *eloop_data,
			      void *user_data);
int wps_upnp_cancel_timeout(eloop_timeout_handler handler, void *eloop_data,
			    void *user_data);
void eloop_sock_table_destroy(struct eloop_sock_table *table);
void wps_upnp_destroy(void);
#endif

/* Data structure definition for main loop */
struct wps_sock_s {
	/** socket no */
	int sock;
	/** private data for callback */
	void *callback_data;
	/** handler */
	void (*handler)(int sock, void *sock_ctx);
};

struct wps_timeout_s {
	/** next pointer */
	struct wps_timeout_s *next;
	/** time */
	struct timeval time;
	/** private data for callback */
	void *callback_data;
	/** timeout handler */
	void (*handler)(void *timeout_ctx);
};

typedef struct wps_loop_s {
	/** terminate */
	int terminate;
	/** max socket number */
	int max_sock;
	/** read count */
	int reader_count;
	/** read socket */
	struct wps_sock_s *readers;
	/** timeout */
	struct wps_timeout_s *timeout;
#ifdef CONFIG_WPS_UPNP
	struct eloop_sock_table upnp_readers;
	struct eloop_sock_table upnp_writers;
	struct upnp_dl_list upnp_timeout;
#endif
} WPS_LOOP_S;

static WPS_LOOP_S wps_loop;

/********************************************************
	Global Variables
********************************************************/

extern struct EVENT_INFO evt_info;

/********************************************************
	Local Functions
********************************************************/
/**
 *  @brief Free structure used in main loop function
 *
 *  @return         None
 */
static void wps_main_loop_free(void)
{
	struct wps_timeout_s *timeout, *prev;

	ENTER();

	timeout = wps_loop.timeout;
	while (timeout != NULL) {
		prev = timeout;
		timeout = timeout->next;
		FREE(prev);
	}
	FREE(wps_loop.readers);

	LEAVE();
}

/********************************************************
	Global Functions
********************************************************/

/**
 *  @brief Process reception of event from wlan
 *
 *  @param fd           File descriptor for reading
 *  @param context      A pointer to user private information
 *  @return             None
 */
void wps_event_receive(int fd, void *context)
{
	u8 evt_buffer[EVENT_MAX_BUF_SIZE];
	int count = -1;
	struct EVENT_INFO *p_evt_info = (struct EVENT_INFO *)context;
	char if_name[IFNAMSIZ + 1];
	u32 event_id = 0;

	ENTER();

	count = recvmsg(fd, (struct msghdr *)p_evt_info->evt_msg_head, 0);

	if (count < 0) {
		mwu_printf(MSG_ERROR, "ERR:NETLINK read failure!\n");
		LEAVE();
		return;
	}
	if (count <= NLMSG_HDRLEN) {
		mwu_printf(MSG_ERROR, "ERR:NETLINK read insufficient data!\n");
		LEAVE();
		return;
	}
	mwu_printf(DEBUG_EVENT, "Received event payload (%d)\n", count);
	if (count > NLMSG_SPACE(NL_MAX_PAYLOAD)) {
		mwu_printf(MSG_INFO, "ERR: Buffer overflow!\n");
		LEAVE();
		return;
	}
	memcpy(evt_buffer,
	       NLMSG_DATA((struct nlmsghdr *)p_evt_info->evt_nl_head),
	       count - NLMSG_HDRLEN);
	mwu_hexdump(DEBUG_EVENT, "New event", evt_buffer, count - NLMSG_HDRLEN);

	memcpy(&event_id, evt_buffer, sizeof(event_id));
	if (((event_id & 0xFF000000) == 0x80000000) ||
	    ((event_id & 0xFF000000) == 0)) {
		/* Event doesn't contain interface information */
		wps_wlan_event_parser(context, NULL, (char *)evt_buffer,
				      count - NLMSG_HDRLEN);
	} else {
		/* Event has the interface information it it */
		memset(if_name, 0, IFNAMSIZ + 1);
		memcpy(if_name, evt_buffer, IFNAMSIZ);
		mwu_printf(DEBUG_EVENT, "EVENT on interface %s\n", if_name);
		wps_wlan_event_parser(context, if_name,
				      (char *)(evt_buffer + IFNAMSIZ),
				      count - NLMSG_HDRLEN - IFNAMSIZ);
	}
	LEAVE();
	return;
}

/**
 *  @brief Determine the netlink number
 *
 *  @return         Netlink number to use
 */
static int get_netlink_num(char *cfg_path)
{
	FILE *fp;
	int netlink_num = NETLINK_NXP;
	char str[64];
	char *pstr = str;
	char *srch = "netlink_num";

	/* Try to open /proc/mwlan/config */
	fp = fopen(cfg_path, "r");
	if (fp) {
		while (!feof(fp)) {
			pstr = fgets(str, sizeof(str), fp);
			if (pstr == NULL)
				break;
			if (strncmp(str, srch, strlen(srch)) == 0) {
				netlink_num = atoi(str + strlen(srch) + 1);
				break;
			}
		}
		fclose(fp);
	}

	mwu_printf(DEBUG_INIT, "Netlink number = %d\n", netlink_num);
	return netlink_num;
}

/** Socket */
int sockfd;
/**
 *  @brief Process event socket initialization
 *
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_event_init(char *cfg_path)
{
	int fd = 0;
	int netlink_num = 0;
	int ret = WPS_STATUS_SUCCESS;
	struct nlmsghdr *nlh = NULL;
	struct sockaddr_nl src_addr, *ptr_dest_addr = NULL;
	struct msghdr *pmsg = NULL;
	struct iovec *piov = NULL;

	ENTER();

	netlink_num = get_netlink_num(cfg_path);

	fd = socket(PF_NETLINK, SOCK_RAW, netlink_num);
	if (fd < 0) {
		mwu_printf(MSG_ERROR, "ERR: netlink socket creation error.\n");
		ret = WPS_STATUS_FAIL;
		goto done;
	}

	sockfd = fd;
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();
	src_addr.nl_groups = NL_MULTICAST_GROUP;
	if (bind(fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
		mwu_printf(MSG_ERROR, "ERR: Bind netlink socket\n");
		ret = WPS_STATUS_FAIL;
		goto done;
	}

	/* Initialize netlink header & msg header */
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(NL_MAX_PAYLOAD));
	if (!nlh) {
		mwu_printf(MSG_ERROR, "ERR: Could not alloc buffer\n");
		ret = WPS_STATUS_FAIL;
		goto done;
	}
	memset(nlh, 0, NLMSG_SPACE(NL_MAX_PAYLOAD));
	pmsg = (struct msghdr *)malloc(sizeof(struct msghdr));
	if (!pmsg) {
		mwu_printf(MSG_ERROR, "ERR: Could not alloc buffer\n");
		ret = WPS_STATUS_FAIL;
		goto msg_error;
	}
	memset(pmsg, 0, sizeof(*pmsg));

	ptr_dest_addr =
		(struct sockaddr_nl *)malloc(sizeof(struct sockaddr_nl));
	if (!ptr_dest_addr) {
		mwu_printf(MSG_ERROR, "ERR: Could not alloc buffer\n");
		ret = WPS_STATUS_FAIL;
		goto addr_error;
	}
	memset(ptr_dest_addr, 0, sizeof(*ptr_dest_addr));

	piov = (struct iovec *)malloc(sizeof(struct iovec));
	if (!piov) {
		mwu_printf(MSG_ERROR, "ERR: Could not alloc buffer\n");
		ret = WPS_STATUS_FAIL;
		goto piov_error;
	}
	memset(piov, 0, sizeof(*piov));

	/* Set destination address */
	ptr_dest_addr->nl_family = AF_NETLINK;
	ptr_dest_addr->nl_pid = 0; /* Kernel */
	ptr_dest_addr->nl_groups = NL_MULTICAST_GROUP;

	/* Initialize I/O vector */
	piov->iov_base = (void *)nlh;
	piov->iov_len = NLMSG_SPACE(NL_MAX_PAYLOAD);

	/* Initialize message header */
	memset(pmsg, 0, sizeof(struct msghdr));
	pmsg->msg_name = (void *)ptr_dest_addr;
	pmsg->msg_namelen = sizeof(*ptr_dest_addr);
	pmsg->msg_iov = piov;
	pmsg->msg_iovlen = 1;

	evt_info.evt_fd = fd;
	evt_info.evt_nl_head = nlh;
	evt_info.evt_msg_head = pmsg;
	evt_info.evt_iov = piov;
	evt_info.evt_dest_addr = ptr_dest_addr;

	/* register the read socket handler for event packet */
	wps_register_rdsock_handler(fd, wps_event_receive, &evt_info);

	LEAVE();
	return ret; /* success */

/* failure */
piov_error:
	FREE(ptr_dest_addr);
addr_error:
	FREE(pmsg);
msg_error:
	FREE(nlh);
done:
	LEAVE();
	return ret;
}

int wps_loop_init()
{
	ENTER();

	/* clear data structure for main processing loop */
	memset(&wps_loop, 0, sizeof(wps_loop));

#ifdef CONFIG_WPS_UPNP
	upnp_dl_list_init(&wps_loop.upnp_timeout);
#endif

	LEAVE();
	return WPS_STATUS_SUCCESS;
}
#if 0
int mwu_l2_init(struct mwu_iface_info *cur_if)
{
    if (cur_if->pwps_info->wps_data.l2) {
        mwu_printf(MSG_WARNING, "Warning: L2 socket is already initialized. Deinit and Continue/");
        wps_l2_deinit(cur_if->pwps_info->wps_data.l2);
        cur_if->pwps_info->wps_data.l2 = NULL;
    }

    /* Initial and hook l2 callback function */
    cur_if->pwps_info->wps_data.l2 =
        wps_l2_init(cur_if, ETH_P_EAPOL, wps_rx_eapol, 0);

    if (cur_if->pwps_info->wps_data.l2 == NULL) {
        mwu_printf(DEBUG_INIT, "Init l2 return NULL !\n");
        LEAVE();
        return WPS_STATUS_FAIL;
    }

    if (cur_if->pwps_info->wps_data.l2 && cur_if->role != WIFIDIR_ROLE &&
        wps_l2_get_mac(cur_if->pwps_info->wps_data.l2, cur_if->device_mac_addr)) {
        mwu_printf(DEBUG_INIT, "Failed to get own L2 address\n");
        LEAVE();
        return WPS_STATUS_FAIL;
    }

    mwu_printf(DEBUG_INIT, "Own MAC address: " UTIL_MACSTR,
            UTIL_MAC2STR(cur_if->device_mac_addr));

    return WPS_STATUS_SUCCESS;
}

#endif
/**
 *  @brief Process event handling free
 *
 *  @return             None
 */
void wps_event_deinit()
{
	ENTER();

	FREE(evt_info.evt_nl_head);
	FREE(evt_info.evt_dest_addr);
	FREE(evt_info.evt_msg_head);
	FREE(evt_info.evt_iov);

	if (evt_info.evt_fd > 0) {
		wps_unregister_rdsock_handler(evt_info.evt_fd);
		close(evt_info.evt_fd);
		evt_info.evt_fd = 0;
	}

	LEAVE();
}

/**
 *  @brief Process main loop free
 *
 *  @return             None
 */
void wps_loop_deinit()
{
	ENTER();

	wps_main_loop_free();

	LEAVE();
}

/**
 *  @brief Disable main loop
 *
 *  @return         None
 */
void wps_main_loop_shutdown(void)
{
	ENTER();
	wps_loop.terminate = WPS_SET;
	LEAVE();
}

/**
 *  @brief Enable main loop
 *
 *  @return         None
 */
void wps_main_loop_enable(void)
{
	ENTER();
	wps_loop.terminate = WPS_CANCEL;
	LEAVE();
}

/**
 *  @brief Register a handler to main loop socket read function
 *
 *  @param sock             Socket number
 *  @param handler          A function pointer of read handler
 *  @param callback_data    Private data for callback
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_register_rdsock_handler(int sock,
				void (*handler)(int sock, void *sock_ctx),
				void *callback_data)
{
	struct wps_sock_s *tmp;

	ENTER();

	tmp = (struct wps_sock_s *)realloc(wps_loop.readers,
					   (wps_loop.reader_count + 1) *
						   sizeof(struct wps_sock_s));
	if (tmp == NULL)
		return WPS_STATUS_FAIL;

	tmp[wps_loop.reader_count].sock = sock;
	tmp[wps_loop.reader_count].callback_data = callback_data;
	tmp[wps_loop.reader_count].handler = handler;
	wps_loop.reader_count++;
	wps_loop.readers = tmp;
	if (sock > wps_loop.max_sock)
		wps_loop.max_sock = sock;

	LEAVE();
	return WPS_STATUS_SUCCESS;
}

/**
 *  @brief Unregister a handler to main loop socket read function
 *
 *  @param sock     Socket number
 *  @return         None
 */
void wps_unregister_rdsock_handler(int sock)
{
	int i;

	ENTER();

	if (wps_loop.readers == NULL || wps_loop.reader_count == 0) {
		LEAVE();
		return;
	}

	for (i = 0; i < wps_loop.reader_count; i++) {
		if (wps_loop.readers[i].sock == sock)
			break;
	}
	if (i == wps_loop.reader_count)
		return;
	if (i != wps_loop.reader_count - 1) {
		memmove(&wps_loop.readers[i], &wps_loop.readers[i + 1],
			(wps_loop.reader_count - i - 1) *
				sizeof(struct wps_sock_s));
	}
	wps_loop.reader_count--;

	LEAVE();
}

/**
 *  @brief Register a time-out handler to main loop timer function
 *
 *  @param secs             Time-out value in seconds
 *  @param usecs            Time-out value in micro-seconds
 *  @param handler          A function pointer of time-out handler
 *  @param callback_data    Private data for callback
 *  @return         WPS_STATUS_SUCCESS--success, otherwise--fail
 */
int wps_start_timer(unsigned int secs, unsigned int usecs,
		    void (*handler)(void *user_data), void *callback_data)
{
	struct wps_timeout_s *timeout, *tmp, *prev;

	ENTER();

	timeout = (struct wps_timeout_s *)malloc(sizeof(*timeout));
	if (timeout == NULL)
		return WPS_STATUS_FAIL;

	gettimeofday(&timeout->time, NULL);
	timeout->time.tv_sec += secs;
	timeout->time.tv_usec += usecs;
	while (timeout->time.tv_usec >= 1000000) {
		timeout->time.tv_sec++;
		timeout->time.tv_usec -= 1000000;
	}

	timeout->callback_data = callback_data;
	timeout->handler = handler;
	timeout->next = NULL;

	if (wps_loop.timeout == NULL) {
		wps_loop.timeout = timeout;
		LEAVE();
		return WPS_STATUS_SUCCESS;
	}

	prev = NULL;
	tmp = wps_loop.timeout;
	while (tmp != NULL) {
		if (timercmp(&timeout->time, &tmp->time, <))
			break;
		prev = tmp;
		tmp = tmp->next;
	}

	if (prev == NULL) {
		timeout->next = wps_loop.timeout;
		wps_loop.timeout = timeout;
	} else {
		timeout->next = prev->next;
		prev->next = timeout;
	}

	LEAVE();
	return WPS_STATUS_SUCCESS;
}

/**
 *  @brief Cancel time-out handler to main loop timer function
 *
 *  @param handler          Time-out handler to be canceled
 *  @param callback_data    Private data for callback
 *  @return         Number of timer being removed
 */
int wps_cancel_timer(void (*handler)(void *timeout_ctx), void *callback_data)
{
	struct wps_timeout_s *timeout, *prev, *next;
	int removed = 0;

	ENTER();

	prev = NULL;
	timeout = wps_loop.timeout;
	while (timeout != NULL) {
		next = timeout->next;

		if ((timeout->handler == handler) &&
		    (timeout->callback_data == callback_data)) {
			if (prev == NULL)
				wps_loop.timeout = next;
			else
				prev->next = next;
			FREE(timeout);
			removed++;
		} else
			prev = timeout;

		timeout = next;
	}

	LEAVE();
	return removed;
}

#ifdef CONFIG_WPS_UPNP
static void wps_upnp_sock_table_set_reader_fds(struct eloop_sock_table *table,
					       fd_set *fds)
{
	int i;

	if (table->table == NULL)
		return;

	for (i = 0; i < table->count; i++)
		FD_SET(table->table[i].sock, fds);
}

static void wps_upnp_sock_table_set_writer_fds(struct eloop_sock_table *table,
					       fd_set *fds)
{
	int i;

	FD_ZERO(fds);

	if (table->table == NULL)
		return;

	for (i = 0; i < table->count; i++)
		FD_SET(table->table[i].sock, fds);
}

static void wps_upnp_sock_table_dispatch(struct eloop_sock_table *table,
					 fd_set *fds)
{
	int i;

	if (table == NULL || table->table == NULL)
		return;

	table->changed = 0;
	for (i = 0; i < table->count; i++) {
		if (FD_ISSET(table->table[i].sock, fds)) {
			table->table[i].handler(table->table[i].sock,
						table->table[i].eloop_data,
						table->table[i].user_data);
			if (table->changed)
				break;
		}
	}
}

static void wps_upnp_remove_timeout(struct eloop_timeout *timeout)
{
	upnp_dl_list_del(&timeout->list);
	if (timeout) {
		free(timeout);
	}
}

#endif /*CONFIG_WPS_UPNP*/

/**
 *  @brief Main loop procedure for socket read and timer functions
 *
 *  @return             None
 */
void wps_main_loop_proc(void)
{
	int i, res;
	struct timeval tv, now;
	struct timeval tv_nowait;
	fd_set *rfds;
#ifdef CONFIG_WPS_UPNP
	fd_set *wfds_upnp;
	struct timeval upnp_tv;
	struct os_time upnptv, upnpnow;
	struct eloop_timeout *upnp_timeout = NULL;
	int upnp_flag = 0;
#endif /*CONFIG_WPS_UPNP*/

	ENTER();

	rfds = malloc(sizeof(*rfds));
	if (rfds == NULL) {
		mwu_printf(MSG_ERROR, "wps_main_loop_proc : malloc failed !\n");
		return;
	}

#ifdef CONFIG_WPS_UPNP
	wfds_upnp = malloc(sizeof(*wfds_upnp));
	if (wfds_upnp == NULL) {
		mwu_printf(MSG_ERROR,
			   "wps_main_loop_proc : upnp fd malloc failed !\n");
		return;
	}
#endif

	while (!wps_loop.terminate &&
	       (wps_loop.timeout || wps_loop.reader_count > 0
#ifdef CONFIG_WPS_UPNP
		|| (wps_upnp_start == 1 &&
		    (!upnp_dl_list_empty(&wps_loop.upnp_timeout) ||
		     wps_loop.upnp_readers.count > 0 ||
		     wps_loop.upnp_writers.count > 0))
#endif
			)) {
		if (wps_loop.timeout) {
			gettimeofday(&now, NULL);
			if (timercmp(&now, &wps_loop.timeout->time, <))
				timersub(&wps_loop.timeout->time, &now, &tv);
			else
				tv.tv_sec = tv.tv_usec = 0;
		}

#ifdef CONFIG_WPS_UPNP
		upnp_flag = 0;
		upnp_tv.tv_usec = 0;
		upnp_tv.tv_sec = 0;
		if (wps_upnp_start == 1 &&
		    (!upnp_dl_list_empty(&wps_loop.upnp_timeout) ||
		     wps_loop.upnp_readers.count > 0 ||
		     wps_loop.upnp_writers.count > 0)) {
			upnp_flag = 1;
		}

		if (upnp_flag == 1) {
			upnp_timeout =
				upnp_dl_list_first(&wps_loop.upnp_timeout,
						   struct eloop_timeout, list);
			if (upnp_timeout) {
				upnp_get_time(&upnpnow);
				if (upnp_time_before(&upnpnow,
						     &upnp_timeout->time))
					upnp_time_sub(&upnp_timeout->time,
						      &upnpnow, &upnptv);
				else
					upnptv.sec = upnptv.usec = 0;
				upnp_tv.tv_sec = upnptv.sec;
				upnp_tv.tv_usec = upnptv.usec;
			}
		}
#endif /*CONFIG_WPS_UPNP*/

		FD_ZERO(rfds);

		for (i = 0; i < wps_loop.reader_count; i++) {
			FD_SET(wps_loop.readers[i].sock, rfds);
		}

#ifdef CONFIG_WPS_UPNP
		if (upnp_flag == 1) {
			wps_upnp_sock_table_set_reader_fds(
				&wps_loop.upnp_readers, rfds);
			wps_upnp_sock_table_set_writer_fds(
				&wps_loop.upnp_writers, wfds_upnp);
		}
#endif

		/* Be sure to reset timeout each time because some select
		 * implementations alter it.
		 */
		tv_nowait.tv_sec = 0;
		tv_nowait.tv_usec = 100000;
#ifdef CONFIG_WPS_UPNP
		if (upnp_flag == 1) {
			if (upnp_timeout) {
				if ((upnp_tv.tv_sec < tv.tv_sec) ||
				    (upnp_tv.tv_sec == tv.tv_sec &&
				     upnp_tv.tv_usec < tv.tv_usec)) {
					tv.tv_sec = upnp_tv.tv_sec;
					tv.tv_usec = upnp_tv.tv_usec;
				}
			}
			res = select(wps_loop.max_sock + 1, rfds, wfds_upnp,
				     NULL, wps_loop.timeout ? &tv : &tv_nowait);
		} else {
			res = select(wps_loop.max_sock + 1, rfds, NULL, NULL,
				     wps_loop.timeout ? &tv : &tv_nowait);
		}
#else
		res = select(wps_loop.max_sock + 1, rfds, NULL, NULL,
			     wps_loop.timeout ? &tv : &tv_nowait);
#endif /*CONFIG_WPS_UPNP*/

		if (res < 0 && errno != EINTR) {
			mwu_printf(MSG_ERROR, "select loop error\n");
			perror("select()");
			FREE(rfds);
#ifdef CONFIG_WPS_UPNP
			FREE(wfds_upnp);
#endif
			LEAVE();
			return;
		}

		/* check if some registered timeouts have occurred */
		if (wps_loop.timeout) {
			struct wps_timeout_s *tmp;

			gettimeofday(&now, NULL);
			if (!timercmp(&now, &wps_loop.timeout->time, <)) {
				tmp = wps_loop.timeout;
				wps_loop.timeout = wps_loop.timeout->next;
				tmp->handler(tmp->callback_data);
				FREE(tmp);
			}
		}

#ifdef CONFIG_WPS_UPNP
		if (upnp_flag == 1) {
			/* check if some upnp registered timeouts have occurred
			 */
			upnp_timeout =
				upnp_dl_list_first(&wps_loop.upnp_timeout,
						   struct eloop_timeout, list);
			if (upnp_timeout) {
				upnp_get_time(&upnpnow);
				if (!upnp_time_before(&upnpnow,
						      &upnp_timeout->time)) {
					void *eloop_data =
						upnp_timeout->eloop_data;
					void *user_data =
						upnp_timeout->user_data;
					eloop_timeout_handler handler =
						upnp_timeout->handler;
					wps_upnp_remove_timeout(upnp_timeout);
					handler(eloop_data, user_data);
				}
			}
		}
#endif /*CONFIG_WPS_UPNP*/
		if (res <= 0) {
			continue;
		}

		/* call socket read handler function */
		for (i = 0; i < wps_loop.reader_count; i++) {
			if (FD_ISSET(wps_loop.readers[i].sock, rfds)) {
				wps_loop.readers[i].handler(
					wps_loop.readers[i].sock,
					wps_loop.readers[i].callback_data);
			}
		}
#ifdef CONFIG_WPS_UPNP
		if (upnp_flag == 1) {
			wps_upnp_sock_table_dispatch(&wps_loop.upnp_readers,
						     rfds);
			wps_upnp_sock_table_dispatch(&wps_loop.upnp_writers,
						     wfds_upnp);
		}
#endif
	}

	FREE(rfds);
#ifdef CONFIG_WPS_UPNP
	FREE(wfds_upnp);
#endif
	LEAVE();
}

void wps_generate_nonce_16B(u8 *buf)
{
	int i, randNum, seed;
	struct timeval now;

	ENTER();

	gettimeofday(&now, NULL);
	seed = now.tv_sec | now.tv_usec;

	for (i = 0; i < 4; i++) {
		srand(seed);
		randNum = rand();
		memcpy(buf + i * 4, &randNum, sizeof(int));
		seed = seed * 1103515245 + 12345 * i;
	}

	LEAVE();
}

#ifdef CONFIG_WPS_UPNP
/**
 *  @brief upnp loop procedure for socket read and timer functions
 *
 *  @return             None
 */

struct eloop_sock_table *wps_upnp_get_sock_table(eloop_event_type type)
{
	switch (type) {
	case EVENT_TYPE_READ:
		return &wps_loop.upnp_readers;
	case EVENT_TYPE_WRITE:
		return &wps_loop.upnp_writers;
	}

	return NULL;
}

int wps_upnp_sock_table_add_sock(struct eloop_sock_table *table, int sock,
				 eloop_sock_handler handler, void *eloop_data,
				 void *user_data)
{
	struct eloop_sock *tmp;
	int new_max_sock;

	if (sock > wps_loop.max_sock)
		new_max_sock = sock;
	else
		new_max_sock = wps_loop.max_sock;

	if (table == NULL)
		return -1;

	tmp = (struct eloop_sock *)realloc(
		table->table, (table->count + 1) * sizeof(struct eloop_sock));
	if (tmp == NULL)
		return -1;

	tmp[table->count].sock = sock;
	tmp[table->count].eloop_data = eloop_data;
	tmp[table->count].user_data = user_data;
	tmp[table->count].handler = handler;
	table->count++;
	table->table = tmp;
	wps_loop.max_sock = new_max_sock;
	table->changed = 1;

	return 0;
}

void wps_upnp_sock_table_remove_sock(struct eloop_sock_table *table, int sock)
{
	int i;

	if (table == NULL || table->table == NULL || table->count == 0)
		return;

	for (i = 0; i < table->count; i++) {
		if (table->table[i].sock == sock)
			break;
	}
	if (i == table->count)
		return;

	if (i != table->count - 1) {
		memmove(&table->table[i], &table->table[i + 1],
			(table->count - i - 1) * sizeof(struct eloop_sock));
	}
	table->count--;
	table->changed = 1;
}

int wps_upnp_register_sock(eloop_event_type type, int sock,
			   eloop_sock_handler handler, void *eloop_data,
			   void *user_data)
{
	struct eloop_sock_table *table;

	table = wps_upnp_get_sock_table(type);
	return wps_upnp_sock_table_add_sock(table, sock, handler, eloop_data,
					    user_data);
}

int wps_upnp_register_timeout(unsigned int secs, unsigned int usecs,
			      eloop_timeout_handler handler, void *eloop_data,
			      void *user_data)
{
	struct eloop_timeout *timeout, *tmp;
	os_time_t now_sec;

	timeout = upnp_zalloc(sizeof(*timeout));

	if (timeout == NULL)
		return -1;
	if (upnp_get_time(&timeout->time) < 0) {
		free(timeout);
		return -1;
	}
	now_sec = timeout->time.sec;
	timeout->time.sec += secs;
	if (timeout->time.sec < now_sec) {
		/*
		 * Integer overflow - assume long enough timeout to be assumed
		 * to be infinite, i.e., the timeout would never happen.
		 */
		printf("ELOOP: Too long timeout (secs=%u) to "
		       "ever happen - ignore it",
		       secs);
		free(timeout);
		return 0;
	}
	timeout->time.usec += usecs;
	while (timeout->time.usec >= 1000000) {
		timeout->time.sec++;
		timeout->time.usec -= 1000000;
	}
	timeout->eloop_data = eloop_data;
	timeout->user_data = user_data;
	timeout->handler = handler;

	// Maintain timeouts in order of increasing time
	upnp_dl_list_for_each(tmp, &wps_loop.upnp_timeout, struct eloop_timeout,
			      list)
	{
		if (upnp_time_before(&timeout->time, &tmp->time)) {
			upnp_dl_list_add(tmp->list.prev, &timeout->list);
			return 0;
		}
	}
	upnp_dl_list_add_tail(&wps_loop.upnp_timeout, &timeout->list);

	return 0;
}

int wps_upnp_cancel_timeout(eloop_timeout_handler handler, void *eloop_data,
			    void *user_data)
{
	struct eloop_timeout *timeout, *prev;
	int removed = 0;

	upnp_dl_list_for_each_safe(timeout, prev, &wps_loop.upnp_timeout,
				   struct eloop_timeout, list)
	{
		if (timeout->handler == handler &&
		    (timeout->eloop_data == eloop_data ||
		     eloop_data == WPS_UPNP_ALL_CTX) &&
		    (timeout->user_data == user_data ||
		     user_data == WPS_UPNP_ALL_CTX)) {
			wps_upnp_remove_timeout(timeout);
			removed++;
		}
	}

	return removed;
}

void wps_upnp_sock_table_destroy(struct eloop_sock_table *table)
{
	if (table) {
		if (table->table && table->count > 0) {
			free(table->table);
			table->table = NULL;
			table->count = 0;
		}
	}
}

void wps_upnp_destroy(void)
{
	struct eloop_timeout *timeout, *prev;
	struct os_time now;

	upnp_get_time(&now);
	upnp_dl_list_for_each_safe(timeout, prev, &wps_loop.upnp_timeout,
				   struct eloop_timeout, list)
	{
		int sec, usec;
		sec = timeout->time.sec - now.sec;
		usec = timeout->time.usec - now.usec;
		if (timeout->time.usec < now.usec) {
			sec--;
			usec += 1000000;
		}
		wps_upnp_remove_timeout(timeout);
	}
	wps_upnp_sock_table_destroy(&wps_loop.upnp_readers);
	wps_upnp_sock_table_destroy(&wps_loop.upnp_writers);
}
#endif /*CONFIG_WPS_UPNP*/
