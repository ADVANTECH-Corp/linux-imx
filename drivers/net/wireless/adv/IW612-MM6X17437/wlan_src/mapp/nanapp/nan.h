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

#ifndef __NAN_INTERNAL__
#define __NAN_INTERNAL__

/* Internal interface for "wifidirect" module
 *
 * ...
 * ...
 * ...
 * ...
 * ...
 * ...
 * ...
 * ...
 * ...
 * ...
 * ...
 * ...
 * ...
 *
 */

#include <ctype.h>
#include <unistd.h>
#include "queue.h"
#include "module.h"
#include "wps_def.h"
#include "mwu_timer.h"
#include "wlan_hostcmd.h" // for mrvl_cmd_head_buf
#include "mwu_defs.h"

#define MAX_SERVICE_NAME 256
#define MAX_FILTER 256
#define MAX_SERVICE_INFO 256
#define NAN_DEVICE_ADDR_FLIP_BIT (0x1 << 1)

#define MAX_SCHEDULE_ENTRIES 4
#define NAN_PMK_LEN 32
#define DEFAULT_2G_OP_CLASS 81
#define DEFAULT_2G_OP_CHAN 6
#define DEFAULT_5G_OP_CLASS 125
#define DEFAULT_5G_OP_CHAN 149
#define DEFAULT_WINDOW_SIZE 512
#define DEFAULT_BITMAP_LEN 32
#define NAN_DEFAULT_BITMAP 0x0ffffe7e
#define NAN_DEFAULT_CONCURRENT_BITMAP 0x07e00006
#define NAN_POTENTIAL_BITMAP 0x7ffffe7e
#define NAN_COUNTER_BITMAP 0x7f7ff67e
#define NAN_QOS_MIN_SLOTS 0
#define NAN_QOS_MAX_LATENCY 65535
/*FA types*/
#define NAN_FAV 0
#define NAN_NDL 1
#define NAN_RANGE 2

#define IPV6_IFACE_IDENTIFIER_LEN 8

/* enum wifidir_error
 *
 * These are the return codes that can be returned by the wifidirect module
 *
 * NAN_ERR_SUCCESS: The operation succeeded.
 * NAN_ERR_BUSY: The module is busy and cannot handle the operation.
 * NAN_ERR_INVAL: The arguments were invalid.
 * NAN_ERR_NOMEM: Insufficient available memory.
 * NAN_ERR_COM: A low-level driver error occurred.
 * NAN_ERR_UNSUPPORTED: The operation is not supported.
 * NAN_ERR_RANGE: Parameter is out of range.
 * NAN_ERR_NOENT: No such entity.
 * NAN_ERR_TIMEOUT: the operation timed out
 * NAN_ERR_NOTREADY: the module is not ready to perform the operation
 */
enum nan_error {
	NAN_ERR_SUCCESS = 0,
	NAN_ERR_BUSY,
	NAN_ERR_INVAL,
	NAN_ERR_NOMEM,
	NAN_ERR_COM,
	NAN_ERR_UNSUPPORTED,
	NAN_ERR_RANGE,
	NAN_ERR_NOENT,
	NAN_ERR_TIMEOUT,
	NAN_ERR_NOTREADY,
};

/* enum nan_state
 * These are the all possible states of the NAN state machine. The state
 * is not to be modified by anybody except the state machine
 */

enum nan_state {
	NAN_STATE_INIT,
	NAN_STATE_IDLE,
	NAN_STATE_STARTED,
	NAN_STATE_PUBLISH,
	NAN_STATE_SUBSCRIBE,
};

enum ndp_state {
	NDP_IDLE = 0,
	NDP_INIT,
	NDP_CONFIRM_PENDING,
	NDP_CONNECTED,
	NDP_INVALID = 255,
};

enum nan_event_type {
	NAN_EVENT_DISCOVERY_RESULT,
	NAN_EVENT_FOLLOW_UP_RECVD,
	NAN_EVENT_REPLIED,
	NAN_EVENT_DATA_INDICATION,
	NAN_EVENT_DATA_CONFIRM,
	NAN_EVENT_DATA_TERMINATION,
	NAN_EVENT_NDPE_DATA,
};

/* nan_neighbour: structure representing a nan neighbour device
 *
 * device_name: The name of this peer device
 *
 * list_item: list data used to keep the wifidir_peer in a list.  This data is
 * for internal use only.
 */
