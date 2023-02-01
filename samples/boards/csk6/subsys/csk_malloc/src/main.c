/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr.h>
#include <csk_malloc.h>
#define MALLOC_SIZE	(1000)
#define REALLOC_SIZE	(4000)
#define CALLOC_NUM	(2)
#define CALLOC_NUM_SIZE (4)
void main(void)
{
	char *ptr = NULL;
	char *ptr_calloc = NULL;

	printk("\nTest 1: ################csk_malloc################\n");
	printk("System init csk heap info:\n");
	csk_heap_info();
	ptr = csk_malloc(MALLOC_SIZE);
	if (ptr == NULL) {
		printk("csk_malloc size %d failed!\n", MALLOC_SIZE);
		return;
	}
	printk("Malloc success ptr %p\n", ptr);

	printk("After malloc %d bytes ,heap info:\n", MALLOC_SIZE);
	csk_heap_info();

	printk("\nTest 2: ################csk_realloc################\n");
	ptr = csk_realloc(ptr, REALLOC_SIZE);
	if (ptr == NULL) {
		printk("csk_realloc size %d failed!\n", REALLOC_SIZE);
		return;
	}
	printk("csk_realloc success ptr %p\n", ptr);
	printk("After realloc %d bytes ,heap info:\n", REALLOC_SIZE);
	csk_heap_info();

	printk("\nTest 3: ################csk_calloc################\n");
	ptr_calloc = csk_calloc(CALLOC_NUM, CALLOC_NUM_SIZE);
	if (ptr_calloc == NULL) {
		printk("csk_calloc numb %d size %d failed!\n", CALLOC_NUM, CALLOC_NUM_SIZE);
		return;
	}
	printk("csk_calloc success ptr_calloc %p,data[0...7]={%d,%d,%d,%d,%d,%d,%d,%d}\n",
	       ptr_calloc, ptr_calloc[0], ptr_calloc[1], ptr_calloc[2], ptr_calloc[3],
	       ptr_calloc[4], ptr_calloc[5], ptr_calloc[6], ptr_calloc[7]);

	printk("After csk_calloc numb %d size %d bytes ,heap info:\n", CALLOC_NUM, CALLOC_NUM_SIZE);
	csk_heap_info();

	printk("\nTest 4: ################csk_free################\n");
	csk_free(ptr);
	csk_free(ptr_calloc);
	printk("After free all ,heap info:\n");
	csk_heap_info();

	printk("CSK Heap test complete!\n");
}
