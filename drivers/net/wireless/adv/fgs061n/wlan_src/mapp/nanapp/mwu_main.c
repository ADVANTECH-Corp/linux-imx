/*
 *  Copyright 2012-2024 NXP
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
/** mwu_main.c (derived from wps_main.c)
 *
 * mwu is a daemon that manages wifi-direct, wps, wpa connectivity.  After
 * launch, it is controlled via a socket interface.  The control application
 * that sends commands to and receives events from the socket interface is
 * usually written custom for the system.  See mwu_cli for an example of such
 * an application.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#define UTIL_LOG_TAG "MWU SM"
#include "version.h"
#include "mwu_log.h"
#include "mwu_ioctl.h"
#include "lockf.h"
#include "wps_os.h"
#include "wps_wlan.h"
#include "wlan_hostcmd.h"
#include "util.h"
#include "mwu_internal.h"
#include "queue.h"
#include "mwu_if_manager.h"

#include "mlocation_mwu.h"

#include "event.h"

#if 0
#include "wps_def.h"
#include "wps_msg.h"
#include "mwu_test.h"
#include "wlan_wifidir.h"
#include "wifidir_mwu.h"
#include "wifidir.h"
#include "mwpamod_mwu.h"
#include "mwpamod.h"
#include "mwpsmod_mwu.h"
#include "mwpsmod.h"
#ifdef CONFIG_HOTSPOT
#include "mhotspotmod_mwu.h"
#endif
#endif

#include "nan_mwu.h"

#ifdef ANDROID
#define PID_FILE "/tmp/mwu.pid"
#define RUN_DIR "/tmp"
#define DEFAULT_WPA_OUTFILE "/data/wpas_wifidir.conf"
#else
#define PID_FILE "/var/run/mwu.pid"
#define DEFAULT_WPA_OUTFILE "/tmp/wpas_wifidir.conf"
#endif

/* location of output file where wpa credential will be written. */
char *wpa_outfile = DEFAULT_WPA_OUTFILE;

/** Configuration file for initialization */
char *wps_init_cfg_file;

/** WIFIDIR Config file for initial download */
char *wifidir_cfg_file;

struct EVENT_INFO evt_info;

wls_csi_cfg_t gwls_csi_cfg;

/** Option string for the app*/
#define OPTSTR "hiBl:Vv"
static void help_menu(char *program)
{
	printf("\nUsage: nanapp [options]\n\n");
	printf("Options:\n");
	printf("-h                   Print this help message\n");
	printf("-v                   Print version and exit\n");
	printf("-i                   Start interactive CLI for NAN commands\n");
	printf("-B                   Run in background (Daemonize)\n");
	printf("-l <logfile>         Print to logfile instead of stdout\n");
	printf("-V                   Be verbose.\n");
	// printf("-m                   interface index [ex. 0/1/2]\n");
	return;
}

/**
 *  @brief Linux alarm signal handler
 *
 *  @param sig_num      signal number
 *  @return             None
 */
#if 0
static void
wps_alarm_handler(int sig_num)
{
    ENTER();

    if (sig_num != SIGALRM)
        return;

    printf("\n");
    LEAVE();
    exit(1);
}
#endif

static void mwu_signal_handler(int sig_num)
{
	static int flag = 0;

	ENTER();

	if ((sig_num == SIGINT || sig_num == SIGTERM) && !flag) {
		++flag;

		wps_main_loop_shutdown();

		mwu_iface_deinit_all();

		// wps_clear_running_instance();

		/* Use SIGALRM to break out from potential busy loops that would
		   not allow the program to be killed. */
		// signal(SIGALRM, wps_alarm_handler);
		// alarm(2);
	}

	LEAVE();
}

static void mwu_set_signal_handler(void)
{
	signal(SIGINT, mwu_signal_handler);
	signal(SIGTERM, mwu_signal_handler);
}

extern char *optarg;
extern int optind, opterr, optopt;
int cli_exit = 0;
int cli_main(int argc, char **argv);

/**
 *  @brief Initialize ftm command private data
 *  @return     MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int wls_csi_init(void)
{
	int ret = MLAN_STATUS_SUCCESS;

	memset(&gwls_csi_cfg, 0, sizeof(wls_csi_cfg_t));

	gwls_csi_cfg.channel = 0;

	/*CSI processing config*/
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

	return ret;
}

