/*
 * Copyright (c) 2022, listenai
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_dvp

#include <zephyr.h>
#include <drivers/video.h>
#include "hal/Driver_DVP.h"
#include <soc.h>
#include <soc_dt.h>
#include <drivers/pinmux.h>
#include "hal/venus_ap.h"

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(csk6_dvp);

#define AP2CP_PSRAM_ADDR_OFFSET   (0x30000000)
#define DVP_PSRAM_ADDR_WRAP(x)    (((uint32_t)x + AP2CP_PSRAM_ADDR_OFFSET))

struct video_csk6_dvp_config {
	uint8_t data_align:1;
	uint8_t pclk_polarity:1;
	uint8_t hsync_polarity:1;
	uint8_t vsync_polarity:1;

	char *sensor_label;
	uint16_t clock_prescaler;
	/* DVP alternate configuration */
	const struct csk6_pinctrl_cfg *pinctrl_list;
	uint8_t pinctrl_list_size;
};

struct video_csk6_dvp_data {
	/* driver handle */
    void *handle; 

	const struct device *dev;
	const struct device *sensor_dev;

	struct k_fifo fifo_in;
	struct k_fifo fifo_out;
	struct k_poll_signal *signal;
	struct video_buffer *cur_vbuf;

	struct video_format fmt;
};

static int csk6_dvp_start(const struct device *dev, struct video_buffer *vbuf);

static void set_current_vbuf(const struct device *dev, struct video_buffer *vbuf)
{
	struct video_csk6_dvp_data *data = dev->data;

	data->cur_vbuf = vbuf;
}

static struct video_buffer *get_current_vbuf(const struct device *dev)
{
	struct video_csk6_dvp_data *data = dev->data;
	struct video_buffer *vbuf;

	vbuf = data->cur_vbuf;

	return vbuf;
}

static void dvp_framedata_proc(void)
{
	const struct device *dev = DEVICE_DT_INST_GET(0);
	struct video_csk6_dvp_data *data = dev->data;
	struct video_buffer *vbuf;
	struct video_buffer *temp_vbuf;
	DVP_HandleTypeDef *dvp_dev = data->handle;

	temp_vbuf = get_current_vbuf(dev);
	temp_vbuf->timestamp = k_uptime_get_32();

	vbuf = k_fifo_get(&data->fifo_in, K_NO_WAIT);
	if(vbuf){
		DVP_Stop(dvp_dev);
		csk6_dvp_start(dev, vbuf);
		set_current_vbuf(dev, vbuf);
		k_fifo_put(&data->fifo_out, temp_vbuf);
	}else{
		DVP_Stop(dvp_dev);
		csk6_dvp_start(dev, temp_vbuf);
		set_current_vbuf(dev, temp_vbuf);		
		LOG_DBG("not enough fifo_in \r\n");
	}

}

static void dvp_callback(uint32_t event, uint32_t param)
{
	switch(event)
	{
		case CSK_DVP_STATUS_FRM_CMPLT:
			dvp_framedata_proc();
			break;
		case CSK_DVP_STATUS_JLB0_FULL:
		case CSK_DVP_STATUS_JLB1_FULL:
			break;
		case CSK_DVP_STATUS_FIFO_OVF:
		case CSK_DVP_STATUS_AHB_ERR:
			LOG_WRN("dvp err interrupt %d\r\n", event);
			break;
		default:
			break;
	}
	return;
}

static int video_csk6_dvp_set_fmt(const struct device *dev,
				  enum video_endpoint_id ep,
				  struct video_format *fmt)
{
	const struct video_csk6_dvp_config *config = dev->config;
	struct video_csk6_dvp_data *data = dev->data;
	DVP_HandleTypeDef *dvp_dev = data->handle;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	if(!config->pclk_polarity){
		dvp_dev->Init.PCKPolarity = CSK_DVP_POL_CNTL_CLOCK_RISING;
	}else{
		dvp_dev->Init.PCKPolarity = CSK_DVP_POL_CNTL_CLOCK_FALLING;
	}

