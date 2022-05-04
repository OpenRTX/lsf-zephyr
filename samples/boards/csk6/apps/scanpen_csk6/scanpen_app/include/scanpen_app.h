#ifndef __LISA_SCANPEN_APP_H__
#define __LISA_SCANPEN_APP_H__

#include <string.h>
#include "lisa_evs.h"
#include "lisa_evs_config.h"
#include "lisa_evs_rec.h"
#include "lisa_evs_ws.h"
#include "lisa_evs_system.h"
#include <lisa_log.h>
#include <lisa_thread.h>
#include <lisa_queue.h>
#include <lisa_semaphore.h>
#include <lisa_mem.h>
#include <lisa_typedef.h>
#include "evs_auth_mgr.h"
#include "product_config.h"
#include "cjson/cJSON.h"
#include "utils/evs_utils.h"
#include "lisa_evs_spi_parser.h"
#include "listenai_uart_protocol.h"
#include "lisa_evs_feedback_notebook.h"
#include "utils/lisa_ring_buffer.h"
#include "lisa_buttons.h"

#include "lisa_evs_websocket.h"

//启动scanpen app
void start_scanpen_app();


#endif  // __LISA_EVS_APP_H__
