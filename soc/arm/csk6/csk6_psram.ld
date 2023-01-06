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

	MPU_ALIGN(region_size);
	SECTION_PROLOGUE(psram_section, (NOLOAD),)
	{
		__psram_section_start = .;
		KEEP(*(.psram_section))
		KEEP(*(".psram_section.*"))
		__psram_section_end = .;
	} GROUP_LINK_IN(PSRAMAP)
	MPU_ALIGN(region_size);

	MPU_ALIGN(region_size);
	SECTION_DATA_PROLOGUE(.psramdata_section,,)
	{
		__psramdata_section_start = .;
		KEEP(*(.psramdata_section))
		KEEP(*(".psramdata_section.*"))
		__psramdata_section_end = .;
	} GROUP_DATA_LINK_IN(PSRAMAP, ROMABLE_REGION)
	MPU_ALIGN(region_size);

	__psramdata_section_size = __psramdata_section_end - __psramdata_section_start;
	__psramdata_section_load_start = LOADADDR(.psramdata_section);

	MPU_ALIGN(region_size);
	SECTION_DATA_PROLOGUE(.psramfunc_section,,)
	{
		__psramfunc_section_start = .;
		*(.psramfunc_section)
		*(".psramfunc_section.*")
		__psramfunc_section_end = .;
	} GROUP_DATA_LINK_IN(PSRAMAP, ROMABLE_REGION)
	MPU_ALIGN(region_size);

	__psramfunc_section_size = __psramfunc_section_end - __psramfunc_section_start;
	__psramfunc_section_load_start = LOADADDR(.psramfunc_section);
GROUP_END(PSRAMAP)
#endif

#endif
