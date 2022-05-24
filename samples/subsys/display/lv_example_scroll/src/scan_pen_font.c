#include <lvgl.h>

#define LOG_LEVEL 3
#include <logging/log.h>
LOG_MODULE_REGISTER(font_cache);

struct font_cache {
	lv_ft_info_t info;
	uint16_t font_size;
	struct font_cache *next;
};

static struct font_cache cjk_cache;

void init_font_engine(void)
{
	cjk_cache.next = NULL;
	cjk_cache.font_size = -1;
}

static lv_ft_info_t generate_cjk_font_info(uint16_t font_size, uint16_t font_style)
{
	struct font_cache *temp = &cjk_cache;
	while (temp) {
		if (temp->font_size == font_size) {
			LOG_INF("hit font cache of size %d", font_size);
			return temp->info;
		}
		if (temp->next == NULL) {
			break;
		}
		temp = temp->next;
	}
	static lv_ft_info_t info;
	info.mem = 0x18000000 + 0x1C0000;
	info.mem_size = 2171336;
	info.weight = font_size;
	info.style = font_style;
	lv_ft_font_init(&info);

	struct font_cache *next = lv_mem_alloc(sizeof(struct font_cache));

	next->info = info;
	next->font_size = font_size;
	next->next = NULL;

	temp->next = next;
	LOG_INF("generate font for size %d", font_size);

	return info;
}

static void destroy_font_event_cb(lv_event_t *e)
{
	lv_obj_t *label = e->target;
	const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
	lv_ft_font_destroy((lv_font_t *)font);
}

void lv_label_set_cjk_font(lv_obj_t *label, uint16_t font_size)
{
	lv_ft_info_t info = generate_cjk_font_info(font_size, FT_FONT_STYLE_NORMAL);
	lv_obj_set_style_text_font(label, info.font, LV_PART_MAIN);
	lv_obj_add_event_cb(label, destroy_font_event_cb, LV_EVENT_DELETE, NULL);
}
