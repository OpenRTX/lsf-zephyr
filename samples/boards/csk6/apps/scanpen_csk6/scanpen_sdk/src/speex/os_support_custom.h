#ifndef _OS_SUPPORT_CUSTOM_H_
#define _OS_SUPPORT_CUSTOM_H_

#define OVERRIDE_SPEEX_ALLOC 1
#define OVERRIDE_SPEEX_ALLOC_SCRATCH 1
#define OVERRIDE_SPEEX_REALLOC 1
#define OVERRIDE_SPEEX_FREE 1
#define OVERRIDE_SPEEX_FREE_SCRATCH 1

#include <lisa_mem.h>

/** Speex wrapper for calloc. To do your own dynamic allocation, all you need to do is replace this function, speex_realloc and speex_free 
    NOTE: speex_alloc needs to CLEAR THE MEMORY */
#ifdef OVERRIDE_SPEEX_ALLOC
static inline void *speex_alloc (int size)
{
   /* WARNING: this is not equivalent to malloc(). If you want to use malloc() 
      or your own allocator, YOU NEED TO CLEAR THE MEMORY ALLOCATED. Otherwise
      you will experience strange bugs */
   return lisa_mem_calloc(size,1);
}
#endif

/** Same as speex_alloc, except that the area is only needed inside a Speex call (might cause problem with wideband though) */
#ifdef OVERRIDE_SPEEX_ALLOC_SCRATCH
static inline void *speex_alloc_scratch (int size)
{
   /* Scratch space doesn't need to be cleared */
   return lisa_mem_calloc(size,1);
}
#endif

/** Speex wrapper for realloc. To do your own dynamic allocation, all you need to do is replace this function, speex_alloc and speex_free */
#ifdef OVERRIDE_SPEEX_REALLOC
static inline void *speex_realloc (void *ptr, int size)
{
   return lisa_mem_realloc(ptr, size);
}
#endif

/** Speex wrapper for calloc. To do your own dynamic allocation, all you need to do is replace this function, speex_realloc and speex_alloc */
#ifdef OVERRIDE_SPEEX_FREE
static inline void speex_free (void *ptr)
{
   lisa_mem_free(ptr);
}
#endif

/** Same as speex_free, except that the area is only needed inside a Speex call (might cause problem with wideband though) */
#ifdef OVERRIDE_SPEEX_FREE_SCRATCH
static inline void speex_free_scratch (void *ptr)
{
   lisa_mem_free(ptr);
}
#endif

#endif
