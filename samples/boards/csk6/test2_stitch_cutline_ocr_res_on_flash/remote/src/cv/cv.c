#include <string.h>

#include "hal/ClockManager.h"
#include "algo_lib.h"

#if CP_ZEPHYR_DEMO
#include "cv.h"
#include "luna/luna.h"
#else
#include <sof/audio/cv/cv.h>
#include <sof/audio/cv/include/luna/luna.h>
#endif

#define SOF_TEST        (0)

#if SOF_TEST
#include "img.h"
#define BMP_HEADER_SIZE (1078)
#endif

#if COST_TIME_ENBALE
static uint32_t tick = 0;
#endif

static img_frame_t iframe;                          /* cv_xy_crop_proc input parameter */
static __align_4__ cv_image_send_t oframe;      /* cv_xy_crop_proc output parameter */
static uint8_t __align_4__ in_image[IMG_SIZE];  /* image buffer used by cv_xy_crop_proc */

#define BLACK_BOARD_HEIGTH              (280)       /* max black board heigth */
#define BLACK_BOARD_WIDTH	            (3200)      /* max black board width */
#define BLACK_BOARD_DEFAULT_Y_OFESET    ((BLACK_BOARD_HEIGTH - IMG_HEIGHT) / 2)         /* default y_offset */
#define BLACK_BOARD_MAX_Y_OFESET        (BLACK_BOARD_HEIGTH - IMG_HEIGHT)               /* max y_offset */

static uint8_t __align_4__ black_board[BLACK_BOARD_HEIGTH][BLACK_BOARD_WIDTH] = {0};
static char __align_4__ ai_ocr_result[OCR_BUFFER_SIZE];

static img_frame_t o_frame;     /*  */
static int total_width = 0;     /* total width record */
static int g_y_offset = 0;      /* y_offset record */


/* tts private data */
#define TTS_STRING_LEN      (1024)
#define TTS_OUTPUT_STEP     (1024)
#define TTS_STACK_SIZE      (8192)
#define TTS_SILENCE_CNT     (4)

#define TTS_PIPE_SIZE       (20 * 1024)

K_THREAD_STACK_DEFINE(tts_stack_area, TTS_STACK_SIZE);
struct k_thread tts_thread_data;

typedef enum{
    TTS_STATUS_IDLE = 0,
    TTS_STATUS_START,
    TTS_STATUS_COMPLETE,
    TTS_STATUS_OUT_DONE,
}tts_status_e;

typedef struct {
    struct k_pipe pipe;
    uint32_t str_len;
    char string[TTS_STRING_LEN];
    tts_status_e state;
    uint32_t last_cnt;
} tts_data_t;

static tts_data_t tts_data;


void sys_init(void)
{
    // enable NPU Clock
	__HAL_CRM_NPU_CLK_ENABLE();

    // release NPU
    if(IP_SYSCTRL->REG_SW_RESET_NPU.all){
        IP_SYSCTRL->REG_SW_RESET_NPU.all = 0xCAFE000C;
    }

	luna_init();

	uint32_t luna_val = *((volatile uint32_t *)(ADDR_LUNA_MODULE_ST));

	LOGI("luna_val: %d", luna_val);
}


/* xy algorithm and crop logic */
void ai_xy_crop_new(void)
{
    uint32_t clk = sys_clock_hw_cycles_per_sec();
    LOGI("CP CLK: %d", clk);

    sys_init();
}

void ai_xy_crop_prepare(void)
{
    memset(&iframe, 0, sizeof(iframe));
    memset(&oframe, 0, sizeof(oframe));
    
    cv_xy_crop_init();    
}

void ai_xy_crop_copy(void *src, int src_len, void *dest, int *dest_len)
{
    LOGI("xy in");
    /* copy source to in_image */
#if SOF_TEST
    if(iframe.fuid == sizeof(img_resource) / (IMG_SIZE + BMP_HEADER_SIZE)) iframe.fuid = 0;

    uint8_t *addr = (uint8_t *)img_resource  + iframe.fuid * (IMG_SIZE + BMP_HEADER_SIZE) + BMP_HEADER_SIZE;
    memcpy(in_image, addr, sizeof(in_image));
#else
    /* parameter check */
    if(src_len != IMG_SIZE){
        LOGE("src_len: %d != 23040", src_len);
        *dest_len = 0;
        return;
    }

    if(src == NULL || dest == NULL){
        LOGE("err, src: %p  dest: %p", src, dest);
        *dest_len = 0;
        return;        
    }
    
    memcpy(in_image, (uint8_t *)src, sizeof(in_image));
#endif

    /* configure parameter */
    iframe.data = in_image;
    iframe.w = IMG_WIDTH;
    iframe.h = IMG_HEIGHT;
    iframe.fuid++;

#if COST_TIME_ENBALE
    cost_calc_getus(&tick);
#endif

    memset(&oframe, 0, sizeof(oframe));

    /* ai deal with image data */
    cv_xy_crop_proc(iframe, &oframe);

#if COST_TIME_ENBALE
    LOGI("cv_xy_crop_proc:%d us", cost_calc_getus(&tick));
#endif

    /* output cut image data */
    int len = CV_IMAGE_SEND_HEADER_LEN + oframe.width * oframe.heigth;
    if(len != CV_IMAGE_SEND_HEADER_LEN){
        memcpy((uint8_t *)dest, (uint8_t *)&oframe, len);
        *dest_len = len;
    }else{
        *dest_len = 0;
    }

    LOGI("xy out, %d", *dest_len);

    return;
}

