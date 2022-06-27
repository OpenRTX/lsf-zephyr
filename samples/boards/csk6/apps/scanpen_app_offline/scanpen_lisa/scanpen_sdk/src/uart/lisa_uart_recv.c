#include "lisa_uart_recv.h"
#include "lisa_uart_send.h"
#include <lisa_log.h>

#undef	TAG
#define TAG	"[RECV]"
#define DEFAULT_VOLUME		(0xFE)
#define HISTORY_VOLUME		(0xFF)
#define DEFAULT_SLEEP_TIME	(0x06) // default sleep time 60s

// receive frame
static frame_desc_u cmd_pack = { { 0 } };
static frame_desc_u *head = &cmd_pack;
static recv_cmd_pack_e *p_recv_cmd = NULL; // receive command package
static frame_info_e e_frame_info = e_frame_ready;
// static uint8_t history_baudrate = 1; // default: 115200
static lisa_uart_recv_cb s_uart_rx_callback = NULL;

static frame_info_e recv_proc_uart_set(frame_desc_u *head)
{
	// p_recv_cmd = (recv_cmd_pack_e *)&(head->frame_u.data_t.data[0]);

	// // 1. parameter check
	// if (e_baud_cnt <= p_recv_cmd->uart_set.baudrate) {
	// 	LISA_LOGW(TAG, "set error");
	// 	return (e_frame_info = e_frame_error_argv);
	// }
	// if (history_baudrate == p_recv_cmd->uart_set.baudrate)
	// 	return (e_frame_info = e_frame_correct);

	// // 2. first set then send
	// history_baudrate = p_recv_cmd->uart_set.baudrate;
	prot_reset_fifo();

	e_frame_info = e_frame_correct;
	return e_frame_info;
}

static frame_info_e recv_proc_scan_set_para(frame_desc_u *head)
{
#if 0
	p_recv_cmd = (recv_cmd_pack_e *)&(head->frame_u.data_t.data[0]);
	e_frame_info = e_frame_error_argv;

	switch (p_recv_cmd->scan_para.type) {
		case e_scan_para_multiline_time:
			multiline_scan_watiing_time_set(
					p_recv_cmd->scan_para.data * 100);  // scan_para.data -- 0.1S
			e_frame_info = e_frame_correct;
			break;
		case e_tts_base_voice_speed:
		case e_tts_synth_voice_speed:
			#if EN_ALGO_TTS
			if(0 == set_tts_voice_speech(p_recv_cmd->scan_para.type, (uint8_t)p_recv_cmd->scan_para.data, p_recv_cmd->scan_para.data >> 8)) {
				e_frame_info = e_frame_correct;
			} else {
				e_frame_info = e_frame_error_state;
			}
			#endif
			break;
		case e_left_hand_hold:
			set_pen_hold_hand(p_recv_cmd->scan_para.data);
			e_frame_info = e_frame_correct;
			break;
		case e_image_transfer_proto_type:
			sysconf.image_transfer_proto = p_recv_cmd->scan_para.data;
			e_frame_info = e_frame_correct;
			break;
		default:
			return e_frame_info;
			break;
	}
#endif
	return e_frame_info;
}

static frame_info_e recv_proc_scan_text(frame_desc_u *head)
{
	p_recv_cmd = (recv_cmd_pack_e *)&(head->frame_u.data_t.data[0]);
	e_frame_info = e_frame_error_argv;

	// if(e_scan_mode_offline_ocr != scan_update_mode_get()) {
	// 	LISA_LOGW(TAG, "not offline ocr mode, current mode[%d], update mode[%d]", scan_cur_mode_get(), scan_update_mode_get());
	// 	return e_frame_info;
	// }

	uint32_t txt_len = 0;
	memcpy(&txt_len, &p_recv_cmd->tts_txt.len[0], 4);
	LISA_LOGI(TAG, "text cmd:%d, txt:%s, len:%d", p_recv_cmd->tts_txt.cmd, &p_recv_cmd->tts_txt.data[0], txt_len);
	if(NULL != s_uart_rx_callback) {
		s_uart_rx_callback(p_recv_cmd->tts_txt.cmd, &p_recv_cmd->tts_txt.data[0], txt_len);
	}
	return e_frame_info;
}

