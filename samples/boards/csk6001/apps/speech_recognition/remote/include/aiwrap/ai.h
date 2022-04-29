/**
 * @mainpage Listnai AI Wrapper with CAE and ESR algorithms
 *
 * This wrapper library is designed to simplify the algorithms' intenal
 * diversified call logics, reduce the coupling between algorithms, and
 * hide the end users' uninterested logic to internal, such as wakeup
 * points registry, audio array transposition, in order to get best
 * performance and experience for end user.
 *
 * This document is the only necessary interface description file of the
 * algorithm wrapper library, and which also includes the most end user
 * concerned apis description notes and examples.
 *
 * Generally, end user should copy this description file(ai.h) and the
 * wrapper library(libai.a) to his own project, and configure the include
 * path to help compiler to find the ai.h, then link the libai.a to his
 * application, that all.
 *
 * Copyright Listenai(c) 2021
 */

/**
 * @file ai.h
 * @brief header of wrapper library for ai algorithms
 * @author danny(xhtan@listenai.com)
 * @version 1.0.0
 * @date 2021-04-07
 * @copyright Copyright Listenai(c) 2021
 */

#ifndef __LISTENAI_AIWRAP_HEADER__
#define __LISTENAI_AIWRAP_HEADER__
 
#ifdef _WIN32
#ifdef  _DLL
#define DLL_EXPORT     __declspec(dllexport)
#else
#define DLL_EXPORT
#endif
#else
#define DLL_EXPORT
#endif

#define AI_STEP_MSECS		(160)	///< aiwrap step time in ms
#define AI_STEP_IN_SAMPS	(256)	///< aiwrap step input samples
#define AI_STEP_OUT_SAMPS	(2560)	///< aiwrap step output samples

