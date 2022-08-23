#include <pilot_app.h>
#include <w_pilot_err.h>
#include <stdlib.h>
#include "luna.h"
#include "pilot/w_pilot.h"

#define TEST_HEAD_DETECT (1)
// #define TEST_FACE_DETECT (1)
//#define TEST_FACE_ALIGN  (1)
//#define TEST_FACE_VERIFY (1)
#define TEST_GESTURE     (1)


#define CNN_MLP_RES_FLASH_SHOULDER_ADDR				(void *)(0x68000000)//(void *)(0X80400000)
#define CNN_MLP_RES_FLASH_DETECT_ADDR				(void *)(0x68100000)//(void *)(0X80200000)
#define CNN_MLP_RES_FLASH_ALIGN_ADDR				(void *)(0x68200000)//(void *)(0X80100000)
#define CNN_MLP_RES_FLASH_VERIFY_ADDR				(void *)(0x68300000)//(void *)(0X80600000)
#define CNN_MLP_RES_FLASH_GESTURE_ADDR				(void *)(0x68550000)//(void *)(0X80300000)

//thinker res size
#define HEAD_SHOULDER_SIZE (1 * 670 * 1024)

//inst size
#define PSRAM_INST_SIZE (1*1024*1024)
#define SHARE_MEME_SIZE (640*1024)
static char g_psram_inst_buffer[PSRAM_INST_SIZE];

static ePilotResMgr res_type_mgr;

static char *g_share_meme_buffer = (char *)LUNA_SHARE_MEM_BASE;

#ifdef TEST_FACE_DETECT
static char gDetectResBuffer[1 * 380 * 1024];
#include"./res/face_detect_luna.h"
#endif

#ifdef TEST_HEAD_DETECT
#include"./res/castor_v1.6_luna_thinker.h"
static char gHeadShoulderBuffer[HEAD_SHOULDER_SIZE];
#endif

#ifdef TEST_FACE_VERIFY
#include"./res/face_verify_luna.h"
static char gVerifyResBuffer[2 * 1024 * 1024 + 1 * 150 * 1024];
#endif

#ifdef TEST_FACE_ALIGN
#include"./res/face_align_luna.h"
static char gAlignResBuffer[1*380*1024];
#endif

#ifdef TEST_GESTURE
#include"./res/mb_tiny_hand_0524_128_16bias_luna.h"
static char gGestureResBuffer[1*1024*1024+1*230*1024];
#endif

#include "./res/input_640x480.h"

int SET_PARMA_ENGINE(IPLT_INST ifr_inst)
{
	int inst_size;
	int ret_error = 0;
	float getValue = 0;
	char *value = NULL;
	int valueLen = 0;

	ret_error = IPLTSetParams(ifr_inst, PARAM_DETECT_THRES, "0.73");
	ret_error = IPLTSetParams(ifr_inst, PARAM_HEAD_SHOULDER_DETECT_THRES, "0.25");
	ret_error = IPLTSetParams(ifr_inst, PARAM_HEAD_SHOULDER_DETECT_LOSS_CNT, "6");
	ret_error = IPLTSetParams(ifr_inst, PARAM_HEAD_SHOULDER_DETECT_IOU, "0.2");
	ret_error = IPLTSetParams(ifr_inst, PARAM_HEAD_SHOULDER_DETECT_PIXESIZE, "15");
	ret_error = IPLTSetParams(ifr_inst, PARAM_HEAD_SHOULDER_DETECT_IOA, "0.75");
	ret_error = IPLTSetParams(ifr_inst, PARAM_HEAD_SHOULDER_DETECT_PROTHRES, "0.2");
	ret_error = IPLTSetParams(ifr_inst, PARAM_HEAD_SHOULDER_DETECT_NMSTHRS, "0.6");

	ret_error = IPLTSetParams(ifr_inst, PARAM_GESTURE_YES_THRES, "0.15");
	ret_error = IPLTSetParams(ifr_inst, PARAM_GESTURE_OK_THRES, "0.15");
	ret_error = IPLTSetParams(ifr_inst, PARAM_GESTURE_STOP_THRES, "0.15");
	ret_error = IPLTSetParams(ifr_inst, PARAM_GESTURE_LIKE_THRES, "0.15");

	ret_error = IPLTGetParams(ifr_inst, PARAM_VERIFY_RESVERSION, &value, &valueLen);
	if (ret_error != IPILOT_SUCCESS)
	{
		printf("PARAM_VERIFY_RESVERSION | erroe code = %d\n", ret_error);
	}
	else
	{
		printf("PARAM_VERIFY_RESVERSION = %s\n", value);
	}
	
	ret_error = IPLTGetParams(ifr_inst, PARAM_HEAD_SHOULDER_DETECT_RESVERSION, &value, &valueLen);
	if (ret_error != IPILOT_SUCCESS)
	{
		printf("PARAM_HEAD_SHOULDER_DETECT_RESVERSION | erroe code = %d\n", ret_error);
	}else
	{
		printf("PARAM_HEAD_SHOULDER_DETECT_RESVERSION = %s\n", value);
	}
	
	ret_error = IPLTGetParams(ifr_inst, PARAM_GESTURE_RESVERSION, &value, &valueLen);
	if (ret_error != IPILOT_SUCCESS)
	{
		printf("PARAM_GESTURE_RESVERSION | erroe code = %d\n", ret_error);
	}
	else
	{
		printf("PARAM_GESTURE_RESVERSION = %s\n", value);
	}
	
	return IPILOT_SUCCESS;
}

