/*
 * ================================================================
 *
 *			Filename:	lisa_evs_feedback_notebook.h
 *
 *		 Description:	lisa_evs_feedback_notebook header file
 *
 *			 Version:	1.0
 *			 Created:	03/28/2022 19:30:24
 *			Revision:	None
 *
 *			  Author:	dyye@listenai.com
 *			 Company:	www.listenai.com
 *
 * =================================================================
 */

#include <string.h>
#include <fs/fs.h>
#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_http.h>
#include <lisa_typedef.h>

#include "utils/evs_utils.h"
#include "lisa_evs_feedback_notebook.h"
#include "lisa_evs_uuid.h"
#include "sockets/lisa_evs_websocket.h"

#define TAG "lisa_evs_feedback_notebook"

#define URL_FEEDBACK ("http://staging-api.iflyos.cn/external/ocr_process/ocr/feedback")
#define URL_NOTEBOOK_CREATE ("http://staging-api.iflyos.cn/external/ocr_tool/notebook/create")
#define URL_NOTEBOOK_GET_LIST ("http://staging-api.iflyos.cn/external/ocr_tool/notebook/get_list")
#define URL_NOTEBOOK_DELETE ("http://staging-api.iflyos.cn/external/ocr_tool/notebook/delete")

#define HTTP_TIME_OUT (10 * 1000)

#define LISA_FEEDBACK_LOCAL_FILE "/lfs1/feedback.json"
#define LISA_NOTEBOOK_LOCAL_FILE "/lfs1/notebook.json"
#define LISA_FEEDBACK_LOCAL_MAX (10)
#define LISA_NOTEBOOK_LOCAL_WORD_MAX (50)
#define LISA_NOTEBOOK_LOCAL_TEXT_MAX (10)

static void *s_access_token = NULL;

/**
 * @brief	get notebook http request headers
 *
 * @retval  header string
 */
static void *_get_notebook_headers(void)
{
	static char header[512];
	sprintf(header, "Authorization:Bearer %s\r\nContent-Type:application/json;charset=utf-8",
			s_access_token);
	return header;
}

/**
 * @brief	get feedback http request headers
 *
 * @retval	header string
 */
static void *_get_feedback_headers(void)
{
	static char header[512];
	sprintf(header, "Authorization:Bearer %s\r\nContent-Type:application/json", s_access_token);
	return header;
}

/**
 * @brief	http data callback, pass data to user callback
 *
 * @param data	http data
 */
static void _get_evaluate_data_cb(lisa_http_data_t *data)
{
	lisa_http_resp_cb cb = (lisa_http_resp_cb)data->user;
	cb(data->buf, data->len);
}

/**
 * @brief	http event callback
 *
 * @param event	event type
 */
static void _get_evaluate_event_cb(lisa_http_event_t *event)
{
	LISA_LOGD(TAG, "_get_evaluate_event_cb event->what [%d]", event->what);
}

/**
 * @brief	http request common operate by json data
 *
 * @param req_body	request data
 * @param cb	user callback
 * @param method	http method
 * @param http_url	http url
 * @param headers	http get headers callback
 *
 * @retval	OK or FAIL
 */
static lisa_err_t _http_common_request_json(const uint8_t *const req_body,
		const lisa_http_resp_cb cb, const lisa_http_method_e method, const uint8_t *const http_url,
		const void *const headers)
{
	lisa_http_request_t *http_param = NULL;
	http_param = lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (!http_param) return LISA_FAIL;

	const uint32_t url_len = strlen(http_url);
	unsigned char *url = (unsigned char *)lisa_mem_alloc(url_len + 1);
	if (!url) {
		lisa_mem_free(http_param);
		return LISA_FAIL;
	}
	memcpy(url, http_url, url_len + 1);

	http_param->method = method;
	http_param->url = url;
	http_param->headers = (void *)headers;
	http_param->timeout_ms = HTTP_TIME_OUT;
	http_param->body_len = strlen(req_body);
	http_param->body = req_body;
	http_param->user = cb;
	http_param->on_data = (void *)_get_evaluate_data_cb;
	http_param->on_event = (void *)_get_evaluate_event_cb;

	lisa_http_t *http_req = lisa_http_init(http_param);
	if (!http_req) {
		LISA_LOGE(TAG, "http init error");
		lisa_mem_free(url);
		lisa_mem_free(http_param);
		return LISA_FAIL;
	}

	lisa_err_t ret = LISA_OK;
	if (lisa_http_perform(http_req) != LISA_HTTP_OK) {
		LISA_LOGE(TAG, "http get request info err..");
		ret = LISA_FAIL;
	}

	lisa_http_cleanup(http_req);
	lisa_mem_free(url);
	lisa_mem_free(http_param);

	return ret;
}

