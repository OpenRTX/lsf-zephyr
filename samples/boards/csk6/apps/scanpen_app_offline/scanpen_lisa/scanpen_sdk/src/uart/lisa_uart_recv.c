#include "lisa_uart_recv.h"
#include <lisa_mem.h>
#include <lisa_err.h>
#include <lisa_typedef.h>
#include <lisa_log.h>
#include "lisa_uart_send.h"
#include "lisa_evs_spi_parser.h"

#define TAG "lisa_uart_recv"

#define UART_QUEUE_CNT (5)
#define HEART_BEAT_DELAY (4000)


static lisa_uart_recv_t *g_uart_recv = NULL;

static up_cmd_u *g_push_up_cmd = NULL;

static algo_t s_algo __attribute__((aligned(32))) = {0};
static frame_desc_u *g_head = NULL;
static up_cmd_u *g_recv_up_cmd = NULL;
extern lisa_image_transfer_status_e g_img_status;

static void _frame_cmd_send_algo(frame_desc_u *head)
{
	g_push_up_cmd = (up_cmd_u *)&(head->frame_u.data_t.data[0]);
	switch (head->frame_u.data_t.data[0]) {
		case e_up_cmd_software_version:
		case e_up_cmd_esr_angle:
		case e_up_cmd_all_version:
		case e_up_cmd_cst_kw:
		case e_up_cmd_scan_work_status:
		case e_up_cmd_esr_result:
		case e_up_cmd_heartbeat:
		case e_cmd_scan_tts_encode:
		case e_up_cmd_esr_timeout: {
			uart_send_msg_t msg;
            memcpy(&msg.dat, head, sizeof(frame_desc_u));

            lisa_err_t ret = lisa_queue_push(g_uart_recv->recv_queue, &msg, sizeof(uart_send_msg_t), 0);
            if (ret != LISA_OK) LISA_LOGE(TAG, "[UART]xQueueSend");
		} break;
		case e_up_cmd_com_stat: {
			switch (g_push_up_cmd->com_stat.status) {
				case e_frame_ready: {
					uart_send_msg_t msg;
                    memcpy(&msg.dat, head, sizeof(frame_desc_u));

					lisa_err_t ret = lisa_queue_push(g_uart_recv->recv_queue, &msg, sizeof(uart_send_msg_t), 0);
                    if (ret != LISA_OK) LISA_LOGE(TAG, "[UART]xQueueSend");
				} break;
				case e_frame_correct: {
					uart_send_msg_t msg;
                    memcpy(&msg.dat, head, sizeof(frame_desc_u));

					lisa_err_t ret = lisa_queue_push(g_uart_recv->recv_queue, &msg, sizeof(uart_send_msg_t), 0);
                    if (ret != LISA_OK) LISA_LOGE(TAG, "[UART]xQueueSend");
                }	break;
				case e_frame_error_frame:
				case e_frame_error_argv:
					LISA_LOGE(TAG, "[%s:%d] Frame Err\n", __FUNCTION__, __LINE__);
					break;
				default:
					LISA_LOGE(TAG, "unknown up cmd");
					break;
			}
		} break;
		default:
			break;
	}
}

lisa_uart_recv_t* lisa_uart_recv_create(lisa_uart_protocol_t* protocol, listen_algo_config_t config)
{

    lisa_queue_t* recv_queue = lisa_queue_create(UART_QUEUE_CNT, "uart_recv_queue", sizeof(uart_send_msg_t));
    if (!recv_queue) {
        LISA_LOGE(TAG, "uart_recv_queue create failed!!!!");
        return NULL;
    }
    if (!config.algo_cb) {
        return NULL;
    }
    s_algo.algo_cb = config.algo_cb;
    lisa_uart_recv_t* handle = lisa_mem_calloc(1, sizeof(lisa_uart_recv_t));
    handle->recv_queue = recv_queue;
    lisa_uart_add_recv_frame_cb(_frame_cmd_send_algo);
    g_uart_recv = handle;
    return handle;
}

lisa_err_t lisa_uart_recv()
{
    lisa_err_t ret = LISA_FAIL;
    uart_send_msg_t uart_msg = {0};
    static uint32_t heart_beat_cnt = 0;
    ret = lisa_queue_receive(g_uart_recv->recv_queue, &uart_msg, sizeof(uart_send_msg_t), 5);
    if (LISA_OK == ret) {
        heart_beat_cnt = 0;
        g_head = (frame_desc_u *)&uart_msg.dat;
        g_recv_up_cmd = (up_cmd_u *)&(g_head->frame_u.data_t.data[0]);
        switch (g_head->frame_u.data_t.data[0]) {
            case e_up_cmd_com_stat: {
                switch (g_recv_up_cmd->com_stat.status) {
                    case e_frame_ready:
                        // send_channels_mask();
                        ret = lisa_uart_send_start(e_listen_algo_mode_mult_shot, 10);
                        if (LISA_OK != ret)
                            LISA_LOGE(TAG, "lisa_uart_send_start fail\n");
                        break;
                    case e_frame_correct:
                        break;
                    case e_frame_error_frame:
                    case e_frame_error_argv:
                        break;
                    default:
                        break;
                }
            } break;
            case e_up_cmd_esr_result:  // esr result
                break;
            case e_up_cmd_esr_timeout:  // esr time_out
                break;
            case e_up_cmd_software_version:
                break;
            case e_up_cmd_all_version: {
                s_algo.algo_cb(e_listen_algo_msg_soft_version, g_recv_up_cmd->all_version.type, (uint8_t*)g_recv_up_cmd->all_version.ver, 0);
            } break;
            case e_up_cmd_esr_angle:	// esr angle
                break;
            case e_up_cmd_cst_kw:
            {
                LISA_LOGI(TAG, "4002 cst_kw.status:%d\n",  g_recv_up_cmd->cst_kw.status);
            } break;
            case e_up_cmd_scan_work_status:
            {
                uint8_t *status = (uint8_t*)&g_recv_up_cmd->scan_work_status.work_status;

                if(e_work_status_idle == status[0]) {
                    g_img_status = LISA_IMAGE_FRAME_END;
                } else if(e_work_status_scanning == status[0]) {
                    g_img_status = LISA_IMAGE_FRAME_START;
                } else if (e_work_status_text_finish == status[0]) {
                } else if (e_work_status_tts_finish == status[0]) {
                }
                s_algo.algo_cb(e_listen_algo_scan_info, (int)status[0], status, 0);
            } break;
            case e_cmd_scan_tts_encode:
            {   
                int len = g_recv_up_cmd->tts_encode.len[0];
                s_algo.algo_cb(e_listen_algo_tts_encode, len, (char*)g_recv_up_cmd->tts_encode.encode, 0);
            } break;
            default:
                //ALGO_LOGE("[ALGO:%s:%d]no result\n", __FUNCTION__, __LINE__);
                break;
        }
    }
    else {
        heart_beat_cnt++;//表示未收到有效数据;这个线程没隔5ms做检测; HEART_BEAT_DELAY == 1000
        if ((HEART_BEAT_DELAY == heart_beat_cnt) || ((HEART_BEAT_DELAY * 2 ) == heart_beat_cnt) || ((HEART_BEAT_DELAY * 3) == heart_beat_cnt)) {
            //printf("[ALGO:%s:%d] send heart beat cmd[ONE].\n", __FUNCTION__, __LINE__);//发送心跳指令，等待回复
            lisa_uart_send_heartbeat();
        }
        else if ((HEART_BEAT_DELAY * 4) == heart_beat_cnt) {
            heart_beat_cnt = 0;
        }
    }
    return LISA_OK;
}
