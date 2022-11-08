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
		MPU_ALIGN(region_size);
		__psram_section_start = .;
		KEEP(*(.psram_section))
		KEEP(*(".psram_section.*"))
		MPU_ALIGN(region_size);
		__psram_section_end = .;
	} GROUP_LINK_IN(PSRAMAP)
	SECTION_DATA_PROLOGUE(.psramfunc_section,,)
	{
		MPU_ALIGN(region_size);
		__psramfunc_section_start = .;
		*(.psramfunc_section)
		*(".psramfunc_section.*")
		MPU_ALIGN(region_size);
		__psramfunc_section_end = .;
	} GROUP_DATA_LINK_IN(PSRAMAP, ROMABLE_REGION)

	__psramfunc_section_size = __psramfunc_section_end - __psramfunc_section_start;
	__psramfunc_section_load_start = LOADADDR(.psramfunc_section);
GROUP_END(PSRAMAP)
#endif

#endif
