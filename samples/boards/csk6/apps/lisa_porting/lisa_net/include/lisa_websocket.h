#ifndef __LISA_WEBSOCKET__
#define __LISA_WEBSOCKET__

#include <string.h>
#include <stdint.h>
#include "lisa_err.h"

typedef enum {
	LISA_WS_OK = 0,
	LISA_WS_COMMON_ERR,
	LISA_WS_DISCONECT,
} lisa_ws_err_e;

typedef enum {
	LISA_WS_STATUS_NULL,
	LISA_WS_STATUS_OK,
	LISA_WS_STATUS_NOT_OK,
} lisa_ws_status_e;

typedef enum {
	LISA_WS_ON_ERROR,
	LISA_WS_ON_HEADER,  // 多次发生
	LISA_WS_ON_CONNECTED,
	LISA_WS_ON_DISCONNECTED,
} lisa_ws_event_e;

typedef struct {
	lisa_ws_event_e what;
	const uint8_t *header;
	void *user;
} lisa_ws_event_t;

typedef enum {
	LISA_WS_TEXT,
	LISA_WS_BIN,
	LISA_WS_BIN_FLASH,
} lisa_ws_data_type_e;

typedef struct {
	lisa_ws_data_type_e type;
	const void *buf;
	uint32_t len;
	void *user;
} lisa_ws_data_t;

typedef struct {
	uint8_t *scheme;  // "ws" "wss"
	uint8_t *host;
	uint8_t *path;
	uint8_t *port;
	uint32_t timeout_ms;
	void *user;
	void (*on_event)(lisa_ws_event_t *event);
	void (*on_data)(lisa_ws_data_t *data);
} lisa_ws_request_t;

typedef struct {
	uint8_t scheme[16];  // "ws" "wss"
	uint8_t host[512];
	uint8_t path[512];
	uint8_t port[16];
} url_info_t;

typedef struct {
	void *ws_mngt;
	url_info_t u_info;
	lisa_ws_status_e ws_status;
	uint32_t timeout_ms;
	void *user;
	void (*inter_on_event)(lisa_ws_event_t *event);
	void (*inter_on_data)(lisa_ws_data_t *data);
} lisa_ws_t;

/**
 * @brief init
 * @param  req              
 * @return lisa_ws_t* 
 */
lisa_ws_t *lisa_ws_init(lisa_ws_request_t *req);

/**
 * @brief connect
 * @param  ins              
 * @return lisa_ws_err_e 
 */
lisa_ws_err_e lisa_ws_connect(lisa_ws_t *ins);

/**
 * @brief disconnect
 * @param  ins              
 * @return lisa_ws_err_e 
 */
lisa_ws_err_e lisa_ws_disconnect(lisa_ws_t *ins);

/**
 * @brief send text
 * @param  ins              
 * @param  text             
 * @return lisa_ws_err_e 
 */
lisa_ws_err_e lisa_ws_send_text(lisa_ws_t *ins, const uint8_t *text);

/**
 * @brief send binary
 * @param  ins              
 * @param  buf              
 * @param  len              
 * @return lisa_ws_err_e 
 */
lisa_ws_err_e lisa_ws_send_binary(lisa_ws_t *ins, const void *buf, uint32_t len);

/**
 * @brief clean up
 * @param  ins              
 * @return lisa_ws_err_e 
 */
lisa_ws_err_e lisa_ws_cleanup(lisa_ws_t *ins);

#endif  //__LISA_WEBSOCKET__
