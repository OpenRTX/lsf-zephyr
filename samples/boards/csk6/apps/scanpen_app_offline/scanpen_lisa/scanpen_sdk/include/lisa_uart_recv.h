#ifndef __LISA_EVS_UART_RECV_H__
#define __LISA_EVS_UART_RECV_H__


#include <zephyr.h>
#include <lisa_mem.h>
#include <lisa_time.h>
#include <lisa_thread.h>
#include <lisa_uart_port.h>
#include <string.h>

#define VOICE_LIST_CNT		(4)
#define I2S_IN_CH_CNT		(4)
#define I2S_OUT_CH_CNT		(8)
#define SUPPORT_DIALECT_SEL	(1)

#define FRM_HDR_TAG				(0x4658) // XF
#define FRM_HDR_TAG_L			(0x58) // X
#define FRM_HDR_TAG_H			(0x46) // F
#define FRM_MAX_LEN				(1024)
#define FRM_HEAD_LEN			(6)

#define portMAX_DELAY 0xffffffff

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
			uint8_t data[0]; // last byte for checksum
		} data_t;
	} frame_u;
} frame_desc_u;

typedef enum {
	e_cmd_system_set		= 0x01,			// system set
	e_cmd_uart_set			= 0x02,			// set UART baudrate
	e_cmd_version_query		= 0x03,			// system version query
	e_cmd_status_query		= 0x04,			// system status query
	e_cmd_sleep				= 0x05,			// set system sleep
#if SUPPORT_DIALECT_SEL
	e_cmd_dialect_sel		= 0x06,			// dialect select
#endif//SUPPORT_DIALECT_SEL
	e_cmd_stop				= 0x10,			// stop all module
	e_cmd_start				= 0x11,			// start Mini-Esr
	e_cmd_play				= 0x12,			// start play
	e_cmd_i2s_ch_sel		= 0x15,			// i2s out channel set command
	e_cmd_adc_gain_ctrl	    = 0x16,			// i2s out channel set command
	e_cmd_all_ver_query     = 0x19,
	e_cmd_adc_ch_ctrl       = 0x21,         // adc channel control
	e_cmd_esr_hold_frms     = 0x22,         // esr hold time
	e_cmd_switch_mic_ch		= 0x23,
	// scan pen
	e_cmd_scan_work_mode    = 0x50,
	e_cmd_scan_set_para     = 0x51,
	e_cmd_dict_size_query	= 0x52,			// system version query
	e_cmd_scan_tts_text  	= 0x53,
	e_cmd_scan_tts_auth_code= 0x54,
	e_cmd_manual_adjust_info= 0x55,
	e_cmd_manual_adjust_abs_info = 0x56,
	e_cmd_scan_translate_text = 0x57,
	e_cmd_scan_word_search_text=0x58,
	e_cmd_proto_version     = 0xFE,         // uart proto version
	e_cmd_heart_beat		= 0xFF			// heart beat
} recv_cmd_type_e;

typedef enum {
	e_reday_low				= 0x00,
	e_ready_high			= 0xaa,
	e_ready_default			= 0xfe,
	e_ready_history			= 0xff
} ready_option_type_e;

typedef enum {
	e_sleep_time_disable	= 0x00,
	e_sleep_time_max		= 0xf0,
	e_sleep_time_default	= 0xfe,
	e_sleep_time_history	= 0xff
} sleep_time_type_e;

typedef enum {
	e_esr_result_keyid_text	= 0x00,
	e_esr_result_keyid		= 0x01,
	e_esr_result_text		= 0x02,
	e_esr_resullt_sum		= 0x03
} esr_result_type_e;

typedef struct {
	uint8_t cmd;
	uint8_t volume;			// set voice volume
	uint8_t ready;			// Ready signal trigger mode
	uint8_t sleep_time;		// set Idle time to go sleep
	uint8_t esr_result_opt;	// esr result output option
	uint8_t check;
} cmd_system_set_t;

typedef struct {
	uint8_t cmd;
	uint8_t baudrate;		// 0: 9600; 1: 19200; 2: 57600; 3: 115200; 4: 345600;
	uint8_t check;
} cmd_uart_set_t;

typedef struct {
	uint8_t cmd;
	uint8_t check;
} cmd_version_query_t;

typedef struct {
	uint8_t cmd;
	uint8_t check;
} cmd_status_query_t;

