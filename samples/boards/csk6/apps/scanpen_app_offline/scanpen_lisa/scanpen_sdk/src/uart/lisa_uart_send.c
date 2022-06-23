#include "lisa_uart_recv.h"
#include "lisa_uart_send.h"
#include <lisa_log.h>

#undef	TAG
#define TAG	"[SEND]"

#define FRAME_DEVRPT_FID				(0xFF)
#define FRAEM_LEN_MINUS_DATA			(9)			// 9 = FRAME_LEN - DAT_LEN
#define FRAME_HEADER_LEN			    (6)			// header len
#define FRAME_HEADER_CHK_LEN			(5)			// header check len
#define SEND_FRAME_RIGISTER_TYPE		(0xFF)		// must be 0xFF, no use
#define SEND_FARME_RIGISTER_ADDR		(0)			// must be 0x00, no use
#define FRAME_DATA_CHK_LEN_MINUS_DATA	(2)			// FRAME_DATA_CHK_LEN - DAT_LEN

static up_cmd_pack_u u_send_cmd_pack = { { 0 } };	// send_cmd_pack
static frame_desc_u u_send_frame = { { 0 } };		// send frame

// pack send data into an protocol frame
static void pack_send(void *data, short len, uint8_t fid)
{
	// header packet
	u_send_frame.frame_u.header_t.tag = FRM_HDR_TAG;
	u_send_frame.frame_u.header_t.len = len + FRAEM_LEN_MINUS_DATA;
	u_send_frame.frame_u.header_t.frame_id = fid;
	u_send_frame.frame_u.header_t.head_check
		= -prot_pack_check(&u_send_frame, FRAME_HEADER_CHK_LEN);
	// data packet
	u_send_frame.frame_u.data_t.type = SEND_FRAME_RIGISTER_TYPE;
	u_send_frame.frame_u.data_t.address = SEND_FARME_RIGISTER_ADDR;
	memcpy(&u_send_frame.frame_u.data_t.data[0], data, len);
	u_send_frame.frame_u.data_t.data[len]
		= -prot_pack_check(&u_send_frame.frame_u.data_t.type, len + FRAME_DATA_CHK_LEN_MINUS_DATA);
	// send data to cache pool
	prot_send_data(&u_send_frame, u_send_frame.frame_u.header_t.len);
}

// up send ready status(directly send, without thread wrapping)
void send_proc_frame_ready(void)
{
	int len = sizeof(up_cmd_pack_frame_info_t);
	// header packet
	u_send_frame.frame_u.header_t.tag = FRM_HDR_TAG;
	u_send_frame.frame_u.header_t.len = len + FRAEM_LEN_MINUS_DATA;
	u_send_frame.frame_u.header_t.frame_id = 0;
	u_send_frame.frame_u.header_t.head_check
		= -prot_pack_check(&u_send_frame, FRAME_HEADER_CHK_LEN);
	// data packet
	u_send_frame.frame_u.data_t.type = SEND_FRAME_RIGISTER_TYPE;
	u_send_frame.frame_u.data_t.address = SEND_FARME_RIGISTER_ADDR;
	u_send_frame.frame_u.data_t.data[0] = e_up_cmd_communicate_status;
	u_send_frame.frame_u.data_t.data[1] = e_frame_ready;
	u_send_frame.frame_u.data_t.data[2]
		= -prot_pack_check(&u_send_frame.frame_u.data_t.type, len + FRAME_DATA_CHK_LEN_MINUS_DATA);
	// send packet visa uart directlly
	// bool uart_app_send(void *data, int len, int ticks);
	// uart_app_send(&u_send_frame, u_send_frame.frame_u.header_t.len, portMAX_DELAY);
}

// up send communicate status
void send_proc_frame_info(frame_info_e e_frame_info, uint8_t fid)
{
	u_send_cmd_pack.frame_info.cmd = e_up_cmd_communicate_status;
	u_send_cmd_pack.frame_info.type = e_frame_info;
	pack_send(&u_send_cmd_pack, sizeof(up_cmd_pack_frame_info_t), FRAME_DEVRPT_FID == fid ? -fid : fid);
}

