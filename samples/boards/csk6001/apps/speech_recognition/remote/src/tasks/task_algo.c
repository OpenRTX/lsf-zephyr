
#include <zephyr.h>
#include <sys/printk.h>
#include <string.h>

#include "ai.h"
#include "algo_lib..h"
#include "task_algo.h"

#include "cJSON.h"
#include "cJSON_Utils.h"

#include "xiaoyouxiaoyou.h"

#define ALGO_BLK_PER_SAMPS	(256)
#define CAE_BLK_CNT			(10)

/**
 * @brief 断言函数
 */
#define ASSERT(exp, fmt, ...)                                                                \
	do {                                                                                     \
		if (!(exp)) {                                                                        \
			printk("assert@%s:%d:%s: \n" fmt, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
		}                                                                                    \
	} while (0)

static cae_mic_ref_t  algo_inp[ALGO_BLK_PER_SAMPS]__attribute__((aligned(4)));
static cae_out_t  algo_cache[CAE_BLK_CNT]__attribute__((aligned(4)));
static algo_ctx_t algo = {0};

static uint32_t mic_len = 0;
static uint32_t mic_blk = 0;


static void
algo_wakeup_proc(const int chn, const char *json)
{
	cJSON *root = NULL;
	if ((root = cJSON_Parse(json))) {

		cJSON *item = cJSON_GetObjectItem(root, "rlt")->child;

		printk("WAKEUP:[KEY]:%s, [NCM]:%d, [Kid]: %d \n", 
					cJSON_GetObjectItem(item, "keyword")->valuestring,
					cJSON_GetObjectItem(item, "ncm")->valueint,
					cJSON_GetObjectItem(item, "iresid")->valueint);

		cJSON_Delete(root);

		// algo.wake = chn;

		// printk("WAKEUP@%d@%s", chn, json);

	} else {
		printk("JSON:\"%s\" \n", json);
	}
	printk("12345 \n");
}

void algo_audio_outs(ori_out_t* ori_audio, cae_out_t* algo_out)
{
#if 0
	void *recording_frame;
	int32_t ret;
	uint16_t cae_buf[STREAM_FRAME_SIZE * 2] = {0};
	uint16_t frame = 0;
	uint32_t frame_len =  STREAM_FRAME_SIZE * sizeof(int);
	for(int blk = 0; blk < CAE_BLK_CNT; blk++) {
		for (int samp = 0; samp < ALGO_BLK_PER_SAMPS; samp++) {

			cae_buf[frame++] = (int)algo_out[blk][samp].ivw1;
//			cae_buf[frame++] = samp;
			
			if (STREAM_FRAME_SIZE * 2 == frame) {
				uint8_t *audioBlock = ((uint8_t *)cae_buf);
				frame = 0;
				// 能写就写, 满了不能写就忽略, 会造成上位机录音丢帧
				ret = AudioStream_Producer_AcquireFrame(recording_stream, & recording_frame);
				if (0 == ret) {
					// 成功获取空frame, 录音数据填入中转流, recording_stream
					// TODO: 暂时填入回声, 方便测试
					memcpy(recording_frame, audioBlock, frame_len);

					ret = AudioStream_Producer_ReleaseFrame(recording_stream, recording_frame);
					APP_ASSERT(ret == 0, "err");
				}
				else {
					// 满了不能写, 记录错误
					//error_recording_too_fast += 1;
				}
			}
		}	
	}
#else

#endif
}

int algo_run (void)
{
	void *algo_out = NULL;
	ai_err_e ret = ai_wrap_none;

	static uint8_t flag = 0;
	static cae_mic_t *cae_mic = (cae_mic_t *)pcm_in;
	static uint32_t pos = 0;

	if(flag == 0){
		// 测试音频
		mic_len = sizeof(pcm_in) / sizeof(uint16_t);
		mic_blk = mic_len / ALGO_BLK_PER_SAMPS / CAE_BLK_CNT;
		//printk("mic_len:%d, mic_blk:%d", mic_len, mic_blk);
		flag = 1;
	}

	for (int blk = 0; blk < CAE_BLK_CNT; blk++) {
		for (int i = 0; i < ALGO_BLK_PER_SAMPS; i++){
			algo_inp[i].mic1 = cae_mic->mic1;
			algo_inp[i].mic2 = (cae_mic++)->mic2;
			algo_inp[i].ref1 = 0;
			algo_inp[i].ref2 = 0;
		}

		ret = algo_step_proc(algo_inp, &algo_out);
	}

	if(++pos == mic_blk) {
		pos = 0;
		cae_mic = (cae_mic_t *)pcm_in;
	}

	if (algo.cache)
	{
		/*输出缓存的数据*/
		algo_audio_outs(NULL, algo_cache);

		if (ai_wrap_norm == ret)
			memcpy(algo_cache, algo_out, sizeof(algo_cache));
		else
			algo.cache = false;
	} else {

		//输出算法数据
		algo_audio_outs(NULL, algo_out);
		if (ai_wrap_more == ret)
		{
			ret = algo_step_proc(NULL, &algo_out);
			memcpy(algo_cache, algo_out, sizeof(algo_cache));
			algo.cache = true;
		}
	}

	return 0;

}

void algo_result(void)
{
    return;
}

static void
cjson_hook_set(void)
{
	cJSON_Hooks hook = {
			.malloc_fn = k_malloc,
			.free_fn = k_free,
	};
	cJSON_InitHooks(&hook);
}

static void
algo_get_info(void)
{
	// get engine information
	int len = 1024;
	char *info = (char *)k_malloc(len);
	ASSERT(info, "malloc");
	ai_err_e ret = ai_get_info(info, &len);
	ASSERT(ai_ok == ret, "getinfo(%d)", ret);

	cJSON *root = cJSON_Parse(info);
	ASSERT(root, "json:\"%s\"", info);
	char *const json = cJSON_PrintUnformatted(root);
	printk("json: %s\n", json);

	k_free(json);
	k_msleep(100);  // ensure print done
	cJSON_Delete(root);
	k_free(info);
}


void algo_thread(void *v1, void *v2, void *v3)
{

	cjson_hook_set();
	algo_get_info();
	algo_create(algo_wakeup_proc);

	while(true){
		// audio in

		// algothrim run
		algo_run();

		// algothrim result
		algo_result();
	}

}