int UINIT_ENGINE(IPLT_INST ifr_inst)
{
	int ret_error = IPILOT_SUCCESS;
	ret_error = IPLTDestroyInst(ifr_inst);
	if (IPILOT_SUCCESS != ret_error) {
		printf("IPLTDestroyInst Failed | Error Code = %d\r\n", ret_error);
		return ret_error;
	}
	ret_error = IPLTUninit();
	if (IPILOT_SUCCESS != ret_error) {
		printf("IPLTUninit Failed | Error Code = %d\r\n", ret_error);
		return ret_error;
	}

	return IPILOT_SUCCESS;
}

int INIT_ENGINE(IPLT_INST *ifr_inst)
{
	int inst_size;
	int ret_error = 0;
	float getValue = 0;
	char *value = NULL;
	int valueLen = 0;


#ifdef TEST_HEAD_DETECT
	memcpy(gHeadShoulderBuffer, CNN_MLP_RES_FLASH_SHOULDER_ADDR, sizeof(gHeadShoulderBuffer));
#endif

#ifdef TEST_FACE_DETECT
	memcpy(gDetectResBuffer, CNN_MLP_RES_FLASH_DETECT_ADDR, sizeof(gDetectResBuffer));
#endif

#ifdef TEST_FACE_ALIGN
	memcpy(gAlignResBuffer, CNN_MLP_RES_FLASH_ALIGN_ADDR, sizeof(gAlignResBuffer));
#endif

#ifdef TEST_FACE_VERIFY
	memcpy(gVerifyResBuffer, CNN_MLP_RES_FLASH_VERIFY_ADDR, sizeof(gVerifyResBuffer));
#endif

#ifdef TEST_GESTURE
	memcpy(gGestureResBuffer, CNN_MLP_RES_FLASH_GESTURE_ADDR, sizeof(gGestureResBuffer));
#endif


#ifdef TEST_FACE_DETECT
	res_type_mgr.res_type_arr_[0].res_buffer_type = RES_FACE_DETECT;
	res_type_mgr.res_type_arr_[0].res_buffer_addr = gDetectResBuffer;
	res_type_mgr.res_type_arr_[0].res_buffer_addr = face_detect_luna_resource;
#endif

#ifdef TEST_FACE_VERIFY
	res_type_mgr.res_type_arr_[1].res_buffer_type = RES_FACE_VERIFY;
	res_type_mgr.res_type_arr_[1].res_buffer_addr = gVerifyResBuffer;
	res_type_mgr.res_type_arr_[1].res_buffer_addr = face_verify_luna_resource;
#endif

#ifdef TEST_HEAD_DETECT
	res_type_mgr.res_type_arr_[2].res_buffer_type = RES_HEAD_SHOULDER;
	res_type_mgr.res_type_arr_[2].res_buffer_addr = gHeadShoulderBuffer;
	res_type_mgr.res_type_arr_[2].res_buffer_addr = castor_luna_thinker_resource;
	res_type_mgr.res_type_arr_[2].res_buffer_size = HEAD_SHOULDER_SIZE;
#endif

#ifdef TEST_FACE_ALIGN
	res_type_mgr.res_type_arr_[3].res_buffer_type = RES_FACE_ALIGN;
	res_type_mgr.res_type_arr_[3].res_buffer_addr = gAlignResBuffer;
	res_type_mgr.res_type_arr_[3].res_buffer_addr = face_align_luna_resource;
#endif

#ifdef TEST_GESTURE
	res_type_mgr.res_type_arr_[4].res_buffer_type = RES_GESTURE_RECOGNITION;
	res_type_mgr.res_type_arr_[4].res_buffer_addr = gGestureResBuffer;
	res_type_mgr.res_type_arr_[4].res_buffer_addr = mb_tiny_hand_0524_128_16bias_luna_resource;
#endif

	//1.Init 
	ret_error = IPLTInit(&res_type_mgr, NULL);
	if (IPILOT_SUCCESS != ret_error) {
		printf("pInit Failed | Error Code = %d\n", ret_error);
		return ret_error;
	}
	//2.getInstSize
	inst_size = IPLTGetInstSize();
	if (IPILOT_SUCCESS != ret_error) {
		printf("pGetInstSize Failed | Error Code = %d\n", ret_error);
		return ret_error;
	}
	printf("inst_size: %d \n\r", inst_size);
#if 0
	ret_error = IPLTCreateInst(ifr_inst, NULL);
#else
	PILOT_INST_PARAM inst_param = {0};
	memset(&inst_param,0,sizeof(PILOT_INST_PARAM));
	inst_param.psram_addr  = (void*)g_psram_inst_buffer;
	inst_param.psram_total = PSRAM_INST_SIZE;
	inst_param.ram_addr    = (void*)g_share_meme_buffer;
	inst_param.ram_total   = SHARE_MEME_SIZE;

	ret_error = IPLTCreateInst(ifr_inst, &inst_param);
#endif
	if (IPILOT_SUCCESS != ret_error) {
		printf("pCreateInst Failed | Error Code = %d\n", ret_error);
		return ret_error;
	}

	return IPILOT_SUCCESS;
}

