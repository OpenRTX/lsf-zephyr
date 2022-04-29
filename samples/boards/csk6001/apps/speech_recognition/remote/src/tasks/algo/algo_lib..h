
#ifndef __CSK_ALGO_LIB_HEADER__
#define __CSK_ALGO_LIB_HEADER__


typedef void (*algo_wake_cb)(const int chn, const char *json);

int algo_set_mode(uint8_t wakeup);
int algo_create(algo_wake_cb cbwak);
int algo_step_proc(void *idata, void **odata);

#endif /*__CSK_APPLICATION_TASK_HEADER__*/