// up send software version
void send_proc_soft_version(void)
{
	u_send_cmd_pack.soft_version.cmd = e_up_cmd_software_version;
	u_send_cmd_pack.soft_version.version[0] = VERSION_MAJOR;
	u_send_cmd_pack.soft_version.version[1] = VERSION_MINOR;
	u_send_cmd_pack.soft_version.version[2] = VERSION_RESVE;
	// u_send_cmd_pack.soft_version.version[3] = VERSION_BUILD - 1000;
	pack_send(&u_send_cmd_pack, sizeof(up_cmd_pack_soft_version_t), FRAME_DEVRPT_FID);
}

// up send system status
void send_proc_system_status(uint32_t status)
{
	u_send_cmd_pack.system_status.cmd = e_up_cmd_system_status;
}

// up send MINI-ESR result
void send_proc_esr_result(uint16_t key_id, uint16_t score, const char *text)
{
	// int len = 0;
	// switch(esr_result_option){
	// case e_esr_result_keyid_text:
	// 	if(0 == key_id) key_id = 501;
	// 	u_send_cmd_pack.esr_result.esr_result_t.cmd = e_up_cmd_esr_result;
	// 	u_send_cmd_pack.esr_result.esr_result_t.keyword_id[0] = key_id & (0x00ff);
	// 	u_send_cmd_pack.esr_result.esr_result_t.keyword_id[1] = (key_id >> 8) & (0x00ff);
	// 	u_send_cmd_pack.esr_result.esr_result_t.id_score[0] = score & (0x00ff);
	// 	u_send_cmd_pack.esr_result.esr_result_t.id_score[1] = (score >> 8) & (0x00ff);
	// 	len += 5;
	// 	cae_get_realtime_angle(u_send_cmd_pack.esr_result.esr_result_t.angle, ESR_ANGLE_LEN);
	// 	len += ESR_ANGLE_LEN;
	// 	strncpy((char *)&u_send_cmd_pack.esr_result.esr_result_t.id_text[0]
	// 		, text, sizeof(u_send_cmd_pack.esr_result.data));
	// 	int len_txt = strlen((char *)&u_send_cmd_pack.esr_result.esr_result_t.id_text[0]);
	// 	if (strlen(text) != len_txt)
	// 		LISA_LOGW(TAG, "trunc:%s(%d)", text, strlen(text));
	// 	len += len_txt;
	// 	break;
	// case e_esr_result_keyid:
	// 	u_send_cmd_pack.esr_opt_result.esr_id_t.cmd = e_up_cmd_esr_result;
	// 	u_send_cmd_pack.esr_opt_result.esr_id_t.keyword_id[0] = key_id & (0x00ff);
	// 	u_send_cmd_pack.esr_opt_result.esr_id_t.keyword_id[1] = (key_id >> 8) & (0x00ff);
	// 	len += 2;
	// 	break;
	// case e_esr_result_text:
	// 	u_send_cmd_pack.esr_opt_result.esr_text_t.cmd = e_up_cmd_esr_result;
	// 	strncpy((char *)&u_send_cmd_pack.esr_opt_result.esr_text_t.id_text[0]
	// 		, text, sizeof(u_send_cmd_pack.esr_opt_result.data));
	// 	len = strlen((char *)&u_send_cmd_pack.esr_opt_result.esr_text_t.id_text[0]);
	// 	if (strlen(text) != len)
	// 		LISA_LOGW(TAG, "trunc:%s(%d)", text, strlen(text));
	// 	len += 2;
	// 	break;
	// default:
	// 	break;
	// }
	// pack_send(&u_send_cmd_pack, len, FRAME_DEVRPT_FID);
}

