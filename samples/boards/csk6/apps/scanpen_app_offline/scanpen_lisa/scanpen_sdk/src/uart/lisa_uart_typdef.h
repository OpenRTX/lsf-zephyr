#ifndef __LISA_EVS_SDK_UART_TYPDEF_H__
#define __LISA_EVS_SDK_UART_TYPDEF_H__\

#include "lisa_evs_uart_parser.h"

#define FRM_HDR_TAG (0x4658)  // XF
#define FRM_HDR_TAG_L (0x58)  // X
#define FRM_HDR_TAG_H (0x46)  // F
#define FRM_MAX_LEN 512
#define FRM_HDR_LEN (6)

#define FRM_DAT_LEN        (3)
#define TEXT_INFO          (4)
#define TEXT_HDR           (FRM_HDR_LEN + FRM_DAT_LEN + TEXT_INFO)
#define TEXT_MAX_LEN       (FRM_MAX_LEN - TEXT_HDR)
#define ANGLE_INFO         (2)
#define ANGLE_HDR          (FRM_HDR_LEN + FRM_DAT_LEN + ANGLE_INFO)
#define ANGLE_MAX_LEN      (FRM_MAX_LEN - ANGLE_HDR)

#define CST_KEYS_STATE_MAX_SIZE 256
#define CST_KEYS_TXT_MAX_SIZE 36
#define CST_KEYS_TTS_MAX_SIZE 2048

typedef union {
	uint8_t frame_data[FRM_MAX_LEN];
	struct {
		struct {
			uint16_t tag;
			uint16_t len;
			uint8_t frame_id;
			uint8_t head_check;
		} header_t;
		struct {
			uint8_t type;
			uint8_t address;
			uint8_t data[1];  // last byte for checksum
		} data_t;
	} frame_u;
} frame_desc_u;

typedef struct {
	frame_desc_u dat;
} uart_send_msg_t;

typedef struct {
	frame_desc_u *buff;
	uint32_t pos;
	uint32_t cnt;
} uart_fifo_t;

typedef enum {
	e_cmd_version_query = 0x03,
	e_cmd_stop = 0x10,
	e_cmd_esr = 0x11,
	e_cmd_i2s_ch_sel = 0x15,
	e_cmd_i2s_test_mode = 0x16,
	e_cmd_all_ver_query = 0x19,
	e_cmd_custom_keyword = 0x20,
	// scan pen
	e_cmd_scan_work_mode = 0x50,
	e_cmd_scan_set_para = 0x51,
	e_cmd_heartbeat = 0xff
} down_cmd_e;

typedef enum {
	e_scan_para_multiline_time = 0x00,
} scan_setpara_type_e;

typedef struct {
	uint8_t cmd;
} cmd_stop_t;

typedef struct {
	uint8_t cmd;
	uint8_t	voice_id[2];		// voice id
	struct {
		uint8_t mode : 4;  // 3:0 esr_mode
		uint8_t beam : 3;  // 6:4 asr_beam
		uint8_t loop : 1;  // 7 loop
	};
	uint8_t timeout;  // recognize timeout
} cmd_esr_t;

typedef struct {
	uint8_t cmd;
	uint8_t data[4];
} cmd_i2s_ch_sel_t;

typedef struct {
	uint8_t cmd;
} cmd_heartbeat_t;

typedef struct {
	uint8_t cmd;
	uint8_t opt;
} cmd_i2s_test_mode_t;

typedef struct {
	uint8_t cmd;
	uint8_t version[4];
} cmd_pack_soft_version_t;

typedef struct {
	uint8_t cmd;
	uint8_t type;  //       master  |  cae	| ear 	| ...
	uint8_t ver[3];  //  build   |  mlp 	| mlp	| ...
} cmd_pack_all_version_t;

typedef struct {
	uint8_t cmd;
	uint8_t type;
	uint8_t operation;
	uint8_t keys_txt[CST_KEYS_TXT_MAX_SIZE];
	uint8_t keys_state[CST_KEYS_STATE_MAX_SIZE];
	uint8_t delete_keys[128];
} cmd_cst_kw_t;

typedef struct {
	uint8_t cmd;
	uint8_t mode;
} cmd_scan_workmode_t;

typedef struct {
	uint8_t cmd;
	uint8_t type;
	uint16_t data;
} cmd_scan_setpara_t;

typedef struct {
	uint8_t cmd;
	uint8_t len[4];
	uint8_t data[256];
} cmd_scan_tts_text_t;

typedef struct {
	uint8_t cmd;
	uint8_t len[4];
	uint8_t data[256];
} cmd_scan_tts_auth_code_t;

typedef struct {
	uint8_t cmd;
	uint8_t data[4];
} cmd_scan_adjustment;