//#ifdef __cplusplus
//extern "C"
//{
//#endif

	/**
	 * @brief enumeration for error code
	 */
	typedef enum {
		/**
		 * the base error group for wrapper internal
		 */
		ai_ok = 0,					///< indicates no error or success

		ai_wrap_none = ai_ok + 0,	///< indicates algorithm none output
		ai_wrap_norm = ai_ok + 1,	///< indicates normal output
		ai_wrap_more = ai_ok + 2,	///< indicates algorithm need continue

		ai_error = 1000,			///< indicates no error or success
		ai_algo_err = ai_error + 1,	///< indicates general algorithm internal error
		ai_wrap_err,				///< indicates general wrapper unfiled error
		ai_arg_inv,					///< indicates invalid arguments
		ai_res_inv,					///< indicates invalid resources data
		ai_api_inv,					///< indicates api calling at invalid condition
		ai_mem_inv,					///< indicates invalid memory(address or size)
		ai_buf_not_enough,			///< indicates buf len not enough
		ai_sample_not_fit,			///< indicates input sample not fit

		/**
		* the base error group for esr wrap:
		* ai_module_esr + 0: indicates esr wrap success, generally this state only used for wrapper
		*   to check esr wrap status, and redirects to ai_ok to caller
		* ai_module_esr + x: x > 0, indicates esr wrap failed, and x stands for wrap error code
		*/
		ai_module_esr = 10000,     

		/**
		* the base error group for cae wrap:
		* ai_module_cae + 0: indicates cae wrap success, generally this state only used for wrapper
		*   to check cae wrap status, and redirects to ai_ok to caller
		* ai_module_cae + x: x > 0, indicates cae wrap failed, and x stands for wrap error code
		*/
		ai_module_cae = 20000,

		/**
		 * the base error group for cae-library:
		 * ai_cae_ok + 0: indicates cae success, generally this state only used for wrapper
		 *   to check cae internal status, and redirects to ai_ok to caller
		 * ai_cae_ok + x: x > 0, indicates cae failed, and x stands for cae error code
		 */
		ai_cae_ok = 10000000,

		/**
		 * the base error group for esr-library:
		 * ai_esr_ok + 0: indicates esr success, generally, this state only used for wrapper
		 *   to check esr internal status, and redirects to ai_ok to end caller
		 * ai_esr_ok + x: x > 0, indicates esr failed, and x stands for esr error code
		 */
		ai_esr_ok = 20000000,
	} ai_err_e;

	/**
	 * @brief enumeration for callback message type
	 */
	typedef enum {
		ai_msg_wakeup = 0x00,		///< voice wakeup message
		ai_msg_prewak = 0x01,		///< voice pre-wakeup message
		ai_msg_error = 0x02,		///< error message
		ai_msg_trace = 0x03,		///< debug trace message
	} ai_msg_e;

	/**
	 * @brief enumeration for adc type(audio memory layout)
	 */
	typedef enum {
		ai_adc_1x4ch = 0x00,		///< for one adc with each quadruple channels, such as es7210
		ai_adc_2x2ch = 0x01,		///< for two adc with each dual channels, such as es7243
	} ai_adc_e;

	/**
	* @brief enumeration for wakeup mask
	*/
	typedef enum {
		wkmask_wake = 0x01,			///< proc wake up msg
		wkmask_prewake = 0x02,		///< proc pre wake up msg
	} ai_wkmask_e;

	/**
	 * @brief structure for memory node description
	 */
	typedef struct {
		char *addr;					///< indicate memory address
		long size;					///< indicate memory size in byte
	} ai_addr_t;

	/**
	 * @brief structure for memory map description
	 */
	typedef struct {
		ai_addr_t share;			///< description for share memory
		ai_addr_t psram;			///< description for psram memory

		/// description for resources locations, usually locate in flash or preloaded psram
		struct {
			ai_addr_t cae_mlp;		///< description for mlp resource of cae
			ai_addr_t esr_mlp;		///< description for mlp resource of esr
			ai_addr_t esr_main;		///< description for main keywords resource of esr
			ai_addr_t esr_cmds;		///< description for commands keywords resource of esr
		} resmap;
	} ai_mem_t;

	/**
	 * @brief wrapper instance hander
	 */
	typedef void const *ai_inst_t;

	/**
	 * @brief the message process callback function
	 * @param[out] msg: the message type
	 * @param[out] larg: the integer type argument, the actual meaning should according to msg
	 * @param[out] parg: the pointer type argument, the actual meaning should according to msg
	 * @param[out] user: the user identification information pointer
	 * @return the actual meaning should according to msg, see #ai_msg_e
	 * @note this function should defind in end user side
	 * @remark example:
	 * @code
	 *	extern long ai_msg_proc(ai_msg_e msg, int larg, void *parg, void *user)
	 *	{
	 *		switch (msg) {
	 *		case ai_msg_wakeup:
	 *			printf("[wrap]wakeup: chn=%d; result=%s", larg, (char *)parg);
	 *			break;
	 *		case ai_msg_prewak:
	 *			printf("[wrap]prewak: chn=%d; result=%s", larg, (char *)parg);
	 *			break;
	 *		case ai_msg_error:
	 *			printf("[wrap]error: code=%d; msg=%s", larg, (char *)parg);
	 *			break;
	 *		case ai_msg_trace:
	 *			printf("[wrap]trace: %s", (char *)parg);
	 *			break;
	 *		default:
	 *			return -1;
	 *		}
	 *		return 0;
	 *	}
	 * @endcode
	 */
	typedef long(*ai_msg_proc_fun)(ai_msg_e msg, int larg, void *parg, void *user);
	//extern long ai_msg_proc(ai_msg_e msg, int larg, void *parg, void *user);

	/**
	 * @brief creare instance of ai wrapper with cae and esr algorithm libraries
	 * @param[out] inst: the wapper instance hander
	 * @param[in,out] mem: the memory map descriptions, include psram/share/resmap
	 * @param[in] json: the configuration string in json format, pelease use the config fit for the algorithm 
	 * @param[in] call_back: the callback function address, see #ai_msg_proc_fun
	 * @param[in] user: the user identification information pointer
	 * @remark
	 * <table>
	 *   <caption>Configuration JSON Description Table</caption>
	 *   <tr><th>group<th>key<th>subkey<th>description<th>valid values
	 *   <tr><td rowspan="8" align="middle">wrap<td colspan="2" rowspan="3" align="middle">dev<td rowspan="3">device type
	 *     <td>"4002": default, for 4002 serials platform
	 *   <tr><td>"3002": for 3002 serials platform
	 *   <tr><td>"3001": for 3001 serials platform
	 *   <tr><td colspan="2" rowspan="2" align="middle">adc<td rowspan="2">adc type
	 *     <td>0: for 1x4ch serials adc(such as es7210)
	 *   <tr><td>1: for 2x2ch serials adc(such as es7243)
	 *   <tr><td colspan="2" rowspan="2" align="middle">trans
	 *     <td rowspan="2">audio data transpose after cae output
	 *     <td>0: disable transpose, that means the orignal cae output
	 *   <tr><td>1: enable transpose
	 *   <tr><td rowspan="1" colspan="2" align="middle">param<td>wrap params support by #ai_set_param <td>array of key value pair in json format
	 *   <tr><td rowspan="10" align="middle">cae<td colspan="2" rowspan="1" align="middle">dist<td>micphone distance<td>in mm
	 *   <tr><td colspan="2" rowspan="1" align="middle">bits<td>sample width<td>16/32
	 *   <tr><td colspan="1" rowspan="3" align="middle">chns
	 *     <td align="middle">mic<td>input: micphone<td>2
	 *   <tr><td align="middle">ref<td>input: reference<td>2
	 *   <tr><td align="middle">out<td>output<td>4
	 *   <tr><td colspan="2" rowspan="4" align="middle">wkmask
	 *     <td rowspan="4">wakeup(bit0)/prewakeup(bit0) points enable mask
	 *     <td>0: disable wakeup and prewakeup points
	 *   <tr><td>1: enable wakeup and disable prewakeup points
	 *   <tr><td>2: enable prewakeup and disable wakeup points
	 *   <tr><td>3: enable wakeup and prewakeup points
	 *   <tr><td colspan="2" rowspan="1" align="middle">param<td>cae params support by #ai_set_param <td>array of key value pair in json format
	 *   <tr><td>esr<td colspan="2" align="middle">param<td>esr params support by #ai_set_param <td>array of key value pair in json format
	 * </table>
	 * @return the error code, see #ai_err_e
	 *   @retval ai_ok: indicate success
	 *   @retval other: indicate failed
	 * @remark example:
	 * @code
	 *	ai_mem_t memap = {
	 *		.psram = { psram_pool, sizeof(psram_pool) },
	 *		.share = { share_pool, sizeof(share_pool) },
	 *		.resmap = {
	 *			.cae_mlp = { cae.mlp, sizeof(cae.mlp) },
	 *			.esr_mlp = { esr.mlp, sizeof(esr.mlp) },
	 *			.esr_main = { esr.main, sizeof(esr.main) },
	 *			.esr_cmds = { esr.cmds, sizeof(esr.cmds) },
	 *          .esr_wake_list = { esr.wake_list, sizeof(esr.wake_list) },
	 *		}
	 *	};
	 *	ai_inst_t inst = NULL;
	 *	if (ai_ok == ai_create(&inst, &memap,
	 *		"{\"wrap\":{\"dev\":\"4002\",\"adc\":0,\"trans\":1,\"param\":[{\"key\":\"wakesoffset\",\"val\":160},{\"key\":\"wakeeoffset\",\"val\":160}]},"
	 *      "\"cae\":{\"dist\":35,\"bits\":16,\"chns\":{\"mic\":2,\"ref\":2,\"out\":4},"
	 *      "\"wkmask\":3,\"param\":[{\"key\":\"mode\",\"val\":\"0\"},{\"key\":\"textdep\",\"val\":1}]},"
	 *      "\"esr\":{\"param\":[{\"key\":\"prewstat\",\"val\":5},{\"key\":\"prewthr\",\"val\":600},{\"key\":\"mode\",\"val\":1},"
	 *      "{\"key\":\"model\",\"val\":0}]}}"
	 *	) {
	 *		// ...
	 *		ai_destory(inst);
	 *	}
	 * @endcode
	 * @see #ai_destory, #ai_get_info
	 */
	DLL_EXPORT ai_err_e  ai_create(ai_inst_t *inst, ai_mem_t *mem, const char *json, ai_msg_proc_fun call_back, void* user);

	/**
	 * @brief desory instance of ai wrapper
	 * @param[in] inst: the wapper instance hander
	 * @return the error code, see #ai_err_e
	 * 	 @retval ai_ok: indicate success
	 * 	 @retval other: indicate failed
	 * @see #ai_create
	 */
	DLL_EXPORT ai_err_e ai_destory(ai_inst_t inst);


	/**
	* @brief register message process callback function with user param
	* @param[in] inst: the wapper instance hander
	* @param[in] call_back: the callback function address, see #ai_msg_proc_fun
	* @param[in] user: the user identification information pointer
	* @return the error code, see #ai_err_e
	* 	 @retval ai_ok: indicate success
	* 	 @retval other: indicate failed
	*/
	DLL_EXPORT ai_err_e ai_register_call_back(ai_inst_t inst, ai_msg_proc_fun call_back, void* user);

	/**
	 * @brief append audio data to wrapper(algorithms)
	 * @param[in] inst: the wapper instance hander
	 * @param[in] data: the orignal audio data, include micphones and references
	 * @return the error code, see #ai_err_e
	 * 	 @retval ai_wrap_none: reserved
	 * 	 @retval ai_wrap_norm: indicate append success
	 * 	 @retval ai_wrap_more: indicate the last outputs have more data in cache fifo,
	 * 	 so data will been skipped in this step, generally, caller should cache this
	 * 	 frame input, and re-append which in the next time
	 * 	 @retval other: indicate failed
	 * @note the audio data format(samples counts, bit width, channels) should according
	 * to the type configuration when create, see #ai_create
	 * @see #ai_process, #ai_create
	 */
	DLL_EXPORT ai_err_e ai_append_data(ai_inst_t inst, void *data);

	/**
	 * @brief step run wrapper(algorithms) and get result from it
	 * @param[in] inst: the wapper instance hander
	 * @param[out] data: the audio data after algorithm process
	 * @return the error code, see #ai_err_e
	 * 	 @retval ai_wrap_none: indicate none output
	 * 	 @retval ai_wrap_norm: indicate success with normal one frame output
	 * 	 @retval ai_wrap_more: indicate success, but there are two frames outputs in
	 * 	 cache, generally, caller should call ai_run_step twice in one ai_append_data
	 * 	 @retval other: indicate failed
	 * @note the audio data format(samples counts, bit width, channels) should according
	 * to the type configuration when create, see #ai_create
	 * @see #ai_process, #ai_create
	 */
	DLL_EXPORT ai_err_e ai_run_step(ai_inst_t inst, void **data);

	/**
	 * @brief step append to, run, and get from wrapper(algorithms)
	 * @param[in] inst: the wapper instance hander
	 * @param[in] idata: the orignal audio data, include micphones and references
	 * @param[out] odata: the audio data after algorithm process
	 *   @return the error code, see #ai_err_e
	 *   @retval ai_wrap_none: reserved
	 *   @retval ai_wrap_norm: indicate success with normal one output frame
	 *   @retval ai_wrap_more: indicate success with one input frame, but two outputs
	 * 	 frames. please note that, because there are more than one frames to output,
	 * 	 caller should call the ai_process twice to get the full outputs, howerer, the
	 * 	 input will been skipped in the 2nd calling in this case, so caller should re-
	 * 	 append the 2nd input again to avoid data loss
	 *   @retval other: indicate failed
	 * @note the audio data format(samples counts, bit width, channels) should according
	 * to the type configuration when create, see #ai_create, this api just combined from
	 * #ai_append_data and #ai_run_step, that means the arguments are the same with those,
	 * end caller can also call those two apis instead
	 * @see #ai_append_data, #ai_run_step, #ai_create
	 */
	DLL_EXPORT ai_err_e ai_process(ai_inst_t inst, void *idata, void **odata);

	/**
	 * @brief set global parameters of wrapper(algorithms) in non-runtime state
	 * @param[in] inst: the wapper instance hander
	 * @param[in] json: the parameters configurations in json format
	 * @return the error code, see #ai_err_e
	 *   @retval ai_ok: indicate success
	 *   @retval other: indicate failed
	 * @note this api should only used before calling #ai_process(#ai_append_data, #ai_run_step)
	 * @remark
	 * <table>
	 *   <caption>Parameters Description Table</caption>
	 *   <tr><th>group<th>key<th>description<th>valid values
	 *   <tr><td rowspan="3" align="middle">wrap<td>wakesoffset<td>wakeup start time offset<td>milliseconds
	 *   <tr><td>wakeeoffset<td>wakeup end time offset<td>milliseconds
	 *   <tr><td>...<td>-<td>-
	 *   <tr><td rowspan="8" align="middle">cae<td rowspan="2">mode
	 *     <td rowspan="2">algorithm working mode
	 *     <td>0: wakeup mode without asr channel output
	 *   <tr><td>1: record mode with asr channel output
	 *   <tr><td rowspan="3">beam<td rowspan="3">beam value<td>0: 0-60 degree
	 *   <tr><td>1: 60-120 degree<tr><td>2: 120-180 degree
	 *   <tr><td rowspan="2">textdep<td rowspan="2">text dependence switch
	 *     <td>0: disable textdep function
	 *   <tr><td>1: enable textdep function
	 *   <tr><td>...<td>treat as integer, pass to algorithm(cae) without translation
	 *     <td>string in integer format
	 *   <tr><td rowspan="8" align="middle">esr<td rowspan="2">mode
	 *     <td rowspan="2">algorithm working mode<td>0: command keyword mode
	 *   <tr><td>1: main keyword mode
	 *   <tr><td rowspan="2">model<td rowspan="2">algorithm resource type
	 *     <td>0: main keyword resource
	 *   <tr><td>1: command keyword resource
	 *   <tr><td>prewstat<td>pre-wakeup state number<td>refer to esr algorithm manual
	 *   <tr><td>prewthr<td>pre-wakeup threshold value<td>refer to esr algorithm manual
	 *   <tr><td>wakethr<td>wakeup threshold value<td>refer to esr algorithm manual
	 *   <tr><td>...<td>treat as integer, pass to algorithm(esr) without translation
	 *     <td>string in integer format
	 * </table>
	 * @remark example:
	 * @code
	 *	request json:
	 *		{
	 *			"wrap": [
	 *				{ "key": "KEY1", "val": VAL1 },
	 *				{ "key": "KEY2", "val": VAL2 }
	 *			],
	 *			"cae": [
	 *				{ "key": "KEY1", "val": VAL1 },
	 *				{ "key": "KEY2", "val": VAL2 }
	 *			],
	 *			"esr": [
	 *				{ "key": "KEY3", "val": VAL3 },
	 *				{ "key": "KEY4", "val": VAL4 }
	 *			]
	 *		}
	 * @endcode
	 * @see #ai_get_param, #ai_get_status, #ai_get_info
	 */
	DLL_EXPORT ai_err_e ai_set_param(ai_inst_t inst, const char *json);

	/**
	 * @brief get the configurable parameters values in non-runtime state
	 * @param[in] inst: the wapper instance hander
	 * @param[in] json: the parameters configurations in json format
	 * @param[out] buff: the buffer to receive parameters string(json format)
	 * @param[in,out] size: the max buffer size for input(the size should one byte bigger than
	 * real need at least, otherwise ai_mem_inv will get), and real used size for output
	 * @return the error code, see #ai_err_e
	 * 	 @retval ai_ok: indicate success
	 * 	 @retval other: indicate failed
	 * @remark the details descriptions, see "Parameters Description Table" in #ai_set_param
	 * @note this api may fail when buffer size not enough(equal or smaller than need),
	 * also this api should only used before calling #ai_process(#ai_append_data, #ai_run_step)
	 * @remark example:
	 * @code
	 * 	request json:
	 * 		{
	 *          "wrap": [ "KEY1", "KEY2" ],
	 * 			"cae": [ "KEY1", "KEY2" ],
	 * 			"esr": [ "KEY3", "KEY4" ]
	 * 		}
	 * 	response json:
	 * 		{
	 * 			"wrap": { "KEY1": VAL1, "KEY2": VAL2 },
	 * 			"cae": { "KEY1": VAL1, "KEY2": VAL2 },
	 * 			"esr": { "KEY3": VAL3, "KEY4": VAL4 }
	 * 		}
	 * @endcode
	 * @see #ai_set_param, #ai_get_status, #ai_get_info
	 */
	DLL_EXPORT ai_err_e ai_get_param(ai_inst_t inst, const char *json, char *buff, int *size);

	/**
	 * @brief get wrapper(algorithm) status in runtime state, such as reference signal state,
	 * power energy, beams, and so on
	 * @param[in] inst: the wapper instance hander
	 * @param[in] json: the status requires in json format
	 * @param[out] buff: the buffer to receive parameters string(json format)
	 * @param[in,out] size: the max buffer size for input(the size should one byte bigger than
	 * real need at least, otherwise ai_mem_inv will get), and real used size for output
	 * @return the error code, see #ai_err_e
	 * 	 @retval ai_ok: indicate success
	 * 	 @retval other: indicate failed
	 * @note this api may fail when buffer size not enough(equal or smaller than need),
	 * however, different with ai_get_param, this api should calling after #ai_process(
	 * #ai_append_data, #ai_run_step)
	 * @remark
	 * <table>
	 * 	 <caption>Status Description Table</caption>
	 * 	 <tr><th>group<th>key<th>subkey<th>description<th>valid values
	 * 	 <tr><td rowspan="1" colspan="1" align="middle">wrap
	 * 	   <td colspan="3" align="middle">wrapper status<td>-
	 *   <tr><td rowspan="10" align="middle">cae<td rowspan="3">power
	 *     <td colspan="2" align="middle">power status<td>
	 *   <tr><td>index<td>beam index<td>
	 *   <tr><td>fPower<td>power value<td>float type
	 *   <tr><td rowspan="4">beam<td colspan="2" align="middle">beam status<td>
	 *   <tr><td>index<td>the index of channel<td>
	 *   <tr><td>fPower<td>the power value<td>float type
	 *   <tr><td>nPhysical<td>the physical value<td>
	 *   <tr><td rowspan="2" colspan="2" align="middle">refstat
	 *     <td rowspan="2">the reference signal state<td>0: reference signal invalid
	 *   <tr><td>1: reference signal valid
	 *   <tr><td rowspan="1" colspan="2" align="middle">angle<td>the array contains angle values of 10 frames<td>
	 *   <tr><td rowspan="2" align="middle">esr<td colspan="2" align="middle">result
	 *     <td>the last wakeup result<td>json format
	 *   <tr><td rowspan="1" colspan="2" align="middle">prior
	 *     <td>the prior information of model<td>
	 * </table>
	 * @remark example:
	 * @code
	 *  request json:
	 *		{
	 *			"cae": [ "KEY1", "KEY2" ],
	 *			"esr": [ "KEY3", "KEY4" ]
	 *		}
	 *	response json:
	 *		{
	 *			"cae": { "KEY1": "VAL1", "KEY2": "VAL2" },
	 *			"esr": { "KEY3": "VAL3", "KEY4": "VAL4" }
	 *		}
	 * @endcode
	 * @see #ai_set_param, #ai_get_param, #ai_get_info
	 **/
	DLL_EXPORT ai_err_e ai_get_status(ai_inst_t inst, const char *json, char *buff, int *size);

	/**
	 * @brief get static informations before wrapper(algorithm) create
	 * @param[out] buff: the buffer to receive parameters string(json format)
	 * @param[in,out] size: the max buffer size for input(the size should one byte bigger than
	 * real need at least, otherwise ai_mem_inv will get), and real used size for output
	 * @return the error code, see #ai_err_e
	 * 	 @retval ai_ok: indicate success
	 * 	 @retval other: indicate failed
	 * @note this api valid in anytime, especial, before wrapper created
	 * @see #ai_set_param, #ai_get_param, #ai_get_status
	 */
	DLL_EXPORT ai_err_e ai_get_info(char *buff, int *size);

//#ifdef __cplusplus
//};
//#endif

#endif//__LISTENAI_AIWRAP_HEADER__
