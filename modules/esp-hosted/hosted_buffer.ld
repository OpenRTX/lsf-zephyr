/*
 * SPDX-License-Identifier: Apache-2.0
 */

SECTION_PROLOGUE(hosted_buffer,(NOLOAD),)
{
    /*
     * For performance, hosted_buffer section is assumed to be 32 byte aligned and
     * a multiple of 32 bytes
     */
    . = ALIGN(32);
    *(.hosted_buffer)
    *(".hosted_buffer.*")
    . = ALIGN(32);
#ifdef CONFIG_HOSTED_PSRAM_BUFFER
} GROUP_LINK_IN(PSRAMAP)
#else
} GROUP_LINK_IN(RAMABLE_REGION)
#endif
