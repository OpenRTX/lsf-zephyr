#include "lisa_evs_pref.h"
#include <lisa_mem.h>
#include <fs/fs.h>
#include <lisa_time.h>
#include "cjson/cJSON.h"
#include <string.h>
#include <lisa_log.h>
#include <shell/shell.h>
typedef enum {
	LISA_PREF_TYPE_INT = 0,
	LISA_PREF_TYPE_STRING,
	LISA_PREF_TYPE_BOOL,
} lisa_pref_type_e;

#define TAG "lisa_pref"

#define CONFIG_FILE_PATH ("/NAND:/config.json")
#define CONFIG_BACK_FILE_PATH ("/NAND:/config.json.back")
#define CONFIG_FILE_MAX_SIZE (4 * 1024)

static char *s_config_buffer = NULL;
static lisa_evs_pref_t *s_lisa_pref = NULL;

// update cfg and write to buffer
static bool _put_to_buffer(const char *k, const char *v_str, const int v_int, const bool v_bool,
		char *buffer, const lisa_pref_type_e type)
{
	cJSON *root = cJSON_Parse(buffer);
	bool ret = false;
	char *json_str = NULL;
	if (root) {
		if (cJSON_HasObjectItem(root, k)) {
			switch (type) {
				case LISA_PREF_TYPE_INT:
					cJSON_ReplaceItemInObject(root, k, cJSON_CreateNumber(v_int));
					break;
				case LISA_PREF_TYPE_STRING:
					cJSON_ReplaceItemInObject(root, k, cJSON_CreateString(v_str));
					break;
				case LISA_PREF_TYPE_BOOL:
					cJSON_ReplaceItemInObject(root, k, cJSON_CreateString(v_str));
					break;
				default:
					break;
			}
		} else {
			goto ADD_OBJ;
		}
		goto PRINT_JSON;
	} else {
		root = cJSON_CreateObject();
		if (root) {
			goto ADD_OBJ;
		} else {
			LISA_LOGE(TAG, "create json obj failed");
			return false;
		}
	}
ADD_OBJ:
	switch (type) {
		case LISA_PREF_TYPE_INT:
			cJSON_AddNumberToObject(root, k, v_int);
			break;
		case LISA_PREF_TYPE_STRING:
			cJSON_AddStringToObject(root, k, v_str);
			break;
		case LISA_PREF_TYPE_BOOL:
			cJSON_AddBoolToObject(root, k, v_bool);
			break;
		default:
			break;
	}
PRINT_JSON:
	json_str = cJSON_PrintUnformatted(root);
	if (strlen(json_str) <= CONFIG_FILE_MAX_SIZE) {
		strcpy(buffer, json_str);
		ret = true;
	} else {
		LISA_LOGE(TAG, "config json oversize");
	}
	cJSON_free(json_str);
	cJSON_Delete(root);

	return ret;
}

// read file
static void _read_file(struct fs_file_t *fp)
{
	fs_seek(fp, 0, FS_SEEK_END);
	const long cfg_file_size = fs_tell(fp);
	fs_seek(fp, 0, FS_SEEK_SET);
	memset(s_config_buffer, 0, CONFIG_FILE_MAX_SIZE + 1);
	fs_read(fp, s_config_buffer, cfg_file_size);
	s_config_buffer[cfg_file_size] = '\0';
}

// write buffer to file
static bool _write_to_file(const char *k, const char *v_str, const int v_int, const bool v_bool,
		const lisa_pref_type_e type, bool back_cfg)
{
	struct fs_file_t config_fp;
	fs_file_t_init(&config_fp);
	char *path = back_cfg ? CONFIG_BACK_FILE_PATH : CONFIG_FILE_PATH;
	int err = fs_open(&config_fp, path, FS_O_CREATE | FS_O_WRITE);
	if (!err) {
		// save cfg to buffer
		if (_put_to_buffer(k, v_str, v_int, v_bool, s_config_buffer, type)) {
			// write file
			fs_write(&config_fp, s_config_buffer, strlen(s_config_buffer));
			fs_close(&config_fp);
			if (back_cfg) {
				// delete config
				fs_unlink(CONFIG_FILE_PATH);
				// rename back
				fs_rename(CONFIG_BACK_FILE_PATH, CONFIG_FILE_PATH);
			}
		} else {
			LISA_LOGE(TAG, "%s %d put to buffer failed", __FUNCTION__, __LINE__);
			// close and delete file
			fs_close(&config_fp);
			fs_unlink(path);
			return false;
		}
		return true;
	} else {
		LISA_LOGE(TAG, "%s %d open cfg file failed", __FUNCTION__, __LINE__);
		return false;
	}
}