struct nan_neighbour {
	/* @TODO: add elements to neighbour struct */

	/* private members */
	SLIST_ENTRY(nan_neighbour) list_item;
};

#define NAN_BUF_HDR_SIZE (sizeof(struct nan_generic_buf))
struct nan_generic_buf {
	int size;
	int used;
	int buf_type;
	u8 buf[0];
};

struct nan_cfg {
	unsigned char a_band;
	int disc_bcn_period;
	int scan_time;
	unsigned char master_pref;
};

struct nan_publish_service_conf {
	char service_name[MAX_SERVICE_NAME];
	u8 tx_filter_len;
	char matching_filter_tx[MAX_FILTER];
	u8 rx_filter_len;
	char matching_filter_rx[MAX_FILTER];
	char service_info[MAX_SERVICE_INFO];
	int publish_type;
	u8 discovery_range;
	int solicited_tx_type;
	int announcement_period;
	int ttl;
	int event_cond;
	int matching_filter_flag;
	int bloom_filter_index;
	char bloom_filter_tx[256];
};

struct nan_subscribe_service {
	char service_name[MAX_SERVICE_NAME];
	u8 rx_filter_len;
	char matching_filter_rx[MAX_FILTER];
	u8 tx_filter_len;
	char matching_filter_tx[MAX_FILTER];
	char service_info[MAX_SERVICE_INFO];
	int subscribe_type;
	u8 discovery_range;
	int query_period;
	int ttl;
	int matching_filter_flag;
};

typedef struct _nan_ndp_req {
	int type;
	int pub_id;
	char responder_mac[ETH_ALEN];
	int security;
	char service_info[MAX_SERVICE_INFO];
	int confirm_required;
} nan_ndp_req;

typedef struct _nan_ndp_resp {
	int type;
	int pub_id;
	char initiator_mac[ETH_ALEN];
	int security;
	char service_info[MAX_SERVICE_INFO];
	int confirm_required;
} nan_ndp_resp;

typedef struct _nan_ulw_param {
	u32 start_time;
	u32 duration;
	u32 period;
	u8 count_down;
	u8 op_class;
	u8 channel;
	u8 avail_type;
} nan_ulw_param;

struct nan_state_info {
	int hold_role_flag;
	int cur_role;
	int hold_master_pref_flag;
	int cur_master_pref;
	int hold_rfactor_flag;
	int cur_rfactor;
	int hold_hop_cnt_flag;
	int cur_hop_cnt;
	int disable_2g_flag;
	int disable_2g;
};

struct nan_params_cfg {
	int a_band;
	int disc_bcn_period;
	int scan_time;
	int master_pref;
	int sdf_delay;
	int op_chan_a;
	int hightsf;
	int warm_up_period;
	int include_fa_attr;
	int process_fa_attr;
	int awake_dw_interval;
	int data_path_needed;
	int data_path_type;
	int ranging_required;
	int security;
	int pmf_required;
	int dual_map;
	int qos_enabled;
	int invalid_sched_required;
	int set_apple_dut_test;
	int ndpe_attr_supported;
	int ndpe_attr_protocol;
	int ndpe_attr_iface_identifier;
	int ndpe_attr_trans_port;
	int ndpe_attr_negative;
	int ndp_attr_present;
};

struct nan_params_fa {
	int interval;
	int repeat_entry;
	int op_class;
	int op_chan;
	int availability_map;
};
struct nan_params_p2p_fa {
	u8 dev_role;
	u8 mac[6];
	u8 map_id;
	u8 ctrl;
	u8 op_class;
	u8 op_chan;
	u32 availability_map;
};
#if 0
struct nan_params_p2p_fa {
    u8 dev_role;
    u8 mac[6];
    u8 map_id;
    u8 ctrl;
    u8 op_class;
    u8 op_chan;
    int availability_map;
};
#endif

struct nan_follow_up {
	int local_instance_id;
	int remote_instance_id;
	unsigned char mac[ETH_ALEN];
};

#define MAX_SRF_MAC 10

struct nan_srf {
	unsigned char srf_ctrl;
	u8 num_mac;
	u8 mac_list[MAX_SRF_MAC][6];
};

struct discover_result {
	u8 remote_instance_id;
	u8 local_instance_id;
	u8 mac[ETH_ALEN];
	u8 is_fa_p2p;
	u8 fa_dev_role;
	u8 fa_mac[ETH_ALEN];
	u8 fa_chan;
};