	if(!config->hsync_polarity){
		dvp_dev->Init.HSPolarity = CSK_DVP_POL_CNTL_HSYNC_RISING;
	}else{
		dvp_dev->Init.HSPolarity = CSK_DVP_POL_CNTL_HSYNC_FALLING;
	}

	if(!config->vsync_polarity){
		dvp_dev->Init.VSPolarity = CSK_DVP_POL_CNTL_VSYNC_RISING;
	}else{
		dvp_dev->Init.VSPolarity = CSK_DVP_POL_CNTL_VSYNC_FALLING;
	}

	if(!config->data_align){
		dvp_dev->Init.DataAlign = CSK_DVP_DATA_BUS_SEL_RIGHT;
	}else{
		dvp_dev->Init.DataAlign = CSK_DVP_DATA_BUS_SEL_LEFT;
	}

    dvp_dev->Init.HandshakingMode = CSK_DVP_JLB_HANSHK_SEL_MODE_SOFT;
    dvp_dev->Init.InputFormat = CSK_DVP_INPUT_FORM_LUMINA_8BIT;
	dvp_dev->Init.LineOffset = 0;
    dvp_dev->Init.PixelOffset = 0;
	dvp_dev->Init.FrameWidth = fmt->pitch;
	dvp_dev->Init.FrameHeight = fmt->height;

	if(CSK_DRIVER_OK != DVP_Init(dvp_dev, dvp_callback))
    {
		LOG_ERR("DVP failed to set fmt\r\n");
		return -EIO;
    }

	if (data->sensor_dev && video_set_format(data->sensor_dev, ep, fmt)) {
		return -EIO;
	}

	memcpy(&data->fmt, fmt, sizeof(struct video_format));

	return 0;
}

static int video_csk6_dvp_get_fmt(const struct device *dev,
				  enum video_endpoint_id ep,
				  struct video_format *fmt)
{
	struct video_csk6_dvp_data *data = dev->data;

	if (fmt == NULL || ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	fmt->width = data->fmt.width;
	fmt->height = data->fmt.height;
	fmt->pitch = data->fmt.pitch;
	fmt->pixelformat = data->fmt.pixelformat;

	return 0;
}

static int check_buf_addr(uint32_t addr)
{
	uint32_t psram_ap_addr = DT_REG_ADDR(DT_NODELABEL(psram_ap));
	uint32_t psram_ap_len = DT_REG_SIZE(DT_NODELABEL(psram_ap));

	//目前只支持PSRAM作为数据缓存
	if((addr >= psram_ap_addr) && (addr < (psram_ap_addr + psram_ap_len))){
		return 1;
	}

	return 0;
}

static int csk6_dvp_start(const struct device *dev, struct video_buffer *vbuf)
{
	struct video_csk6_dvp_data *data = dev->data;
	DVP_HandleTypeDef *dvp_dev = data->handle;
	DVP_BufferDef dvp_buf;
	int ret = 0;

	vbuf->bytesused = data->fmt.pitch * data->fmt.height;

	memset(&dvp_buf, 0 ,sizeof(DVP_BufferDef));

	//DVP驱动访问的是CP侧的AHB总线，如果在AP侧使用DVP驱动，传入的地址必须重新映射
	dvp_buf.pAddrY = (uint8_t *)DVP_PSRAM_ADDR_WRAP(vbuf->buffer);
	dvp_buf.bufLenY = vbuf->size;

	SCB_InvalidateDCache_by_Addr((void *)vbuf->buffer, vbuf->bytesused);

	ret = DVP_Start(dvp_dev, dvp_buf);
	if(ret != 0){
		LOG_ERR("dvp satrt faild \r\n");
		return -EIO; 
	}

	return 0;
} 


static int video_csk6_dvp_stream_start(const struct device *dev)
{
	struct video_csk6_dvp_data *data = dev->data;
	struct video_buffer *vbuf;

	vbuf = k_fifo_get(&data->fifo_in, K_NO_WAIT);
	if(NULL == vbuf){
		LOG_ERR("fifo_in is empty \r\n");
		return -EIO;
	}

	csk6_dvp_start(dev, vbuf);

	set_current_vbuf(dev, vbuf);

	if (data->sensor_dev && video_stream_start(data->sensor_dev)) {
		return -EIO;
	}

	return 0;
}

static int video_csk6_dvp_stream_stop(const struct device *dev)
{
	struct video_csk6_dvp_data *data = dev->data;
	DVP_HandleTypeDef *dvp_dev = data->handle;

	if (data->sensor_dev && video_stream_stop(data->sensor_dev)) {
		return -EIO;
	}

	DVP_Stop(dvp_dev);

	return 0;
}

static int video_csk6_dvp_flush(const struct device *dev,
				enum video_endpoint_id ep,
				bool cancel)
{
	struct video_csk6_dvp_data *data = dev->data;
	struct video_buffer *vbuf;	

	if (!cancel) {
		/* wait for all buffer to be processed */
		do {
			k_sleep(K_MSEC(1));
		} while (!k_fifo_is_empty(&data->fifo_in));
	} else {
		vbuf = get_current_vbuf(dev);
		if(vbuf){
			k_fifo_put(&data->fifo_out, vbuf);
		}

		while ((vbuf = k_fifo_get(&data->fifo_in, K_NO_WAIT))) {
			k_fifo_put(&data->fifo_out, vbuf);
		}
	}

	return 0;
}

static int video_csk6_dvp_enqueue(const struct device *dev,
				  enum video_endpoint_id ep,
				  struct video_buffer *vbuf)
{
	struct video_csk6_dvp_data *data = dev->data;

