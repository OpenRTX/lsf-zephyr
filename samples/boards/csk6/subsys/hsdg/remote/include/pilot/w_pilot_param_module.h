#pragma once

#define MAX_FACE_RES_NUMS (10)
#define MAX_FACE_RES_TYPE (10)

#define MAX_FEATURE_DIMS  (384)

typedef void *IPLT_INST;

typedef enum
{
	PARAM_PILOT_NONE					= 0,

	//FACE DETECT
	PARAM_PILOT_FACE_DETECT_BEGIN       = 3000,
	PARAM_DETECT_THRES                  = 3001,
	PARAM_DETECT_PROTHRES				= 3002,
	PARAM_DETECT_NMSTHRES				= 3003,
	PARAM_DETECT_PIXESIZE				= 3004,
	PARAM_DETECT_RESVERSION				= 3005,
	PARAM_PILOT_FACE_DETECT_END,

	//FACE VERIFY
	PARAM_PILOT_FACE_VERIFY_BEGIN       = 4000,
	PARAM_VERIFY_THRES                  = 4001,
	PARAM_VERIFY_REGU_A					= 4002,
	PARAM_VERIFY_REGU_B					= 4003,
	PARAM_VERIFY_RESVERSION             = 4004,
	PARAM_PILOT_FACE_VERIFY_END,

	//HEAD SHOULDER
	PARAM_PILOT_HEAD_SHOULDER_DETECT_BEGIN	= 5000,
	PARAM_HEAD_SHOULDER_DETECT_THRES	= 5001,
	PARAM_HEAD_SHOULDER_DETECT_LOSS_CNT = 5002,
	PARAM_HEAD_SHOULDER_DETECT_IOU		= 5003,
	PARAM_HEAD_SHOULDER_DETECT_PIXESIZE = 5004,
	PARAM_HEAD_SHOULDER_DETECT_PROTHRES = 5005,
	PARAM_HEAD_SHOULDER_DETECT_NMSTHRS  = 5006,
	PARAM_HEAD_SHOULDER_DETECT_MODEL    = 5007,
	PARAM_HEAD_SHOULDER_DETECT_IOA		= 5008,
	PARAM_HEAD_SHOULDER_DETECT_IOB		= 5009,
	PARAM_HEAD_SHOULDER_DETECT_CIOU		= 5010,
	PARAM_HEAD_SHOULDER_DETECT_TIMEOUT	= 5011,
	PARAM_HEAD_SHOULDER_DETECT_RESVERSION=5012,
	PARAM_PILOT_HEAD_SHOULDER_DETECT_END,

	//FACE ALIGN
	PARAM_PILOT_FACE_ALIGN_BEGIN 		= 6000,
	PARAM_ALIGN_RESVERSION              = 6001,
	PARAM_PILOT_FACE_ALIGN_END,

	//GESTURE
	PARAM_PILOT_GESTURE_BEGIN           = 7000,
	PARAM_GESTURE_YES_THRES             = 7001,
	PARAM_GESTURE_OK_THRES              = 7002,
	PARAM_GESTURE_STOP_THRES            = 7003,
	PARAM_GESTURE_LIKE_THRES            = 7004,
	PARAM_GESTURE_SIX_THRES             = 7005,
	PARAM_GESTURE_OTHER_THRES           = 7006,
	PARAM_GESTURE_RESVERSION            = 7007,
	PARAM_PILOT_GESTURE_END,
}PILOT_PARAM;

typedef enum
{
	PIX_FMT_GRAY8 = 1,
	PIX_FMT_BGR888 = 3,
	PIX_FMT_BGRA8888 = 4
}pixel_format;

typedef enum
{
	FACE_ALIGN_5P                       = 5,
	FACE_ALIGN_7P                       = 7,
	FACE_ALIGN_21P                      = 21,
	FACE_ALIGN_68P						= 68,
}ifr_align_type;

typedef enum
{
	RES_FACE_DETECT                     = 1,
	RES_FACE_QA                         = 2,
	RES_FACE_ALIVE                      = 3,
	RES_FACE_VERIFY                     = 4,
	RES_FACE_ALIGN						= 5,
	RES_HEAD_SHOULDER					= 6,
	RES_GESTURE_RECOGNITION				= 7,
}ePilotResType;

typedef enum
{
	GESTURE_OTHER = 0,
	GESTURE_LIKE = 1,
	GESTURE_OK = 2,
	GESTURE_STOP = 3,
	GESTURE_YES = 4,
	GESTURE_SIX = 5,
	GESTRUE_COUNT,
}GESTURE_STAT;

typedef struct _PILOT_INST_PARAM
{
	void *ram_addr;
	void *psram_addr;

	unsigned int ram_total;
	unsigned int ram_used;
	unsigned int psram_total;
	unsigned int psram_used;
}PILOT_INST_PARAM, *P_PILOT_INST_PARAM;

typedef struct PilotRes
{
	ePilotResType res_buffer_type;
	char *res_buffer_addr;
	int  res_buffer_size;
}ePilotRes;

typedef struct PilotResMgr
{
	ePilotRes res_type_arr_[MAX_FACE_RES_TYPE];
}ePilotResMgr, *pPilotResMgr;

typedef struct
{
	int x;
	int y;
	int w;
	int h;
}ifr_rect;

typedef struct
{
	float x;
	float y;
}ifr_point_f;

typedef struct
{
	ifr_rect    rect;
	float       score;
	ifr_point_f p_align_point[68];
	int         n_align_point;
	int			id;
}face_detect;

typedef struct
{
	ifr_rect    rect;
	float       score;
	float		iou_score;
	int         detect_loss_cnt;
	int			id;
	int			loss_cnt_thres;
	int			update;
	GESTURE_STAT gesture_state;
	float        gesture_scores[GESTRUE_COUNT];
	int          gesture_status[GESTRUE_COUNT];
}head_shoulder_detect;

typedef struct
{
	ifr_rect    face_rect;
	float       feature[MAX_FEATURE_DIMS];
	ifr_point_f p_align_point[68];
	int n_align_point;
	int			feature_dim;
	float		score;
}register_result;

typedef struct
{
	ifr_rect   head_shoulder_rect;	
	ifr_rect   face_rect;			
	float      compare_score;		
	float      iou_score;			
	float      head_shoulder_score; 
	int        id;					
	int        face_valid;
	float      feature[MAX_FEATURE_DIMS];
}recognition_result;
