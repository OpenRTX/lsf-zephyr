#ifndef _IFR_TYPES_H_
#define _IFR_TYPES_H_

#include <stdint.h>
#include "ifr_defines.h"

typedef enum
{
	IFR_PIX_FMT_UNKNOW = 0,
	IFR_PIX_FMT_GRAY8 = 1,
	IFR_PIX_FMT_BGR888 = 3,
	IFR_PIX_FMT_BGRA8888 = 4,
	IFR_PIX_FMT_COUNT,
}ifr_pixel_format_t;

typedef enum
{
	IFR_GESTURE_OTHER = 0,
	IFR_GESTURE_LIKE = 1,
	IFR_GESTURE_OK = 2,
	IFR_GESTURE_STOP = 3,
	IFR_GESTURE_YES = 4,
	IFR_GESTURE_SIX = 5,
	IFR_GESTRUE_COUNT,
}ifr_gasture_t;

typedef enum
{
	IFR_ANTI_SPOOFING_OTHER = 0,
	IFR_ANTI_SPOOFING_LIVING = 1,
	IFR_ANTI_SPOOFING_COUNT,
}ifr_anti_spoofing_cls_t;


typedef struct 
{
	int x;
	int y;
	int w;
	int h;
}ifr_rect_t;

typedef struct 
{
	float x;
	float y;
}ifr_point_t;

typedef enum
{
	IFR_MODULE_UNKNOW = 0,
	IFR_MODULE_HEAD_DETECT = 0,
	IFR_MODULE_FACE_DETECT = 1,
	IFR_MODULE_FACE_ALIGN = 2,
	IFR_MODULE_FACE_VERIFY = 3,
	IFR_MODULE_GASTURE_RECOGNIZE = 4,
	IFR_MODULE_ANTI_SPOOFING = 5,
	IFR_MODULE_COUNT,
}ifr_module_t;

typedef struct 
{
	ifr_rect_t	rect;
	float       score;
	float		iou_score; //unuse
}ifr_head_shoulder_detect_result_t;

typedef struct 
{
	ifr_rect_t	rect;
	float       score;
	float		iou_score; //unuse
}ifr_face_detect_result_t;

typedef struct ifr_gasture_recognize_result_t_
{
	ifr_gasture_t gesture_state;
	float         gesture_scores[IFR_GESTRUE_COUNT];
	int           gesture_status[IFR_GESTRUE_COUNT];
}ifr_gasture_recognize_result_t;

typedef struct ifr_face_align_result_t_
{
	ifr_point_t	align_points[MAX_ALIGIN_POINTS];
	int			align_points_size;
	float       head_yaw;
	float       head_pitch;
	float       head_roll;
	int			head_pose_status;
}ifr_face_align_result_t;

typedef struct ifr_anti_spoofing_result_t_
{
	float         anti_spoofing_scores[IFR_ANTI_SPOOFING_COUNT];
	int			  anti_spoofing_status;
}ifr_anti_spoofing_result_t;

typedef struct
{
	uint32_t psram_size;
	uint32_t share_size;
}ifr_face_info_t;

typedef struct 
{
	int8_t* psram_addr;
	uint32_t psram_size;
	uint32_t psram_size_used;
	int8_t* share_addr;
	uint32_t share_size;
	uint32_t share_size_used;
	int8_t* res_data[IFR_MODULE_COUNT];
	uint32_t res_size[IFR_MODULE_COUNT];
}ifr_face_param_t;

typedef struct 
{
	ifr_rect_t	detect_rect;
	float       detect_score;
	float		detect_iou_score;
	float		anti_spoofing_score;
	int			anti_spoofing_status;
	ifr_point_t align_points[MAX_ALIGIN_POINTS];
	int			align_points_size;
	float       head_yaw;
	float       head_pitch;
	float       head_roll;
	int			head_pose_status;
	float		feature[MAX_FEATURE_DIMS];
	int			feature_dim;
}ifr_face_recog_result_t;


#endif 