	//The hardware design requires address 4-byte alignment
	if((uint32_t)vbuf->buffer % 4 != 0){
		LOG_ERR("enqueue buf must be 4-byte aligned \r\n");
		return -EINVAL;
	}

	if(!check_buf_addr((uint32_t)vbuf->buffer)){
		LOG_ERR("err bus addr 0x%x\r\n", (uint32_t)vbuf->buffer);
		return -EINVAL;
	}

	k_fifo_put(&data->fifo_in, vbuf);

	return 0;
}

static inline int video_csk6_dvp_set_ctrl(const struct device *dev,
					  unsigned int cid,
					  void *value)
{
	struct video_csk6_dvp_data *data = dev->data;
	int ret = -ENOTSUP;

	/* Forward to sensor dev if any */
	if (data->sensor_dev) {
		ret = video_set_ctrl(data->sensor_dev, cid, value);
	}

	return ret;
}

static inline int video_csk6_dvp_get_ctrl(const struct device *dev,
					  unsigned int cid,
					  void *value)
{
	struct video_csk6_dvp_data *data = dev->data;
	int ret = -ENOTSUP;

	/* Forward to sensor dev if any */
	if (data->sensor_dev) {
		ret = video_get_ctrl(data->sensor_dev, cid, value);
	}

	return ret;
}

static int video_csk6_dvp_dequeue(const struct device *dev,
				  enum video_endpoint_id ep,
				  struct video_buffer **vbuf,
				  k_timeout_t timeout)
{
	struct video_csk6_dvp_data *data = dev->data;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	*vbuf = k_fifo_get(&data->fifo_out, timeout);
	if (*vbuf == NULL) {
		return -EAGAIN;
	}

	return 0;
}

static int video_csk6_dvp_get_caps(const struct device *dev,
				   enum video_endpoint_id ep,
				   struct video_caps *caps)
{
	struct video_csk6_dvp_data *data = dev->data;
	int err = -ENODEV;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	/* Just forward to sensor dev for now */
	if (data->sensor_dev) {
		err = video_get_caps(data->sensor_dev, ep, caps);
	}

	//the minimal count of video buffers to enqueue before being able to start the stream.
	caps->min_vbuf_count = 2;