static lisa_err_t _lisa_pref_put(const char *k, const char *v_str, const int v_int,
		const bool v_bool, const lisa_pref_type_e type)
{
	if (!s_lisa_pref) return LISA_FAIL;
	lisa_mutex_lock(s_lisa_pref->m_mutex, LISA_WAIT_FOREVER);

	// read config.json
	struct fs_file_t fp;
	fs_file_t_init(&fp);
	int err = fs_open(&fp, CONFIG_FILE_PATH, FS_O_READ);
	if (err) {
		// read config failed try read config.json.back
		err = fs_open(&fp, CONFIG_BACK_FILE_PATH, FS_O_READ);
		if (err) {
			// read config.json.back failed and create config.json
			memset(s_config_buffer, 0, CONFIG_FILE_MAX_SIZE + 1);

			bool write_ret = _write_to_file(k, v_str, v_int, v_bool, type, false);
			if (write_ret)
				goto WRITE_SUCCESS;
			else
				goto OPEN_FILE_ERR;

		} else {
			// read config.json.back success
			goto WRITE_CONFIG;
		}
	} else {
		// read config.json success
		goto WRITE_CONFIG;
	}

WRITE_CONFIG:
	// read file
	_read_file(&fp);
	fs_close(&fp);

	bool write_ret = _write_to_file(k, v_str, v_int, v_bool, type, true);
	if (write_ret)
		goto WRITE_SUCCESS;
	else
		goto OPEN_FILE_ERR;

WRITE_SUCCESS:
	lisa_mutex_unlock(s_lisa_pref->m_mutex);
	return LISA_OK;

OPEN_FILE_ERR:
	LISA_LOGE(TAG, "write config failed");
	lisa_mutex_unlock(s_lisa_pref->m_mutex);
	return LISA_FAIL;
}

static bool _delete_to_buffer(const char *k, char *buffer)
{
	bool ret = false;
	cJSON *root = cJSON_Parse(buffer);
	if (root) {
		if (cJSON_HasObjectItem(root, k)) {
			cJSON_DeleteItemFromObject(root, k);
			char *json_str = cJSON_PrintUnformatted(root);
			if (strlen(json_str) < CONFIG_FILE_MAX_SIZE) {
				memset(buffer, 0, CONFIG_FILE_MAX_SIZE + 1);
				strcpy(buffer, json_str);
				ret = true;
			} else {
				LISA_LOGE(TAG, "config json oversize");
			}
			cJSON_free(json_str);
		}
		cJSON_Delete(root);
		return ret;
	} else {
		return false;
	}
}

static bool _delete_to_file(const char *k, bool back_cfg)
{
	struct fs_file_t config_fp;
	fs_file_t_init(&config_fp);
	char *path = back_cfg ? CONFIG_BACK_FILE_PATH : CONFIG_FILE_PATH;
	int err = fs_open(&config_fp, path, FS_O_CREATE | FS_O_WRITE);
	if (!err) {
		// save cfg to buffer
		if (_delete_to_buffer(k, s_config_buffer)) {
			// write file
			fs_write(&config_fp, s_config_buffer, strlen(s_config_buffer));
			fs_close(&config_fp);
			if (back_cfg) {
				// delete config
				fs_unlink(CONFIG_FILE_PATH);
				// rename back
				fs_rename(CONFIG_BACK_FILE_PATH, CONFIG_FILE_PATH);
			}
		} else {
			LISA_LOGE(TAG, "%s %d put to buffer failed", __FUNCTION__, __LINE__);
			// close and delete file
			fs_close(&config_fp);
			fs_unlink(path);
			return false;
		}
		return true;
	} else {
		LISA_LOGE(TAG, "%s %d open cfg file failed", __FUNCTION__, __LINE__);
		return false;
	}
}

static lisa_err_t _lisa_pref_delete(const char *k)
{
	if (!s_lisa_pref) return LISA_FAIL;
	lisa_mutex_lock(s_lisa_pref->m_mutex, LISA_WAIT_FOREVER);

	// read config.json
	struct fs_file_t fp;
	fs_file_t_init(&fp);
	int err = fs_open(&fp, CONFIG_FILE_PATH, FS_O_READ);
	if (err) {
		// read config failed try read config.json.back
		err = fs_open(&fp, CONFIG_BACK_FILE_PATH, FS_O_READ);
		if (err) {
			// read config.json.back failed
			memset(s_config_buffer, 0, CONFIG_FILE_MAX_SIZE + 1);

			// create cfg file
			bool write_ret = _delete_to_file(k, false);
			if (write_ret)
				goto WRITE_SUCCESS;
			else
				goto OPEN_FILE_ERR;
		} else {
			// read config.json.back success
			goto WRITE_CONFIG;
		}
	} else {
		// read config.json success
		goto WRITE_CONFIG;
	}

WRITE_CONFIG:
	// read file
	_read_file(&fp);
	fs_close(&fp);

	bool write_ret = _delete_to_file(k, true);
	if (write_ret)
		goto WRITE_SUCCESS;
	else
		goto OPEN_FILE_ERR;

WRITE_SUCCESS:
	lisa_mutex_unlock(s_lisa_pref->m_mutex);
	return LISA_OK;

OPEN_FILE_ERR:
	LISA_LOGE(TAG, "write config failed");
	lisa_mutex_unlock(s_lisa_pref->m_mutex);
	return LISA_FAIL;
}