struct ndp_data_indication {
	u8 type;
	u8 publish_id;
	u8 ndp_id;
	char initiator_addr[ETH_ALEN];
	char responder_addr[ETH_ALEN];
	struct {
		u32 len;
		char data[0];
	} service_info;
};

struct ndp_data_confirm {
	u8 type;
	struct {
		u8 status;
		u8 reason;
	} status_reason;
	u8 publish_id;
	u8 ndp_id;
	u8 ndp_chan;
	u8 ndp_chan2;
	char initiator_addr[ETH_ALEN];
	char responder_addr[ETH_ALEN];
	char peer_ndi[ETH_ALEN];
	struct {
		u32 len;
		char data[0];
	} service_info;
};

struct ndp_confirm {
	u8 sub_type;
	char peer_mac[ETH_ALEN];
	u8 ndp_id;
};

struct ndp_terminate {
	u8 sub_type;
	char peer_mac[ETH_ALEN];
	u8 ndp_id;
};

typedef struct _nan_ndp_frame {
	char peer_mac[ETH_ALEN];
	char responder_mac[ETH_ALEN];
	char initiator_ndi[ETH_ALEN];
	char responder_ndi[ETH_ALEN];
	u8 dialogue_token;
	u8 ndp_id;
	u8 pub_id;
	u8 ndp_ctrl;
	u8 counter_proposal_needed;
	u8 wait_for_confirm;
	u32 counter_bitmap;
	u32 final_bitmap;
	u8 final_interval;
	u8 final_repeat_entry;
	u8 final_op_class;
	u8 final_chan;
	u8 confirm_required;
	u8 type_status;
	u8 rejected;
} nan_ndp_frame;

struct nan_schedule {
	u8 op_class;
	u8 op_chan;
	u8 period; // bitmap period = period*512 TUs
	u8 time_bitmap_count;
	u32 availability_map[MAX_SCHEDULE_ENTRIES];
	u16 start_offset[MAX_SCHEDULE_ENTRIES];
};

struct nan_set_pmk_attr {
	u8 pmk[NAN_PMK_LEN];
};

struct nan_user_attr {
	u8 len;
	u8 data[64];
};

typedef struct _avail_entry_t {
	u8 usage_pref;
	u8 map_id;
	u8 period; // bitmap period = period*512 TUs
	u8 op_class;
	u8 time_bitmap_count;
	u8 channels[21];
	u32 combined_time_bitmap;
	u32 time_bitmap[MAX_SCHEDULE_ENTRIES];
	u16 start_offset[MAX_SCHEDULE_ENTRIES];
} avail_entry_t;

typedef struct _ndl_qos_t {
	u8 min_slots;
	u16 max_latency;
} ndl_qos_t;

typedef struct _peer_availability_info {
	u8 peer_mac[6];
	u8 seq_id;
	u8 map_id;
	avail_entry_t entry_potential[MAX_SCHEDULE_ENTRIES];
	avail_entry_t entry_committed[MAX_SCHEDULE_ENTRIES];
	avail_entry_t entry_conditional[MAX_SCHEDULE_ENTRIES];
	ndl_qos_t qos;
	u8 potential_valid : MAX_SCHEDULE_ENTRIES;
	u8 conditional_valid : MAX_SCHEDULE_ENTRIES;
	u8 committed_valid : MAX_SCHEDULE_ENTRIES;
	u8 band_entry_potential : 1;
	u8 time_bitmap_present_potential : 1;
	u8 peer_ranging_required : 1;
	u8 committed_changed : 1;
	u8 potential_changed : 1;
	u8 ndc_changed : 1;
	u8 single_band : 1;
	u8 ndpe_attr_supported;
} peer_availability_info;

#define MAX_SUPPORTED_NDC 1
#define MAX_NDL_PEERS 1
#define MAX_DATA_STREAMS 1

typedef struct {
	u8 ndp_id;
	u8 initiator_ndi[ETH_ALEN]; /*This is NDI of this NDP's Initiator*/
	u8 responder_ndi[ETH_ALEN]; /*This is NDI of this NDP's Responder*/
	u8 peer_ndi[ETH_ALEN]; /*This is NDI of this NDP's peer*/
	u32 ndp_slots;
	u8 dialogue_token;
} NDP_INFO;