/**
 * @brief	get size of file_name
 *
 * @param file_name	file name
 *
 * @retval file size
 */
static long _get_file_size(const char *const file_name)
{
	struct fs_file_t fp;
	fs_file_t_init(&fp);

	int err = fs_open(&fp, file_name, FS_O_READ);
	if (err) return 0;
	fs_seek(&fp, 0, FS_SEEK_END);
	long size = fs_tell(&fp);
	fs_close(&fp);

	return size;
}

/**
 * @brief	read specified file data
 *
 * @param file_name	file name
 *
 * @retval	NULL or buf pointer
 */
static char *_load_file_to_buf(const char *const file_name)
{

	struct fs_file_t fp;
	fs_file_t_init(&fp);

	int err = fs_open(&fp, file_name, FS_O_READ);
	if (err) {
		LISA_LOGE(TAG, "open %s failed\n", file_name);
		goto _err_open;
	}

	fs_seek(&fp, 0, FS_SEEK_END);
	const long file_size = fs_tell(&fp);
	if (file_size <= 0) goto _err_size;
	fs_seek(&fp, 0, FS_SEEK_SET);

	char *data_buf = lisa_mem_calloc(1, file_size);
	if (data_buf == NULL) {
		goto _err_alloc;
	}

	long read_size = fs_read(&fp, data_buf, file_size);
	if (read_size != file_size) {
		LISA_LOGE(TAG, "read %s failed: size %d actual %d\n", file_name, file_size, read_size);
		goto _err_read;
	}
	fs_close(&fp);

	return data_buf;
_err_read:
	lisa_mem_free(data_buf);
_err_alloc:
_err_size:
	fs_close(&fp);
_err_open:
	return NULL;
}

/**
 * @brief	cleanup specified file
 *
 * @param file_name	file name
 *
 * @retval	OK or FAIL
 */
static lisa_err_t _truncate_file(const char *const file_name)
{
	if (fs_unlink(file_name)) {
		LISA_LOGE(TAG, "delete %s failed\n", file_name);
		return LISA_FAIL;
	}

	return LISA_OK;
}

/**
 * @brief	save data to specified file
 *
 * @param req_body	save data
 *
 * @retval	OK or FAIL
 */
static lisa_err_t _saved_to_file(const char *const file_name, const uint8_t *const data)
{

	struct fs_file_t fp;
	fs_file_t_init(&fp);

	int err = fs_open(&fp, file_name, FS_O_WRITE | FS_O_CREATE);
	if (err) {
		LISA_LOGE(TAG, "open %s failed\n", file_name);
		return LISA_FAIL;
	}

	int data_len = strlen(data);
	if (fs_write(&fp, data, data_len) != data_len) {
		fs_close(&fp);
		return LISA_FAIL;
	}
	fs_close(&fp);

	return LISA_OK;
}

/**
 * @brief	save notebook data to local file
 *
 * @param child_array	child array
 *
 * @retval OK or FAIL
 */