static bool _get_from_buffer(const char *k, char **out_str, int **out_int, bool *out_bool,
		char *buffer, const lisa_pref_type_e type)
{
	cJSON *root = cJSON_Parse(buffer);
	bool ret = false;
	if (root) {
		if (!cJSON_HasObjectItem(root, k)) {
			cJSON_Delete(root);
			return false;
		}
		cJSON *param = cJSON_GetObjectItem(root, k);
		switch (type) {
			case LISA_PREF_TYPE_INT:
				if (cJSON_IsNumber(param)) {
					*out_int = param->valueint;
					ret = true;
				}
				break;
			case LISA_PREF_TYPE_STRING:
				if (cJSON_IsString(param)) {
					*out_str = lisa_mem_calloc(1, strlen(param->valuestring) + 1);
					strcpy(*out_str, param->valuestring);
					ret = true;
				}
				break;
			case LISA_PREF_TYPE_BOOL:
				if (cJSON_IsBool(param)) {
					*out_bool = param->valueint;
					ret = true;
				}
				break;
			default:
				break;
		}
		cJSON_Delete(root);
		return ret;
	} else {
		return false;
	}
}

static lisa_err_t _lisa_pref_get(
		const char *k, char **out_str, int **out_int, bool *out_bool, const lisa_pref_type_e type)
{
	if (!s_lisa_pref) return LISA_FAIL;
	lisa_mutex_lock(s_lisa_pref->m_mutex, LISA_WAIT_FOREVER);
	lisa_err_t ret = LISA_FAIL;

	if (s_config_buffer[0] != '\0') {
		// get from buffer
		goto READ_BUFFER;
	} else {
		struct fs_file_t fp;
		fs_file_t_init(&fp);
		int err = fs_open(&fp, CONFIG_FILE_PATH, FS_O_READ);
		if (err) {
			// get from back file
			err = fs_open(&fp, CONFIG_BACK_FILE_PATH, FS_O_READ);
			if (!err) {
				_read_file(&fp);
				fs_close(&fp);
				goto READ_BUFFER;
			} else {
				LISA_LOGE(TAG, "%s %d open file failed", __FUNCTION__, __LINE__);
				goto UNLOCK;
			}
		} else {
			// get from cfg file
			_read_file(&fp);
			fs_close(&fp);
			goto READ_BUFFER;
		}
	}

READ_BUFFER:
	ret = _get_from_buffer(k, out_str, out_int, out_bool, s_config_buffer, type) ? LISA_OK
																				 : LISA_FAIL;
UNLOCK:
	lisa_mutex_unlock(s_lisa_pref->m_mutex);
	return ret;
}

static void _printf_pref_dump(const char *buffer)
{
	cJSON *root = cJSON_Parse(buffer);
	if (root) {
		char *json_str = cJSON_PrintUnformatted(root);
		LISA_LOGE(TAG, "config = %s", json_str);
		cJSON_free(json_str);
		cJSON_Delete(root);
	} else {
		LISA_LOGE(TAG, "no config");
	}
}

lisa_evs_pref_t *lisa_pref_init()
{
	s_config_buffer = lisa_mem_calloc(1, CONFIG_FILE_MAX_SIZE + 1);
	if (!s_config_buffer) return NULL;

	lisa_evs_pref_t *handle = lisa_mem_calloc(1, sizeof(lisa_evs_pref_t));
	if (!handle) goto ERR_MALLOC_HANDLE;

	handle->m_mutex = lisa_mutex_create();
	if (!handle->m_mutex) goto ERR_MUTEX;

	s_lisa_pref = handle;
	return handle;

ERR_MUTEX:
	lisa_mem_free(handle);
ERR_MALLOC_HANDLE:
	lisa_mem_free(s_config_buffer);
	return NULL;
}

void lisa_pref_deinit(lisa_evs_pref_t *handle)
{
	if (handle) {
		if (handle->m_mutex) {
			lisa_mutex_lock(handle->m_mutex, LISA_WAIT_FOREVER);
			lisa_mutex_unlock(handle->m_mutex);
			lisa_mutex_delete(handle->m_mutex);
		}
		lisa_mem_free(handle);
		lisa_mem_free(s_config_buffer);
		s_config_buffer = NULL;
		s_lisa_pref = NULL;
	}
}