typedef union {
	cmd_stop_t cmd_stop;
	cmd_esr_t cmd_esr;
	cmd_i2s_ch_sel_t cmd_i2s_ch_sel;
	cmd_heartbeat_t cmd_heartbeat;
	cmd_i2s_test_mode_t cmd_i2s_test_mode;
	cmd_pack_soft_version_t soft_version;
	cmd_pack_all_version_t all_version;
	cmd_cst_kw_t cst_kw;
	cmd_scan_workmode_t workmode;
	cmd_scan_setpara_t scan_para;
	cmd_scan_tts_text_t scan_txt;
	cmd_scan_tts_auth_code_t auth_code;
	cmd_scan_adjustment adjustment;
} down_cmd_pack_u;

/////// UP CMD  //////////////////////////////
typedef enum {
	e_up_cmd_com_stat = 0x01,
	e_up_cmd_software_version = 0x02,
	e_up_cmd_all_version = 0x04,
	e_up_cmd_esr_result = 0x10,
	e_up_cmd_esr_timeout = 0x11,
	e_up_cmd_esr_angle = 0x12,
	e_up_cmd_bt_play = 0x21,
	e_up_cmd_bt_stop = 0x22,
#ifdef __CONFIG_FACTORY_TEST
	e_up_cmd_bt_record = 0x23,
#endif
	e_up_cmd_cst_kw = 0x30,
	e_up_cmd_scan_work_status = 0x50,
	e_up_cmd_heartbeat = 0xff,
	e_cmd_scan_tts_encode = 0x52,
	e_cmd_scan_tts_txt = 0x53,
	e_cmd_scan_tts_auth = 0x54,
	e_cmd_scan_adjustment = 0x55,
} up_cmd_e;

typedef enum {
	e_frame_ready = 0x00,
	e_frame_correct = 0x10,
	e_frame_error_frame = 0x11,
	e_frame_error_argv = 0x12,
	e_frame_tts_unsupport = 0x13,
	e_frame_cnt = 0x14
} frame_info_e;

typedef struct {
	uint8_t cmd;
	uint8_t status;
} up_cmd_com_stat_t;

typedef union {
#ifdef USE_EVB_CSK4002
	uint8_t		data[200 + 4];
#else
	uint8_t data[300];
#endif
	struct {
		uint8_t cmd;
#ifdef USE_EVB_CSK4002
		uint8_t		key_id[2];
		uint8_t		id_score[2];
#else
		uint8_t key_id;
		uint16_t id_score;
#endif
		uint8_t id_text[1];
	} esr_result_t;  // size = 6byte, not 5 byte
} up_cmd_esr_result_t;

typedef struct {
	uint8_t cmd;
	uint8_t type;
} up_cmd_esr_err_result_t;

typedef union {
       uint8_t data[300];
       struct {
			uint8_t cmd;
			uint8_t key_id;
			uint8_t angle[1];
       } esr_angle_t;
}up_cmd_esr_angle_u;

typedef struct {
	uint8_t cmd;
	uint8_t status;
} up_cmd_cst_kw_t;

typedef struct {
	uint8_t cmd;
	uint8_t work_status;
} up_cmd_scan_work_status_t;

typedef struct {
	uint8_t cmd;
	uint8_t len[4];
	uint8_t encode[256];
} up_cmd_tts_encode_t;

typedef union {
	up_cmd_com_stat_t com_stat;
	up_cmd_esr_angle_u esr_angle;
	up_cmd_esr_result_t esr_result;
	up_cmd_esr_err_result_t esr_err_result;
	cmd_pack_soft_version_t soft_version;
	cmd_pack_all_version_t all_version;
	up_cmd_cst_kw_t cst_kw;
	up_cmd_scan_work_status_t scan_work_status;
	up_cmd_tts_encode_t tts_encode;
} up_cmd_u;

// @描  述: 离线引擎运行模式枚举
typedef enum {
	e_listen_algo_mode_ivw_only = 0,  // 仅运行离线唤醒引擎
	e_listen_algo_mode_esr_only = 1,  // 仅运行离线识别引擎
	e_listen_algo_mode_one_shot = 2,  // 运行一次离线唤醒+一次离线识别交互模式
	e_listen_algo_mode_mult_shot = 3,  // 运行一次离线唤醒+多次离线识别交互模式
	e_listen_algo_mode_cnt = 4
} listen_algo_mode_e;

typedef struct {
	listen_algo_callback_t algo_cb;  // esr call back
	listen_algo_msg_e mode;  // algo mode
	uint8_t i2s_ch[4];  // i2s_ch sel
	scan_mode_e scan_mode;
	all_version_query_e query_type;
	char* tts_auth_code;
	uint32_t *tts_authlen;
	uint8_t* tts_text;
	uint8_t scan_param_type;
	uint16_t scan_param;
	int16_t adjust_x_offset;
	int16_t adjust_y_offset;
} algo_t;

typedef enum
{
	e_listen_set_multiline_timeout = 0,
	e_listen_set_tts_base_speed,
	e_listen_set_tts_speed,
	e_listen_set_scan_direction,
} listen_scan_param_type_e;

#endif
