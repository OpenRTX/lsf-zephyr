#include <string.h>
#include <zephyr/device.h>
#include <zephyr/sys/sys_heap.h>
#include <zephyr/sys/mutex.h>

static struct sys_heap hosted_buffer_heap;
static struct sys_mutex hosted_heap_mutex;

Z_GENERIC_SECTION(.hosted_buffer) static char heap_mem_buffer[CONFIG_HOSTED_BUFFER_HEAP_SIZE];

static int hosted_heap_prepare(const struct device *unused)
{
    ARG_UNUSED(unused);
    sys_heap_init(&hosted_buffer_heap, heap_mem_buffer, CONFIG_HOSTED_BUFFER_HEAP_SIZE);
    sys_mutex_init(&hosted_heap_mutex);
    return 0;
}

void *csk_os_malloc(size_t size)
{
	return k_aligned_alloc(32, size);
}

void csk_os_free(void *ptr)
{
	k_free(ptr);
}

static inline bool size_mul_overflow(size_t a, size_t b, size_t *result)
{
	return __builtin_mul_overflow(a, b, result);
}

void *csk_os_calloc(size_t nmemb, size_t size)
{
	size_t bounds;
	if (size_mul_overflow(nmemb, size, &bounds)) {
		return NULL;
	}
	void *ptr = csk_os_malloc(nmemb * size);
	if (ptr != NULL) {
		(void)memset(ptr, 0, bounds);
	}
	return ptr;
}

void *csk_buffer_malloc(size_t size)
{
    __ASSERT_NO_MSG(sys_mutex_lock(&hosted_heap_mutex, K_FOREVER) == 0);
    void *ret = sys_heap_aligned_alloc(&hosted_buffer_heap,
                                       32,
                                       size);
    __ASSERT(ret,"csk_buffer_malloc size %d failed, called by: 0x%08lx\n",
             size, (intptr_t)__builtin_return_address(0) - 2);
    if (ret == NULL && size != 0) {
        errno = ENOMEM;
    }
    sys_mutex_unlock(&hosted_heap_mutex);
    return ret;
}

void csk_buffer_free(void *ptr)
{
    __ASSERT_NO_MSG(sys_mutex_lock(&hosted_heap_mutex, K_FOREVER) == 0);
    sys_heap_free(&hosted_buffer_heap, ptr);
    sys_mutex_unlock(&hosted_heap_mutex);
}

SYS_INIT(hosted_heap_prepare, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