static frame_info_e recv_proc_scan_tts_authcode(frame_desc_u *head)
{
	// int auth_len = 0;
	// int encryptcode_len = 0;
	// char *encryptcode = NULL;

	p_recv_cmd = (recv_cmd_pack_e *)&(head->frame_u.data_t.data[0]);
	e_frame_info = e_frame_error_argv;

	// memcpy(&auth_len, &p_recv_cmd->tts_auth_code.len[0], sizeof(int));
	// LOGV("auth_len=%d", auth_len);
	// int ret = process_authcode(&p_recv_cmd->tts_auth_code.data[0], auth_len, &encryptcode, &encryptcode_len);
	// if(0 == ret) {
	// 	send_proc_tts_encryptcode(encryptcode, encryptcode_len);
	// 	LOGV("encryptcode_len=%d", encryptcode_len);
	// 	e_frame_info = e_frame_correct;
	// 	// shell_dbg_dump(encryptcode, encryptcode_len);
	// } else {
	// 	e_frame_info = e_frame_error_state;
	// 	send_proc_frame_info(e_frame_info, head->frame_u.header_t.frame_id);
	// }

	return e_frame_info;
}

static frame_info_e recv_proc_manual_adjust_info(frame_desc_u *head)
{
	p_recv_cmd = (recv_cmd_pack_e *)&(head->frame_u.data_t.data[0]);
	e_frame_info = e_frame_error_argv;

	// if(e_scan_mode_adjust != scan_update_mode_get()) {
	// 	LISA_LOGW(TAG, "not adjust mode, don't support manual set adjust info. current[%d], update[%d]", scan_cur_mode_get(), scan_update_mode_get());
	// 	return e_frame_error_state;
	// }
	// int16_t x_shift = 0;
	// int16_t y_shift = 0;

	// memcpy(&x_shift, &p_recv_cmd->manual_adjust_info.x_shift[0], sizeof(int16_t));
	// memcpy(&y_shift, &p_recv_cmd->manual_adjust_info.y_shift[0], sizeof(int16_t));

	// LISA_LOGD(TAG, "manual adjust info x_shift=%d, y_shift=%d", x_shift, y_shift);
	// int ret = recv_uart_manual_adjust_info(x_shift, y_shift, false);
	// if(0 == ret) {
	// 	e_frame_info = e_frame_correct;
	// } else {
	// 	e_frame_info = e_frame_error_state;
	// }

	return e_frame_info;
}

static frame_info_e recv_proc_manual_adjust_abs_info(frame_desc_u *head)
{
	p_recv_cmd = (recv_cmd_pack_e *)&(head->frame_u.data_t.data[0]);
	e_frame_info = e_frame_error_argv;

	// if(e_scan_mode_adjust != scan_update_mode_get()) {
	// 	LISA_LOGW(TAG, "not adjust mode, don't support manual set adjust info. current[%d], update[%d]", scan_cur_mode_get(), scan_update_mode_get());
	// 	return e_frame_error_state;
	// }

	// LISA_LOGD(TAG, "manual adjust info x_start=%d, x_stop=%d, y_start=%d, y_stop=%d",
	// 	p_recv_cmd->manual_adjust_abs_info.x_start, p_recv_cmd->manual_adjust_abs_info.x_stop,
	// 	p_recv_cmd->manual_adjust_abs_info.y_start, p_recv_cmd->manual_adjust_abs_info.y_stop);
	// check coordinate valid
	// if(p_recv_cmd->manual_adjust_abs_info.x_stop - p_recv_cmd->manual_adjust_abs_info.x_start != 128) {
	// 	LISA_LOGE(TAG, "x error");
	// 	return e_frame_error_argv;
	// }
	// if(p_recv_cmd->manual_adjust_abs_info.y_stop - p_recv_cmd->manual_adjust_abs_info.y_start != 180) {
	// 	LISA_LOGE(TAG, "y error");
	// 	return e_frame_error_argv;
	// }

	// int ret = recv_uart_manual_adjust_info(p_recv_cmd->manual_adjust_abs_info.x_start, p_recv_cmd->manual_adjust_abs_info.y_start, true);
	// if(0 == ret) {
	// 	e_frame_info = e_frame_correct;
	// } else {
	// 	e_frame_info = e_frame_error_state;
	// }

	return e_frame_info;
}

