#pragma once

#include "kernel.h"
#include "sys_clock.h"
#include "ztest_assert.h"
#include "ztest_test.h"
#include <zephyr.h>
#include <ztest.h>
#include "csk6/csk_wifi.h"


int csk_wifi_connected_msgq_give(csk_wifi_event_t events);
int csk_wifi_connected_msgq_take(csk_wifi_event_t *events, k_timeout_t timeout);

int csk_wifi_disconnected_msgq_give(csk_wifi_event_t events);
int csk_wifi_disconnected_msgq_take(csk_wifi_event_t *events, k_timeout_t timeout);

void test_wifi_timeout_para(void);
