#if (CONFIG_LISA_HEAP_ARENA_SIZE > 0)

#include <stdlib.h>
#include <zephyr.h>
#include <init.h>
#include <errno.h>
#include <sys/math_extras.h>
#include <string.h>
#include <app_memory/app_memdomain.h>
#include <sys/mutex.h>
#include <sys/sys_heap.h>
#include <zephyr/types.h>

#include <lisa_mem.h>
#include <lisa_log.h>

#define TAG "lisa_mem"

#define POOL_SECTION .csk6_psram_section
#define HEAP_BYTES CONFIG_LISA_HEAP_ARENA_SIZE

Z_GENERIC_SECTION(POOL_SECTION) static struct sys_heap z_malloc_heap;
Z_GENERIC_SECTION(POOL_SECTION) static struct sys_mutex z_malloc_heap_mutex;
Z_GENERIC_SECTION(POOL_SECTION) static char z_malloc_heap_mem[HEAP_BYTES];

static int lisa_heap_prepare(const struct device *unused)
{
	ARG_UNUSED(unused);

	sys_heap_init(&z_malloc_heap, z_malloc_heap_mem, HEAP_BYTES);
	sys_mutex_init(&z_malloc_heap_mutex);

	return 0;
}

void *lisa_mem_alloc(uint32_t size)
{
	int lock_ret;

	lock_ret = sys_mutex_lock(&z_malloc_heap_mutex, K_FOREVER);
	__ASSERT_NO_MSG(lock_ret == 0);

	void *ret = sys_heap_aligned_alloc(&z_malloc_heap, __alignof__(z_max_align_t), size);
	if (ret == NULL && size != 0) {
		errno = ENOMEM;
	}

	(void)sys_mutex_unlock(&z_malloc_heap_mutex);

	return ret;
}

void lisa_mem_free(void *ptr)
{
	int lock_ret;

	lock_ret = sys_mutex_lock(&z_malloc_heap_mutex, K_FOREVER);
	__ASSERT_NO_MSG(lock_ret == 0);
	sys_heap_free(&z_malloc_heap, ptr);
	(void)sys_mutex_unlock(&z_malloc_heap_mutex);
}

void *lisa_mem_calloc(uint32_t nmemb, uint32_t size)
{
	void *ret;

	if (size_mul_overflow(nmemb, size, &size)) {
		errno = ENOMEM;
		return NULL;
	}

	ret = lisa_mem_alloc(size);

	if (ret != NULL) {
		(void)memset(ret, 0, size);
	}

	return ret;
}

void *lisa_mem_realloc(void *ptr, uint32_t requested_size)
{
	int lock_ret;

	lock_ret = sys_mutex_lock(&z_malloc_heap_mutex, K_FOREVER);
	__ASSERT_NO_MSG(lock_ret == 0);

	void *ret = sys_heap_aligned_realloc(&z_malloc_heap, ptr, __alignof__(z_max_align_t),
					     requested_size);

	if (ret == NULL && requested_size != 0) {
		errno = ENOMEM;
	}

	(void)sys_mutex_unlock(&z_malloc_heap_mutex);

	return ret;
}

char *lisa_strdup(const char *s)
{
	__ASSERT(s, "The ptr is NULL");

	size_t len = strlen(s) + 1;
	char *tmp = (char *)lisa_mem_alloc(len);

	if (!tmp)
		return NULL;

	memcpy(tmp, s, len);

	return tmp;
}

void lisa_hexdump(char *buf, int len, int addr)
{
	__ASSERT(0, "%s is unsupportd", __FUNCTION__);
}

void lisa_heap_info(void)
{
	sys_heap_print_info(&z_malloc_heap, 0);
}

uint32_t lisa_thread_info(void *task)
{
	__ASSERT(0, "%s is unsupportd", __FUNCTION__);
	return 0;
}

// testself

//#define PSRAM_TESTSELF

#ifdef PSRAM_TESTSELF
static void psram_mem_rw_test(uint32_t start_address, uint32_t size)
{
	_ASSERT(0, "%s is unsupportd", __FUNCTION__);
}

static void lisa_heap_test(int argc, char *argv[])
{
	__ASSERT(0, "%s is unsupportd", __FUNCTION__);
}

#endif

SYS_INIT(lisa_heap_prepare, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif
