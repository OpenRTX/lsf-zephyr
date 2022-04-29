/*
 * Copyright (c) 2020 Teslabs Engineering S.L.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* to make this section is existent */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(section0_partition), okay)

/* setion start */
GROUP_START(PSRAM_SECTION0)

	SECTION_PROLOGUE(csk6_psram_section0, (NOLOAD),)
	{
		__csk6_psram_section0_start = .;
		KEEP(*(.csk6_psram_section0))
		KEEP(*(".csk6_psram_section0.*"))
		__csk6_psram_section0_end = .;
	} GROUP_LINK_IN(PSRAM_SECTION0)

GROUP_END(PSRAM_SECTION0)
#endif


#if DT_NODE_HAS_STATUS(DT_NODELABEL(section1_partition), okay)
GROUP_START(PSRAM_SECTION1)

	SECTION_PROLOGUE(csk6_psram_section1, (NOLOAD),)
	{
		__csk6_psram_section1_start = .;
		KEEP(*(.csk6_psram_section1))
		KEEP(*(".csk6_psram_section1.*"))
		__csk6_psram_section1_end = .;
	} GROUP_LINK_IN(PSRAM_SECTION1)


GROUP_END(PSRAM_SECTION1)
#endif