	/* no sensor dev */
	return err;
}

extern void DVP_IRQ_Handler();
static void video_csk6_dvp_isr(void)
{
	DVP_IRQ_Handler();
}

static int video_csk6_dvp_init(const struct device *dev)
{
	const struct video_csk6_dvp_config *config = dev->config;
	struct video_csk6_dvp_data *data = dev->data;
	DVP_HandleTypeDef *dvp_dev = data->handle;
	int ret = 0;

	k_fifo_init(&data->fifo_in);
	k_fifo_init(&data->fifo_out);

	if(config->clock_prescaler != 0){
		DVP_EnableClockout(dvp_dev, config->clock_prescaler - 1);
	}

	/* check if there is any sensor device (video ctrl device) */
	if (config->sensor_label) {
		data->sensor_dev = device_get_binding(config->sensor_label);
		if (data->sensor_dev == NULL) {
			LOG_ERR("Failed to get  video ctrl device\r\n");
			return -ENODEV;
		}
	}

	//some SENSOR can communicate with I2C only when DVP interface outputs PCLK first
	if (data->sensor_dev) {
		ret = video_set_ctrl(data->sensor_dev, 0, 0);
	}

	return 0;
}

extern void HAL_CP_Interrupt_Handler();
static void csk_cp2ap_isr(const struct device *dev)
{
	ARG_UNUSED(dev);
	HAL_CP_Interrupt_Handler();
}
static const struct video_driver_api video_csk6_dvp_driver_api = {
	.set_format = video_csk6_dvp_set_fmt,
	.get_format = video_csk6_dvp_get_fmt,
	.stream_start = video_csk6_dvp_stream_start,
	.stream_stop = video_csk6_dvp_stream_stop,
	.flush = video_csk6_dvp_flush,
	.enqueue = video_csk6_dvp_enqueue,
	.dequeue = video_csk6_dvp_dequeue,
	.set_ctrl = video_csk6_dvp_set_ctrl,
	.get_ctrl = video_csk6_dvp_get_ctrl,
	.get_caps = video_csk6_dvp_get_caps,
};

static int video_csk6_dvp_init_0(const struct device *dev)
{
	struct video_csk6_dvp_data *data = dev->data;
	const struct video_csk6_dvp_config *config = dev->config;

	HAL_CP_Interrupt_RegisterISR(IRQ_CP_VIC_VECTOR, video_csk6_dvp_isr, NULL);
	HAL_CP_Interrupt_Enable(IRQ_CP_VIC_VECTOR);
	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
		    csk_cp2ap_isr, NULL, 0);

	irq_enable(DT_INST_IRQN(0));

	data->dev = dev;
	data->handle = DVP_Instance();

	PINCTRL_CONFIG_PINS(config->pinctrl_list, config->pinctrl_list_size);

	return video_csk6_dvp_init(dev);
}

static const struct csk6_pinctrl_cfg                                       						\
		dvp_alts[DT_INST_NUM_PINCTRLS_BY_IDX(0, 0)] =          									\
			CSK6_DT_ALT_ITEMS_LIST(0); 

static const struct video_csk6_dvp_config video_csk6_dvp_config_0 = {
	.sensor_label = DT_INST_PROP(0, sensor_label),
	.data_align = DT_ENUM_IDX(DT_DRV_INST(0), data_align_type),
	.pclk_polarity = DT_ENUM_IDX(DT_DRV_INST(0), pclk_polarity),
	.hsync_polarity = DT_ENUM_IDX(DT_DRV_INST(0), hsync_polarity),
	.vsync_polarity = DT_ENUM_IDX(DT_DRV_INST(0), vsync_polarity),
	
#if DT_NODE_HAS_PROP(DT_DRV_INST(0), clock_prescaler)
	.clock_prescaler = DT_INST_PROP(0, clock_prescaler),
#endif
	.pinctrl_list = dvp_alts, 													   
	.pinctrl_list_size = ARRAY_SIZE(dvp_alts),	
};

static struct video_csk6_dvp_data video_csk6_dvp_data_0;

DEVICE_DT_INST_DEFINE(0, &video_csk6_dvp_init_0,
		    NULL, &video_csk6_dvp_data_0,
		    &video_csk6_dvp_config_0,
		    POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY,
		    &video_csk6_dvp_driver_api);