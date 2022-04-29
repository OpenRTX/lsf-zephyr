#define ALGO_RES_FIX_RAM
#include <zephyr.h>
#include <sys/printk.h>
#include <stdio.h>

#include "ai.h"
#include "algo_lib..h"

#ifdef ALGO_RES_FIX_RAM
#include "res_cae.h" 
#include "res_luna.h"
#include "main_keywords.h"
#include "filler_keywords.h"
#endif

#define CLOG(fmt, ...)   printk(fmt"\n", ##__VA_ARGS__)

#define CLOG_LEVEL_NONE     0
#define CLOG_LEVEL_ERROR    1
#define CLOG_LEVEL_WARN     2
#define CLOG_LEVEL_INFO     3
#define CLOG_LEVEL_DEBUG    4
#define CLOG_LEVEL_VERBOSE  5

#ifndef CLOG_LEVEL
#define CLOG_LEVEL   CLOG_LEVEL_DEBUG
#endif

#define CLOGE(fmt, ...)     do {if (CLOG_LEVEL >= CLOG_LEVEL_ERROR) CLOG(fmt,##__VA_ARGS__);} while(0)
#define CLOGW(fmt, ...)     do {if (CLOG_LEVEL >= CLOG_LEVEL_WARN) CLOG(fmt,##__VA_ARGS__);} while(0)
#define CLOGI(fmt, ...)     do {if (CLOG_LEVEL >= CLOG_LEVEL_INFO) CLOG(fmt,##__VA_ARGS__);} while(0)
#define CLOGD(fmt, ...)     do {if (CLOG_LEVEL >= CLOG_LEVEL_DEBUG) CLOG(fmt,##__VA_ARGS__);} while(0)
#define CLOGV(fmt, ...)     do {if (CLOG_LEVEL >= CLOG_LEVEL_VERBOSE) CLOG(fmt,##__VA_ARGS__);} while(0)

#define PSRAM_POOL_SIZE (2500*1024)
#define FCC_SHARED_ADDR (0x5FE00000)
#define FCC_SHARED_SIZE (655360)
#define RES_SIZE_ESR_MLP  (1369088)
#define RES_SIZE_ESR_MAIN (48240)
#define RES_SIZE_ESR_KEY  (59248)
#define RES_SIZE_CAE_MLP  (1655332)  

static ai_inst_t wrap = NULL;
static algo_wake_cb wakeup = NULL;
static char psram_pool[PSRAM_POOL_SIZE] __attribute__((aligned(32)));

int algo_set_mode(uint8_t wakeup)
{
	char conf[100] = { 0 };
	int len = snprintf(conf, sizeof(conf),
		"{"
		"\"cae\":["
		"{\"key\":\"mode\",\"val\":%d}"  // [0]CAE_WORK_MODE_WAKE; [1]CAE_WORK_MODE_REC
		"],"
		"\"esr\":["
		"{\"key\":\"mode\",\"val\":%d},"  // [0]ASR_MODE; [1]WAKEUP_MODE
		"{\"key\":\"model\",\"val\":%d}"  // [0]MAIN; [1]ASR
		"]"
		"}",
		wakeup, !wakeup, wakeup);

	ai_set_param(wrap, conf);
	return 0;
}

static long algo_msg_proc(ai_msg_e msg, int larg, void *parg, void *user)
{
	switch (msg) {
	case ai_msg_wakeup:
        if(wakeup) wakeup(larg, parg);
        CLOGD("[wrap]wakeup: chn=%d; result=%s", larg, (char *)parg);
		return 0;
	case ai_msg_prewak:
		CLOGD("[wrap]prewak: chn=%d; result=%s", larg, (char *)parg);
		return 0;
	case ai_msg_error:
		CLOGE("[wrap]error: code=%d; msg=%s", larg, (char *)parg);
		return 0;
	case ai_msg_trace:
		CLOGD("[wrap]trace: %s", (char *)parg);
		return 0;
	default:
		return -1;
	}
}

int algo_create(algo_wake_cb cbwak)
{
#ifdef ALGO_RES_FIX_RAM
	ai_mem_t memap = {
		.psram = { psram_pool, sizeof(psram_pool) },
		.share = { (char *)FCC_SHARED_ADDR, FCC_SHARED_SIZE },
		.resmap = {
			.cae_mlp = { res_cae_resource, RES_SIZE_CAE_MLP },
			.esr_mlp = { res_luna_resource, RES_SIZE_ESR_MLP },
			.esr_main = { main_keywords_resource, RES_SIZE_ESR_MAIN },
			.esr_cmds = { filler_keywords_resource, RES_SIZE_ESR_KEY },
		}
	};
	char *conf ="{\"wrap\":{\"dev\":\"4002\",\"adc\":0,\"trans\":1,\"param\":[{\"key\":\"wakesoffset\",\"val\":160},{\"key\":\"wakeeoffset\",\"val\":160}]},\"cae\":{\"dist\":35,\"bits\":16,\"chns\":{\"mic\":2,\"ref\":2,\"out\":4},\"wkmask\":3,\"param\":[{\"key\":\"mode\",\"val\":\"0\"},{\"key\":\"textdep\",\"val\":1}]},\"esr\":{\"param\":[{\"key\":\"prewstat\",\"val\":5},{\"key\":\"prewthr\",\"val\":600},{\"key\":\"mode\",\"val\":1},{\"key\":\"model\",\"val\":0}]}}";
	int ret = ai_create(&wrap, &memap, conf, algo_msg_proc, NULL);
#else
	algo_res_t * algo_res_p = (algo_res_t *)res;
	ai_mem_t memap = {
		.psram = { psram_pool, sizeof(psram_pool) },
		.share = { (char *)FCC_SHARED_ADDR, FCC_SHARED_SIZE },
		.resmap = {
			.cae_mlp = {NULL, 0}, 
			.esr_mlp = {NULL, 0}, 
			.esr_main = {NULL, 0}, 
			.esr_cmds = {NULL, 0}, 
		}
	};

	LOG_DBG("conf：%s", algo_res_p->conf);

	if (!algo_res_p)
		return ai_mem_inv;

	memcpy(&memap.resmap.cae_mlp, &algo_res_p->cae_mlp, sizeof(res_addr_t));
	memcpy(&memap.resmap.esr_mlp, &algo_res_p->esr_mlp, sizeof(res_addr_t));
	memcpy(&memap.resmap.esr_main, &algo_res_p->esr_main, sizeof(res_addr_t));
	memcpy(&memap.resmap.esr_cmds, &algo_res_p->esr_cmds, sizeof(res_addr_t));

	int ret = ai_create(&wrap, &memap, algo_res_p->conf, algo_msg_proc, NULL);
#endif

	if(ret != ai_ok){
        CLOGE("ai_create fail! ret is 0x%x", ret);
    }

	CLOGI("memory used: psram=%#lx(%0.3fKB), share=%#lx(%0.3fKB)"
		, memap.psram.size, memap.psram.size / 1024.0f
		, memap.share.size, memap.share.size / 1024.0f);

	// algo_set_mode(0);

    wakeup = cbwak;
	return ret;
}

int algo_step_proc(void *idata, void **odata)
{
	ai_err_e ret = ai_process(wrap, idata, odata);
	//ASSERT(ai_wrap_more == ret || ai_wrap_none == ret || ai_wrap_norm == ret, "aiproc(%d)", ret);

	return ret;
}
