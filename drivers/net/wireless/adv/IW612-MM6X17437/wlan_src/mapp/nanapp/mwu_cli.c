/*
 *  Copyright 2012-2020 NXP
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
/* mwu_cli.c: command-line utility for marvell wireless utilities
 */
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#if !defined(ANDROID) || defined(HAVE_GLIBC)
#include <unistd.h>
#endif
#include "mwu.h"
#include "mwu_defs.h"

#define ERR(...) fprintf(stderr, __VA_ARGS__)

static int send_message(struct mwu *mwu, int argc, char **argv)
{
	int max_msg_size, i, remaining, written, ret = 0;
	struct mwu_msg *msg = NULL, *resp = NULL;
	char *p;

	/* How big of a message buffer should we use?  This is module-dependent.
	 * For now just use 1kB and fail if we can't fit all of the key-value
	 * pairs.  In the future, we may want to peek at the kv pairs, find the
	 * module=<module> param and decide based on that.
	 */
	max_msg_size = 1024;

	msg = malloc(sizeof(struct mwu_msg) + max_msg_size);
	if (msg == NULL) {
		ERR("cli: Failed to allocate memory for message\n");
		goto done;
	}
	msg->data[0] = 0;

	p = msg->data;
	remaining = max_msg_size - 1; /* don't forget about trailing 0! */
	written = 0;
	for (i = 0; i < argc; i++) {
		written = snprintf(p, remaining, "%s\n", argv[i]);
		if (written >= remaining) {
			ERR("cli: Failed to fit all key-value pairs in message.\n");
			ret = -1;
			goto done;
		}
		remaining -= written;
		p += written;
	}
	msg->len = max_msg_size - remaining;

	/* Okay.  Send the message out */
	ret = mwu_send_message(mwu, msg, &resp);
	if (ret != MWU_ERR_SUCCESS) {
		ERR("cli: Failed to send message (ret=%d)\n", ret);
		goto done;
	}

	/* print response message if necessary */
	if (resp) {
		printf("%s", resp->data);
		mwu_free_msg(resp);
	}

done:
	FREE(msg);

	return ret;
}

static int send_message_oneshot(int argc, char **argv)
{
	struct mwu mwu;
	int ret;

	/* connect to mwu */
	ret = mwu_connect(&mwu);
	if (ret != MWU_ERR_SUCCESS) {
		ERR("cli: Failed to connect to mwu (ret=%d).  Is daemon running?\n",
		    ret);
		return -1;
	}

	ret = send_message(&mwu, argc, argv);

	mwu_disconnect(&mwu);

	return ret;
}

static struct mwu mwu;
void clean_up(int signum)
{
	printf("Disconnecting from mwu\n");
	mwu_disconnect(&mwu);
}

#define IS_WHITE(c) ((c) == ' ' || (c) == '\t')

/* given a string str, make **argv point to its parts and set argc to the
 * number of args.  Return 0 or an error.  Note that the caller is responsible
 * for freeing *argv.
 *
 * This function could use some improvement.  Namely, it doesn't handle
 * trailing space or multiple whitespace very well.
 */
static int str_to_argv(char *str, int *argc, char ***argv)
{
	int i = 0;
	char *arg, c;

	*argv = NULL;
	*argc = 0;

	/* advance past any initial whitespace */
	while (IS_WHITE(str[i]))
		i++;

	if (str[i] == 0)
		return 0;

	arg = &str[i];

	while (1) {
		switch (str[i]) {
		case '\0':
			/* this is the end of the string.  Process it like
			 * usual, but quit
			 */
		case ' ':
			/* Just finished parsing an arg.  Terminate it and add
			 * it to the argv list
			 */
			c = str[i];
			str[i] = 0;
			*argc = *argc + 1;
			*argv = realloc(*argv, *argc * sizeof(char *));
			if (!*argv) {
				ERR("Failed to alloc argv\n");
				return -1;
			}
			(*argv)[*argc - 1] = arg;
			i++;
			while (IS_WHITE(str[i]))
				i++;
			arg = &str[i];
			if (c == 0)
				goto done;
			break;

		case '"':
			/* This is the beginning of a quoted string.  Find the
			 * end. */
			i++;
			while (str[i] != 0 && str[i] != '"') {
				i++;
			}
			if (str[i] == 0) {
				ERR("Unterminated quoted string\n");
				return -1;
			}
			i++;
			break;

		default:
			i++;
			break;
		}
	}

done:
	return 0;
}

