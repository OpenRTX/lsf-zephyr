#ifndef LISA_EVS_SDK_RECOGNIZER_H
#define LISA_EVS_SDK_RECOGNIZER_H

#include <stdbool.h>
#include <stdint.h>
#include <lisa_err.h>

typedef struct lisa_evs_recognizer_state {
	uint8_t version[12];
} lisa_evs_recognizer_state_t;

typedef enum lisa_evs_color_space {
	EVS_COMPRESS_UNKNOWN,
	/* monochrome */
	EVS_COMPRESS_GRAYSCALE,
	/* red/green/blue, standard RGB (sRGB) */
	EVS_COMPRESS_RGB,
	/* Y/Cb/Cr (also known as YUV), standard YCC */
	EVS_COMPRESS_YCbCr,
	/* C/M/Y/K */
	EVS_COMPRESS_CMYK,
	/* Y/Cb/Cr/K */
	EVS_COMPRESS_YCCK,
	/* big gamut red/green/blue, bg-sRGB */
	EVS_COMPRESS_BG_RGB,
	/* big gamut Y/Cb/Cr, bg-sYCC */
	EVS_COMPRESS_BG_YCC
} lisa_evs_color_space_e;

/**
 * @brief 	图片压缩接口
 * @param	image_data			输入图片数据
 * @param	image_width			输入图片宽度
 * @param	image_height		输入图片高度
 * @param	components			输入图片每像素颜色组成
 * @param	color_space			输入图片颜色空间
 * @param	quality				输入图片压缩质量
 * @param	compress_data		输出压缩后图片数据
 * @param	compress_len		输出压缩后图片数据长度
 */
typedef int (*evs_rec_img_compress)(const uint8_t *const image_data,
					int image_width,
					int image_height,
					int components,
					lisa_evs_color_space_e color_space,
					int quality,
					uint8_t **compress_data,
					uint32_t *compress_len);

typedef enum {
	EVS_REC_MODE_OCR,
	EVS_REC_MODE_OCR_TTS,
	EVS_REC_MODE_OCR_TRANS,
	EVS_REC_MODE_OCR_TRANS_TTS,
	EVS_REC_MODE_TEXTBOOK
} lisa_evs_rec_image_mode_t;

typedef enum {
	// AUDIO_L16_RATE_16000_CHANNELS_1
	EVS_REC_AUDIO_FORMAT_BASE,
	// OPUS
	EVS_REC_AUDIO_FORMAT_OPUS,
	// SPEEX_WB_QUALITY_9
	EVS_REC_AUDIO_FORMAT_SPEEX
} lisa_evs_rec_audio_format_e;

typedef enum {
	// 近场识别，适合3米内的使用场景
	EVS_REC_AUDIO_PROFILE_CLOSE_TALK,
	// 远场识别，适合10米内的使用场景
	EVS_REC_AUDIO_FORMAT_FAR,
	// 语音评测，适合中英文评测场景
	EVS_REC_AUDIO_PROFILE_EVALUATE
} lisa_evs_rec_audio_profile_e;

/** 音频识别参数 */
typedef struct lisa_evs_rec_audio_param_s {
	lisa_evs_rec_audio_format_e format;
	lisa_evs_rec_audio_profile_e profile;
	// 是否使用云端VAD，默认为true
	bool enable_vad;
	// VAD后端点时长，单位为毫秒, 需大于800ms
	int vad_eos;
	uint8_t *reply_key;
	bool translation;
} lisa_evs_rec_audio_param_t;

typedef enum {
	// 评测语言，取值：zh_cn（中文）
	EVS_REC_EVALUATE_ZH_CN,
	// 评测语言，取值：en_us（美式英文
	EVS_REC_EVALUATE_EN_US,
} lisa_evs_rec_evaluate_language;
typedef enum {
	// 篇章朗读,仅英文可用，300个英文字符以内
	EVS_REC_EVALUATE_READ_CHAPTER,
	// 句子朗读, 中英文可用, 一句话，英文建议100个字符以内，中文建议30个字符以内
	EVS_REC_EVALUATE_READ_SENTENCE,
	// 词语朗读, 中英文可用, 词语，不支持标点符号
	EVS_REC_EVALUATE_READ_WORD,
	// 单字朗读, 仅中文可用,单字，不支持标点符号
	EVS_REC_EVALUATE_READ_SYLLABLE,
	// 口语练习，详细试题格式见 https://doc.iflyos.cn/device/upgrades/evaluate.html#%E8%AF%95%E9%A2%98text%E6%A0%BC%E5%BC%8F
	EVS_REC_EVALUATE_READ_CHOICE,
} lisa_evs_rec_evaluate_category;

typedef struct lisa_evs_rec_assess_param
{
	lisa_evs_rec_evaluate_language language;
	lisa_evs_rec_evaluate_category category;
	uint8_t* text;
} lisa_evs_rec_evaluate_param_t;


