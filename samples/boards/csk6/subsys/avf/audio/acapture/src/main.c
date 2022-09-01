/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <avf/framework/avf_platform.h>
#include <avf/modules/audio/acapture.h>
#include <avf/slogger.h>
#include "dsp_resource.h"

#define ACAPTURE_DATA_EVENT_THRES (1024)
#define ACAPTURE_DATA_TOTAL_LENGTH (1024 * 100)
#define ACAPTURE_MSGQ_NUMBER (10)
typedef struct {
	char *pdata;
	int datalen;
} msgq_data_t;

struct k_msgq acapture_msgq;

static void acapture_data_process(void *priv, acapture_event_msg_t msg)
{
	acapture_t *handle = (acapture_t *)priv;
	msgq_data_t msgq_data;

	uint32_t avail;
	int32_t readlen;
	int ret = 0;

	if (msg.event_flags & AVF_STREAM_EVENT_THRES_NOTIFY) {
		avail = msg.value.value;
		msgq_data.pdata = k_malloc(sizeof(char) * avail);
		if (msgq_data.pdata == NULL) {
			SLOGE(LOG_TAG, "[%s %d]k_malloc FAILED!", __FUNCTION__, __LINE__);
			return;
		}
		readlen = acap_get_datai(handle, msgq_data.pdata, avail);
		if (readlen > 0) {
			msgq_data.datalen = readlen;
			ret = k_msgq_put(&acapture_msgq, &msgq_data, K_NO_WAIT);
			if (ret) {
				k_free(msgq_data.pdata);
				SLOGE(LOG_TAG, "[%s %d]msgq message dropped", __FUNCTION__,
				      __LINE__);
			}
		} else {
			k_free(msgq_data.pdata);
		}
	}
}

void main(void)
{
	acapture_t *acapture = NULL;
	amedia_fmt_t fmt = {
		.rate = 16000,
		.channels = 1,
		.bits = 16,
		.compr = AMEDIA_COMPR_PCM,
	};
	uint32_t recv_bytes = 0;
	int iret = 0;
	static char buffer[sizeof(msgq_data_t) * ACAPTURE_MSGQ_NUMBER];
	msgq_data_t msgq_data;

	avf_stream_platform_register(dsp_firmware, sizeof(dsp_firmware), dsp_tplg,
				     sizeof(dsp_tplg));

	k_msgq_init(&acapture_msgq, buffer, sizeof(msgq_data_t), ACAPTURE_MSGQ_NUMBER);

	/*step1: create acapture instance*/
	acapture = acap_create();
	if (acapture == NULL) {
		printk("acap_create failed!\n");
		return;
	}
	printk("Create acapture success\n");

	/*step2: set callback function*/
	if (0 != (iret = acap_event_register(acapture, AVF_STREAM_EVENT_THRES_NOTIFY,
					     acapture_data_process, acapture,
					     ACAPTURE_DATA_EVENT_THRES))) {
		printk("Register data event callback failed,iret %d\n",iret);
		return;
	}
	printk("Register data event callback success.\n");

	/*step3: set acapture paramter*/
	if (0 != (iret = acap_set_fmt(acapture, &fmt))) {
		printk("acap_set_fmt failed!\n");
		return;
	}
	printk("Set audio fmt<%d %d %d> to acapture success\n", fmt.rate, fmt.channels, fmt.bits);

	/*step4: start capture*/
	if (0 != (iret = acap_start(acapture))) {
		printk("acap_start failed.\n");
		return;
	}
	printk("Trigger capture start success.\n");

	/*step5: get audio data*/
	for (;;) {
		k_msgq_get(&acapture_msgq, &msgq_data, K_FOREVER);
		printk("Get audio data length %d \n", msgq_data.datalen);
		k_free(msgq_data.pdata);
		recv_bytes += msgq_data.datalen;
		if (recv_bytes >= ACAPTURE_DATA_TOTAL_LENGTH) {
			break;
		}
	}
	/*step6: stop acapture*/
	acap_stop(acapture);

	/*step7: destroy aplayer*/
	if (0 != (iret = acap_destroy(acapture))) {
		printk("aplay_destroy failed.\n");
		goto _END;
	}
	printk("acapture run compelete and exit.\n");
_END:
	/*clear message queue*/
	for (;;) {
		if (0 != k_msgq_get(&acapture_msgq, &msgq_data, K_NO_WAIT)) {
			break;
		}
		k_free(msgq_data.pdata);
	}
}