static void cli(void)
{
	int ret = 0, maxfd;
	fd_set rdset;
	char cmdline[256];
	int end = 0;
	char **argv;
	int argc;
	struct mwu_msg *msg;
	char *p;

	/* connect to mwu */
	ret = mwu_connect(&mwu);
	if (ret != MWU_ERR_SUCCESS) {
		ERR("cli: Failed to connect to mwu (ret=%d).  Is daemon running?\n",
		    ret);
		return;
	}

	printf("> ");
	fflush(stdout);

	ret = fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	if (ret == -1) {
		ERR("Failed to make STDIN nonblocking: %s\n", strerror(errno));
		goto done;
	}

	while (1) {
		FD_ZERO(&rdset);
		FD_SET(STDIN_FILENO, &rdset);
		maxfd = STDIN_FILENO;
		FD_SET(mwu.efd, &rdset);
		maxfd = mwu.efd > maxfd ? mwu.efd : maxfd;

		ret = select(maxfd + 1, &rdset, NULL, NULL, NULL);
		if (ret == -1 && errno == EINTR) {
			/* somebody hit ctrl-C or something */
			goto done;
		}

		if (ret == -1) {
			ERR("select failed: %s\n", strerror(errno));
			goto done;
		}
		if (FD_ISSET(STDIN_FILENO, &rdset)) {
			ret = read(STDIN_FILENO, &cmdline[end],
				   sizeof(cmdline) - end);
			if (ret == -1) {
				ERR("stdin read failed: %s\n", strerror(errno));
				goto done;
			}
			end = end + ret;
			if (end == sizeof(cmdline)) {
				ERR("cmd buffer overflow!\n");
				end = 0;
			} else if (cmdline[end - 1] == '\n') {
				cmdline[end - 1] = 0;
				ret = str_to_argv(cmdline, &argc, &argv);
				if (ret != 0)
					continue;
				if (argc != 0) {
					send_message(&mwu, argc, argv);
					FREE(argv);
				}
				end = 0;
				printf("> ");
				fflush(stdout);
			}
		}

		if (FD_ISSET(mwu.efd, &rdset)) {
			ret = mwu_recv_message(&mwu, &msg);
			if (ret != MWU_ERR_SUCCESS) {
				ERR("Failed to read event: ret=%d\n", ret);
				continue;
			}
			/* Remove newlines so printing is prettier */
			p = msg->data;
			while (*p != '\0') {
				if (*p == '\n')
					*p = ' ';
				p++;
			}
			printf("\nRECEIVED EVENT: %s\n", msg->data);
			mwu_free_msg(msg);
		}
	}

done:
	mwu_disconnect(&mwu);
}

#define HELP_TEXT                                                                 \
	"Usage: mwu_cli [options]\n\n"                                            \
	"       mwu_cli [options] [key1=val1 [...]]\n\n"                          \
	"When invoked without any trailing arguments, mwu_cli drops into an\n"    \
	"interactive shell.  When invoked with a module, command, and optional\n" \
	"arguments, it sends the specified key-value pairs to mwu as a single\n"  \
	"message\n"                                                               \
	"\n"                                                                      \
	"-h           Print this help text\n"

int cli_main(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
		case 'h':
			printf(HELP_TEXT);
			return 0;
			break;

		default:
			ERR("Unknown option: %c\n", opt);
			return -1;
		}
	}

	if (optind >= argc) {
		signal(SIGHUP, clean_up);
		signal(SIGTERM, clean_up);
		signal(SIGINT, clean_up);
		cli();
	} else {
		return send_message_oneshot(argc - optind, &argv[optind]);
	}
	return 0;
}
