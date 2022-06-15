#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <lisa_mem.h>

//配置项------------------------------------------------------------------------------------------------------------------------
#define TARGET_FREQ					(1000)					//录音正弦波频率
#define MIC_AUDIO_CHANNEL_NUM		(1)						//通道数,1,2,3,4,5,6,7,8

//阈值--------------------------------------------------------------------------------------------------------------------------
#define NOISE_DB					(70)					//噪声上限，小于0.05
#define DELTA_SINALE_WAVE			(2)						//能量范围波动，单位DB
#define AUDIO_CUR_V					(90)					//最大能量
#define AUDIO_SILENCE				(55)					//最小能量
#define DELTA_MAXMIN_WAVE			(3)						//最大能量与最能量的容忍差值，单位DB

//常量定义----------------------------------------------------------------------------------------------------------------------
#define DATA_BUFFER_SIZE	(16000)							//一个通道所使用的空间大小
#define FRAMESHIT			(256)							//一帧的大小
#define ONE_SAMPLING		(2)								//两个字节
#define SAMPLE_RATE			(16000)							//采样率
#define FFT_FRAME			(1024)							//快速傅里叶变化用的一帧数据大小
#define PI					(3.1415926)
#define TARGET_INDEX		((TARGET_FREQ * FFT_FRAME) / DATA_BUFFER_SIZE)			//标准频率对应的FFT后的位置
#define OFFSET				(10 * ONE_SAMPLING * FRAMESHIT * MIC_AUDIO_CHANNEL_NUM)	//偏移十帧吧

//结构定义----------------------------------------------------------------------------------------------------------------------
typedef struct data_channel_t {
	//通道号
	int		m_nIndex;

	//数据
	float	*m_pBuffer;

	//谐波
	float	m_fNoiseValue;
	float	m_fNoiseFreq;

	//分贝
	float	m_fMaxDB;
	float	m_fMinDB;

	//最大频率
	float	m_nMaxFreq;
} data_channel_t;

//功能函数----------------------------------------------------------------------------------------------------------------------
int Init(data_channel_t *pDataTime) {
	int i;
	for (i = 0; i < MIC_AUDIO_CHANNEL_NUM; i++) {
		pDataTime[i].m_nIndex = i + 1;
		pDataTime[i].m_pBuffer = (float *)lisa_mem_alloc(DATA_BUFFER_SIZE * sizeof(float));
		memset(pDataTime[i].m_pBuffer, 0, DATA_BUFFER_SIZE * sizeof(float));

		pDataTime[i].m_nMaxFreq = 0;
	}

	return 0;
}

void Unint(data_channel_t *pDataTime) {
	int i;
	for (i = 0; i < MIC_AUDIO_CHANNEL_NUM; i++) {
		lisa_mem_free(pDataTime[i].m_pBuffer);
	}
}

int LoadAudioFile(data_channel_t *pDataTime, const unsigned char *data, int size) {
	const int nSize = size;
	int nSampleCount = 0;
	int i = OFFSET;
	short sample;

	if (nSize < DATA_BUFFER_SIZE * 2 * MIC_AUDIO_CHANNEL_NUM + OFFSET) {
		return -1;
	}

	//取数据
	while (i < nSize && nSampleCount < DATA_BUFFER_SIZE) {
		((char *)&sample)[0] = *(data + i);
		((char *)&sample)[1] = *(data + i + 1);

		pDataTime[0].m_pBuffer[nSampleCount++] = sample;
		i += 2;
	}
	return 0;
}

void swap(float *a, float *b) {
	float t;
	t = *a;
	*a = *b;
	*b = t;
}

void bitrp(float xreal[], float ximag[], int n) {
	// 位反转置换 Bit-reversal Permutation
	int i, j, a, b, p;

	for (i = 1, p = 0; i < n; i *= 2) {
		p++;
	}
	for (i = 0; i < n; i++) {
		a = i;

		b = 0;

		for (j = 0; j < p; j++) {
			b = (b << 1) + (a & 1);    // b = b * 2 + a % 2;
			a >>= 1;        // a = a / 2;
		}
		if (b > i) {
			swap(&xreal[i], &xreal[b]);
			swap(&ximag[i], &ximag[b]);
		}
	}
}

