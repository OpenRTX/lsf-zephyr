#ifndef __CSK_APPLICATION_TASK_HEADER__
#define __CSK_APPLICATION_TASK_HEADER__

void ai_xy_crop_new(void);
void ai_xy_crop_prepare(void);
void ai_xy_crop_copy(void *src, int src_len, void *dest, int *dest_len);
void ai_xy_crop_free(void);

void ai_cutline_ocr_prepare(void);
void ai_cutline_ocr_copy(void* src, int src_len);
void ai_cutline_ocr_deal(void** dest, int *dest_len);
void ai_cutline_ocr_free(void);

void ai_trans_init(void);
void ai_trans_proc(void* src, int src_len, void *dest, int **dest_len);
void ai_trans_deinit(void);

void ai_tts_init(void);
void ai_tts_start(char *src, int len);
void ai_tts_copy(void *dest, int *dest_len, int *notify);
void ai_tts_deinit(void);

#endif /*__CSK_APPLICATION_TASK_HEADER__*/