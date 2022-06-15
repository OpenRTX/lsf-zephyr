#include "lisa_uart_send.h"
#include <lisa_queue.h>
#include <lisa_mem.h>
#include <lisa_log.h>
#include "lisa_uart_protocol.h"

#define TAG "lisa_uart_send"
#define UART_QUEUE_CNT (5)

static algo_t s_algo __attribute__((aligned(32))) = {0};
static lisa_queue_t* g_send_queue = NULL;

typedef enum {
	e_algo_none = 0,
	e_algo_init = 1,
	e_algo_start = 2,
	e_algo_stop = 3,
	e_algo_deinit = 4,
	e_algo_i2s_ch_sel = 5,
	e_algo_i2s_test = 6,
	e_algo_heartbeat = 7,
	e_algo_version_query = 8,
	e_algo_all_version_query = 9,
	e_algo_cst_kw = 10,
	// scan pen
	e_algo_scan_work_mode = 11,
	e_algo_scan_set_para = 12,
	e_algo_scan_tts_text = 13,
	e_algo_scan_tts_auth = 14,
	e_algo_scan_adjustment = 15,
	e_algo_last,
} algo_config_type_e;

typedef struct {
	algo_config_type_e msg;
} algo_msg_t;

static down_cmd_pack_u u_send_cmd_pack __attribute__((aligned(32))) = {{0}};
static cmd_cst_kw_t _cst_kw;

static void send_proc_esr(algo_config_type_e e_algo_config)
{
	switch (e_algo_config) {
		case e_algo_init:
			break;
		case e_algo_start:
			u_send_cmd_pack.cmd_esr.cmd = e_cmd_esr;
			u_send_cmd_pack.cmd_esr.voice_id[0] = 0;
			u_send_cmd_pack.cmd_esr.voice_id[1] = 0;

			u_send_cmd_pack.cmd_esr.mode = s_algo.mode;
			u_send_cmd_pack.cmd_esr.beam = 1;  // if esr then beam always 1
			u_send_cmd_pack.cmd_esr.loop = 1;  // always loop
			u_send_cmd_pack.cmd_esr.timeout = 10;  // always wait	//TODO

			lisa_uart_pack_send((void *)&u_send_cmd_pack, 5);

			break;
		case e_algo_deinit:
			break;
		case e_algo_heartbeat:
			//printf("[ALGO:%s:%d] e_algo_heartbeat.\n", __FUNCTION__, __LINE__);
			u_send_cmd_pack.cmd_heartbeat.cmd = e_cmd_heartbeat;
			lisa_uart_pack_send((void *)&u_send_cmd_pack, 1);
			break;
		case e_algo_all_version_query:
			u_send_cmd_pack.all_version.cmd = e_cmd_all_ver_query;
			u_send_cmd_pack.all_version.type = s_algo.query_type;
			lisa_uart_pack_send((void *)&u_send_cmd_pack, 2);
			break;
		case e_algo_scan_work_mode:
			u_send_cmd_pack.workmode.cmd = e_cmd_scan_work_mode;
			u_send_cmd_pack.workmode.mode = s_algo.scan_mode;
			lisa_uart_pack_send((void *)&u_send_cmd_pack, 2);
			break;
		case e_algo_scan_set_para:
			u_send_cmd_pack.scan_para.cmd = e_cmd_scan_set_para;
			u_send_cmd_pack.scan_para.type = s_algo.scan_param_type;
			u_send_cmd_pack.scan_para.data = s_algo.scan_param;
			lisa_uart_pack_send((void *)&u_send_cmd_pack, 4);
			break;
		case e_algo_scan_tts_text: {
			uint32_t txt_len = strlen(s_algo.tts_text);
			u_send_cmd_pack.scan_txt.cmd = e_cmd_scan_tts_txt;
			memcpy(&u_send_cmd_pack.scan_txt.len[0], &txt_len, 4);
			memcpy(&u_send_cmd_pack.scan_txt.data[0], s_algo.tts_text, txt_len);
			lisa_uart_pack_send((void *)&u_send_cmd_pack, 1+4+txt_len);
			lisa_mem_free(s_algo.tts_text);
			break;
		}
		case e_algo_scan_tts_auth: {
			uint32_t auth_len = s_algo.tts_authlen;
			u_send_cmd_pack.auth_code.cmd = e_cmd_scan_tts_auth;
			memcpy(&u_send_cmd_pack.auth_code.len[0], &auth_len, 4);
			memcpy(&u_send_cmd_pack.auth_code.data[0], s_algo.tts_auth_code, auth_len);
			lisa_uart_pack_send((void *)&u_send_cmd_pack, 1+4+auth_len);
			lisa_mem_free(s_algo.tts_auth_code);
			break;
		}
		case e_algo_scan_adjustment: {
			u_send_cmd_pack.adjustment.cmd = e_cmd_scan_adjustment;
			u_send_cmd_pack.adjustment.data[0] = (s_algo.adjust_x_offset >> 8);
			u_send_cmd_pack.adjustment.data[1] = (s_algo.adjust_x_offset & 0x00FF);
			u_send_cmd_pack.adjustment.data[2] = (s_algo.adjust_y_offset >> 8);
			u_send_cmd_pack.adjustment.data[3] = (s_algo.adjust_y_offset & 0x00FF);
			lisa_uart_pack_send((void *)&u_send_cmd_pack, 5);
			break;
		}
		default:
			break;
	}
}

