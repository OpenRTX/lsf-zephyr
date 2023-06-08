/*  Self-test demonstration program
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

#include <zephyr/sys/printk.h>
#define  MBEDTLS_PRINT (int(*)(const char *, ...)) printk

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <zephyr/zephyr.h>
#include <zephyr/linker/sections.h>
#include <errno.h>

#include <tc_util.h>
#include <ztest.h>

#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "mbedtls/build_info.h"


#include "mbedtls/entropy.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/dhm.h"
#include "mbedtls/gcm.h"
#include "mbedtls/ccm.h"
#include "mbedtls/cmac.h"
#include "mbedtls/md5.h"
#include "mbedtls/ripemd160.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "mbedtls/des.h"
#include "mbedtls/aes.h"
#include "mbedtls/camellia.h"
#include "mbedtls/aria.h"
#include "mbedtls/chacha20.h"
#include "mbedtls/poly1305.h"
#include "mbedtls/chachapoly.h"
#include "mbedtls/base64.h"
#include "mbedtls/bignum.h"
#include "mbedtls/rsa.h"
#include "mbedtls/x509.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecjpake.h"
#include "mbedtls/timing.h"
#include "mbedtls/nist_kw.h"

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_printf     printf
#define mbedtls_snprintf   snprintf
#define mbedtls_exit       exit
#define MBEDTLS_EXIT_SUCCESS EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE EXIT_FAILURE
#endif

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include "mbedtls/memory_buffer_alloc.h"
#endif

extern int csk_aes_self_test( int verbose );

static int test_snprintf(size_t n, const char *ref_buf, int ref_ret)
{
	int ret;
	char buf[10] = "xxxxxxxxx";
	const char ref[10] = "xxxxxxxxx";

	ret = mbedtls_snprintf(buf, n, "%s", "123");
	if (ret < 0 || (size_t) ret >= n) {
		ret = -1;
	}

	if (strncmp(ref_buf, buf, sizeof(buf)) != 0 ||
	    ref_ret != ret || memcmp(buf + n, ref + n, sizeof(buf) - n) != 0) {
		return 1;
	}

	return 0;
}

static int run_test_snprintf(void)
{
	return (test_snprintf(0, "xxxxxxxxx", -1) != 0 ||
		test_snprintf(1, "", -1) != 0 ||
		test_snprintf(2, "1", -1) != 0 ||
		test_snprintf(3, "12", -1) != 0 ||
		test_snprintf(4, "123", 3) != 0 ||
		test_snprintf(5, "123", 3) != 0);
}

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
ZTEST_BMEM unsigned char buf[16000];
#endif

void test_mbedtls(void)
{
	int v, suites_tested = 0, suites_failed = 0;

	void *pointer;

	TC_START("Performing mbedTLS crypto tests:");

/*
 * The C standard doesn't guarantee that all-bits-0 is the representation
 * of a NULL pointer. We do however use that in our code for initializing
 * structures, which should work on every modern platform. Let's be sure.
 */
	(void)memset(&pointer, 0, sizeof(void *));
	if (pointer != NULL) {
		mbedtls_printf("all-bits-zero is not a NULL pointer\n");
		mbedtls_exit(MBEDTLS_EXIT_FAILURE);
	}

	/*
	 * Make sure we have a snprintf that correctly zero-terminates
	 */
	if (run_test_snprintf() != 0) {
		mbedtls_printf("the snprintf implementation is broken\n");
		mbedtls_exit(MBEDTLS_EXIT_FAILURE);
	}

	v = 1;
	mbedtls_printf("\n");

	if (csk_aes_self_test(v) != 0) {
		suites_failed++;
	}
	suites_tested++;

	if (v != 0) {
		mbedtls_printf("  Executed %d test suites\n\n", suites_tested);
		if (suites_failed > 0) {
			mbedtls_printf("  [ %d tests FAIL ]\n\n",
				       suites_failed);
		} else {
			mbedtls_printf("  [ All tests PASS ]\n\n");
		}
		zassert_not_equal(suites_tested, 0,
			      "ran %d tests", suites_tested);
		zassert_equal(suites_failed, 0,
			      "%d tests failed", suites_failed);
	}

}
