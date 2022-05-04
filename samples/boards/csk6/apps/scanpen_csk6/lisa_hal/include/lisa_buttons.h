#ifndef _LISA_BUTTONS_H_
#define _LISA_BUTTONS_H_

#include <stdint.h>

#define INVALID_BUTTON_STATE 0xffffffff
#define ALL_BUTTONS_RELEASE 0

/* one bit one hardware button, support up to 32 hardware buttons. */
#define KEY0 (1 << 0)
#define KEY1 (1 << 1)
#define KEY2 (1 << 2)
#define KEY3 (1 << 3)
#define KEY4 (1 << 4)
#define KEY5 (1 << 5)
#define KEY6 (1 << 6)
#define KEY7 (1 << 7)
#define KEY8 (1 << 8)
#define KEY9 (1 << 9)
#define KEY10 (1 << 10)
#define KEY11 (1 << 11)
#define KEY12 (1 << 12)
#define KEY13 (1 << 13)
#define KEY14 (1 << 14)
#define KEY15 (1 << 15)
#define KEY16 (1 << 16)
#define KEY17 (1 << 17)
#define KEY18 (1 << 18)
#define KEY19 (1 << 19)
#define KEY20 (1 << 20)
#define KEY21 (1 << 21)
#define KEY22 (1 << 22)
#define KEY23 (1 << 23)
#define KEY24 (1 << 24)
#define KEY25 (1 << 25)
#define KEY26 (1 << 26)
#define KEY27 (1 << 27)
#define KEY28 (1 << 28)
#define KEY29 (1 << 29)
#define KEY30 (1 << 30)
#define KEY31 (1 << 31)

typedef enum {
	SHORT_BUTTON,
	LONG_BUTTON,
	SHORT_LONG_BUTTON,
	COMBINED_LONG_BUTTON,
	REPEAT_LONG_BUTTON,
} BUTTON_TYPE;

typedef enum {
	PRESS, /* used in LONG_BUTTON/SHORT_LONG_BUTTON/COMBINED_LONG_BUTTON/REPEAT_LONG_BUTTON */
	RELEASE, /* used in all types of buttons */
	REPEAT_PRESS, /* used in REPEAT_LONG_BUTTON */
	REPEAT_RELEASE, /* used in SHORT_LONG_BUTTON */
	INVALID_STA, /* invalid button state */
} BUTTON_STATE;

typedef struct button_handle {
	void (*start)(struct button_handle *handle);
	void (*stop)(struct button_handle *handle);
	int (*destroy)(struct button_handle *handle);
	int (*get_state)(struct button_handle *handle);
	void (*cb)(BUTTON_STATE sta, void *arg); /* buttons callback function */
	void *arg; /* the argument to callback function */
} button_handle;

int buttons_init(void);
int buttons_deinit(void);
void buttons_set_thread_stack_size(uint32_t size);
void buttons_set_thread_priority(uint32_t priority);

button_handle *create_short_button(uint32_t id_mask);
button_handle *create_long_button(uint32_t id_mask, uint32_t timeout_ms);
button_handle *create_short_long_button(uint32_t id_mask, uint32_t timeout_ms);
button_handle *create_combined_long_button(uint32_t id_mask, uint32_t timeout_ms);
button_handle *create_repeat_long_button(
		uint32_t id_mask, uint32_t timeout_ms, uint32_t repeat_timeout_ms);

#endif /* _LISA_BUTTONS_H_ */