int main(int argc, char *argv[])
{
	int exitcode;
	int opt;
	int ret;
	char *logfile = NULL;
	int background = 0;
	int lock;
	char str[sizeof(pid_t) * 2 + 2]; /* just for sprintfing the PID\n\0 */
	char *adapter_idx = NULL;
	char proc_entry[32];

	exitcode = WPS_STATUS_SUCCESS;

	printf("\n**************************************************\n");
	printf("********  NXP Wi-Fi Aware Utility (NANAPP)********\n");
	printf("********       Version: %s            ********\n", VERSION);
	printf("**************************************************\n");

#ifdef ENABLE_STRICT_ALIGNMENT_CHECK
#if defined(__GNUC__)
#if defined(__i386__)
	/* Enable Alignment Checking on x86 */
	INFO("**** Enabling strict alignment check on x86 ****");
	__asm__("pushf\norl $0x40000,(%esp)\npopf");
#elif defined(__x86_64__)
	/* Enable Alignment Checking on x86_64 */
	INFO("**** Enabling strict alignment check on x86_64 ****");
	__asm__("pushf\norl $0x40000,(%rsp)\npopf");
#endif
#endif
#endif

	/* initialize default arguments */
	wps_init_cfg_file = NULL;
	wifidir_cfg_file = NULL;
	wps_init_cfg_file = FILE_WPSINIT_CONFIG_NAME;
	memset(&evt_info, 0, sizeof(evt_info));

	opt = getopt(argc, argv, OPTSTR);
	do {
		switch ((char)opt) {
		case 'h':
			help_menu(argv[0]);
			return WPS_STATUS_SUCCESS;

		case 'i':
			printf("\n[INFO] Starting Command Line Interface for NAN\n\n");
			cli_main(argc, argv);
			cli_exit = 1;
			return WPS_STATUS_SUCCESS;

		case 'B':
			printf("\n[INFO] Starting NANAPP Daemon service\n\n");
			background = 1;
			break;
#if 0
	    case 'm':
            adapter_idx = optarg;
            break;
#endif
		case 'l':
			printf("\n[INFO] Log file %s\n\n", optarg);
			logfile = optarg;
			break;

		case 'V':
			printf("Verbose mode enabled\n");
			mwu_set_debug_level(DEBUG_ALL);
			break;
		case 'v':
			printf("Version : %s\n", VERSION);
			return WPS_STATUS_SUCCESS;

		default: /* '?' */
			ERR("Unsupported option: -%c\n", (char)opt);
			help_menu(argv[0]);
			return WPS_STATUS_FAIL;
		}
	} while ((opt = getopt(argc, argv, OPTSTR)) != -1);

	/* Never run more than one instance of mwu */
#ifdef ANDROID
	chdir(RUN_DIR);
#endif
	lock = open(PID_FILE, O_RDWR | O_CREAT, 0640);
	if (lock < 0) {
		ERR("Failed to create PID file %s\n", PID_FILE);
		return errno;
	}
	ret = lockf(lock, F_TEST, 0);
	if (-1 == ret && (errno == EACCES || errno == EAGAIN)) {
		ERR("pid file is locked.  If mwu is not running, rm %s\n",
		    PID_FILE);
		return WPS_STATUS_FAIL;
	}
	if (ret != 0) {
		perror("failed to check lock file");
		return errno;
	}

	wps_loop_init();
	wls_csi_init();
	mwu_set_signal_handler();

#if 1
#define CDRV_PROC_CONFIG "/proc/mwlan/config"
#define MXDRV_PROC_CONFIG "/proc/mwlan/adapter0/config"
/* The below macro is used to initlize netlink events for dual MAC chipsets*/
#define MXDRV_PROC_CONFIG_1 "/proc/mwlan/adapter1/config"
	memset(proc_entry, 0, sizeof(proc_entry));
	/** Initialize netlink socket interface for receiving events. */
	if (adapter_idx != NULL) {
		strcpy(proc_entry, CDRV_PROC_CONFIG);
		if (adapter_idx[0] != '0')
			strcat(proc_entry, adapter_idx);
		if (access(proc_entry, F_OK) != 0) {
			memset(proc_entry, 0, sizeof(proc_entry));
			sprintf(proc_entry, "/proc/mwlan/adapter%s/config",
				adapter_idx);
			if (access(proc_entry, F_OK) != 0)
				memset(proc_entry, 0, sizeof(proc_entry));
		}
	} else {
		if (access(CDRV_PROC_CONFIG, F_OK) == 0)
			strcpy(proc_entry, CDRV_PROC_CONFIG);
		else if (access(MXDRV_PROC_CONFIG, F_OK) == 0)
			strcpy(proc_entry, MXDRV_PROC_CONFIG);
	}

	if (proc_entry[0] != '\0') {
		if (wps_event_init(proc_entry) != 0) {
			exitcode = WPS_STATUS_FAIL;
			ERR("Fail to initialize event socket !\n");
			goto exit;
		}
	} else {
		exitcode = WPS_STATUS_FAIL;
		ERR("Unable to detect SoC, Either driver not loaded or no chip present\n");
		goto exit;
	}
	if (adapter_idx == NULL) {
		if (access(MXDRV_PROC_CONFIG_1, F_OK) == 0) {
			memset(proc_entry, 0, sizeof(proc_entry));
			strcpy(proc_entry, MXDRV_PROC_CONFIG_1);
			if (proc_entry[0] != '\0' &&
			    (wps_event_init(proc_entry) != 0)) {
				ERR("Failed to initialize socket for adapter 1\n");
			}
		}
	}
#endif
	/* initialize the socket interface for the mwu interface */
	ret = mwu_internal_init();
	if (ret != MWU_ERR_SUCCESS)
		return ret;
#if 0
    /* initialize the mwu test module */
    ret = test_init();
    if (ret != MWU_ERR_SUCCESS)
        return ret;

    /* initialize the wifi direct module with the mwu interface */
    ret = wifidir_mwu_launch();
    if (ret != MWU_ERR_SUCCESS)
        return ret;

    ret = wifidir_launch();
    if (ret != WIFIDIR_ERR_SUCCESS)
        return ret;

    /* initialize the wps module with the mwu interface */
    ret = mwpsmod_mwu_launch();
    if (ret != MWU_ERR_SUCCESS)
        return ret;

    ret = mwpsmod_launch();
    if (ret != MWPSMOD_ERR_SUCCESS)
        return ret;

    /*Initilize the mwpamod modue with the mwu interface */
    ret = mwpamod_mwu_launch();
    if (ret != WIFIDIR_ERR_SUCCESS)
        return ret;

    /* Launch mwpamod module Initialization of the module can be done by higher
     * layer after calling mwpamod_sta_init() or mwpamod_ap_init
     */
    ret = mwpamod_launch();
    if (ret != MWPAMOD_ERR_SUCCESS)
        return ret;

#ifdef CONFIG_HOTSPOT
    /* Launch hotspot module Initialization of the module can be done by higher
        * layer after calling
        */
    ret = hotspot_mwu_launch();
    if (ret != MWPAMOD_ERR_SUCCESS)
        return ret;
#endif
#endif

	/* initialize the mlocation module */
	ret = mlocation_mwu_launch();
	if (ret != MWU_ERR_SUCCESS)
		return ret;

	/* initialize the nan module with the mwu interface */
	ret = nan_mwu_launch();
	if (ret != MWU_ERR_SUCCESS)
		return ret;

	/* Go into the background if necessary */
	if (background) {
		ret = fork();
		if (ret < 0) {
			perror("failed to fork mwu");
			return errno;
		} else if (ret > 0) {
			/* parent process. */
			INFO("Launched mwu\n");
			return 0;
		} else {
			/* child process */
			setsid();

			/* redirect stdin, out, and err */
			if (freopen("/dev/null", "r", stdin) == NULL)
				return errno;
			if (logfile) {
				if (freopen(logfile, "w", stdout) == NULL) {
					return errno;
				}
			} else {
				if (freopen("/dev/null", "w", stdout) == NULL)
					return errno;
			}
			umask(027);
			ret = lockf(lock, F_TLOCK, 0);
			if (ret < 0) {
				return errno;
			}
			sprintf(str, "%d\n", getpid());
			ret = write(lock, str, strlen(str));
			if (0 > ret) {
				return errno;
			}

			signal(SIGCHLD, SIG_IGN);
			signal(SIGTSTP, SIG_IGN);
			signal(SIGTTOU, SIG_IGN);
			signal(SIGTTIN, SIG_IGN);
			signal(SIGHUP, mwu_signal_handler);
			signal(SIGINT, mwu_signal_handler);
			signal(SIGTERM, mwu_signal_handler);
		}
	} else {
		umask(027);
		ret = lockf(lock, F_TLOCK, 0);
		if (ret < 0) {
			return errno;
		}
		sprintf(str, "%d\n", getpid());
		ret = write(lock, str, strlen(str));
		if (0 > ret) {
			return errno;
		}
	}
#if 1
	/* Enable main loop procedure */
	wps_main_loop_enable();

	/* Main loop for socket read and timeout function */
	wps_main_loop_proc();

	/* For unconditionally, the function has checks for NULL, 0 */
	wps_event_deinit();
#endif

exit:
#if 0
    mwpsmod_halt();
    mwpsmod_mwu_halt();

    wifidir_halt();
    wifidir_mwu_halt();

    mwpamod_halt();
    mwpamod_mwu_halt();

    mlocationmod_halt();
    mlocation_mwu_halt();

    test_deinit();
#endif
	if (cli_exit != 1) {
		wps_loop_deinit();
		mwu_internal_deinit();
	}
	return exitcode;
}
