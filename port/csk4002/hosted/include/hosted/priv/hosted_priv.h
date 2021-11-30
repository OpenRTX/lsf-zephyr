#pragma once

#include "hosted/hosted.h"

#ifdef __cplusplus
extern "C" {
#endif

void hosted_priv_event_handler(struct esp_priv_event *priv_event);

#ifdef __cplusplus
}
#endif