// if MINI-ESR timeout, up send
void send_proc_esr_err_result(esr_error_type_e e_type)
{
	u_send_cmd_pack.esr_error_result.cmd = e_up_cmd_esr_error_result;
	u_send_cmd_pack.esr_error_result.type = e_type;
	pack_send(&u_send_cmd_pack, sizeof(up_cmd_pack_esr_err_result_t), FRAME_DEVRPT_FID);
}

// when play end, up send message
void send_proc_play_end(void)
{
	u_send_cmd_pack.play_end.cmd = e_up_cmd_play_end;
	pack_send(&u_send_cmd_pack, sizeof(up_cmd_pack_play_end_t), FRAME_DEVRPT_FID);
}

// send heart beat
void send_proc_heart_beat(void)
{
#if EN_HEART_BEAT
	u_send_cmd_pack.play_end.cmd = e_up_cmd_heart_beat;
	pack_send(&u_send_cmd_pack, sizeof(up_cmd_pack_heart_beat_t), FRAME_DEVRPT_FID);
#endif//EN_HEART_BEAT
}

void send_proc_proto_version(void)
{
	// u_send_cmd_pack.proto_version.cmd = e_up_cmd_proto_verison;
	// u_send_cmd_pack.proto_version.ver = UART_PROTO_VERSION;
	// pack_send(&u_send_cmd_pack, sizeof(up_cmd_pack_proto_version_t), FRAME_DEVRPT_FID);
}

void send_proc_scan_work_status(uint8_t stu)
{
	int len = 0;
	int wait_ms = 5;

	u_send_cmd_pack.scan_work_status.cmd = e_up_cmd_scan_work_status;
	u_send_cmd_pack.scan_work_status.work_status = stu;

	len = sizeof(up_cmd_scan_work_status_t);
	pack_send((void *)&u_send_cmd_pack, len, wait_ms);
}

void send_proc_adjust_work_status(uint8_t stu,uint8_t x_shift, uint8_t y_shift)
{
	int wait_ms = 5;

	u_send_cmd_pack.adjust_work_status.cmd = e_up_cmd_adjust_work_status;
	u_send_cmd_pack.adjust_work_status.adjust_status = stu;
	u_send_cmd_pack.adjust_work_status.x_shift = x_shift;
	u_send_cmd_pack.adjust_work_status.y_shift = y_shift;
	pack_send((void *)&u_send_cmd_pack, sizeof(up_cmd_adjust_work_status_t), wait_ms);
}

void send_proc_tts_encryptcode(char *code, int size)
{
	u_send_cmd_pack.tts_encryptcode.cmd = e_up_cmd_tts_encryptcode;

	memcpy(&u_send_cmd_pack.tts_encryptcode.len[0], &size, sizeof(int));

	memcpy(&u_send_cmd_pack.tts_encryptcode.data[0], code, size);

	int len = size + sizeof(cmd_scan_tts_encryptcode_t);
	pack_send(&u_send_cmd_pack, len, FRAME_DEVRPT_FID);
}

static void send_master_ver(void)
{
	u_send_cmd_pack.all_version.cmd = e_up_cmd_all_version;
	u_send_cmd_pack.all_version.type = version_query_master;
	u_send_cmd_pack.all_version.ver_1 = VERSION_MAJOR;
	u_send_cmd_pack.all_version.ver_2 = VERSION_MINOR;
	u_send_cmd_pack.all_version.ver_3 = VERSION_RESVE;
	pack_send((void *)&u_send_cmd_pack, sizeof(up_cmd_pack_all_version_t), 5);
}

static void send_stitch_ver(void)
{
	u_send_cmd_pack.all_version.cmd = e_up_cmd_all_version;
	u_send_cmd_pack.all_version.type = version_query_stitch;
	u_send_cmd_pack.all_version.ver_1 = STITCH_VERSION_MAJOR;
	u_send_cmd_pack.all_version.ver_2 = STITCH_VERSION_MINOR;
	u_send_cmd_pack.all_version.ver_3 = STITCH_VERSION_RESVE;
	pack_send((void *)&u_send_cmd_pack, sizeof(up_cmd_pack_all_version_t), 5);
}

