/*
 * Copyright (c) 2020 Teslabs Engineering S.L.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_CSK6_PSRAM

/* to make this section is existent */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(ap_partition), okay)

/* setion start */
GROUP_START(PSRAM_AP)

	SECTION_PROLOGUE(csk6_psram_section, (NOLOAD),)
	{
		__csk6_psram_section_start = .;
		KEEP(*(.csk6_psram_section))
		KEEP(*(".csk6_psram_section.*"))
		__csk6_psram_section_end = .;
	} GROUP_LINK_IN(PSRAM_AP)

GROUP_END(PSRAM_AP)
#endif

#endif