void FFT(float xreal[], float ximag[], int n) {
	// 快速傅立叶变换，将复数 x 变换后仍保存在 x 中，xreal, ximag 分别是 x 的实部和虚部
	float *wreal = (float *)lisa_mem_alloc((FFT_FRAME / 2) * sizeof(float));
	float *wimag = (float *)lisa_mem_alloc((FFT_FRAME / 2) * sizeof(float));
	float treal;
	float timag;
	float ureal;
	float uimag;
	float arg;
	int m, k, j, t, index1, index2;

	bitrp(xreal, ximag, n);

	// 计算 1 的前 n / 2 个 n 次方根的共轭复数 W'j = wreal [j] + i * wimag [j] , j = 0, 1, ... , n / 2 - 1
	arg = -2 * PI / n;
	treal = cos(arg);
	timag = sin(arg);
	wreal[0] = 1.0;
	wimag[0] = 0.0;
	for (j = 1; j < n / 2; j++) {
		wreal[j] = wreal[j - 1] * treal - wimag[j - 1] * timag;
		wimag[j] = wreal[j - 1] * timag + wimag[j - 1] * treal;
	}

	for (m = 2; m <= n; m *= 2) {
		for (k = 0; k < n; k += m) {
			for (j = 0; j < m / 2; j++) {
				index1 = k + j;
				index2 = index1 + m / 2;
				t = n * j / m;    // 旋转因子 w 的实部在 wreal [] 中的下标为 t
				treal = wreal[t] * xreal[index2] - wimag[t] * ximag[index2];
				timag = wreal[t] * ximag[index2] + wimag[t] * xreal[index2];
				ureal = xreal[index1];
				uimag = ximag[index1];
				xreal[index1] = ureal + treal;
				ximag[index1] = uimag + timag;
				xreal[index2] = ureal - treal;
				ximag[index2] = uimag - timag;
			}
		}
	}

	lisa_mem_free(wreal);
	lisa_mem_free(wimag);
}

float complexAbs(float r, float i) {
	float fTemp0 = sqrt(r * r + i * i) * 2;
	float fTemp1 = fTemp0 / FFT_FRAME;

	return fTemp1;
}