lisa_err_t listen_send_uart_frame()
{
	algo_msg_t msg = {0};

	lisa_err_t ret = lisa_queue_receive(g_send_queue, &msg, sizeof(algo_msg_t), 0);
	if (LISA_OK == ret) {
		if (msg.msg < e_algo_last) {
			send_proc_esr(msg.msg);
		} else {
			LISA_LOGE(TAG, "unknow uart msg");
		}
	}
	return ret;
}

lisa_err_t lisa_uart_send_init(int wait_ms)
{
	if (g_send_queue == NULL) {
		g_send_queue = lisa_queue_create(UART_QUEUE_CNT, "lisa_uart_send_queue", sizeof(algo_msg_t));
	}
	
	algo_msg_t msg = {0};
	msg.msg = e_algo_init;
	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), wait_ms);
	return ret;
}

lisa_err_t lisa_uart_send_deinit(int wait_ms)
{
	algo_msg_t msg = {0};
	msg.msg = e_algo_deinit;
	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), wait_ms);
	return ret;
}

lisa_err_t lisa_uart_send_start(listen_algo_mode_e mode, int wait_ms)
{
	if (mode >= e_listen_algo_mode_cnt) {
		LISA_LOGE(TAG, "algo deinit mode error");
		return LISA_FAIL;
	}
	s_algo.mode = mode;
	algo_msg_t msg = {0};
	msg.msg = e_algo_start;
	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), wait_ms);
	return ret;
}

lisa_err_t lisa_uart_send_heartbeat(void)
{
	algo_msg_t msg = {0};
	msg.msg = e_algo_heartbeat;
	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), 0);
	return ret;
}

// lisa_err_t lisa_csk_all_version(void)
// {
// 	algo_msg_t msg = {0};
// 	msg.msg = e_algo_all_version_query;
// 	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), 0);
// 	return ret;
// }

lisa_err_t lisa_uart_set_scan_mode(scan_mode_e mode, int wait_ms)
{
	algo_msg_t msg = {0};
	msg.msg = e_algo_scan_work_mode;
	s_algo.scan_mode = mode;
	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), wait_ms);
	return ret;
}

lisa_err_t lisa_uart_scan_tts_text(const uint8_t* const text, uint32_t len)
{
	algo_msg_t msg = {0};
	s_algo.tts_text = lisa_mem_alloc(len + 1);
	strcpy(s_algo.tts_text, text);
	msg.msg = e_algo_scan_tts_text;
	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), 0);
	return ret;
}

lisa_err_t lisa_uart_get_csk_version(all_version_query_e type)
{
	algo_msg_t msg = {0};
	msg.msg = e_algo_all_version_query;
	s_algo.query_type = type;
	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), 0);
	return ret;
}

lisa_err_t lisa_uart_set_scan_param(listen_scan_param_type_e type, uint16_t data)
{
	algo_msg_t msg = {0};
	msg.msg = e_algo_scan_set_para;
	s_algo.scan_param_type = type;
	s_algo.scan_param = data;
	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), 0);
	return ret;
}

lisa_err_t lisa_uart_scan_tts_auth(uint8_t *code, uint32_t len)
{
	algo_msg_t msg = {0};
	s_algo.tts_auth_code = lisa_mem_alloc(len);
	s_algo.tts_authlen = len;
	memcpy(s_algo.tts_auth_code, code, len);
	msg.msg = e_algo_scan_tts_auth;
	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), 0);
	return ret;
}

lisa_err_t lisa_uart_adjustment(int16_t x_offset, int16_t y_offset) {
	algo_msg_t msg = {0};
	s_algo.adjust_x_offset = x_offset;
	s_algo.adjust_y_offset = y_offset;
	msg.msg = e_algo_scan_adjustment;
	lisa_err_t ret = lisa_queue_push(g_send_queue, &msg, sizeof(algo_msg_t), 0);
	return ret;
}