void ai_xy_crop_free(void)
{ 
    cv_xy_crop_deinit();    

    memset(&iframe, 0, sizeof(iframe));
    memset(&oframe, 0, sizeof(oframe));
}


/* cutline algorithm */
void ai_cutline_ocr_new(void)
{

}

void ai_cutline_ocr_prepare(void)
{
    total_width = 0;
    g_y_offset = BLACK_BOARD_DEFAULT_Y_OFESET;

    memset(black_board, 0, sizeof(black_board));
    memset(&o_frame, 0, sizeof(o_frame));
    memset(ai_ocr_result, 0, sizeof(ai_ocr_result));
}

void ai_cutline_ocr_copy(void* src, int src_len)
{
    LOGI("cut in");
    /* parameter check */
    if((src == NULL) || (src_len == 0)){
        LOGE("src: %p, src_len: %d", src, src_len);
        return;
    }

    /* get header */
    cv_image_send_t *head = (cv_image_send_t *)src;
    if(head->tag != CV_IMAGE_SEND_TAG){
        LOGE("tag error : %d != %d", head->tag, CV_IMAGE_SEND_TAG);
        return;
    }

    if((src_len - CV_IMAGE_SEND_HEADER_LEN) != (head->width * head->heigth)){
        LOGE("src: %p, src_len: %d bad", src, src_len);
        return;
    }

    /* write cropped image data to black board with total_width and y_offset*/
    if(total_width + head->width >= BLACK_BOARD_WIDTH){
        LOGW("total_width: %d overflow max line: %d", total_width, BLACK_BOARD_WIDTH);
        return;
    }

    for(int i = 0; i < head->heigth; i++){
        for(int j = 0; j < head->width; j++){
            black_board[g_y_offset + i][total_width + j] = *((uint8_t *)head->data + i * head->width + j);
        }
    }

    total_width += head->width;

    /* add y_shift after write */
    g_y_offset += head->y_shift;
    if(g_y_offset > BLACK_BOARD_MAX_Y_OFESET){
        LOGD("ERROR OFFSET: %d", g_y_offset);
        g_y_offset = BLACK_BOARD_MAX_Y_OFESET;
    }else if(g_y_offset < 0){
        LOGD("ERROR OFFSET: %d", g_y_offset);
        g_y_offset = 0;
    }

    LOGI("cut out");

}

void ai_cutline_ocr_deal(void** dest, int *dest_len)
{
    LOGD("cut_deal in");
    if(total_width == 0){
        *dest = NULL;
        *dest_len = 0;
        LOGE("[1]: ocr input len err: %d", total_width);
        return;
    }

#ifndef NOT_DEAL_AFTER_CUT
    for(int i = 0; i < BLACK_BOARD_HEIGTH; i++){
        for(int j = 0; j < total_width; j++){
            *((uint8_t *)black_board + i * total_width + j) = black_board[i][j];
        }
    }
    LOGI("[2]: deal with done");
#endif

    /* xy crop deinit */
    // ai_xy_crop_free();
    LOGD("[3]: xy crop free done");

    iframe.data = (uint8_t *)black_board;
#ifdef NOT_DEAL_AFTER_CUT
    iframe.w = BLACK_BOARD_WIDTH;
#else
    iframe.w = total_width;
#endif
    iframe.h = BLACK_BOARD_HEIGTH;

    /* cutline algothrim */
    cv_cutline_proc(iframe, &o_frame);
    LOGI("[4]: cutline done");

    /* ocr algothrim */
    int len = sizeof(ai_ocr_result);
    cv_ocr_proc(o_frame, (void *)ai_ocr_result, &len);
    if(len > 0){
        *dest = (char *)ai_ocr_result;
        *dest_len = len;
    }else{
        *dest = NULL;
        *dest_len = 0;
    }

    LOGI("[5]: ocr done");

    /* xy crop prepare */
    // ai_xy_crop_prepare();
}

