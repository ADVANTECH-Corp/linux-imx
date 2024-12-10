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
