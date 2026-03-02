
/** @file  mlancsi.c
 *
 * @brief Wifi CSI processing application
 *
 *  Usage:
 *
 *
 * Copyright 2025 NXP
 *
 * NXP CONFIDENTIAL
 * The source code contained or described herein and all documents related to
 * the source code (Materials) are owned by NXP, its
 * suppliers and/or its licensors. Title to the Materials remains with NXP,
 * its suppliers and/or its licensors. The Materials contain
 * trade secrets and proprietary and confidential information of NXP, its
 * suppliers and/or its licensors. The Materials are protected by worldwide
 * copyright and trade secret laws and treaty provisions. No part of the
 * Materials may be used, copied, reproduced, modified, published, uploaded,
 * posted, transmitted, distributed, or disclosed in any way without NXP's prior
 * express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by NXP in writing.
 *
 */

/************************************************************************
Change log:
     01/24/2022: initial version
	 06/13/2025: copy from mlanwls to mlancsi
************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/ethernet.h>
#include "mlancsi.h"
#include "../libcsi/event.h"

/********************************************************
		Functions Declarations
********************************************************/

t_u8 hexc2bin(char chr);
t_u32 a2hex(char *s);
t_u32 a2hex_or_atoi(char *value);
t_void hexdump(char *prompt, t_void *p, t_s32 len, char delim);
extern char *config_get_line(char *s, int size, FILE *stream, int *line,
			     char **_pos);
int parse_line(char *line, char *args[], t_u16 args_count);
int mac2raw(char *mac, t_u8 *raw);

extern int mlancsi_event_monitor(int nl_sk);
static void mlancsi_init(void);
static int mlancsi_read_config(char *file_name);
static t_void mlancsi_terminate_handler(int signal);

static int get_netlink_num(int dev_index);
static int open_netlink(int dev_index);
static t_void display_help(t_u32 n, char **data);
static int process_csi_cmd(int argc, char *argv[]);
static int process_event_cmd(int argc, char *argv[]);

/********************************************************
		Local Variables
********************************************************/
struct command_node command_list[] = {
	{"csi", process_csi_cmd},
	{"event", process_event_cmd},
};

/** MLANCSI app usage */
static char *mlancsi_help[] = {
	"Usage: ", "	mlancsi <interface> csi config/<csi_filter_file_name>",
	"	mlancsi <interface> event config/<csi_event_file_name>",
	"	where config/<xxx_file_name> is a configuration file.", ""};

/** MLANCSI csi cmd usage */
static char *mlancsi_csi_usage[] = {
	"Usage: ", "	Enable:  mlancsi <interface> csi config/<csi_file_name>",
	"	Disable: mlancsi <interface> csi 0",
	"	where config/<xxx_file_name> is a configuration file.", ""};

/** MLANCSI event cmd usage */
static char *mlancsi_event_usage[] = {
	"Usage: ",
	"	mlancsi <interface> event config/<csi_filter_file_name>",
	"	where config/<xxx_file_name> is a configuration file.",
	"	mlancsi <interface> event",
	"	when no configuration file is given, the first CSI record is used as template.",
	""};

/** MLANCSI app data*/
wls_app_data_t gwls_data;
wls_csi_cfg_t gwls_csi_cfg;

/**DBG Printf*/
#define DBG_LOG(x)                                                             \
	if (gwls_data.debug_level == 2) {                                      \
		printf(x);                                                     \
	}
#define DBG_ERROR(x)                                                           \
	if (gwls_data.debug_level >= 0) {                                      \
		printf(x);                                                     \
	}
#define PRINT_CFG(x, y) printf(x, y)

/********************************************************
		Global Variables
********************************************************/

/** Socket */
t_s32 sockfd;

/** Device name */
char dev_name[IFNAMSIZ];
#define HOSTCMD "hostcmd"

/********************************************************
		Local Functions
********************************************************/
/**
 *  @brief Display help text
 *
 *  @return       NA
 */
static t_void display_help(t_u32 n, char **data)
{
	t_u32 i;
	for (i = 0; i < n; i++)
		fprintf(stderr, "%s\n", data[i]);
}

/**
 *  @brief Prepare command buffer
 *  @param buffer   Command buffer to be filled
 *  @param cmd      Command id
 *  @param num      Number of arguments
 *  @param args     Arguments list
 *  @return         MLAN_STATUS_SUCCESS
 */
