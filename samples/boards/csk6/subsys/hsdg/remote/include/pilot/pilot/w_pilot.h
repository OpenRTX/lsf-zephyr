#ifndef __W_PILOT_H__
#define __W_PILOT_H__

#include "../w_pilot_defines.h"     //int       ��������
#include "../w_pilot_param_module.h"    //PARAM       ����
#include "../w_pilot_err.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int IPLTInit(const pPilotResMgr type_mgr,void *reserved);
int IPLTUninit();
int IPLTGetInstSize();
int IPLTCreateInst(IPLT_INST  *pInst, const void *params);
int IPLTSetParams(IPLT_INST pInst, PILOT_PARAM param, const char* value);
int IPLTGetParams(IPLT_INST pInst, PILOT_PARAM param, char **value,int *vLen);
int IPLTFaceDetect(IPLT_INST pInst, unsigned char *data, pixel_format fmt, int width, int height, face_detect **detect_t, int *face_count);
int IPLTFaceAlign(IPLT_INST pInst, unsigned char *data, pixel_format fmt, int width, int height, face_detect *detect_t, ifr_align_type align_type, ifr_point_f* facial_points_array);
int IPLTFaceGetFea(IPLT_INST pInst, unsigned char *data, pixel_format fmt, int width, int height, face_detect *detect_t, float **feature, int *fea_len);
int IPLTFaceCompFea(IPLT_INST pInst, float *feature1, float *feature2, float *score);
int IPLTHeadShoulderDetect(IPLT_INST pInst, unsigned char *data, pixel_format fmt, int width, int height, head_shoulder_detect **detect_t, int *head_shoulder_count);
int IPLTHeadShoulderTrack(IPLT_INST pInst, unsigned char*data, pixel_format fmt, int width, int height, head_shoulder_detect**track_t, int *track_count,int shift);
int IPLTGestureRecognition(IPLT_INST pInst, unsigned char *data, pixel_format fmt, int width, int height, head_shoulder_detect *detect_t);
int IPLTFaceRegister(IPLT_INST pInst, unsigned char *data, pixel_format fmt, int width, int height, register_result **result, int *result_count);
int IPLTFaceRecognition(IPLT_INST pInst, unsigned char *data, pixel_format fmt, int width, int height, float*feature, recognition_result**result, int *result_count,int shift);
int IPLTDestroyInst(IPLT_INST pInst);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //__W_IVW_H__
