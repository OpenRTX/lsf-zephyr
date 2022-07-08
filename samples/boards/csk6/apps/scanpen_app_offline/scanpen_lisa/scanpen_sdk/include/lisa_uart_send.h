#ifndef __LISA_EVS_UART_SEND_H__
#define __LISA_EVS_UART_SEND_H__


#include <zephyr.h>
#include <lisa_mem.h>
#include <lisa_time.h>

#define ESR_RSLT_MAX_LEN		(400)
#define ESR_ANGLE_LEN			(20)

#define VERSION_MAJOR				(1)
#define VERSION_MINOR				(0)
#define VERSION_RESVE				(0)
#define VERSION_BUILD				(0) // alpha

#define STITCH_VERSION_MAJOR 1
#define STITCH_VERSION_MINOR 0
#define STITCH_VERSION_RESVE 0

#define CUTLINE_VERSION_MAJOR 1
#define CUTLINE_VERSION_MINOR 0
#define CUTLINE_VERSION_RESVE 0

#define OCR_VERSION_MAJOR 1
#define OCR_VERSION_MINOR 0
#define OCR_VERSION_RESVE 0

typedef enum {
	e_up_cmd_communicate_status = 0x01,
	e_up_cmd_software_version	= 0x02,
	e_up_cmd_system_status		= 0x03,
	e_up_cmd_all_version        = 0x04,
	e_up_cmd_esr_result			= 0x10,
	e_up_cmd_esr_error_result	= 0x11,
	e_up_cmd_play_end			= 0x20,
	e_up_cmd_scan_work_status   = 0x50,
	e_up_cmd_adjust_work_status = 0x51,
	e_up_cmd_tts_encryptcode    = 0x52,
	e_up_cmd_upload_comm_data   = 0x53,
	e_up_cmd_cv_motor_run       = 0x81,
	e_up_cmd_cv_motor_stop      = 0x82,
	e_up_cmd_proto_verison		= 0xFE,
	e_up_cmd_heart_beat			= 0xFF
} up_cmd_type_e;

typedef enum {
	e_frame_ready				= 0x00,
	e_frame_correct				= 0x10,
	e_frame_error_frame			= 0x11,
	e_frame_error_argv			= 0x12,
	e_frame_error_state			= 0x13
} frame_info_e;

typedef enum {
	e_system_status_idle_ready	= 0x00,
	e_system_status_esr			= 0x10,
	e_system_status_play		= 0x11,
	e_system_status_tts			= 0x12,
	e_system_status_sleep		= 0x13
} system_status_send_e;

typedef enum {
	e_none						= 0x00,
	e_esr_timeout				= 0x01,
	e_esr_refuse				= 0x02,
	e_engine_error				= 0x10
} esr_error_type_e;

typedef enum {
	version_query_fw_stitch          = 0x00,
	version_query_master             = 0x01,
	version_query_stitch             = 0x02,
	version_query_chipid             = 0x03,
	version_query_cutline            = 0x04,
	version_query_ocr                = 0x05,
	version_query_tts_engine         = 0x06,
	version_query_tts_pronunciation  = 0x07,
	version_query_all_version        = 0x10,
} all_version_query_e;

typedef enum {
	common_data_adjust_image             = 0x00,
	common_data_ocr_text                 = 0x01,
	common_data_offline_cutline_image    = 0x02,
	common_data_offline_stitch_image     = 0x03,
	common_data_online_stitch_image      = 0x04,
	common_data_tts_audio                = 0x05,
} common_data_type_e;

typedef struct {
	uint8_t		cmd;
	uint8_t		type;
} up_cmd_pack_frame_info_t;

typedef struct {
	uint8_t		cmd;
	uint8_t		version[4];
} up_cmd_pack_soft_version_t;

typedef struct {
	uint8_t		cmd;
	uint8_t		status;
} up_cmd_pack_system_status_t;

typedef union {
	uint8_t	data[ESR_RSLT_MAX_LEN + 4];
	struct{
		uint8_t		cmd;
		uint8_t	keyword_id[2];
		uint8_t	id_score[2];
		uint8_t angle[ESR_ANGLE_LEN];
		uint8_t		id_text[1];
	} esr_result_t;				// size = 6byte, not 5 byte
} up_cmd_pack_esr_result_u;