static lisa_err_t _notebook_save_to_local(const cJSON *const child_array)
{
	lisa_err_t ret = LISA_OK;
	uint32_t child_word_cnt = 0, child_text_cnt = 0;
	uint32_t local_word_cnt = 0, local_text_cnt = 0;
	cJSON *local_json_root = NULL, *local_json_array = NULL;
	cJSON *child_item = NULL, *child_param = NULL;

	const uint32_t child_cnt = cJSON_GetArraySize(child_array);
	for (int i = 0; i < child_cnt; i++) {
		child_item = cJSON_GetArrayItem(child_array, i);
		child_param = cJSON_GetObjectItem(child_item, "type");
		if (child_param && child_param->valuestring) {
			if (strcmp(child_param->valuestring, "word") == 0) child_word_cnt++;
			if (strcmp(child_param->valuestring, "text") == 0) child_text_cnt++;
		}
	}

	if ((child_word_cnt > LISA_NOTEBOOK_LOCAL_WORD_MAX) ||
			(child_text_cnt > LISA_NOTEBOOK_LOCAL_TEXT_MAX) ||
			((child_word_cnt == 0) && (child_text_cnt == 0)))
		goto _err_child_cnt;

	char *local_file_buf = _load_file_to_buf(LISA_NOTEBOOK_LOCAL_FILE);
	if (local_file_buf != NULL) {
		local_json_root = cJSON_Parse(local_file_buf);
		if (!local_json_root) goto _err_parse;

		local_json_array = cJSON_GetObjectItem(local_json_root, "note");
		if (!local_json_array) goto _err_getob;

		const uint32_t local_array_cnt = cJSON_GetArraySize(local_json_array);
		for (int i = 0; i < local_array_cnt; i++) {
			cJSON *local_item = cJSON_GetArrayItem(local_json_array, i);
			cJSON *local_param = cJSON_GetObjectItem(local_item, "ty");
			if (local_param && local_param->valuestring) {
				if (strcmp(local_param->valuestring, "word") == 0) local_word_cnt++;
				if (strcmp(local_param->valuestring, "text") == 0) local_text_cnt++;
			}
		}

		// printf("lword %d cword %d ltext %d ctext %d\n", local_word_cnt, child_word_cnt,
		//		local_text_cnt, child_text_cnt);
		if ((local_word_cnt + child_word_cnt > LISA_NOTEBOOK_LOCAL_WORD_MAX) ||
				(local_text_cnt + child_text_cnt > LISA_NOTEBOOK_LOCAL_TEXT_MAX))
			goto _err_local_cnt;

	} else {
		LISA_LOGD(TAG, "Create file %s\n", LISA_NOTEBOOK_LOCAL_FILE);
		local_json_array = cJSON_CreateArray();
	}

	for (int i = 0; i < child_cnt; i++) {
		child_item = cJSON_GetArrayItem(child_array, i);
		child_param = cJSON_GetObjectItem(child_item, "type");
		cJSON *child_object = cJSON_CreateObject();
		if (child_param && child_param->valuestring) {
			cJSON_AddStringToObject(child_object, "ty", child_param->valuestring);
		}
		child_param = cJSON_GetObjectItem(child_item, "language_type");
		if (child_param && child_param->valuestring) {
			cJSON_AddStringToObject(child_object, "lty", child_param->valuestring);
		}
		child_param = cJSON_GetObjectItem(child_item, "content");
		if (child_param && child_param->valuestring) {
			cJSON_AddStringToObject(child_object, "cnt", child_param->valuestring);
		}

		cJSON_AddItemToArray(local_json_array, child_object);
	}

	cJSON *saved_root = cJSON_CreateObject();
	cJSON_AddItemToObject(saved_root, "note", local_json_array);
	char *saved_body = cJSON_PrintUnformatted(saved_root);
	_saved_to_file(LISA_NOTEBOOK_LOCAL_FILE, saved_body);

	cJSON_DetachItemFromObject(saved_root, "note");
	cJSON_free(saved_body);
	cJSON_Delete(saved_root);
	if (local_json_root)
		cJSON_Delete(local_json_root);
	else
		cJSON_Delete(local_json_array);
	if (local_file_buf) lisa_mem_free(local_file_buf);

	return LISA_OK;

_err_local_cnt:
_err_getob:
	if (local_json_root) cJSON_Delete(local_json_root);
	if (local_file_buf) lisa_mem_free(local_file_buf);
_err_parse:
_err_child_cnt:
	return LISA_FAIL;
}

/**
 * @brief	noteboot offline get list
 *
 * @param page_num	get page num
 * @param page_size	page size
 * @param type	get type
 * @param cb	data cb
 *
 * @retval	OK or FAIL
 */
