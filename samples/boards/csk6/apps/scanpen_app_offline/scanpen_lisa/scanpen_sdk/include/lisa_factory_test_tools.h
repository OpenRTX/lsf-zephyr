#ifndef _LISA_FACTORY_TEST_TOOLS_H_
#define _LISA_FACTORY_TEST_TOOLS_H_

#include <stdint.h>

/**
 * @brief 					产生用于播放的PCM数据的callback
 * @param  pcm       		PCM数据的指针，每个采样2字节
 * @param  count       		采样的个数，并非字节数
 */
typedef void (*genPcmCb)(const short *pcm, unsigned int count);

/**
 * @brief 					产生单声道16K采样16位的PCM数据，可交给底层PCM设备播放
 * @param  cb       		PCM数据回调的callback
 */
void lisa_factory_genSinePcm(const genPcmCb cb);

/**
 * @brief 					输入录音音频进行分析
 * @param  recBuffer       	16K采样16位声道的录音数据，长度>=1S
 * @param  size				录音数据的字节数，>=32kbytes
 * @return int 				算法分析的结果
 *								ret == 0: OK
 *								(ret&1) == 1: 频率不合规
 *								(ret&2) == 2: 谐波超了
 *								(ret&4) == 4: 能量异常
 *								(ret&8) == 8: 截幅
 *								(ret&16) == 16: 没录到音
 */
int lisa_factory_analysisRecPcm(const uint8_t *recBuffer, int size);

#endif