typedef struct {
	uint8_t cmd;
	uint8_t check;
} cmd_sleep_t;

typedef struct {
	uint8_t cmd;
	uint8_t check;
} cmd_stop_t;

typedef struct {
	uint8_t cmd;
	uint8_t dialect;
	uint8_t check;
} cmd_dialect_t;

typedef struct {
	uint8_t cmd;
	uint8_t voice;				// voice id
	struct {
		uint8_t mode :4;		// 3:0 esr_mode
		uint8_t beam :3;		// 6:4 asr_beam
		uint8_t loop :1;		// 7 loop
	};
	uint8_t sleep;				// recognize sleep timeout
	uint8_t check;
} cmd_start_t;

typedef struct {
	uint8_t cmd;
	uint8_t voice[4];			// mp3 play table
	uint8_t check;
} cmd_play_t;

typedef struct {
	uint8_t cmd;
	uint8_t data[4];			// i2s out channel set table
} cmd_i2s_ch_sel_t;

typedef struct {
	uint8_t cmd;
	uint8_t ch[4];  // adc channel gain
} cmd_adc_gain_ctrl_t;

typedef struct {
	uint8_t cmd;
	uint8_t mic1_ch;
	uint8_t mic2_ch;
	uint8_t ref1_ch;
	uint8_t ref2_ch;
} cmd_adc_ch_ctrl_t;

typedef struct {
	uint8_t cmd;
	uint8_t frames;
} cmd_esr_hold_frms_t;

typedef struct {
	uint8_t cmd;
	uint8_t ver;
} cmd_proto_version_t;

typedef struct {
	uint8_t cmd;
	uint8_t mode;
} cmd_scan_workmode_t;

typedef enum {
	e_scan_para_multiline_time = 0x00,
	e_tts_base_voice_speed,
	e_tts_synth_voice_speed,
	e_left_hand_hold,
	e_image_transfer_proto_type,
} scan_setpara_type_e;

typedef struct {
	uint8_t cmd;
	uint8_t type;
	uint16_t data;
} cmd_scan_setpara_t;

typedef struct {
	uint8_t cmd;
	uint8_t len[4];
	uint8_t data[0];
} cmd_scan_tts_text_t;

typedef struct {
	uint8_t cmd;
	uint8_t len[4];
	uint8_t data[0];
} cmd_scan_tts_auth_code_t;

typedef struct {
	uint8_t cmd;
	uint8_t x_shift[2];
	uint8_t y_shift[2];
} cmd_manual_adjust_info_t;

#pragma pack(1)
typedef struct {
	uint8_t cmd;
	uint16_t x_start;
	uint16_t x_stop;
	uint16_t y_start;
	uint16_t y_stop;
} cmd_manual_adjust_abs_info_t;
#pragma pack()

typedef struct {
	uint8_t cmd;
	uint8_t data;
} cmd_all_ver_query_t;

typedef union {
	cmd_system_set_t config;
	cmd_uart_set_t uart_set;
	cmd_version_query_t version;
	cmd_status_query_t status;
	cmd_sleep_t sleep;
	cmd_stop_t stop;
	cmd_start_t start;
	cmd_play_t play;
	cmd_i2s_ch_sel_t channel;
	cmd_adc_gain_ctrl_t adc_gain_ctrl;
	cmd_adc_ch_ctrl_t adc_ch_ctrl;
	cmd_esr_hold_frms_t esr_hold_frms;
	cmd_proto_version_t proto_version;
	cmd_scan_workmode_t workmode;
	cmd_scan_setpara_t scan_para;
	cmd_scan_tts_text_t tts_txt;
	cmd_scan_tts_auth_code_t tts_auth_code;
	cmd_all_ver_query_t all_ver_query;
	cmd_manual_adjust_info_t manual_adjust_info;
	cmd_manual_adjust_abs_info_t manual_adjust_abs_info;
} recv_cmd_pack_e;

extern uint8_t esr_result_option;

/**
 * @brief uart receive data callback
 *
 * @param type  type: tts/translate/word_search
 * @param data  record data pointer
 * @param len	record len
 *
 * @retval  null
 */
typedef void (*lisa_uart_recv_cb)(recv_cmd_type_e type, void *data, uint32_t len);

bool frame_recv_proc(void *arg);

#endif//__LISA_EVS_UART_RECV_H__