lisa_err_t _notebook_getlist_offline(const uint32_t page_num, const uint32_t page_size,
		const uint8_t *const type, const lisa_http_resp_cb cb)
{
	lisa_err_t ret = LISA_OK;
	cJSON *json_item = NULL;
	cJSON *json_obj = NULL;
	cJSON *json_param = NULL;

	// printf("page_num %d--page_size %d--type %s-\n", page_num, page_size, type);
	char *local_file_buf = _load_file_to_buf(LISA_NOTEBOOK_LOCAL_FILE);
	if (local_file_buf == NULL) goto _err_load;

	cJSON *local_json_root = cJSON_Parse(local_file_buf);
	if (!local_json_root) goto _err_parse;
	cJSON *local_json_array = cJSON_GetObjectItem(local_json_root, "note");
	if (!local_json_array) goto _err_getob;

	cJSON *spec_json_array = cJSON_CreateArray();

	//将所有type 类型的item放进 pJsonTarray里
	const uint32_t local_array_size = cJSON_GetArraySize(local_json_array);
	for (int i = 0; i < local_array_size; i++) {
		json_item = cJSON_GetArrayItem(local_json_array, i);
		json_param = cJSON_GetObjectItem(json_item, "ty");
		if (json_param && json_param->valuestring && (strcmp(json_param->valuestring, type) == 0)) {
			json_obj = cJSON_CreateObject();  // create a json object
			cJSON_AddStringToObject(json_obj, "type", json_param->valuestring);

			json_param = cJSON_GetObjectItem(json_item, "lty");
			if (json_param && json_param->valuestring) {
				cJSON_AddStringToObject(json_obj, "language_type", json_param->valuestring);
			}

			json_param = cJSON_GetObjectItem(json_item, "cnt");
			if (json_param && json_param->valuestring) {
				cJSON_AddStringToObject(json_obj, "content", json_param->valuestring);
			}
			cJSON_AddItemToArray(spec_json_array, json_obj);  // add object to array
		}
	}

	//计算取值区间, 按照分页逻辑，将当前取得page数据 放进pJsonRarray
	const uint32_t spec_array_cnt = cJSON_GetArraySize(spec_json_array);
	if (spec_array_cnt == 0) goto _err_cnt;
	cJSON *get_json_array = cJSON_CreateArray();
	uint32_t page_num_total = spec_array_cnt / page_size + ((spec_array_cnt % page_size) ? 1 : 0);
	if (page_num > page_num_total) goto _err_page_num;
	uint32_t start_cnt = (page_num - 1) * page_size;
	uint32_t end_cnt =
			((page_num * page_size) > spec_array_cnt) ? local_array_size : (page_num * page_size);
	for (int i = start_cnt; i < end_cnt; i++) {
		json_item = cJSON_GetArrayItem(spec_json_array, i);
		json_obj = cJSON_CreateObject();  // create a json object
		json_param = cJSON_GetObjectItem(json_item, "type");
		cJSON_AddStringToObject(json_obj, "type", json_param->valuestring);
		json_param = cJSON_GetObjectItem(json_item, "language_type");
		cJSON_AddStringToObject(json_obj, "language_type", json_param->valuestring);
		json_param = cJSON_GetObjectItem(json_item, "content");
		cJSON_AddStringToObject(json_obj, "content", json_param->valuestring);
		cJSON_AddItemToArray(get_json_array, json_obj);  // add object to array
	}

	cJSON *json_root = cJSON_CreateObject();  // create a json array root
	cJSON_AddItemToObject(json_root, "notebook", get_json_array);
	cJSON_AddNumberToObject(json_root, "page", page_num);
	cJSON_AddNumberToObject(json_root, "size", page_size);
	cJSON_AddNumberToObject(json_root, "total", local_array_size);
	char *get_body = cJSON_Print(json_root);
	//	printf("%s \n", req_body);

	cb(get_body, strlen(get_body));

	cJSON_free(get_body);
	cJSON_free(json_root);
	cJSON_Delete(spec_json_array);
	cJSON_Delete(local_json_root);
	lisa_mem_free(local_file_buf);

	return LISA_OK;
_err_page_num:
	cJSON_Delete(get_json_array);
_err_cnt:
	cJSON_Delete(spec_json_array);
_err_getob:
	cJSON_Delete(local_json_root);
_err_parse:
	lisa_mem_free(local_file_buf);
_err_load:
	cb(NULL, 0);
	return LISA_FAIL;
}