typedef union {
	uint8_t data[ESR_RSLT_MAX_LEN + 4];
	struct{
		uint8_t		cmd;
		uint8_t		keyword_id[2];
	} esr_id_t;
	struct{
		uint8_t		cmd;
		uint8_t		id_text[1];
	} esr_text_t;
} up_cmd_pack_esr_opt_result_u;

typedef struct {
	uint8_t		cmd;
	uint8_t		type;
} up_cmd_pack_esr_err_result_t;

typedef struct {
	uint8_t		cmd;
} up_cmd_pack_play_end_t;

typedef struct {
	uint8_t cmd;
} up_cmd_pack_heart_beat_t;

typedef struct {
	uint8_t cmd;
	uint8_t ver;
} up_cmd_pack_proto_version_t;
typedef struct {
	uint8_t cmd;
	uint8_t len[4];
	uint8_t data[0];
} cmd_scan_tts_encryptcode_t;

typedef struct {
	uint8_t cmd;
	uint8_t direction;
	uint8_t distanct;
	uint8_t duration[2];
} up_cmd_cv_motor_run_t;

typedef struct {
	uint8_t cmd;
} up_cmd_cv_motor_stop_t;

typedef struct {
	uint8_t cmd;
	uint8_t work_status;
} up_cmd_scan_work_status_t;

typedef struct {
	uint8_t cmd;
	uint8_t adjust_status;
	uint8_t x_shift;
	uint8_t y_shift;
} up_cmd_adjust_work_status_t;

typedef struct {
	uint8_t cmd;
	uint8_t type;
	uint8_t ver_1;
	uint8_t ver_2;
	uint8_t ver_3;
} up_cmd_pack_all_version_t;

#pragma pack(1)
typedef struct {
	uint8_t cmd;
	uint8_t type;
	uint64_t chipid;
} chipid_version_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	uint8_t cmd;
	uint8_t type;
	uint32_t role_id;
} up_cmd_pack_tts_role_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	uint8_t cmd;
	uint8_t type;
	uint32_t id;
	uint16_t x_start;
	uint16_t x_stop;
	uint16_t y_start;
	uint16_t y_stop;
	uint32_t len;
	uint8_t resv[16];
	uint8_t data[0];
} up_cmd_pack_comm_data_t;
#pragma pack()

typedef union {
	up_cmd_pack_frame_info_t		frame_info;
	up_cmd_pack_soft_version_t		soft_version;
	up_cmd_pack_system_status_t		system_status;
	up_cmd_pack_esr_result_u		esr_result;
	up_cmd_pack_esr_opt_result_u	esr_opt_result;
	up_cmd_pack_esr_err_result_t	esr_error_result;
	up_cmd_pack_play_end_t			play_end;
	up_cmd_pack_heart_beat_t		heart_beat;
	up_cmd_pack_proto_version_t		proto_version;
	cmd_scan_tts_encryptcode_t		tts_encryptcode;
	up_cmd_cv_motor_run_t           cv_motor_run;
	up_cmd_cv_motor_stop_t          cv_motor_stop;
	up_cmd_scan_work_status_t       scan_work_status;
	up_cmd_adjust_work_status_t     adjust_work_status;
	up_cmd_pack_all_version_t       all_version;
	chipid_version_t                chipid_version;
	up_cmd_pack_tts_role_t          tts_role_id;
	up_cmd_pack_comm_data_t         comm_data;
} up_cmd_pack_u;

void send_proc_frame_info(frame_info_e e_frame_info, uint8_t fid);
void send_proc_soft_version(void);
void send_proc_system_status(uint32_t status);
void send_proc_esr_result(uint16_t key_id, uint16_t score, const char *text);
void send_proc_esr_err_result(esr_error_type_e e_type);
void send_proc_play_end(void);
void send_proc_heart_beat(void);
void send_proc_proto_version(void);
void send_proc_tts_encryptcode(char *code, int size);
void send_proc_cv_motor_run(uint8_t direction, uint8_t distanct, uint16_t duration);
void send_proc_cv_motor_stop(void);
void send_proc_scan_work_status(uint8_t stu);
void send_proc_adjust_work_status(uint8_t stu, uint8_t x_shift, uint8_t y_shift);
void send_proc_version(all_version_query_e type);
void send_proc_upload_comm_data(uint8_t *out, uint8_t *in, uint32_t in_len, common_data_type_e type, uint32_t fuid, uint16_t w, uint16_t h);
void uart_send_image(uint8_t *image, uint16_t w, uint16_t h, uint32_t image_len, common_data_type_e type);

#endif//__LISA_EVS_UART_SEND_H__
