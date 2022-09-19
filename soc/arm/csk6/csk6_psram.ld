/*
 * Copyright (c) 2020 Teslabs Engineering S.L.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_CSK6_PSRAM

/* to make this section is existent */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(psram_ap), okay)

/* setion start */
GROUP_START(PSRAMAP)

	SECTION_PROLOGUE(psram_section, (NOLOAD),)
	{
		__psram_section_start = .;
		KEEP(*(.psram_section))
		KEEP(*(".psram_section.*"))
		__psram_section_end = .;
	} GROUP_LINK_IN(PSRAMAP)

GROUP_END(PSRAMAP)


GROUP_START(PSRAMAP_NOCACHE)

	SECTION_PROLOGUE(psram_nocache, (NOLOAD),)
	{
		__psram_nocache_start = .;
		KEEP(*(.psram_nocache))
		KEEP(*(".psram_nocache.*"))
		__psram_nocache_end = .;
	} GROUP_LINK_IN(PSRAMAP_NOCACHE)

GROUP_END(PSRAMAP_NOCACHE)

#endif

#endif
