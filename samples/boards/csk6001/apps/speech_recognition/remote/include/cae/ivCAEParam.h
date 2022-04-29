#ifndef _IV_CAE_PARAM_H_
#define _IV_CAE_PARAM_H_

#ifdef __cplusplus
extern "C" {
#endif
	#include "ivDefine.h"
	typedef enum _CAEParamID
	{
		CAE_COMMON_PARAM_BASE = 0x00000000,
		CAE_PARAM_PRE_WK_INFO,
		CAE_PARAM_WK_INFO,
		CAE_PARAM_WORK_MODE,
		CAE_PARAM_WK_LOW_DELAY_MODE,

		CAE_AEC_PARAM_BASE = 0x00001000,

		CAE_DER_PARAM_BASE = 0x00002000,

		CAE_MAB_PARAM_BASE = 0x00003000,
		CAE_PARAM_BEAM_USE,


		CAE_AGC_PARAM_BASE = 0x00004000,
		CAE_PARAM_AGC_ON,

		CAE_NC_PARAM_BASE = 0x00005000,
		CAE_PARAM_NC_NOISE_RATIO,
		CAE_PARAM_NC_REVERT_RATIO,

		CAE_TD_PARAM_BASE = 0x00006000,
		CAE_PARAM_TD_ON,

	}TCAEParamID;

	/*  set the work mode of CAE */
	typedef enum _CAE_WORK_MODE
	{
		CAE_WORK_MODE_WAKE = 0, /*  only input the local ivw data ,no rec data for cloud */
		CAE_WORK_MODE_REC = 1,  /*  local ivw data  + rec data for cloud (when set ivw info) */
	}TCAE_WORK_MODE;

	typedef enum _CAEStatusID
	{
		CAE_COMMON_STATUS_BASE = 0x00000000,
		CAE_POWER_STATUS,

		CAE_AEC_STATUS_BASE = 0x00001000,
		CAE_REF_STATUS,

		CAE_DER_STATUS_BASE = 0x00002000,

		CAE_MAB_STAUS_BASE = 0x00003000,
		CAE_BEAM_INFO_STATUS,

		CAE_TDOA_STATUS_BASE = 0x00004000,
		CAE_ANGLE_STATUS,


	}TCAEStatusID;

	typedef enum CAEAudioType
	{
		CAE_AUDIO_TYPE_INVALID = 0, /* the invalid data */
		CAE_AUDIO_TYPE_IVW,  /*  the local ivw data*/
		CAE_AUDIO_TYPE_ASR,  /*   cloud rec data*/

	}TCAEAudioTypeID;
	/*********  param or status structual*********/
	/* wk_info param structal */
	typedef struct CAEIvwInfo
	{
		ivInt32 nKickOff;
		ivInt32 wkPoint[2];
		ivInt32 pre_wkPoint[2];
		ivInt32 wakeup;
		ivInt32 pre_wakeup;
		ivInt32 mFill[1];
	}TCAEIvwInfo, ivPtr PCAEIvwInfo;

	/*beam info structual*/
	typedef struct tagBeamInfo {
		IN ivInt32		channel_index;
		OUT ivInt32		nPhysical;
		OUT ivFloat		fPower;
	}TBeamInfo, ivPtr PBeamInfo;

	//power info
	typedef struct tagPowerInfo
	{
		IN ivInt32 beam_index;
		OUT ivFloat fPower;
	}TPowerInfo,ivPtr PPowerInfo;

#ifdef __cplusplus
}
#endif

#endif