/** 图片识别参数 */
typedef struct lisa_evs_rec_image_param_s {
	// 识别模式
	lisa_evs_rec_image_mode_t rec_mode;
	// 单行/多行扫描
	bool single_line;
	// 中文分词 (true: 原文为中文，则返回中文分词结果, 其他情况返回原文原始文本)
	bool tokenization;
	// 此选项打开可以从特定地址获取传输的数据
	bool debug_image;
	// 传输原图模式
	bool debug;
	// 0917新增: 词典开关
	bool dictionary;
	// 0917新增: 教育内容开关
	bool resources;
} lisa_evs_rec_image_param_t;

#define EVS_REC_IMG_PARAM_INIT(...) { \
		.single_line = true, \
		.debug_image = false, \
		.tokenization = false, \
		.debug = false, \
		## __VA_ARGS__ }

typedef struct lisa_evs_rec {
	struct lisa_evs *evs;
	struct lisa_evs_rec_context_s *ctx;
	struct lisa_evs_ws_s *ws;
} lisa_evs_rec_t;

typedef void (*lisa_evs_rec_data_cb)(const uint8_t *const data, uint32_t len);

/**
 * @brief 					识别初始化，sdk内部调用
 * @param  evs              evs句柄
 * @param  ws               evs_ws句柄
 * @return lisa_evs_rec_t* 
 */
lisa_evs_rec_t *lisa_evs_rec_create(struct lisa_evs *evs, struct lisa_evs_ws_s *ws);

/**
 * @brief 					识别销毁
 * @param  handle           识别句柄
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_rec_destory(lisa_evs_rec_t *handle);

/**
 * @brief 图像识别开始
 * @param handle 			识别句柄
 * @param req_id 		    请求ID
 * @param param 			图片参数
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_image_start(
		const lisa_evs_rec_t *const handle, const uint8_t *req_id, const lisa_evs_rec_image_param_t *const param);

/**
 * @brief 图像识别结束
 * @param handle            识别句柄
 * @param req_id            请求ID
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_image_end(const lisa_evs_rec_t *const handle, const uint8_t *const req_id);

/**
 * @brief 音频识别开始
 * @param handle 			识别句柄
 * @param req_id 		    请求ID
 * @param param 			音频参数
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_audio_start(
		const lisa_evs_rec_t *const handle, const uint8_t *const req_id, const lisa_evs_rec_audio_param_t *const param);

/**
 * @brief 音频识别结束
 * @param handle            识别句柄
 * @param req_id            请求ID
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_audio_end(const lisa_evs_rec_t *const handle, const uint8_t *const req_id);

/**
 * @brief 音频识别停止
 * @param handle 			识别句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_audio_stop(const lisa_evs_rec_t *const handle);

/**
 * @brief 图像识别取消
 * @param handle 			识别句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_image_cancel(const lisa_evs_rec_t *const handle);

/**
 * @brief 音频识别取消
 * @param handle 			识别句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_audio_cancel(const lisa_evs_rec_t *const handle);

/**
 * @brief 					发送音频到云端
 * @param  handle           识别句柄
 * @param  audio            音频buffer(>=640字节数据,建议640)
 * @param  len              音频长度
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_send_audio(
		const lisa_evs_rec_t *const handle, const uint8_t *const audio, int len);

/**
 * @brief 					发送图片到云端
 * @param  handle           识别句柄
 * @param  image_frame_data 图片帧数据
 * @param  image_frame_len  图片帧数据长度
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_send_image(const lisa_evs_rec_t *const handle,
		const uint8_t *const image_frame_data, int image_frame_len);

/**
 * @brief 					多行扫描时的换行帧
 * @param  handle           识别句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_send_image_enter(const lisa_evs_rec_t *const handle);

/**
 * @brief 					设置图片发送的压缩方法
 * @param	compress_fun	压缩图片函数指针 (置为NULL,则恢复自带压缩方式)
 * @return 	lisa_err_t
 */
lisa_err_t lisa_evs_rec_set_compress_fun(evs_rec_img_compress compress_fun);

/**
 * @brief  口语评测开始
 * @param  handle           句柄
 * @param  req_id           请求id
 * @param  param            音频参数
 * @param  evaluate         评测参数
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_rec_evaluate_start(
		const lisa_evs_rec_t *const handle, const uint8_t *const req_id, 
		const lisa_evs_rec_audio_param_t *const param, const lisa_evs_rec_evaluate_param_t *const evaluate);

/**
 * @brief  口语评测结束
 * @param  handle           句柄
 * @param  req_id           请求id
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_rec_evaluate_end(const lisa_evs_rec_t *const handle, const uint8_t *const req_id);

/**
 * @brief  口语评测取消
 * @param  handle           句柄
 * @param  req_id           请求id
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_rec_evaluate_cancel(const lisa_evs_rec_t *const handle, const uint8_t *const req_id);

/**
 * @brief 设置数据回调, 传输数据之前调用
 * @param data_cb			数据回调函数
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_rec_set_data_cb(lisa_evs_rec_t *const handle, lisa_evs_rec_data_cb data_cb);

#endif