void ai_cutline_ocr_free(void)
{
    memset(&iframe, 0, sizeof(iframe));
}


/* translate algorithm */
void ai_trans_init(void)
{
    trans_init();
}

void ai_trans_proc(void* src, int src_len, void *dest, int **dest_len)
{
    /* parameter check */
    if((src == NULL) || (src_len == 0) || (dest == NULL)){
        LOGE("src: %p, src_len: %d, dest: %p", src, src_len, dest);
        return;
    }

    LOGI("[6]: trans input: %s", (char *)src);
    trans_proc((char *)src, src_len, (char *)dest, *dest_len);
}

void ai_trans_deinit(void)
{
    trans_deinit();
}


/* tts algorithm */
static int output_pcm(void *data, int len)
{
    int bytes_written = 0;
	if(k_pipe_put(&(tts_data.pipe), (void *)data, len, &bytes_written, len, K_FOREVER) != 0){
		LOGE("tts out pipe put failed");
        return 0;
	}else{
        LOGD("[w]%d", bytes_written);
        return bytes_written;
    } 
}

void tts_thread(void *v1, void *v2, void *v3)
{
    // LOGI("tts synth text in");
    tts_exec_synth_text(tts_data.string, tts_data.str_len);
    tts_data.state = TTS_STATUS_COMPLETE;
    // LOGI("tts synth text out");
}

void ai_tts_init(void)
{
    LOGI("tts init in");

    /* memory used by both ai_xy_crop algorithm and tts algorithm */
    memset(black_board, 0, sizeof(black_board));
    memset(&tts_data, 0, sizeof(tts_data_t));

    /* TODO: only use 4096 as buffer */
    k_pipe_init(&(tts_data.pipe), black_board, TTS_PIPE_SIZE); 

    tts_engine_init(output_pcm);
    tts_data.state = TTS_STATUS_IDLE;

    LOGI("tts init out");
    return;
}

void ai_tts_start(char *src, int len)
{
    LOGI("tts start in");

    if((len >= TTS_STRING_LEN) || (len == 0) || (src == NULL)){
        LOGE("src: %p, len: %d  (0, %d]", src, len, TTS_STRING_LEN);
        return;
    }

    if(!tts_data.state){
        tts_data.str_len = len;
        memset(tts_data.string, 0, TTS_STRING_LEN);
        memcpy(tts_data.string, (char *)src, tts_data.str_len);

        LOGI("tts src[%d]: %s", tts_data.str_len, tts_data.string);

        /* new thread */
        int pri = k_thread_priority_get(k_current_get());
        k_thread_create(&tts_thread_data, tts_stack_area,
                        K_THREAD_STACK_SIZEOF(tts_stack_area),
                        tts_thread, NULL, NULL, NULL, pri, 0, K_NO_WAIT);

        tts_data.state = TTS_STATUS_START;
    }else{
        LOGE("tts already start");
    }

    LOGI("tts start out");
    return;
}

void ai_tts_copy(void *dest, int *dest_len, int *notify)
{
    LOGD("tts copy in");
    int bytes_read = 0;
    
    if(tts_data.state != TTS_STATUS_IDLE){
        if (k_pipe_read_avail(&(tts_data.pipe)) >= TTS_OUTPUT_STEP) { /* pipe have pcm data */
            if(k_pipe_get(&(tts_data.pipe), dest, TTS_OUTPUT_STEP, &bytes_read, TTS_OUTPUT_STEP, K_NO_WAIT)){
                LOGE("read the tts pipe failed");
                *dest_len = 0;
                return;
            }

            *dest_len = bytes_read;
        }else{
            *dest_len = 0;

            /* when tts complete, and pipe don't have any pcm data, which means that there no more pcm data,
             * then send siclence data to flush dac dma if host does not stop tts */
            if(tts_data.state == TTS_STATUS_COMPLETE){
                memset(dest, 0, 1024);
                *dest_len = 1024; 

                /* after TTC_SICLENCE_CNT siclence data, then notify host that tts is done */
                if(tts_data.last_cnt < TTS_SILENCE_CNT){
                    tts_data.last_cnt++;

                    if(tts_data.last_cnt == TTS_SILENCE_CNT){
                        *notify = 1;
                    }
                }
            }
        }
    }else{ /* tts not start, output sclient data to avoid crack voice */
        memset(dest, 0, 1024);
        *dest_len = 1024;
    }

    LOGD("[o]%d", *dest_len);
    return;
}

void ai_tts_deinit(void)
{
    /* stop tts anyway */
    tts_stop_synth();

    /* deinit tts */
    tts_engine_deinit();

    memset(black_board, 0, sizeof(black_board));
    memset(&tts_data, 0, sizeof(tts_data_t));
}