lisa_err_t lisa_pref_put_string(const char *k, const char *v)
{
	return _lisa_pref_put(k, v, 0, true, LISA_PREF_TYPE_STRING);
}

lisa_err_t lisa_pref_get_string(const char *k, char **out_v)
{
	return _lisa_pref_get(k, out_v, NULL, NULL, LISA_PREF_TYPE_STRING);
}

lisa_err_t lisa_pref_put_int(const char *k, const int v)
{
	return _lisa_pref_put(k, NULL, v, true, LISA_PREF_TYPE_INT);
}

lisa_err_t lisa_pref_get_int(const char *k, int **out_v)
{
	return _lisa_pref_get(k, NULL, out_v, NULL, LISA_PREF_TYPE_INT);
}

lisa_err_t lisa_pref_put_bool(const char *k, bool v)
{
	return _lisa_pref_put(k, NULL, 0, v, LISA_PREF_TYPE_BOOL);
}

lisa_err_t lisa_pref_get_bool(const char *k, bool *out_v)
{
	return _lisa_pref_get(k, NULL, NULL, out_v, LISA_PREF_TYPE_BOOL);
}

lisa_err_t lisa_pref_delete(const char *k)
{
	return _lisa_pref_delete(k);
}

void lisa_pref_clear()
{
	lisa_mutex_lock(s_lisa_pref->m_mutex, LISA_WAIT_FOREVER);
	fs_unlink(CONFIG_FILE_PATH);
	fs_unlink(CONFIG_BACK_FILE_PATH);
	memset(s_config_buffer, 0, CONFIG_FILE_MAX_SIZE + 1);
	lisa_mutex_unlock(s_lisa_pref->m_mutex);
}

void lisa_pref_dump()
{
	if (!s_lisa_pref) return;
	lisa_mutex_lock(s_lisa_pref->m_mutex, LISA_WAIT_FOREVER);

	if (strlen(s_config_buffer) > 0) {
		// get from buffer
		goto READ_BUFFER;
	} else {
		struct fs_file_t fp;
		fs_file_t_init(&fp);
		int err = fs_open(&fp, CONFIG_FILE_PATH, FS_O_READ);
		if (err) {
			// get from back file
			err = fs_open(&fp, CONFIG_BACK_FILE_PATH, FS_O_READ);
			if (!err) {
				_read_file(&fp);
				fs_close(&fp);
				goto READ_BUFFER;
			} else {
				LISA_LOGE(TAG, "no cfg");
				goto UNLOCK;
			}
		} else {
			// get from cfg file
			_read_file(&fp);
			fs_close(&fp);
			goto READ_BUFFER;
		}
	}

READ_BUFFER:
	_printf_pref_dump(s_config_buffer);
UNLOCK:
	lisa_mutex_unlock(s_lisa_pref->m_mutex);
	return;
}

// --------------------------------shell----------------------------------------------------
static int flash_set(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 4) {
		printf("invalid index\n");
		return 0;
	} else {
		char *type = argv[1];
		char *key = argv[2];
		char *value = argv[3];
		if (!strcmp(type, "string")) {
			if (lisa_pref_put_string(key, value) != LISA_OK) {
				printf("flash set %s:%s failed\n", key, value);
			} else {
				printf("flash set %s:%s success\n", key, value);
			}
		} else if (!strcmp(type, "int")) {
			int int_temp = atoi(value);
			if (lisa_pref_put_int(key, int_temp) != LISA_OK) {
				printf("flash set %s:%s failed\n", key, int_temp);
			} else {
				printf("flash set %s:%s success\n", key, int_temp);
			}
		} else if (!strcmp(type, "bool")) {
			int bool_temp = atoi(value);
			if (lisa_pref_put_bool(key, bool_temp) != LISA_OK) {
				printf("flash set %s:%s failed\n", key, bool_temp);
			} else {
				printf("flash set %s:%s success\n", key, bool_temp);
			}
		} else {
			printf("invalid index\n");
		}
	}
	return 0;
}

static int flash_show(const struct shell *shell, size_t argc, char **argv)
{
	lisa_pref_dump();
	return 0;
}

static int flash_clear(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		lisa_pref_clear();
		printf("flash clear success\n");
	} else {
		char *pref_k = argv[1];
		if (lisa_pref_delete(pref_k) == LISA_OK) {
			printf("flash delete config: %s, success\n", pref_k);
		} else {
			printf("flash has no config: %s\n", pref_k);
		}
	}
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
		scanpen_app_test, SHELL_CMD_ARG(flash_set, NULL, "flash_set", flash_set, 0, 0),
		SHELL_CMD_ARG(flash_clear, NULL, "flash_clear", flash_clear, 0, 0),
		SHELL_CMD_ARG(flash_show, NULL, "flash_show", flash_show, 0, 0),

		SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(lisa_cfg, &scanpen_app_test, "lisa_cfg related commands.", NULL);