static frame_info_e recv_proc_scan_work_mode(frame_desc_u *head)
{
	p_recv_cmd = (recv_cmd_pack_e *)&(head->frame_u.data_t.data[0]);
	e_frame_info = e_frame_error_argv;
#if EN_SCAN_TASK
	if(scan_mode_set(p_recv_cmd->workmode.mode)) {
		e_frame_info = e_frame_correct;
	} else {
		e_frame_info = e_frame_error_state;
	}
#endif
	return e_frame_info;
}

static void recv_proc_all_version_query(frame_desc_u *head)
{
	p_recv_cmd = (recv_cmd_pack_e *)&(head->frame_u.data_t.data[0]);
	send_proc_version(p_recv_cmd->all_ver_query.data);

	return;
}

bool frame_recv_proc(void *arg)
{
	if(NULL != arg)
		s_uart_rx_callback = (lisa_uart_recv_cb)arg;

	int to_recv = 0;
	int recved = 0;
	bool flag = 0;//(shell_dbg_level(FETCH_INIT) >= LL_DBG);
	uint8_t chk = 0;

	// 1. receive HDR_L
	to_recv = 1;
	prot_read_data(&head->frame_data[0], to_recv, portMAX_DELAY);
	if (FRM_HDR_TAG_L != head->frame_data[0])
		return false;

	// 2. receive HDR_H
	if (to_recv == prot_read_data(&head->frame_data[1], to_recv, 10))
		if (FRM_HDR_TAG_H != head->frame_data[1])
			return false;

	// 3. receive and check first 6 byte
	to_recv = 4;
	if (to_recv == prot_read_data(&head->frame_data[2], to_recv, 10))
		if ((chk = prot_pack_check(&head->frame_data[0], FRM_HEAD_LEN))) {
			if (flag)
				LISA_LOGW(TAG, "[HCHK(%02X):%02X %02X %02X %02X %02X %02X]", chk
					, head->frame_data[0], head->frame_data[1], head->frame_data[2]
					, head->frame_data[3], head->frame_data[4], head->frame_data[5]);
			return false;
		}

	// 4. receive and check last byte
	const uint8_t fid = head->frame_u.header_t.frame_id;
	to_recv = head->frame_u.header_t.len - FRM_HEAD_LEN;
	// printk("frame len:%d\n", to_recv);
	recved = prot_read_data(&head->frame_data[FRM_HEAD_LEN], to_recv, 10);
	if (recved != to_recv) {
		LISA_LOGW(TAG, "[DLEN(%d!=%d)]", to_recv, recved);
		send_proc_frame_info(e_frame_error_frame, fid);
		return false;
	}
	if ((chk = prot_pack_check(&head->frame_u.data_t.type, to_recv))) {
		LISA_LOGW(TAG, "[DCHK(%02X)]", chk);
		// if (flag)
		// 	shell_dbg_dump(head->frame_data, head->frame_u.header_t.len);
		send_proc_frame_info(e_frame_error_frame, fid);
		return false;
	}

	// 5. print received if need
	// if (shell_dbg_level(FETCH_INIT) >= LL_VERB) {
	// 	LOGV("recv(%d)...", head->frame_u.header_t.len);
	// 	shell_dbg_dump(head->frame_data, head->frame_u.header_t.len);
	// }
	#if 0
	LISA_LOGI(TAG, "recv(%d):%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", head->frame_u.header_t.len,
			head->frame_data[0],
			head->frame_data[1],
			head->frame_data[2],
			head->frame_data[3],
			head->frame_data[4],
			head->frame_data[5],
			head->frame_data[6],
			head->frame_data[7],
			head->frame_data[8],
			head->frame_data[9],
			head->frame_data[10],
			head->frame_data[11],
			head->frame_data[12]);
	#endif

	// 6. parse command and pre_deal
	switch (head->frame_u.data_t.data[0]) {
	case e_cmd_scan_set_para:
	case e_cmd_scan_work_mode:
	case e_cmd_heart_beat:
	case e_cmd_system_set:
	case e_cmd_uart_set:
	case e_cmd_version_query:
	case e_cmd_status_query:
	case e_cmd_adc_gain_ctrl:
	case e_cmd_adc_ch_ctrl:
    case e_cmd_esr_hold_frms:
	case e_cmd_proto_version:
	case e_cmd_switch_mic_ch:
	case e_cmd_scan_tts_text:
	case e_cmd_scan_tts_auth_code:
	case e_cmd_manual_adjust_info:
	case e_cmd_manual_adjust_abs_info:
		break;
	case e_cmd_sleep:
	case e_cmd_stop:
		//	algo_stop();
		// play_stop();
		break;
	case e_cmd_start:
		//	algo_stop();
		break;
	case e_cmd_play:
		// play_stop();
		break;
	case e_cmd_i2s_ch_sel:
		break;
	case e_cmd_all_ver_query:
		break;
	default:
		send_proc_frame_info(e_frame_error_argv, fid);
		LISA_LOGW(TAG, "[CMD:%02X]", head->frame_u.data_t.data[0]);
		return false;
	}

	// 7. receive command deal
	switch (head->frame_u.data_t.data[0]) {
	case e_cmd_heart_beat:
	case e_cmd_scan_work_mode:
	case e_cmd_all_ver_query:
	case e_cmd_scan_tts_auth_code:
		e_frame_info = e_frame_correct;
		break;
	case e_cmd_uart_set:
		e_frame_info = recv_proc_uart_set(head);
		break;
	case e_cmd_scan_set_para:
		e_frame_info = recv_proc_scan_set_para(head);
		break;
	case e_cmd_scan_tts_text:
	case e_cmd_scan_translate_text:
	case e_cmd_scan_word_search_text:
		e_frame_info = recv_proc_scan_text(head);
		break;
	case e_cmd_manual_adjust_info:
		e_frame_info = recv_proc_manual_adjust_info(head);
		break;
	case e_cmd_manual_adjust_abs_info:
		e_frame_info = recv_proc_manual_adjust_abs_info(head);
		break;
	default:
		send_proc_frame_info(e_frame_error_argv, fid);
		LISA_LOGW(TAG, "[CMD:%02X]", head->frame_u.data_t.data[0]);
		return false;
	}

	// 8. send communicate status
	send_proc_frame_info(e_frame_info, fid);

	// 9. deal with e_cmd_version_query and e_cmd_status_query
	switch (head->frame_u.data_t.data[0]) {
	case e_cmd_uart_set:
		// uart_app_conf((baudrate_e)history_baudrate);
		break;
	case e_cmd_version_query:
		// send_proc_soft_version();
		break;
	case e_cmd_all_ver_query:
		// CLOGE("RECV CHECK ALL VER QUERY");
		// vTaskDelay(2);
		recv_proc_all_version_query(head);
		break;
	case e_cmd_dict_size_query:
		break;
	case e_cmd_scan_work_mode:
		// vTaskDelay(1);
		recv_proc_scan_work_mode(head);
		break;
	case e_cmd_scan_tts_auth_code:
		recv_proc_scan_tts_authcode(head);
		break;
	default:
		break;
	}
	return e_frame_correct == e_frame_info;
}