int mlanwls_prepare_buffer(t_u8 *buffer, char *cmd, t_u32 num, char *args[])
{
	t_u8 *pos = NULL;
	unsigned int i = 0;

	memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	/* Flag it for our use */
	pos = buffer;
	memcpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += (strlen(CMD_NXP));

	/* Insert command */
	strncpy((char *)pos, (char *)cmd, strlen(cmd));
	pos += (strlen(cmd));

	/* Insert arguments */
	for (i = 0; i < num; i++) {
		strncpy((char *)pos, args[i], strlen(args[i]));
		pos += strlen(args[i]);
		if (i < (num - 1)) {
			memcpy((char *)pos, " ", strlen(" "));
			pos += 1;
		}
	}

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Determine the netlink number
 *
 *  @return         Netlink number to use
 */
static int get_netlink_num(int dev_index)
{
	FILE *fp = NULL;
	int netlink_num = -1;
	char str[64];
	char *srch = "netlink_num";
	char filename[64];
	t_u8 *buffer = NULL;
	struct eth_priv_cmd *cmd = NULL;
	struct ifreq ifr;

	/* if dev_index is specified by user */
	if (dev_index >= 0) {
		/* Try to open old driver proc: /proc/mwlan/configX first */
		if (dev_index == 0)
			strcpy(filename, "/proc/mwlan/config");
		else if (dev_index > 0)
			sprintf(filename, "/proc/mwlan/config%d", dev_index);
		fp = fopen(filename, "r");
		if (!fp) {
			/* Try to open multi-adapter driver proc:
			 * /proc/mwlan/adapterX/config if old proc access fail
			 */
			snprintf(filename, sizeof(filename),
				 "/proc/mwlan/adapter%d/config", dev_index);
			fp = fopen(filename, "r");
		}

		if (fp) {
			while (fgets(str, sizeof(str), fp)) {
				if (strncmp(str, srch, strlen(srch)) == 0) {
					netlink_num =
						atoi(str + strlen(srch) + 1);
					break;
				}
			}
			fclose(fp);
		} else {
			return -1;
		}
	} else {
		/* Start preparing the buffer */
		/* Initialize buffer */
		buffer = (t_u8 *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
		if (!buffer) {
			DBG_ERROR(
				"[ERROR] Cannot allocate buffer for command!\n");
			return -1;
		}
		/* buffer = CMD_NXP + <cmd_string>*/
		mlanwls_prepare_buffer(buffer, "getnlnum", 0, NULL);

		cmd = (struct eth_priv_cmd *)malloc(
			sizeof(struct eth_priv_cmd));
		if (!cmd) {
			DBG_ERROR(
				"[ERROR] Cannot allocate buffer for command!\n");
			goto done;
		}

		/* Fill up buffer */
#ifdef USERSPACE_32BIT_OVER_KERNEL_64BIT
		memset(cmd, 0, sizeof(struct eth_priv_cmd));
		memcpy(&cmd->buf, &buffer, sizeof(buffer));
#else
		cmd->buf = buffer;
#endif
		cmd->used_len = 0;
		cmd->total_len = MRVDRV_SIZE_OF_CMD_BUFFER;

		/* Perform IOCTL */
		memset(&ifr, 0, sizeof(struct ifreq));
		strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
		ifr.ifr_ifru.ifru_data = (void *)cmd;

		if (ioctl(sockfd, MLAN_ETH_PRIV, &ifr)) {
			DBG_ERROR("[ERROR] mlanwls: getnlnum fail\n");
			goto done;
		}
		netlink_num = *(int *)(buffer);
	}

done:
	if (cmd)
		free(cmd);
	if (buffer)
		free(buffer);
	printf("[INFO] Netlink number = %d\n", netlink_num);
	return netlink_num;
}

/**
 *  @brief opens netlink socket to receive NETLINK events
 *  @return  socket id --success, otherwise--MLAN_STATUS_FAILURE
 */
static int open_netlink(int dev_index)
{
	int sk = -1;
	struct sockaddr_nl src_addr;
	int netlink_num = 0;

	netlink_num = get_netlink_num(dev_index);
	if (netlink_num < 0) {
		DBG_ERROR(
			"[ERROR] Could not get netlink socket. Invalid device number.\n");
		return sk;
	}

	/* Open netlink socket */
	sk = socket(PF_NETLINK, SOCK_RAW, netlink_num);
	if (sk < 0) {
		DBG_ERROR("[ERROR] Could not open netlink socket.\n");
		return sk;
	}

	/* Set source address */
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();
	src_addr.nl_groups = NL_MULTICAST_GROUP;

	/* Bind socket with source address */
	if (bind(sk, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
		DBG_ERROR("[ERROR] Could not bind socket!\n");
		close(sk);
		return -1;
	}
	return sk;
}

/**
 *  @brief Terminate signal handler
 *  @param signal   Signal to handle
 *  @return         NA
 */
static t_void mlancsi_terminate_handler(int signal)
{
	printf("[INFO] Stopping application.\n");
#if DEBUG
	printf("Process ID of process killed = %d\n", getpid());
#endif
	gwls_data.terminate_app = 1;
}

/**
 *  @brief Read mlancsi param from conf file
 *  @param file_name  config file name
 *
 *  @return     MLAN_STATUS_SUCCESS--success, otherwise--fail
 */

static int mlancsi_read_config(char *file_name)
{
	int ret = MLAN_STATUS_SUCCESS;
	FILE *config_file = NULL;
	char *line = NULL;
	char *data = NULL;
	int arg_num, li;
	char *args[30];
	t_u8 param = 0;

	// read config
	config_file = fopen(file_name, "r");
	if (config_file == NULL) {
		perror("CONFIG");
		return MLAN_STATUS_FAILURE;
	}
	line = (char *)malloc(MAX_CONFIG_LINE);
	if (!line) {
		printf("ERR:Cannot allocate memory for line\n");
		ret = MLAN_STATUS_FAILURE;
	} else {
		memset(line, 0, MAX_CONFIG_LINE);

		printf("[INFO] Read CSI config from file %s\n", file_name);
		while (config_get_line(line, MAX_CONFIG_LINE, config_file, &li,
				       &data)) {
			arg_num = parse_line(line, args, 30);

			if (arg_num > 1)
				param = atoi(args[1]);

			if (strcmp(args[0], "CSI_EVENT_CFG") == 0) {
				printf("CSI_EVENT_CFG\n");
			} else if (strcmp(args[0], "MAC_ADDR") == 0) {
				int my_ret =
					mac2raw(args[1],
						&(gwls_csi_cfg.gcsi_filter_param
							  .peer_mac[0]));
				if (my_ret == MLAN_STATUS_SUCCESS) {
					PRINT_CFG("\t MAC_ADDR=%s\n", args[1]);
				} else {
					DBG_ERROR(
						"\t [ERROR] Invalid MAC Address Len\n");
				}
			} else if (strcmp(args[0], "PACKET_TYPE") == 0) {
				gwls_csi_cfg.gcsi_filter_param.packet_format =
					(t_u8)(atoi(args[1]));
				PRINT_CFG("\t PACKET_TYPE=%d\n", param);
			} else if (strcmp(args[0], "FORMAT_BW") == 0) {
				gwls_csi_cfg.gcsi_filter_param.packet_bandwidth =
					(t_u8)(atoi(args[1]));
				PRINT_CFG("\t FORMAT_BW=%d\n", param);
			} else if (strcmp(args[0], "UPDATE_REF") == 0) {
				gwls_csi_cfg.gcsi_filter_param.reference_update =
					(t_u8)(atoi(args[1]));
				PRINT_CFG("\t UPDATE_REF=%d\n", param);
			} else if (strcmp(args[0], "IIR_ALPHA") == 0) {
				gwls_csi_cfg.gcsi_filter_param.IIR_alpha =
					(float)(atof(args[1]));
				PRINT_CFG("\t IIR_ALPHA=%f\n",
					  gwls_csi_cfg.gcsi_filter_param
						  .IIR_alpha);
			} else if (strcmp(args[0], "KALMAN_P0") == 0) {
				gwls_csi_cfg.gcsi_filter_param.kalman_p0 =
					(float)(atof(args[1]));
				PRINT_CFG("\t KALMAN_P0=%f\n",
					  gwls_csi_cfg.gcsi_filter_param
						  .kalman_p0);
			} else if (strcmp(args[0], "KALMAN_ALPHA") == 0) {
				gwls_csi_cfg.gcsi_filter_param.kalman_alpha =
					(float)(atof(args[1]));
				PRINT_CFG("\t KALMAN_ALPHA=%f\n",
					  gwls_csi_cfg.gcsi_filter_param
						  .kalman_alpha);
			} else if (strcmp(args[0], "KALMAN_N0") == 0) {
				gwls_csi_cfg.gcsi_filter_param.kalman_N0 =
					(float)(atof(args[1]));
				PRINT_CFG("\t KALMAN_N0=%f\n",
					  gwls_csi_cfg.gcsi_filter_param
						  .kalman_N0);
			} else if (strcmp(args[0], "NUM_CSI") == 0) {
				gwls_csi_cfg.gcsi_filter_param.num_csi =
					(t_u8)(atoi(args[1]));
				PRINT_CFG("\t NUM_CSI=%d\n", param);
			} else {
				// printf("Invalid line entry\n %s",args[1]);
			}
		}
		gwls_csi_cfg.csiFilterSet = 1;
		free(line);
	}
	fclose(config_file);

	return ret;
}

/**
 *  @brief Initialize mlancsi private data
 *  @return     MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static void mlancsi_init(void)
{
	memset(&gwls_data, 0, sizeof(wls_app_data_t));
	memset(&gwls_csi_cfg, 0, sizeof(wls_csi_cfg_t));

	/*CSI processing config*/
	gwls_csi_cfg.channel = 0;
	gwls_csi_cfg.wls_processing_input.enableCsi = 1; // turn on CSI
							 // processing
	gwls_csi_cfg.wls_processing_input.enableAoA =
		AOA_DEFAULT; // turn on AoA (req. enableCsi==1)
	gwls_csi_cfg.wls_processing_input.nTx = MAX_TX; // limit # tx streams to
							// process
	gwls_csi_cfg.wls_processing_input.nRx = MAX_RX; // limit # rx to process
	gwls_csi_cfg.wls_processing_input.selCal = 0; // choose cal values
	gwls_csi_cfg.wls_processing_input.dumpMul = 0; // dump extra peaks in
						       // AoA
	gwls_csi_cfg.wls_processing_input.enableAntCycling = 0; // enable
								// antenna
								// cycling
	gwls_csi_cfg.wls_processing_input.dumpRawAngle = 0; // Dump Raw Angle
	gwls_csi_cfg.wls_processing_input.useToaMin =
		TOA_MIN_DEFAULT; // 1: use min combining, 0: power combining;
	gwls_csi_cfg.wls_processing_input.useSubspace =
		SUBSPACE_DEFAULT; // 1: use subspace algo; 0: no;
	gwls_csi_cfg.wls_processing_input.useFindAngleDelayPeaks =
		ENABLE_DELAY_PEAKS; // use this algorithm for AoA

	gwls_csi_cfg.gcsi_filter_param.peer_mac[0] = 0xff;
	gwls_csi_cfg.gcsi_filter_param.peer_mac[1] = 0xff;
	gwls_csi_cfg.gcsi_filter_param.peer_mac[2] = 0xff;
	gwls_csi_cfg.gcsi_filter_param.peer_mac[3] = 0xff;
	gwls_csi_cfg.gcsi_filter_param.peer_mac[4] = 0xff;
	gwls_csi_cfg.gcsi_filter_param.peer_mac[5] = 0xff;
	gwls_csi_cfg.gcsi_filter_param.packet_bandwidth = 0xff;
	gwls_csi_cfg.gcsi_filter_param.num_rx = 0xff;
	gwls_csi_cfg.gcsi_filter_param.num_tx = 0xff;
	gwls_csi_cfg.gcsi_filter_param.packet_format = 0xff;

	gwls_csi_cfg.gcsi_filter_param.IIR_alpha = PI_ALPHA_FACTOR;
	gwls_csi_cfg.gcsi_filter_param.kalman_p0 = KALMAN_P0;
	gwls_csi_cfg.gcsi_filter_param.kalman_alpha = KALMAN_ALPHA;
	gwls_csi_cfg.gcsi_filter_param.kalman_N0 = KALMAN_N0;

	/*Initialize app private data with default values*/
	gwls_data.debug_level = 1;
	gwls_data.terminate_app = 0;
}

/**
 *  @brief              This function parses for NETLINK events
 *
 *  @param nlh          Pointer to Netlink message header
 *  @param bytes_read   Number of bytes to be read
 *  @param evt_conn     A pointer to a output buffer. It sets TRUE when it gets
 *  					the event CUS_EVT_OBSS_SCAN_PARAM, otherwise
 * FALSE
 *  @return             MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static int mlancsi_drv_nlevt_handler(struct nlmsghdr *nlh, int bytes_read,
				     int *evt_conn)
{
	int len, plen;
	t_u8 *buffer = NULL;
	t_u32 event_id = 0;
	event_header *event = NULL;
	char if_name[IFNAMSIZ + 1];

	/* Initialize receive buffer */
	buffer = (t_u8 *)malloc(NL_MAX_PAYLOAD);
	if (!buffer) {
		printf("ERR: Could not alloc buffer\n");
		return MLAN_STATUS_FAILURE;
	}
	memset(buffer, 0, NL_MAX_PAYLOAD);

	*evt_conn = FALSE;
	while ((unsigned int)bytes_read >= NLMSG_HDRLEN) {
		len = nlh->nlmsg_len; /* Length of message including header */
		plen = len - NLMSG_HDRLEN;
		if (len > bytes_read || plen < 0) {
			free(buffer);
			/* malformed netlink message */
			return MLAN_STATUS_FAILURE;
		}
		if ((unsigned int)len > NLMSG_SPACE(NL_MAX_PAYLOAD)) {
			printf("ERR:Buffer overflow!\n");
			free(buffer);
			return MLAN_STATUS_FAILURE;
		}
		memset(buffer, 0, NL_MAX_PAYLOAD);
		memcpy(buffer, NLMSG_DATA(nlh), plen);

		if (NLMSG_OK(nlh, len)) {
			memcpy(&event_id, buffer, sizeof(event_id));

			if (((event_id & 0xFF000000) == 0x80000000) ||
			    ((event_id & 0xFF000000) == 0)) {
				event = (event_header *)buffer;
			} else {
				memset(if_name, 0, IFNAMSIZ + 1);
				memcpy(if_name, buffer, IFNAMSIZ);
				event = (event_header *)(buffer + IFNAMSIZ);
			}
		}

		if (event) {
			if (!strncmp((char *)event, CUS_EVT_MLAN_CSI,
				     strlen(CUS_EVT_MLAN_CSI))) {
				int ret_val = proc_csi_event(event);
#ifdef PRINT_CSI_TO_FILE
				print_csi_event(event, bytes_read, if_name);
#endif
				if (ret_val == MLAN_STATUS_FAILURE) { // done,
								      // terminate
								      // app
					gwls_data.terminate_app = 1;
				}
			}
		}
		len = NLMSG_ALIGN(len);
		bytes_read -= len;
		nlh = (struct nlmsghdr *)((char *)nlh + len);
	}
	free(buffer);
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Configure and read event data from netlink socket
 *
 *  @param nl_sk        Netlink socket handler
 *  @param msg          Pointer to message header
 *  @param ptv          Pointer to struct timeval
 *
 *  @return             Number of bytes read or MLAN_STATUS_FAILURE
 */
static int read_event(int nl_sk, struct msghdr *msg, struct timeval *ptv)
{
	int count = -1;
	int ret = MLAN_STATUS_FAILURE;
	fd_set rfds;

	/* Setup read fds and initialize event buffers */
	FD_ZERO(&rfds);
	FD_SET(nl_sk, &rfds);

	/* Wait for reply */
	ret = select(nl_sk + 1, &rfds, NULL, NULL, ptv);

	if (ret == MLAN_STATUS_FAILURE) {
		/* Error */
		ptv->tv_sec = DEFAULT_SCAN_INTERVAL;
		ptv->tv_usec = 0;
		goto done;
	}
	if (!FD_ISSET(nl_sk, &rfds)) {
		/* Unexpected error. Try again */
		ptv->tv_sec = DEFAULT_SCAN_INTERVAL;
		ptv->tv_usec = 0;
		goto done;
	}
	/* Success */
	count = recvmsg(nl_sk, msg, 0);

done:
	return count;
}

/**
 *  @brief Run the application
 *
 *  @param nl_sk    Netlink socket
 *
 *  @return         N/A
 */
int mlancsi_event_monitor(int nl_sk)
{
	int ret = MLAN_STATUS_FAILURE;
	struct timeval tv;
	int bytes_read, evt_conn;
	struct msghdr msg;
	struct sockaddr_nl dest_addr;
	struct nlmsghdr *nlh;
	struct iovec iov;

	/* Initialize timeout value */
	tv.tv_sec = DEFAULT_SCAN_INTERVAL;
	tv.tv_usec = 0;

	/* Initialize netlink header */
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(NL_MAX_PAYLOAD));
	if (!nlh) {
		printf("[ERROR] Could not allocate space for netlink header\n");
	} else {
		memset(nlh, 0, NLMSG_SPACE(NL_MAX_PAYLOAD));
		/* Fill the netlink message header */
		nlh->nlmsg_len = NLMSG_SPACE(NL_MAX_PAYLOAD);
		nlh->nlmsg_pid = getpid(); /* self pid */
		nlh->nlmsg_flags = 0;

		/* Initialize I/O vector */
		memset(&iov, 0, sizeof(struct iovec));
		iov.iov_base = (void *)nlh;
		iov.iov_len = nlh->nlmsg_len;

		/* Set destination address */
		memset(&dest_addr, 0, sizeof(struct sockaddr_nl));
		dest_addr.nl_family = AF_NETLINK;
		dest_addr.nl_pid = 0; /* Kernel */
		dest_addr.nl_groups = NL_MULTICAST_GROUP;

		/* Initialize message header */
		memset(&msg, 0, sizeof(struct msghdr));
		msg.msg_name = (void *)&dest_addr;
		msg.msg_namelen = sizeof(dest_addr);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;

		while (!gwls_data.terminate_app) {
			/* event buffer is received for all the interfaces */
			bytes_read = read_event(nl_sk, &msg, &tv);
			/* handle only NETLINK events here */
			ret = mlancsi_drv_nlevt_handler((struct nlmsghdr *)nlh,
							bytes_read, &evt_conn);
		}

		free(nlh);
	}
	return ret;
}

/**
 *  @brief Convert char to hex integer
 *
 *  @param chr      Char to convert
 *  @return         Hex integer or 0
 */
static int hexval(t_s32 chr)
{
	if (chr >= '0' && chr <= '9')
		return chr - '0';
	if (chr >= 'A' && chr <= 'F')
		return chr - 'A' + 10;
	if (chr >= 'a' && chr <= 'f')
		return chr - 'a' + 10;

	return 0;
}

/**
 *  @brief Convert string to hex
 *
 *  @param ptr      A pointer to data buffer
 *  @param chr      A pointer to return integer
 *  @return         A pointer to next data field
 */
static char *convert2hex(char *ptr, t_u8 *chr)
{
	t_u8 val;

	for (val = 0; *ptr && isxdigit((unsigned char)*ptr); ptr++) {
		val = (val * 16) + hexval(*ptr);
	}

	*chr = val;

	return ptr;
}

/**
 *  @brief read current command
 *  @param ptr      A pointer to data
 *  @param curCmd   A pointer to the buf which will hold current command
 *  @return         NULL or the pointer to the left command buf
 */
static t_s8 *readCurCmd(t_s8 *ptr, t_s8 *curCmd)
{
	t_s32 i = 0;
#define MAX_CMD_SIZE 64 /**< Max command size */

	while (*ptr != ']' && i < (MAX_CMD_SIZE - 1))
		curCmd[i++] = *(++ptr);

	if (*ptr != ']')
		return NULL;

	curCmd[i - 1] = '\0';

	return ++ptr;
}

/**
 *  @brief parse command and hex data
 *  @param fp       A pointer to FILE stream
 *  @param dst      A pointer to the dest buf
 *  @param cmd      A pointer to command buf for search
 *  @return         Length of hex data or MLAN_STATUS_FAILURE
 */
static int fparse_for_cmd_and_hex(FILE *fp, t_u8 *dst, t_u8 *cmd)
{
	t_s8 *ptr;
	t_u8 *dptr;
	t_s8 buf[256], curCmd[64] = {0};
	t_s32 isCurCmd = 0;

	dptr = dst;
	while (fgets((char *)buf, sizeof(buf), fp)) {
		ptr = buf;

		while (*ptr) {
			/* skip leading spaces */
			while (*ptr && isspace((unsigned char)*ptr))
				ptr++;

			/* skip blank lines and lines beginning with '#' */
			if (*ptr == '\0' || *ptr == '#')
				break;

			if (*ptr == '[' && *(ptr + 1) != '/') {
				ptr = readCurCmd(ptr, curCmd);
				if (!ptr)
					return MLAN_STATUS_FAILURE;

				if (strcasecmp((char *)curCmd,
					       (char *)cmd)) /* Not equal */
					isCurCmd = 0;
				else
					isCurCmd = 1;
			}

			/* Ignore the rest if it is not correct cmd */
			if (!isCurCmd)
				break;

			if (*ptr == '[' && *(ptr + 1) == '/')
				return dptr - dst;

			if (isxdigit((unsigned char)*ptr)) {
				ptr = (t_s8 *)convert2hex((char *)ptr, dptr++);
			} else {
				/* Invalid character on data line */
				ptr++;
			}
		}
	}

	return MLAN_STATUS_FAILURE;
}

/**
 *  @brief Check the Hex String
 *  @param s  A pointer to the string
 *  @return   MLAN_STATUS_SUCCESS --HexString, MLAN_STATUS_FAILURE --not
 * HexString
 */
static int ishexstring(char *s)
{
	int ret = MLAN_STATUS_FAILURE;
	t_s32 tmp;

	if (!strncasecmp("0x", s, 2)) {
		s += 2;
	}
	while (*s) {
		tmp = toupper((unsigned char)*s);
		if (((tmp >= 'A') && (tmp <= 'F')) ||
		    ((tmp >= '0') && (tmp <= '9'))) {
			ret = MLAN_STATUS_SUCCESS;
		} else {
			ret = MLAN_STATUS_FAILURE;
			break;
		}
		s++;
	}

	return ret;
}

/**
 *    @brief isdigit for String.
 *
 *    @param x            Char string
 *    @return             MLAN_STATUS_FAILURE for non-digit.
 *                        MLAN_STATUS_SUCCESS for digit
 */
static int ISDIGIT(char *x)
{
	unsigned int i;
	for (i = 0; i < strlen(x); i++)
		if (isdigit((unsigned char)x[i]) == 0)
			return MLAN_STATUS_FAILURE;
	return MLAN_STATUS_SUCCESS;
}

/**
 * Check of decimal or hex string
 * @param   num string
 */
#define IS_HEX_OR_DIGIT(num)                                                   \
	(strncasecmp("0x", (num), 2) ? ISDIGIT((num)) : ishexstring((num)))

/**
 *  @brief Find and execute command
 *
 *  @param argc     Number of arguments
 *  @param argv     A pointer to arguments array
 *  @return         MLAN_STATUS_SUCCESS for success, otherwise failure
 */
static int process_command(int argc, char *argv[])
{
	int i = 0, ret = MLAN_STATUS_NOTFOUND;
	struct command_node *node = NULL;
	for (i = 0; i < (int)NELEMENTS(command_list); i++) {
		node = &command_list[i];
		if (!strcasecmp(node->name, argv[2])) {
			ret = node->handler(argc, argv);
			break;
		}
	}

	return ret;
}

/**
 *  @brief Prepare command buffer
 *  @param buffer   Command buffer to be filled
 *  @param cmd      Command id
 *  @param num      Number of arguments
 *  @param args     Arguments list
 *  @return         MLAN_STATUS_SUCCESS
 */
static int prepare_buffer(t_u8 *buffer, char *cmd, t_u32 num, char *args[])
{
	t_u8 *pos = NULL;
	unsigned int i = 0;
	unsigned int remaining = BUFFER_LENGTH;

	memset(buffer, 0, BUFFER_LENGTH);

	/* Flag it for our use */
	pos = buffer;
	memcpy((char *)pos, CMD_NXP, strlen(CMD_NXP));
	pos += (strlen(CMD_NXP));
	remaining -= strlen(CMD_NXP);

	/* Insert command */
	if (remaining < strlen(cmd))
		return MLAN_STATUS_FAILURE;
	strncpy((char *)pos, (char *)cmd, strlen(cmd));
	pos += (strlen(cmd));
	remaining -= strlen(cmd);

	/* Insert arguments */
	for (i = 0; i < num; i++) {
		if (remaining < strlen(args[i]))
			return MLAN_STATUS_FAILURE;
		strncpy((char *)pos, args[i], strlen(args[i]));
		pos += strlen(args[i]);
		remaining -= strlen(args[i]);

		if (i < (num - 1)) {
			if (remaining < 1)
				return MLAN_STATUS_FAILURE;
			memcpy((char *)pos, " ", strlen(" "));
			pos += 1;
			remaining -= 1;
		}
	}

	return MLAN_STATUS_SUCCESS;
}

#define CSI_FILTER_MAX 16
#define CSI_FILTER_SIZE 9
/**
 * @brief      Enable/disable CSI support
 *
 * @param argc Number of arguments
 * @param argv Pointer to the arguments array
 *
 * @return     MLAN_STATUS_SUCCESS/MLAN_STATUS_FAILURE
 */
static int process_csi_cmd(int argc, char *argv[])
{
	t_u8 *buffer = NULL;
	struct eth_priv_cmd *cmd;
	struct ifreq ifr;
	t_u8 csi_filter[CSI_FILTER_SIZE * CSI_FILTER_MAX];
	t_u8 headID[4];
	t_u8 tailID[4];
	t_u8 chipID = 0;
	t_u8 bandConfig = 0;
	t_u8 channel = 0;
	t_u8 csi_monitor_enable = 0;
	t_u8 ra4us = 0;
	t_u8 commonAGCflag = 0;
	t_u8 csiformat = 0;
	t_u8 i;
	t_u16 csi_enable;
	t_u16 offset = 0;
	t_u8 csi_filter_cnt;
	char csi_filter_name[20];
	int csi_filter_len = 0;
	int id_len = 0;
	FILE *fp = NULL;
	int cmd_header_len = 0, ret = 0;
	char filename[32];

	if (argc != 4) {
		printf("Error: Invalid number of arguments\n");
		display_help(NELEMENTS(mlancsi_csi_usage), mlancsi_csi_usage);
		return MLAN_STATUS_FAILURE;
	}
	/* Initialize buffer */
	buffer = (t_u8 *)malloc(BUFFER_LENGTH);
	if (!buffer) {
		printf("ERR:Cannot allocate buffer for command!\n");
		return MLAN_STATUS_FAILURE;
	}

	cmd_header_len = strlen(CMD_NXP) + strlen(argv[2]);
	prepare_buffer(buffer, argv[2], 0, NULL);
	cmd = (struct eth_priv_cmd *)malloc(sizeof(struct eth_priv_cmd));
	if (!cmd) {
		printf("ERR:Cannot allocate buffer for command!\n");
		free(buffer);
		return MLAN_STATUS_FAILURE;
	}

	/* Fill up buffer */
#ifdef USERSPACE_32BIT_OVER_KERNEL_64BIT
	memset(cmd, 0, sizeof(struct eth_priv_cmd));
	memcpy(&cmd->buf, &buffer, sizeof(buffer));
#else
	cmd->buf = buffer;
#endif
	cmd->used_len = 0;
	cmd->total_len = BUFFER_LENGTH;

	if (IS_HEX_OR_DIGIT(argv[3]) == MLAN_STATUS_FAILURE) {
		csi_enable = 1;
		/*copy filename from user */
		memset(filename, 0, sizeof(filename));
		strncpy(filename, argv[3], sizeof(filename) - 1);
		fp = fopen(filename, "r");
		if (fp == NULL) {
			perror("fopen");
			fprintf(stderr, "Cannot open CSI config file %s\n",
				filename);
			ret = -EFAULT;
			goto done;
		}

		snprintf(csi_filter_name, sizeof(csi_filter_name), "headID");
		id_len = fparse_for_cmd_and_hex(fp, headID,
						(t_u8 *)csi_filter_name);
		if (id_len != 4) {
			printf(" Expected head id size is 4 bytes\n");
			goto done;
		}
		for (i = 0; i < id_len; i++) {
			printf("%02x ", headID[i]);
		}
		printf("\n");

		snprintf(csi_filter_name, sizeof(csi_filter_name), "tailID");
		id_len = fparse_for_cmd_and_hex(fp, tailID,
						(t_u8 *)csi_filter_name);
		if (id_len != 4) {
			printf(" Expected tail id size is 4 bytes\n");
			goto done;
		}
		for (i = 0; i < id_len; i++) {
			printf("%02x ", tailID[i]);
		}
		printf("\n");

		snprintf(csi_filter_name, sizeof(csi_filter_name), "chipID");
		id_len = fparse_for_cmd_and_hex(fp, (t_u8 *)&chipID,
						(t_u8 *)csi_filter_name);
		if (id_len != 1) {
			printf(" Expected chip id size is 1 bytes\n");
			goto done;
		}
		printf("%02x \n", chipID);

		snprintf(csi_filter_name, sizeof(csi_filter_name),
			 "bandConfig");
		id_len = fparse_for_cmd_and_hex(fp, (t_u8 *)&bandConfig,
						(t_u8 *)csi_filter_name);
		if (id_len != 1) {
			printf(" Expected bandConfig size is 1 byte\n");
			goto done;
		}
		printf("%02x \n", bandConfig);

		snprintf(csi_filter_name, sizeof(csi_filter_name), "channel");
		id_len = fparse_for_cmd_and_hex(fp, (t_u8 *)&channel,
						(t_u8 *)csi_filter_name);
		if (id_len != 1) {
			printf(" Expected channel size is 1 byte\n");
			goto done;
		}
		printf("%02x \n", channel);

		snprintf(csi_filter_name, sizeof(csi_filter_name),
			 "csi_monitor_enable");
		id_len = fparse_for_cmd_and_hex(fp, (t_u8 *)&csi_monitor_enable,
						(t_u8 *)csi_filter_name);
		if (id_len != 1) {
			printf(" Expected csi_monitor_enable size is 1 byte\n");
			goto done;
		}
		printf("%02x \n", csi_monitor_enable);

		snprintf(csi_filter_name, sizeof(csi_filter_name), "ra4us");
		id_len = fparse_for_cmd_and_hex(fp, (t_u8 *)&ra4us,
						(t_u8 *)csi_filter_name);
		if (id_len != 1) {
			printf(" Expected ra4us size is 1 byte\n");
			goto done;
		}
		printf("%02x \n", ra4us);

		snprintf(csi_filter_name, sizeof(csi_filter_name),
			 "commonAGCflag");
		id_len = fparse_for_cmd_and_hex(fp, (t_u8 *)&commonAGCflag,
						(t_u8 *)csi_filter_name);
		if (id_len != 1) {
			printf(" Expected commonAGCflag size is 1 byte\n");
			goto done;
		} else if (commonAGCflag > 1) {
			printf("commonAGCflag value should be either 0 or 1\n");
			goto done;
		}
		printf("%02x \n", commonAGCflag);

		snprintf(csi_filter_name, sizeof(csi_filter_name), "csiformat");
		id_len = fparse_for_cmd_and_hex(fp, (t_u8 *)&csiformat,
						(t_u8 *)csi_filter_name);
		if (id_len != 1) {
			printf("Expected csiformat size is 1 byte\n");
			goto done;
		} else if (csiformat > 3) {
			printf(" CSI format value should be in range 0 to 3\n");
			goto done;
		}
		printf("%02x \n", csiformat);

		/* Parse CSI filters */
		for (csi_filter_cnt = 0; csi_filter_cnt < CSI_FILTER_MAX;
		     csi_filter_cnt++) {
			snprintf(csi_filter_name, sizeof(csi_filter_name),
				 "csifilter%d", csi_filter_cnt);
			csi_filter_len = fparse_for_cmd_and_hex(
				fp,
				&csi_filter[CSI_FILTER_SIZE * csi_filter_cnt],
				(t_u8 *)csi_filter_name);

			printf("Found %d bytes in the csifilter%d section of conf file %s.\n",
			       csi_filter_len, csi_filter_cnt, filename);
			if (csi_filter_len != CSI_FILTER_SIZE) {
				printf(" Expected filter size is %d\n",
				       CSI_FILTER_SIZE);
				break;
			} else {
				for (i = 0; i < CSI_FILTER_SIZE; i++) {
					printf("%02x ",
					       csi_filter[CSI_FILTER_SIZE *
								  csi_filter_cnt +
							  i]);
				}
				printf("\n");
			}
		}
		printf("Found %d CSI filters\n", csi_filter_cnt);
		memcpy(buffer + cmd_header_len, (t_u8 *)&csi_enable,
		       sizeof(csi_enable));
		offset = sizeof(csi_enable);
		memcpy(buffer + cmd_header_len + offset, headID,
		       4 * sizeof(t_u8));
		offset += (4 * sizeof(t_u8));
		memcpy(buffer + cmd_header_len + offset, tailID,
		       4 * sizeof(t_u8));
		offset += (4 * sizeof(t_u8));
		memcpy(buffer + cmd_header_len + offset,
		       (t_u8 *)&csi_filter_cnt, sizeof(csi_filter_cnt));
		offset += sizeof(csi_filter_cnt);
		memcpy(buffer + cmd_header_len + offset, (t_u8 *)&chipID,
		       sizeof(chipID));
		offset += sizeof(chipID);
		memcpy(buffer + cmd_header_len + offset, (t_u8 *)&bandConfig,
		       sizeof(bandConfig));
		offset += sizeof(bandConfig);
		memcpy(buffer + cmd_header_len + offset, (t_u8 *)&channel,
		       sizeof(channel));
		offset += sizeof(channel);
		memcpy(buffer + cmd_header_len + offset,
		       (t_u8 *)&csi_monitor_enable, sizeof(csi_monitor_enable));
		offset += sizeof(csi_monitor_enable);
		memcpy(buffer + cmd_header_len + offset, (t_u8 *)&ra4us,
		       sizeof(ra4us));
		offset += sizeof(ra4us);
		memcpy(buffer + cmd_header_len + offset, (t_u8 *)&commonAGCflag,
		       sizeof(commonAGCflag));
		offset += sizeof(commonAGCflag);
		memcpy(buffer + cmd_header_len + offset, (t_u8 *)&csiformat,
		       sizeof(csiformat));
		offset += sizeof(csiformat);

		if (csi_filter_cnt > 0)
			memcpy(buffer + cmd_header_len + offset, csi_filter,
			       CSI_FILTER_SIZE * csi_filter_cnt);
	} else {
		csi_enable = (t_u16)A2HEXDECIMAL(argv[3]);
		if (csi_enable == 0) {
			memcpy(buffer + cmd_header_len, (t_u8 *)&csi_enable,
			       sizeof(csi_enable));
		} else {
			printf("Err: Invalid CSI command parameter\n");
			display_help(NELEMENTS(mlancsi_csi_usage),
				     mlancsi_csi_usage);
			ret = MLAN_STATUS_FAILURE;
			goto done;
		}
	}

	/* Initialize the ifr structure */
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
	ifr.ifr_ifru.ifru_data = (void *)cmd;

	if (ioctl(sockfd, MLAN_ETH_PRIV, &ifr)) {
		perror("mlancsi: csi ioctl");
		ret = -EFAULT;
		goto done;
	}

done:
	if (fp)
		fclose(fp);

	if (buffer)
		free(buffer);
	if (cmd)
		free(cmd);
	return ret;
}

/**
 * @brief      CSI event support
 *
 * @param argc Number of arguments
 * @param argv Pointer to the arguments array
 *
 * @return     MLAN_STATUS_SUCCESS/MLAN_STATUS_FAILURE
 */
static int process_event_cmd(int argc, char *argv[])
{
	int dev_index = 0; /*Default device index is for mlan0, initialise with
			      -1 to open multiple NETLINK Sockets */
	/**< socket descriptor */
	t_s32 nl_sk; /**< netlink socket descriptor to receive an event */
	int ret = MLAN_STATUS_SUCCESS;
	if (argc == 4) {
		ret = mlancsi_read_config(argv[CSI_CFG_FILE_ARG_INDEX]);
	}
	if ((ret == MLAN_STATUS_FAILURE) || (argc > 4)) {
		display_help(NELEMENTS(mlancsi_event_usage),
			     mlancsi_event_usage);
		return ret;
	}
	/* create netlink sockets and bind them with app side addr */
	nl_sk = open_netlink(dev_index);

	if (nl_sk < 0) {
		DBG_ERROR("[ERROR] mlancsi: Cannot open netlink socket.\n");
		return MLAN_STATUS_FAILURE;
	}

	signal(SIGHUP, mlancsi_terminate_handler); /* catch hangup signal */
	signal(SIGTERM, mlancsi_terminate_handler); /* catch kill signal */
	signal(SIGINT, mlancsi_terminate_handler); /* catch kill signal */
	signal(SIGALRM, mlancsi_terminate_handler); /* catch kill signal */

	ret = mlancsi_event_monitor(nl_sk);

	if (nl_sk > 0) {
		close(nl_sk);
	}
	return ret;
}

/********************************************************
		Global Functions
********************************************************/
/**
 *  @brief Entry function for mlancsi processing app
 *  @param argc		number of arguments
 *  @param argv     A pointer to arguments array
 *  @return      	MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
int main(int argc, char *argv[])
{
	int ret = MLAN_STATUS_SUCCESS;

	printf("\n\n---------------------------------------------------\n");
	printf("------- NXP Wifi CSI Processing app v%s ------\n", MLANCSI_VER);
	printf("---------------------------------------------------\n\n");

	if (argc < 3) {
		display_help(NELEMENTS(mlancsi_help), mlancsi_help);
		return MLAN_STATUS_FAILURE;
	}

	/*Initialize private data*/
	printf("[INFO] Initializing App\n");
	mlancsi_init();

	/*Set the interface*/
	memset(dev_name, 0, sizeof(dev_name));
	strncpy(dev_name, argv[1], IFNAMSIZ);

	/* create a socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		DBG_ERROR("[ERROR] mlancsi: Cannot open socket.\n");
		return MLAN_STATUS_FAILURE;
	}

	ret = process_command(argc, argv);

	if (ret == MLAN_STATUS_NOTFOUND) {
		printf("[ERROR] Command %s is not supported\n",
		       argv[CSI_SUBCMD_INDEX]);
		display_help(NELEMENTS(mlancsi_help), mlancsi_help);
		ret = MLAN_STATUS_FAILURE;
	}

	if (sockfd > 0)
		close(sockfd);

	return ret;
}
