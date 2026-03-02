/*
 *  Copyright 2024-2025 NXP
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

#ifndef __MLOCATION_API_H__
#define __MLOCATION_API_H__
#include "mlocation.h"
#include "mwu_if_manager.h"
#include "mwu_internal.h"
/* INTERNAL API */

/* mlocation_init: initialize the test module */
enum mlocation_error mlocation_init(struct module *mod,
				    struct mlocation_cfg *cfg);

/* mlocation_deinit: tear down the test module */
void mlocation_deinit(struct module *mod, struct mwu_module *mwu_mod);

enum mlocation_error do_mlocation_session_ctrl(struct module *mod,
					       struct mlocation_session *ctrl,
					       int nan_ranging);

void mlocation_driver_event(char *ifname, u8 *buffer, u16 size);
enum mlocation_error mlocation_request_frame(struct mwu_iface_info *cur_info,
					     unsigned char *mac);
enum mlocation_error
mlocation_neighbor_report_request_frame(struct mwu_iface_info *cur_info,
					unsigned char *mac);
enum mlocation_error mlocation_send_neighbor_req(struct mwu_iface_info *cur_if,
						 mlocation_neighbor_req *req);
enum mlocation_error
cmd_mlocation_session_ctrl(struct mwu_iface_info *cur_if,
			   mlocation_session_ctrl *mlocation_ctrl);
enum mlocation_error mlocation_send_anqp_req(struct mwu_iface_info *cur_if,
					     mlocation_anqp_cfg *req);
void mlocation_process_scan(struct mwu_iface_info *cur_if);

#endif