/**
 * @brief	save feedback data to local file
 *
 * @param child_array	child array
 *
 * @retval OK or FAIL
 */
static lisa_err_t _feedback_save_to_local(const cJSON *const child_array)
{
	cJSON *local_json_root = NULL, *local_json_array = NULL;
	lisa_err_t ret = LISA_OK;

	const uint32_t child_cnt = cJSON_GetArraySize(child_array);
	if (child_cnt > LISA_FEEDBACK_LOCAL_MAX || child_cnt == 0) goto _err_add;

	char *local_file_buf = _load_file_to_buf(LISA_FEEDBACK_LOCAL_FILE);
	if (local_file_buf != NULL) {
		local_json_root = cJSON_Parse(local_file_buf);
		if (!local_json_root) goto _err_parse;

		local_json_array = cJSON_GetObjectItem(local_json_root, "fk");
		if (!local_json_array) goto _err_getob;

		const uint32_t local_array_cnt = cJSON_GetArraySize(local_json_array);
		if (child_cnt + local_array_cnt > LISA_FEEDBACK_LOCAL_MAX) goto _err_cnt;

	} else {
		LISA_LOGD(TAG, "Create file %s\n", LISA_FEEDBACK_LOCAL_FILE);
		local_json_array = cJSON_CreateArray();
	}

	for (int i = 0; i < child_cnt; i++) {
		cJSON *json_item = cJSON_GetArrayItem(child_array, i);
		cJSON *json_obj = cJSON_CreateObject();  // create a json object
		cJSON *json_param = cJSON_GetObjectItem(json_item, "type");
		if (json_param && json_param->valuestring) {
			cJSON_AddStringToObject(json_obj, "ty", json_param->valuestring);
		}
		json_param = cJSON_GetObjectItem(json_item, "content_type");
		if (json_param && json_param->valuestring) {
			cJSON_AddStringToObject(json_obj, "cty", json_param->valuestring);
		}
		json_param = cJSON_GetObjectItem(json_item, "message");
		if (json_param && json_param->valuestring) {
			cJSON_AddStringToObject(json_obj, "msg", json_param->valuestring);
		}

		cJSON_AddItemToArray(local_json_array, json_obj);  // add object to array
	}
	cJSON *json_root = cJSON_CreateObject();  // create a json array roota
	cJSON_AddItemToObject(json_root, "fk", local_json_array);
	char *req_body = cJSON_PrintUnformatted(json_root);
	// printf("req body: %s \n", req_body);

	_saved_to_file(LISA_FEEDBACK_LOCAL_FILE, req_body);

	cJSON_DetachItemFromObject(json_root, "fk");
	cJSON_free(req_body);
	cJSON_Delete(json_root);
	if (local_json_root)
		cJSON_Delete(local_json_root);
	else
		cJSON_Delete(local_json_array);
	if (local_file_buf) lisa_mem_free(local_file_buf);

	return LISA_OK;

_err_cnt:
_err_getob:
	if (local_json_root) cJSON_Delete(local_json_root);
_err_parse:
	if (local_file_buf) lisa_mem_free(local_file_buf);
_err_add:
	return LISA_FAIL;
}

/**
 * @brief	lisa feedbak sync from local to air, and delete local file
 *
 * @param handle	evs handle
 * @param cb	user callback
 *
 * @retval	OK or FAIL
 */