void CalcOneChannelData(data_channel_t *pDataTime, int nChannelNum) 
{
	float *pDataAudio = pDataTime[nChannelNum].m_pBuffer;
	int i, j;
	float fTempWeight = 0, fMaxWeight = 0, fNoiseWeight = 0, fNoiseFreq = 0, fNoiseValue = 0, fMinDB = 35768, fMaxDB = 0, fTargetFreqDb = 0;
	int nMaxIndex = 0, nNoiseIndex = 0, nMaxFreq = 0;
	float *pRealbuf = (float *)lisa_mem_alloc(FFT_FRAME * sizeof(float));
	float *pImagbuf = (float *)lisa_mem_alloc(FFT_FRAME * sizeof(float));
	int nTagetIndex = TARGET_INDEX;
	int nCount = 0;

	for (i = 0; i + FFT_FRAME < DATA_BUFFER_SIZE; i += FFT_FRAME)
	{
		memcpy(pRealbuf, pDataAudio + i, FFT_FRAME * sizeof(float));
		memset(pImagbuf, 0, FFT_FRAME * sizeof(float));
		FFT(pRealbuf, pImagbuf, FFT_FRAME);

		for (j = 0; j < FFT_FRAME / 2; j++) 
		{
			int nTmp = j % nTagetIndex;

			fTempWeight = complexAbs(pRealbuf[j], pImagbuf[j]);
			if (fTempWeight > fMaxWeight) {
				fMaxWeight = fTempWeight;
				nMaxIndex = j;
			}

			if (nTmp != 0 && fTempWeight > fNoiseWeight)
			{
				fNoiseWeight = fTempWeight;
				nNoiseIndex = j;
			}

			if (j == nTagetIndex)
			{
				fTargetFreqDb += fTempWeight;
			}
		}

		nMaxFreq = (nMaxIndex * SAMPLE_RATE) / FFT_FRAME;
		pDataTime[nChannelNum].m_nMaxFreq += nMaxFreq;

		if (fMaxWeight > fMaxDB)
		{
			fMaxDB = fMaxWeight;
		} 
		else if (fMaxWeight < fMinDB)
		{
			fMinDB = fMaxWeight;
		}

		nCount++;
	}

	pDataTime[nChannelNum].m_nMaxFreq /= nCount;
	fTargetFreqDb /= nCount;

	fNoiseValue = fNoiseWeight;
	fNoiseFreq = (float)(nNoiseIndex * SAMPLE_RATE) / FFT_FRAME;

	pDataTime[nChannelNum].m_fNoiseFreq = fNoiseFreq;
	pDataTime[nChannelNum].m_fNoiseValue = fNoiseValue / fTargetFreqDb;

	pDataTime[nChannelNum].m_fMaxDB = 20 * log10(fMaxDB);
	pDataTime[nChannelNum].m_fMinDB = 20 * log10(fMinDB);

	lisa_mem_free(pRealbuf);
	lisa_mem_free(pImagbuf);

	return;
}

void CheckAudioData(data_channel_t *pDataTime) {
	int nChannelNum;
	for (nChannelNum = 0; nChannelNum < MIC_AUDIO_CHANNEL_NUM; nChannelNum++) {
		CalcOneChannelData(pDataTime, nChannelNum);
	}
}

int CalAudioResult(data_channel_t *pData) {
	int i, nRet = 0;

	//1、最大频率检测
	for (i = 0; i < MIC_AUDIO_CHANNEL_NUM; i++) {
		if (pData[i].m_nMaxFreq != TARGET_FREQ) {
			nRet |= 1;
		}
	}

	//2、异常噪音的检测
	for (i = 0; i < MIC_AUDIO_CHANNEL_NUM; i++) {
		if (pData[i].m_fNoiseValue > NOISE_DB) {
			nRet |= 2;
		}
	}

	//3、能量高低差检测
	for (i = 0; i < MIC_AUDIO_CHANNEL_NUM; i++) {
		float fAbsVal = pData[i].m_fMaxDB - pData[i].m_fMinDB;
		if (fAbsVal > DELTA_SINALE_WAVE) {
			nRet |= 4;
		}
	}

	//4、MIC截幅或者MIC无音
	for (i = 0; i < MIC_AUDIO_CHANNEL_NUM; i++) {
		if (pData[i].m_fMaxDB > AUDIO_CUR_V) {
			nRet |= 8;
		} else if (pData[i].m_fMinDB < AUDIO_SILENCE) {
			nRet |= 16;
		}
	}
	return nRet;
}


//主函数----------------------------------------------------------------------------------------------------------------------
int analysis_record_pcm(const unsigned char *data, int size) 
{
	data_channel_t *pDataTime = (data_channel_t *)lisa_mem_alloc(sizeof(data_channel_t) * MIC_AUDIO_CHANNEL_NUM);
	int nRet = 0;
	
	//初始化
	if (Init(pDataTime) != 0) {
		nRet = -1;
		goto EXIT;
	}

	//载入数据
	if (LoadAudioFile(pDataTime, data, size) != 0) {
		nRet = -2;
		goto EXIT;
	}

	//处理数据
	CheckAudioData(pDataTime);

	//分析数据
	nRet = CalAudioResult(pDataTime);

EXIT:
	if (pDataTime)
	{
		Unint(pDataTime);
		lisa_mem_free(pDataTime);
	}
	return nRet;
}