int CSK_ATOMIC_TEST(IPLT_INST ifr_inst)
{
	float *feature = NULL;
	int feature_len = 0;
	int ret_error = 0;
	face_detect *face_detct_t = NULL;
	int face_count = 0;
	head_shoulder_detect *head_detct_t = NULL;
	int head_shoulder_count = 0;

	face_detect f_detect_temp;

	while (1)
	{
		//do face detect
#ifdef TEST_FACE_DETECT
		ret_error = IPLTFaceDetect(ifr_inst, head_640x480, PIX_FMT_BGR888, 640, 480, &face_detct_t, &face_count);
		if (IPILOT_SUCCESS != ret_error) {
			printf("pDetect Failed | Error Code = %d\n", ret_error);
			return ret_error;
		}
		for (int fi = 0; fi<face_count; fi++)
		{
			LOG_INF("face count = %d\t x = %d\t y=%d\t w=%d\t h=%d\t score=%f\r\n",
				face_count, face_detct_t[fi].rect.x, face_detct_t[fi].rect.y, 
				face_detct_t[fi].rect.w, face_detct_t[fi].rect.h, face_detct_t[fi].score);
		}
#endif

#ifdef TEST_FACE_ALIGN
		//266	 y=48	 w=104	 h=161
		f_detect_temp.rect.x = 266;
		f_detect_temp.rect.y = 48;
		f_detect_temp.rect.w = 104;
		f_detect_temp.rect.h = 161;

		face_detct_t = &f_detect_temp;

		ret_error = IPLTFaceAlign(ifr_inst, head_640x480, PIX_FMT_BGR888, 640, 480, 
			&face_detct_t[0], FACE_ALIGN_68P, face_detct_t->p_align_point);
		if (IPILOT_SUCCESS != ret_error) {
			printf("pDetect Failed | Error Code = %d\r\n", ret_error);
			return ret_error;
		}
		for (int i=0;i<68;i++)
		{
			printf("x=%f,y=%f\n", face_detct_t[0].p_align_point[i].x, face_detct_t[0].p_align_point[i].y);
		}
#endif

#ifdef TEST_HEAD_DETECT
		ret_error = IPLTHeadShoulderDetect(ifr_inst, head_640x480, PIX_FMT_BGR888, 640, 480, &head_detct_t, &head_shoulder_count);
		if (IPILOT_SUCCESS != ret_error) {
			printf("pDetect Failed | Error Code = %d\r\n", ret_error);
			return ret_error;
		}
		for (int fi = 0; fi<head_shoulder_count; fi++)
		{
			printf("head count = %d\t x = %d\t y=%d\t w=%d\t h=%d\t score=%f\r\n", 
				head_shoulder_count, head_detct_t[fi].rect.x, head_detct_t[fi].rect.y,
				head_detct_t[fi].rect.w, head_detct_t[fi].rect.h, head_detct_t[fi].score);
		}
#endif

#ifdef TEST_FACE_VERIFY
		//do get featture
		//266	 y=48	 w=104	 h=161
		f_detect_temp.rect.x = 266;
		f_detect_temp.rect.y = 48;
		f_detect_temp.rect.w = 104;
		f_detect_temp.rect.h = 161;
		f_detect_temp.n_align_point = 68;
		for (int i=0;i<68;i++)
		{
			f_detect_temp.p_align_point[i].x = keypoints[i*2+0];
			f_detect_temp.p_align_point[i].y = keypoints[i * 2 + 1];
		}
		face_detct_t = &f_detect_temp;

		ret_error = IPLTFaceGetFea(ifr_inst, head_640x480, PIX_FMT_BGR888, 640, 480, &face_detct_t[0], &feature, &feature_len);
		if (IPILOT_SUCCESS != ret_error)
		{
			printf("pVerify Failed | Error Code = %d\r\n", ret_error);
			return ret_error;
		}
		printf("[0]=%f\t[192]=%f\t[383]=%f\n", feature[0], feature[192], feature[383]);
#endif

		break;
	}

	return 0;
}