lisa_err_t lisa_feedback_sync_text(const lisa_evs_t *const handle, const lisa_http_resp_cb cb)
{
	if (!handle || !handle->access_token || !cb) return LISA_FAIL;
	s_access_token = handle->access_token;

	lisa_err_t ret = LISA_OK;

	cJSON *local_json_root, *local_json_array = NULL;

	// 1.读取本地文件
	char *local_file_buf = _load_file_to_buf(LISA_FEEDBACK_LOCAL_FILE);
	if (local_file_buf == NULL) {
		LISA_LOGE(TAG, "read %s failed \n", LISA_FEEDBACK_LOCAL_FILE);
		ret = LISA_FAIL;
		goto _err_load;
	}

	// 2.按照json格式解析出数组
	local_json_root = cJSON_Parse(local_file_buf);
	if (!local_json_root) {
		LISA_LOGE(TAG, "parse %s failed \n", LISA_FEEDBACK_LOCAL_FILE);
		ret = LISA_FAIL;
		goto _err_parse;
	}
	local_json_array = cJSON_GetObjectItem(local_json_root, "fk");
	if (!local_json_array) {
		LISA_LOGE(TAG, "%s get fk array failed \n", LISA_FEEDBACK_LOCAL_FILE);
		ret = LISA_FAIL;
		goto _err_getob;
	}

	// 3.json转换,缩写转成全称
	cJSON *sync_json_array = cJSON_CreateArray();
	const uint32_t local_array_cnt = cJSON_GetArraySize(local_json_array);
	for (int i = 0; i < local_array_cnt; i++) {
		cJSON *json_item = cJSON_GetArrayItem(local_json_array, i);
		cJSON *json_obj = cJSON_CreateObject();  // create a json object
		cJSON *json_param = cJSON_GetObjectItem(json_item, "ty");
		if (json_param && json_param->valuestring) {
			cJSON_AddStringToObject(json_obj, "type", json_param->valuestring);
		}
		json_param = cJSON_GetObjectItem(json_item, "cty");
		if (json_param && json_param->valuestring) {
			cJSON_AddStringToObject(json_obj, "content_type", json_param->valuestring);
		}

		char request_id[40] = {0};
		lisa_evs_uuid_generate_string(request_id);
		cJSON_AddStringToObject(json_obj, "request_id", request_id);

		json_param = cJSON_GetObjectItem(json_item, "msg");
		if (json_param && json_param->valuestring) {
			cJSON_AddStringToObject(json_obj, "message", json_param->valuestring);
		}

		cJSON_AddItemToArray(sync_json_array, json_obj);  // add object to array
	}
	cJSON *json_root = cJSON_CreateObject();
	cJSON_AddItemToObject(json_root, "feedback", sync_json_array);
	char *req_body = cJSON_PrintUnformatted(json_root);
	LISA_LOGD(TAG, "%s \n", req_body);

	// 4.发送http请求
	ret = _http_common_request_json(
			req_body, cb, LISA_HTTP_POST, URL_FEEDBACK, _get_feedback_headers);
	if (ret != LISA_OK) {
		LISA_LOGE(TAG, "online update failed \n");
	} else {
		// 5.清空本地文件
		LISA_LOGD(TAG, "_truncate_file %s ... \n", LISA_FEEDBACK_LOCAL_FILE);
		_truncate_file(LISA_FEEDBACK_LOCAL_FILE);
	}

	cJSON_free(req_body);
	cJSON_free(json_root);
_err_cnt:
_err_getob:
	cJSON_Delete(local_json_root);
_err_parse:
	lisa_mem_free(local_file_buf);
_err_load:
	return ret;
}

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
		const void *feedback_info, const uint8_t info_num, const lisa_http_resp_cb cb)
{
	if (!handle || !handle->access_token || !cb || !feedback_info) return LISA_FAIL;
	s_access_token = handle->access_token;

	lisa_err_t ret = LISA_OK;
	cJSON *json_array = cJSON_CreateArray();  // create a json array

	for (int cnt = 0; cnt < info_num; cnt++) {
		cJSON *json_obj = cJSON_CreateObject();  // create a json object
		cJSON_AddStringToObject(
				json_obj, "type", (((lisa_text_info_t *)feedback_info) + cnt)->type);
		cJSON_AddStringToObject(json_obj, "content_type",
				(((lisa_text_info_t *)feedback_info) + cnt)->content_type);

		char request_id[40] = {0};
		lisa_evs_uuid_generate_string(request_id);
		cJSON_AddStringToObject(json_obj, "request_id", request_id);
		cJSON_AddStringToObject(
				json_obj, "message", (((lisa_text_info_t *)feedback_info) + cnt)->message);

		cJSON_AddItemToArray(json_array, json_obj);  // add object to array
	}
	cJSON *json_root = cJSON_CreateObject();  // create a json array root
	cJSON_AddItemToObject(json_root, "feedback", json_array);
	char *req_body = cJSON_PrintUnformatted(json_root);
	LISA_LOGD(TAG, "%s \n", req_body);

	if (isonline) {
		ret = _http_common_request_json(
				req_body, cb, LISA_HTTP_POST, URL_FEEDBACK, _get_feedback_headers);
		if (ret != LISA_OK) {
			LISA_LOGE(TAG, "online update failed \n");
		}
	} else {
		ret = _feedback_save_to_local(json_array);
		if (ret != LISA_OK) {
			LISA_LOGE(TAG, "feedback offline save failed \n");
		}
		LISA_LOGD(TAG, "feedback offline save success \n");
	}

	cJSON_free(req_body);
	cJSON_Delete(json_root);

	return ret;
}

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
		const void *notebook_info, const uint8_t info_num, const lisa_http_resp_cb cb)
{
	if (!handle || !handle->access_token || !cb || !notebook_info) return LISA_FAIL;
	s_access_token = handle->access_token;

	lisa_err_t ret = LISA_OK;

	// create json request
	cJSON *json_array = cJSON_CreateArray();  // create a json array
	for (int cnt = 0; cnt < info_num; cnt++) {
		cJSON *json_obj = cJSON_CreateObject();  // create a json object
		cJSON_AddStringToObject(
				json_obj, "type", (((lisa_text_info_t *)notebook_info) + cnt)->type);
		cJSON_AddStringToObject(json_obj, "language_type",
				(((lisa_text_info_t *)notebook_info) + cnt)->content_type);
		cJSON_AddStringToObject(
				json_obj, "content", (((lisa_text_info_t *)notebook_info) + cnt)->message);
		cJSON_AddItemToArray(json_array, json_obj);  // add object to array
	}
	cJSON *json_root = cJSON_CreateObject();  // create a json array root
	cJSON_AddItemToObject(json_root, "notebook", json_array);
	char *req_body = cJSON_PrintUnformatted(json_root);
	LISA_LOGD(TAG, "%s \n", req_body);

	if (isonline) {
		ret = _http_common_request_json(
				req_body, cb, LISA_HTTP_POST, URL_NOTEBOOK_CREATE, _get_notebook_headers);
		if (ret != LISA_OK) {
			LISA_LOGE(TAG, "notebook online update failed \n");
		}
	} else {
		ret = _notebook_save_to_local(json_array);
		if (ret != LISA_OK) {
			LISA_LOGE(TAG, "note offline create failed \n");
		}
		LISA_LOGE(TAG, "notebook offline save success \n");
	}

	cJSON_free(req_body);
	cJSON_Delete(json_root);

	return ret;
}

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
		const lisa_http_resp_cb cb)
{
	if (!handle || !handle->access_token || !cb || !type) return LISA_FAIL;
	s_access_token = handle->access_token;

	lisa_err_t ret = LISA_OK;
	cJSON *json_obj = cJSON_CreateObject();
	cJSON_AddStringToObject(json_obj, "type", type);
	cJSON_AddNumberToObject(json_obj, "page", page_num);
	cJSON_AddNumberToObject(json_obj, "size", page_size);
	char *req_body = cJSON_PrintUnformatted(json_obj);
	LISA_LOGD(TAG, "%s \n", req_body);

	if (isonline) {
		ret = _http_common_request_json(
				req_body, cb, LISA_HTTP_POST, URL_NOTEBOOK_GET_LIST, _get_notebook_headers);
		if (ret != LISA_OK) {
			LISA_LOGE(TAG, "online update failed \n");
		}
	} else {
		ret = _notebook_getlist_offline(page_num, page_size, type, cb);
		if (ret != LISA_OK) {
			LISA_LOGE(TAG, "notebook offline getlist failed \n");
		}
		LISA_LOGD(TAG, "notebook offline getlist success \n");
		// TODO
	}

	cJSON_free(req_body);
	cJSON_Delete(json_obj);

	return ret;
}

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
		const uint32_t id_cnt, const lisa_http_resp_cb cb)
{
	if (!handle || !handle->access_token || !cb || !id_table) return LISA_FAIL;
	s_access_token = handle->access_token;

	lisa_err_t ret = LISA_OK;

	cJSON *idtable_obj = cJSON_CreateObject();
	cJSON_AddItemToObject(idtable_obj, "ids", cJSON_CreateIntArray(id_table, id_cnt));
	char *req_body = cJSON_Print(idtable_obj);
	LISA_LOGD(TAG, "%s \n", req_body);

	ret = _http_common_request_json(
			req_body, cb, LISA_HTTP_POST, URL_NOTEBOOK_DELETE, _get_notebook_headers);
	if (ret != LISA_OK) {
		LISA_LOGE(TAG, "online update failed \n");
	}

	cJSON_free(req_body);
	cJSON_Delete(idtable_obj);

	return ret;
}

