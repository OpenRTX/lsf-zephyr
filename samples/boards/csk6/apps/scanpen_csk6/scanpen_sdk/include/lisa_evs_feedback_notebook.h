/*
 * ================================================================
 *
 *			Filename:	lisa_evs_feedback_notebook.h
 *
 *		 Description:	lisa_evs_feedback_notebook header file
 *
 *			 Version:	1.0
 *			 Created:	03/28/2022 19:46:55
 *			Revision:	None
 *
 *			  Author:	dyye@listenai.com
 *			 Company:	www.listenai.com
 *
 * =================================================================
 */
#ifndef __LISA_EVS_FEEDBACK_NOTEBOOK_H__
#define __LISA_EVS_FEEDBACK_NOTEBOOK_H__

#include "lisa_evs_error.h"
#include "lisa_evs.h"

/**
 * @brief 反馈、生词本通用结构体
 */
typedef struct {
	/*文本ID*/
	uint32_t id;

	/*文本类型*/
	uint8_t type[20];

	/*文本内容类型*/
	uint8_t content_type[20];

	/*文本内容*/
	uint8_t message[100];
} lisa_text_info_t;

/**
 * @brief	lisa feedbak sync from local to air, and delete local file
 *
 * @param handle	evs handle
 * @param cb	user callback
 *
 * @retval	OK or FAIL
 */
lisa_err_t lisa_feedback_sync_text(const lisa_evs_t *const handle, const lisa_http_resp_cb cb);
/**
 * @brief	lisa feedback upload
 *
 * @param handle	evs handler
 * @param isonline	upload to server or save to local
 * @param feedback_info	upload data
 * @param info_num	data info num
 * @param cb	user callback
 *
 * @retval	OK of FAIL
 */
lisa_err_t lisa_feedback_upload(const lisa_evs_t *const handle, const uint8_t isonline,
		const void *feedback_info, const uint8_t info_num, const lisa_http_resp_cb cb);

/**
 * @brief	upload user notebook
 *
 * @param handle	evs handler
 * @param isonline	upload to air or save to local
 * @param notebook_info	notebook info for upload
 * @param info_num info num
 * @param cb	user callback
 *
 * @retval	OK or FAIL
 */
lisa_err_t lisa_notebook_create(const lisa_evs_t *const handle, const uint8_t isonline,
		const void *notebook_info, const uint8_t info_num, const lisa_http_resp_cb cb);

/**
 * @brief	get notebook list
 *
 * @param handle	evs handler
 * @param isonline	getlist notebook from air or local
 * @param page_num
 * @param page_size
 * @param type	get word or text
 * @param cb	user callback
 *
 * @retval	OK or FAIL
 */
lisa_err_t lisa_notebook_get_list(const lisa_evs_t *const handle, const uint8_t isonline,
		const uint32_t page_num, const uint32_t page_size, const uint8_t *const type,
		const lisa_http_resp_cb cb);
/**
 * @brief	delete notebook info
 *
 * @param handle	evs handler
 * @param id_table	the id table of delete info
 * @param id_cnt	delete info cnt
 * @param cb	user callback
 *
 * @retval	OK or FAILED
 */
lisa_err_t lisa_notebook_delete(const lisa_evs_t *const handle, const uint32_t *const id_table,
		const uint32_t id_cnt, const lisa_http_resp_cb cb);

/**
 * @brief	lisa notebook sync fron local to air, and delete local data
 *
 * @param handle evs handle
 * @param cb	user callback
 *
 * @retval	OK or FAIL
 */
lisa_err_t lisa_notebook_sync_text(const lisa_evs_t *const handle, const lisa_http_resp_cb cb);

#endif  // end of __LISA_EVS_FEEDBACK_NOTEBOOK_H__