typedef struct {
	u8 peer_mac[ETH_ALEN]; /*This is NMI of peer and a primary key for NDL*/
	u8 peer_id;
	u16 max_idle_period;
	u8 min_slots;
	u32 max_latency;
	NDP_INFO ndp_info[MAX_DATA_STREAMS];
	u32 immutable_sched_bitmap;
	u8 immutable_sched_present : 1;
	u8 ndl_setup : 1;
} NDL_INFO;

typedef struct {
	u8 ndc_id[ETH_ALEN];
	u32 slot;
	NDL_INFO ndl_info[MAX_NDL_PEERS];
	u8 ndc_setup : 1;
} NDC_INFO;

typedef struct {
	u32 max_burst_dur : 4;
	u32 min_delta_ftm : 6;
	u32 max_ftms_per_burst : 5;
	u32 ftm_fmt_bw : 6;
	u32 reserved : 3;
} __attribute__((packed)) NAN_FTM_PARAMS;

typedef struct {
	u8 type;
	char peer_mac[ETH_ALEN];
	u8 iface_identifier[IPV6_IFACE_IDENTIFIER_LEN];
	u16 transport_port;
	u8 transport_protocol;
} ndp_ndpe_data;

enum nan_error nan_init(struct module *mod, struct nan_cfg *cfg);
enum nan_error nan_set_state_info(struct module *mod,
				  struct nan_state_info *cfg);

enum nan_error nan_get_state_info(struct module *cur_if,
				  struct nan_state_info *state);
enum nan_error nan_deinit(struct module *mod);
enum nan_error nan_start(struct module *mod);
enum nan_error nan_stop(struct module *mod);
enum nan_error nan_publish(struct module *mod,
			   struct nan_publish_service_conf *service);
enum nan_error nan_subscribe(struct module *mod,
			     struct nan_subscribe_service *service);
enum nan_error nan_follow_up(struct module *mod,
			     struct nan_follow_up *follow_up);
enum nan_error nan_set_config(struct module *mod, struct nan_params_cfg *cfg);

enum nan_error nan_send_ftm_report(struct module *mod, u32 distance,
				   char *mac_addr);
void nan_mwu_event_ftm_cb(struct event *event, void *priv);
enum nan_error nan_ndp_terminate(struct module *mod);
enum nan_error nan_ranging_initiate(struct module *mod,
				    char peer_mac[ETH_ALEN]);
enum nan_error nan_ranging_terminate(struct module *mod);
enum nan_error nan_set_schedule_update(struct module *mod,
				       struct nan_schedule *sched);
enum nan_error nan_sched_request(struct module *mod,
				 char responder_mac[ETH_ALEN]);
enum nan_error nan_sched_response(struct module *mod,
				  char responder_mac[ETH_ALEN]);
enum nan_error nan_ndp_set_immutable_sched(struct module *mod, u32 bitmap);
enum nan_error nan_ndp_set_counter_proposal(struct module *mod,
					    struct nan_schedule *ndl_sched);
enum nan_error nan_set_ndl_schedule(struct module *mod,
				    peer_availability_info *avail_info,
				    u8 clean);
enum nan_error nan_set_availability(struct module *mod,
				    peer_availability_info *avail_info);
enum nan_error nan_ndp_request(struct module *mod, nan_ndp_req *ndp);
enum nan_error nan_ndp_response(struct module *mod, nan_ndp_resp *ndp);
enum nan_error nan_set_avail_attr(struct module *mod, u8 *avail, u8 len);
enum nan_error nan_qos_update(struct module *mod, ndl_qos_t *qos);
enum nan_error nan_ulw_update(struct module *mod, nan_ulw_param *ulw);
enum nan_error nan_send_sched_update(struct module *mod,
				     char responder_mac[ETH_ALEN]);
enum nan_error nan_set_p2p_fa(struct module *mod,
			      struct nan_params_p2p_fa *p2p_fa);
enum nan_error nan_ndp_sec_test(struct module *mod, u8 wrong_mic, u8 m4_reject);
enum nan_error configure_nan_srf(struct module *mod, struct nan_srf *srf);
enum nan_error nan_ftm_init(struct module *mod);
int nan_is_nan_ranging_responder(void);
void nan_iface_open(void);
void nan_iface_close(void);
enum nan_error nan_set_user_pmk(struct module *mod,
				struct nan_set_pmk_attr *nan_pmk_set);

#endif