/**
 * @brief	lisa notebook sync fron local to air, and delete local data
 *
 * @param handle evs handle
 * @param cb	user callback
 *
 * @retval	OK or FAIL
 */
lisa_err_t lisa_notebook_sync_text(const lisa_evs_t *const handle, const lisa_http_resp_cb cb)
{
	if (!handle || !handle->access_token || !cb) return LISA_FAIL;
	s_access_token = handle->access_token;

	lisa_err_t ret = LISA_OK;

	// 1.读取本地文件
	char *local_file_buf = _load_file_to_buf(LISA_NOTEBOOK_LOCAL_FILE);
	if (local_file_buf == NULL) {
		LISA_LOGE(TAG, "read %s failed \n", LISA_NOTEBOOK_LOCAL_FILE);
		ret = LISA_FAIL;
		goto _err_load;
	}

	// 2.按照json格式解析出数组
	cJSON *local_json_root = cJSON_Parse(local_file_buf);
	if (!local_json_root) {
		LISA_LOGE(TAG, "parse %s failed \n", LISA_NOTEBOOK_LOCAL_FILE);
		ret = LISA_FAIL;
		goto _err_parse;
	}
	cJSON *local_json_array = cJSON_GetObjectItem(local_json_root, "note");
	if (!local_json_array) {
		LISA_LOGE(TAG, "%s get note array failed \n", LISA_NOTEBOOK_LOCAL_FILE);
		ret = LISA_FAIL;
		goto _err_getob;
	}

	// 3.json转换,缩写转成全称
	cJSON *sync_json_array = cJSON_CreateArray();
	const uint32_t local_array_cnt = cJSON_GetArraySize(local_json_array);
	for (int i = 0; i < local_array_cnt; i++) {
		cJSON *json_item = cJSON_GetArrayItem(local_json_array, i);
		cJSON *json_obj = cJSON_CreateObject();  // create a json object
		cJSON *json_param = cJSON_GetObjectItem(json_item, "ty");
		if (json_param && json_param->valuestring) {
			cJSON_AddStringToObject(json_obj, "type", json_param->valuestring);
		}
		json_param = cJSON_GetObjectItem(json_item, "lty");
		if (json_param && json_param->valuestring) {
			cJSON_AddStringToObject(json_obj, "language_type", json_param->valuestring);
		}
		json_param = cJSON_GetObjectItem(json_item, "cnt");
		if (json_param && json_param->valuestring) {
			cJSON_AddStringToObject(json_obj, "content", json_param->valuestring);
		}
		cJSON_AddItemToArray(sync_json_array, json_obj);  // add object to array
	}
	cJSON *json_root = cJSON_CreateObject();  // create a json array roota
	cJSON_AddItemToObject(json_root, "notebook", sync_json_array);
	char *req_body = cJSON_PrintUnformatted(json_root);
	LISA_LOGD(TAG, "%s \n", req_body);

	// 4.发送http请求
	ret = _http_common_request_json(
			req_body, cb, LISA_HTTP_POST, URL_NOTEBOOK_CREATE, _get_notebook_headers);
	if (ret != LISA_OK) {
		LISA_LOGE(TAG, "online update failed \n");
	} else {
		// 5.清空本地文件
		LISA_LOGD(TAG, "_truncate_file %s ... \n", LISA_NOTEBOOK_LOCAL_FILE);
		_truncate_file(LISA_NOTEBOOK_LOCAL_FILE);
	}

	cJSON_free(req_body);
	cJSON_free(json_root);
_err_cnt:
_err_getob:
	cJSON_Delete(local_json_root);
_err_parse:
	lisa_mem_free(local_file_buf);
_err_load:
	return ret;
}
