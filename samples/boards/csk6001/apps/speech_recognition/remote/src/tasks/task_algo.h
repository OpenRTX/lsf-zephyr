#ifndef __CSK_APPLICATION_TASK_HEADER__
#define __CSK_APPLICATION_TASK_HEADER__

typedef struct {
	uint16_t mic1;
	uint16_t mic2;
	uint16_t ref1;
	uint16_t ref2;
} __attribute__((packed)) cae_mic_ref_t;

typedef struct {
	uint16_t mic1;
	uint16_t mic2;
}__attribute__((packed)) cae_mic_t;

typedef struct {
	uint16_t ref1;
	uint16_t ref2;
}__attribute__((packed)) cae_ref_t;

typedef struct {
	short mic1;
	short mic2;
	short ref1;
	short ref2;
} ori_out_t[256];

typedef struct {
	short ivw1;
	short ivw2;
	short ivw3;
	short asr1;
} cae_out_t[256];

typedef struct {
	int 			cache;              
	int 			mode;
	uint32_t		bit_mask;
	uint32_t 		chl_map;
	uint32_t		res;
	uint8_t		 	stage;
	int				wake;
	int				frm;
	bool			af_start;
} algo_ctx_t;

void algo_thread(void *v1, void *v2, void *v3);

#endif /*__CSK_APPLICATION_TASK_HEADER__*/