int CSK_GESTURE_TEST_CONSTANCE(IPLT_INST hIPLTTInst)
{
	head_shoulder_detect *head_detct_t = NULL;
	int head_shoulder_count;
	int ret_error = IPLTHeadShoulderDetect(hIPLTTInst, head_640x480, PIX_FMT_BGR888, 640, 480, &head_detct_t, &head_shoulder_count);
	if (IPILOT_SUCCESS != ret_error) {
		printf("pDetect Failed | Error Code = %d\n", ret_error);
		return ret_error;
	}
#ifdef TEST_GESTURE
	//gesture status
	for (int fi = 0; fi < head_shoulder_count; fi++)
	{
		printf("head count = %d\t x = %d\t y=%d\t w=%d\t h=%d\t score=%f\n",
				head_shoulder_count, head_detct_t[fi].rect.x, head_detct_t[fi].rect.y,
				head_detct_t[fi].rect.w, head_detct_t[fi].rect.h, head_detct_t[fi].score);

		ret_error = IPLTGestureRecognition(hIPLTTInst, head_640x480, PIX_FMT_BGR888, 640, 480, &head_detct_t[fi]);
		if (IPILOT_SUCCESS != ret_error)
		{
			printf("IPLTGestureRecognition Failed | Error Code = %d\n", ret_error);
			return ret_error;
		}
		printf("gesture status = %d,gesture_scores=%f\n",
			head_detct_t[fi].gesture_state, 
			head_detct_t[fi].gesture_scores[head_detct_t[fi].gesture_state]);
	}
#endif

	return 0;
}

int CSK_test_function(IPLT_INST ifr_inst)
{
	// CSK_ATOMIC_TEST(ifr_inst);
	CSK_GESTURE_TEST_CONSTANCE(ifr_inst);
	CSK_EFFECTIVE(ifr_inst);
	return 0;
}