static void send_chipid_ver(void)
{
	u_send_cmd_pack.chipid_version.cmd = e_up_cmd_all_version;
	u_send_cmd_pack.chipid_version.type = version_query_chipid;
	u_send_cmd_pack.chipid_version.chipid = 0;// get_chip_id();
	pack_send((void *)&u_send_cmd_pack, sizeof(chipid_version_t), 5);
}

static void send_cutline_ver(void)
{
	u_send_cmd_pack.all_version.cmd = e_up_cmd_all_version;
	u_send_cmd_pack.all_version.type = version_query_cutline;
	u_send_cmd_pack.all_version.ver_1 = CUTLINE_VERSION_MAJOR;
	u_send_cmd_pack.all_version.ver_2 = CUTLINE_VERSION_MINOR;
	u_send_cmd_pack.all_version.ver_3 = CUTLINE_VERSION_RESVE;
	pack_send((void *)&u_send_cmd_pack, sizeof(up_cmd_pack_all_version_t), 5);
}

static void send_ocr_ver(void)
{
	u_send_cmd_pack.all_version.cmd = e_up_cmd_all_version;
	u_send_cmd_pack.all_version.type = version_query_ocr;
	u_send_cmd_pack.all_version.ver_1 = OCR_VERSION_MAJOR;
	u_send_cmd_pack.all_version.ver_2 = OCR_VERSION_MINOR;
	u_send_cmd_pack.all_version.ver_3 = OCR_VERSION_RESVE;
	pack_send((void *)&u_send_cmd_pack, sizeof(up_cmd_pack_all_version_t), 5);
}

static void send_tts_engine_ver(void)
{
	u_send_cmd_pack.all_version.cmd = e_up_cmd_all_version;
	u_send_cmd_pack.all_version.type = version_query_tts_engine;
	u_send_cmd_pack.all_version.ver_1 = 6;
	u_send_cmd_pack.all_version.ver_2 = 0;
	u_send_cmd_pack.all_version.ver_3 = 5;
	pack_send((void *)&u_send_cmd_pack, sizeof(up_cmd_pack_all_version_t), 5);
}

static void send_tts_pronunciation(void)
{
	u_send_cmd_pack.tts_role_id.cmd = e_up_cmd_all_version;
	u_send_cmd_pack.tts_role_id.type = version_query_tts_pronunciation;
#if EN_ALGO_TTS
	u_send_cmd_pack.tts_role_id.role_id = tts_get_role_id();
#else
	u_send_cmd_pack.tts_role_id.role_id = 0;
#endif
	pack_send((void *)&u_send_cmd_pack, sizeof(up_cmd_pack_tts_role_t), 5);
}

void send_proc_version(all_version_query_e type)
{
	switch (type) {
		case version_query_fw_stitch:
			send_master_ver();
			lisa_thread_mdelay(5);
			send_stitch_ver();
			break;
		case version_query_master:
			send_master_ver();
			break;
		case version_query_stitch:
			send_stitch_ver();
			break;
		case version_query_chipid:
			send_chipid_ver();
			break;
		case version_query_cutline:
			send_cutline_ver();
			break;
		case version_query_ocr:
			send_ocr_ver();
			break;
		case version_query_tts_engine:
			send_tts_engine_ver();
			break;
		case version_query_tts_pronunciation:
			send_tts_pronunciation();
			break;
		case version_query_all_version:
			send_master_ver();
			lisa_thread_mdelay(5);
			send_stitch_ver();
			lisa_thread_mdelay(5);
			send_cutline_ver();
			lisa_thread_mdelay(5);
			send_ocr_ver();
			lisa_thread_mdelay(5);
			send_tts_engine_ver();
			lisa_thread_mdelay(5);
			send_tts_pronunciation();
			lisa_thread_mdelay(5);
			send_chipid_ver();
			break;
		default:
			break;
	}
}
