#if !defined(_IV_CAE_STANDARD_API_H_)
#define _IV_CAE_STANDARD_API_H_

#include "ivErrorCode.h"
#include "ivDebug.h"
#include "ivCAEParam.h"


#define API_LIST_CAE(func)         \
	func(CAEGetInfo)			   \
	func(CAECreate)                \
	func(CAEAppendAudioData)       \
	func(CAEAppendAudioData_Mic)       \
	func(CAEAppendAudioData_Ref)       \
	func(CAEDoStep)                \
	func(CAESetParam)              \
	func(CAEGetParam)              \
	func(CAEGetVersion)            \
	func(CAEGetStatus)			   \
	func(CAEDestroy)   


#ifdef __cplusplus
extern "C" {
#endif

	/* CAE initialize param structural */
	typedef struct  tagCAEInitParam {
		IN	ivUInt32 inst_ram_avail_size;	/* the size of available inst ram, which must be on shared memory */
		OUT ivUInt32 inst_ram_size_used;    /* return the actual used size of inst ram */

		IN	ivUInt32 temp_ram_avail_size;   /* the size of available temp ram, which must be on shared memory */
		OUT ivUInt32 temp_ram_size_used;	/* return the actual used size of temp ram*/

		IN	ivUInt32 psram_avail_size;      /* the size of available temp ram, which must be on psram memory */
		OUT	ivUInt32 psram_size_used;		/* return the actual used size of param ram */

		IN	ivUInt32 mic_num;				/* the number of mic that engine need */
		IN	ivUInt32 ref_num;				/* the number of ref that engine need */
		IN	ivUInt32 input_mic_num;			/* the number of actual input mic */
		IN	ivUInt32 input_ref_num;			/* the number of actual input mic */

		IN	ivUInt32  mic_dist;				/* the distance of mic */
		IN	ivUInt32  res_size;				/* the size of resource */

		IN	ivPByte inst_ram_addr;			/* the address of inst ram */
		IN	ivPByte temp_ram_addr;			/* the address of temp ram */
		IN	ivPByte psram_addr;				/* the address of psram */
		IN	ivPByte res_addr;				/* the address of resource */

	}TCAEInitParam, ivPtr PCAEInitParam;


	/* the structual of PCM Packet,which to describe each channel data */
	typedef struct tagPCMPacket
	{
		OUT ivPByte data_buf;   /* the data buffer of pcm data*/
		OUT ivUInt16 nsample;    /* the sample of data */
		OUT ivUInt16 nbits;      /* 16/32 */
		OUT TCAEAudioTypeID type;
	}TPCMPacket,ivPtr PPCMPacket;

	typedef struct tagDataPacket
	{
		OUT	ivUInt32 nchannels;			/* the channel of data*/
		OUT	TPCMPacket ivPtr data_packets;	/* the arrary of PCM Packet*/
	}TDataPacket,ivPtr PDataPacket;

	typedef struct tagCAEInfo
	{
		OUT	ivUInt32 inst_ram_size;
		OUT	ivUInt32 psram_size;
		OUT	ivUInt32 temp_ram_size;
		OUT	ivUInt32 mic_num;
		OUT	ivUInt32 ref_num;
		OUT	ivUInt32 frame_size;
		OUT	ivUInt32 local_out_channels;
		OUT	ivUInt32 asr_out_channels;
		OUT	ivUInt32 nbit;
		OUT	ivUInt32 nsample;
		OUT	ivUInt32 frame_num;
		OUT	ivInt32 support_pre_wk_info;
		OUT	ivInt32 support_wk_info;
		OUT	ivChar	version[76];

	}TCAEInfo, ivPtr PCAEInfo;

	ivStatus ivCall CAEGetInfo(
		OUT	PCAEInfo ivPtr info
	);
	typedef ivStatus ivCall(*Proc_CAEGetInfo)(PCAEInfo ivPtr info);

	/* Create CAE object */
	ivStatus ivCall CAECreate(
		OUT	ivHandle ivPtr		hCAEObj,			/*[In/Out] To Receive the CAE object handle */
		IN_OUT	PCAEInitParam		pInitParam   		/*[In] Input parameter struct */
	);
	typedef ivStatus ivCall (*Proc_CAECreate)(ivHandle ivPtr	hCAEObj, PCAEInitParam	pInitParam);

	/* Append Audio data to the CAE object,In general, Call this function in record thread */
	/* 
		Format 1: mic1 mic2 ref1 ref2 mic1 mic2 ref1 ref2 ... ; NULL
		Format 2: mic1 mic2 mic1 mic2 ...;ref1 ref2 ref1 ref2 ...
	*/

	ivStatus ivCall CAEAppendAudioData(
		IN		ivHandle	hCAEObj,				/* The CAE object handle */
		IN		ivCPointer	pAudioData,				/* [In] Input Audio data buffer */
		IN		ivCPointer	pRefData,				/* [In] Input Ref data buffer */
		IN		ivUInt32	nSamples,			    /* [In] SPAECifies the length, in samples, of data */
		IN		ivUInt32    nBit					/* [IN] 16/32 */
		);

	typedef ivStatus ivCall (*Proc_CAEAppendAudioData)(ivHandle	hCAEObj, ivCPointer	pAudioData, ivCPointer pRefData, ivUInt32 nSamples, ivUInt32 nBit);


	ivStatus ivCall CAEAppendAudioData_Mic(
		IN		ivHandle	hCAEObj,				/* The CAE object handle */
		IN		ivCPointer	pAudioData,				/* [In] Input Audio data buffer */
		IN		ivUInt32	nSamples,			    /* [In] SPAECifies the length, in samples, of Audio data */
		IN		ivUInt32    nBit					/* [IN] 16/32 */
		);
	typedef ivStatus ivCall (*Proc_CAEAppendAudioData_Mic)(ivHandle	hCAEObj, ivCPointer	pAudioData, ivUInt32 nSamples,ivUInt32 nBit);


	ivStatus ivCall CAEAppendAudioData_Ref(
		IN		ivHandle	hCAEObj,				/* The CAE object handle */
		IN		ivCPointer	pRefData,				/* [In] Input Ref data buffer */
		IN		ivUInt32	nSamples,				/* [In] SPAECifies the length, in samples, of Ref data */
		IN		ivUInt32    nBit					/* [IN] 16/32 */
	);
	typedef ivStatus ivCall(*Proc_CAEAppendAudioData_Ref)(ivHandle	hCAEObj, ivCPointer	pRefData, ivUInt32 nSamples, ivUInt32 nBit);

	ivStatus ivCall CAEDoStep(
		IN		ivHandle	hCAEObj,				/* The CAE object handle */
		OUT		TDataPacket ivPtr pDataPacket		/* the output data*/
	);
	typedef ivStatus ivCall(*Proc_CAEDoStep)(ivHandle hCAEObj, TDataPacket *pDataPacket);

	ivStatus ivCall CAESetParam(
		IN		ivHandle	hCAEObj,				/* The CAE object handle */
		IN		TCAEParamID	nParamID,				/*[In] Parameter ID */
		IN		ivCPointer 	nParamValue				/*[In] Parameter Value */
	);
	typedef ivStatus ivCall (*Proc_CAESetParam)(ivHandle hCAEObj, TCAEParamID nParamID, ivCPointer nParamValue);

	ivStatus ivCall CAEGetParam(
		IN		ivHandle	hCAEObj,				/* The CAE object handle */
		IN		TCAEParamID	nParamID,				/*[In] Parameter ID */
		OUT		ivPointer 	nParamValue,			/*[In] Parameter Value */
		IN		ivUInt32 	Length					/*[In] The length of buffer */
	);
	typedef ivStatus ivCall(*Proc_CAEGetParam)(ivHandle	hCAEObj, TCAEParamID nParamID, ivPointer nParamValue,ivUInt32 Length);
	
	ivStatus ivCall CAEGetVersion(
		OUT		ivPointer	ivPtr pCurVersion		/* return version string */
	);
	typedef ivStatus ivCall(*Proc_CAEGetVersion)(ivPointer	ivPtr pCurVersion);

	ivStatus ivCall CAEGetStatus(
		IN		ivHandle		hCAEObj,			/* The CAE object handle */
		IN		TCAEStatusID	StatusID,			/* The StatusID */
		IN_OUT	ivPointer		Info,				/* return status info */
		IN		ivUInt32		Length				/* the length of status info buffer */
	);
	typedef ivStatus ivCall(*Proc_CAEGetStatus)(ivHandle hCAEObj, TCAEStatusID	StatusID, ivPointer	Info, ivUInt32 Length);
	
	ivStatus ivCall CAEDestroy(
		IN		ivHandle		hCAEObj				/* The CAE object handle */
	);
	typedef ivStatus ivCall(*Proc_CAEDestroy)(ivHandle hCAEObj);

#ifdef __cplusplus
}
#endif
#endif
