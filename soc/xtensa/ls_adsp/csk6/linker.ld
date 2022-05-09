/*
 * Copyright (c) 2021 ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Linker command/script file
 *
 * Linker script for the ListenAI CSK6 platform
 */

OUTPUT_ARCH(xtensa)

#include <xtensa/config/core-isa.h>
#include <soc/memory.h>
#include <autoconf.h>
#include <linker/sections.h>

#include "linker-defs.h"


#include <devicetree.h>

#if 1
/* #if DT_HAS_FIXED_PARTITION_LABEL(cp_image) */

#define LOAD_FROM_FLASH   1
/* #define MTD_NODE          DT_MTD_FROM_FIXED_PARTITION(DT_NODE_BY_FIXED_PARTITION_LABEL(cp_image)) */
/* #define MTD_BASE_START    DT_REG_ADDR(MTD_NODE) */
/* #define MTD_BASE_SIZE     DT_REG_SIZE(MTD_NODE) */
/* #define MTD_OFFSET        DT_PROP_OR(MTD_NODE, cp_offset, 0x50000000) */

/* #define FLASH_PHY_START       DT_REG_ADDR(DT_NODE_BY_FIXED_PARTITION_LABEL(cp_image)) + MTD_OFFSET + MTD_BASE_START */
/* #define FLASH_PHY_SIZE        DT_REG_SIZE(DT_NODE_BY_FIXED_PARTITION_LABEL(cp_image)) */

#define FLASH_PHY_START       (0x18000000 + 0x50000000 + 0x200000)
#define FLASH_PHY_SIZE        (0x100000)

#define RAMABLE_REGION    sdram0
#define ROMABLE_REGION    FLASH

#define RESET_TEXT_ADDR   (FLASH_PHY_START)

#define VECTORS_START     (RESET_TEXT_ADDR + MEM_RESET_TEXT_SIZE + MEM_RESET_LIT_SIZE)
#define VECTORS_SIZE      0x298

#define FLASH_START       (VECTORS_START + VECTORS_SIZE)
#define FLASH_SIZE        (FLASH_PHY_START + FLASH_PHY_SIZE - FLASH_START)

#define SDRAM0_START_X    0x60000000
#define SDRAM0_SIZE_X     0x700000

#else

#define RAMABLE_REGION    sdram0
#define ROMABLE_REGION    sdram0

#define RESET_TEXT_ADDR   0x60000000

#define VECTORS_START     (RESET_TEXT_ADDR + MEM_RESET_TEXT_SIZE + MEM_RESET_LIT_SIZE)
#define VECTORS_SIZE      0x298

#define FLASH_START  0
#define FLASH_SIZE   0

#define SDRAM0_START_X    (VECTORS_START + VECTORS_SIZE)
#define SDRAM0_SIZE_X     0x500000 - ISR_VECTOR_SIZE

#endif /* end of DT_HAS_FIXED_PARTITION_LABEL(cp_image) */


#define SECTION_PROLOGUE(name, options, align) name options : align
#define GROUP_ROM_LINK_IN(vregion, lregion) > lregion
#define SECTION_DATA_PROLOGUE(name, options, align) SECTION_PROLOGUE(name, options, align)
#define GROUP_DATA_LINK_IN(vregion, lregion) > vregion

PROVIDE(__memctl_default = 0x00000000);
PROVIDE(_MemErrorHandler = 0x00000000);


MEMORY
{
  luna_sharedmem_dram :
        org = LUNA_SHARED_MEM_BASE,
        len = LUNA_SHARED_DRAM_SIZE
  luna_sharedmem_data :
        org = LUNA_SHARED_MEM_BASE + LUNA_SHARED_DRAM_SIZE,
        len = LUNA_SHARED_DATA_SIZE
  vector_reset_text :
        org = RESET_TEXT_ADDR,
        len = MEM_RESET_TEXT_SIZE
  vector_reset_lit :
        org = RESET_TEXT_ADDR + MEM_RESET_TEXT_SIZE,
        len = MEM_RESET_LIT_SIZE
  VECTORS :
        org = VECTORS_START,
        len = VECTORS_SIZE
  FLASH :
        org = FLASH_START,
        len = FLASH_SIZE
  sdram0 :
        org = SDRAM0_START_X,
        len = SDRAM0_SIZE_X
  sdram1 :
        org = SDRAM1_BASE,
        len = SDRAM1_SIZE


  vector_base_text :
                org = XCHAL_VECBASE_RESET_PADDR_IRAM,
        len = MEM_VECBASE_LIT_SIZE
  vector_int2_lit :
                org = XCHAL_INTLEVEL2_VECTOR_PADDR_IRAM - MEM_VECT_LIT_SIZE,
        len = MEM_VECT_LIT_SIZE
  vector_int2_text :
                org = XCHAL_INTLEVEL2_VECTOR_PADDR_IRAM,
        len = MEM_VECT_TEXT_SIZE
  vector_int3_lit :
                org = XCHAL_INTLEVEL3_VECTOR_PADDR_IRAM - MEM_VECT_LIT_SIZE,
        len = MEM_VECT_LIT_SIZE
  vector_int3_text :
                org = XCHAL_INTLEVEL3_VECTOR_PADDR_IRAM,
        len = MEM_VECT_TEXT_SIZE
  vector_int4_lit :
                org = XCHAL_INTLEVEL4_VECTOR_PADDR_IRAM - MEM_VECT_LIT_SIZE,
        len = MEM_VECT_LIT_SIZE
  vector_int4_text :
                org = XCHAL_INTLEVEL4_VECTOR_PADDR_IRAM,
        len = MEM_VECT_TEXT_SIZE
  vector_int5_lit :
                org = XCHAL_INTLEVEL5_VECTOR_PADDR_IRAM - MEM_VECT_LIT_SIZE,
        len = MEM_VECT_LIT_SIZE
  vector_int5_text :
                org = XCHAL_INTLEVEL5_VECTOR_PADDR_IRAM,
        len = MEM_VECT_TEXT_SIZE
  vector_debug_lit :
                org = XCHAL_DEBUG_VECTOR_PADDR_IRAM - MEM_VECT_LIT_SIZE,
        len = MEM_VECT_LIT_SIZE
  vector_debug_text :
                org = XCHAL_DEBUG_VECTOR_PADDR_IRAM,
        len = MEM_VECT_TEXT_SIZE
  vector_nmi_lit :
                org = XCHAL_NMI_VECTOR_PADDR_IRAM - MEM_VECT_LIT_SIZE,
        len = MEM_VECT_LIT_SIZE
  vector_nmi_text :
                org = XCHAL_NMI_VECTOR_PADDR_IRAM,
        len = MEM_VECT_TEXT_SIZE
  vector_kernel_lit :
                org = XCHAL_KERNEL_VECTOR_PADDR_IRAM - MEM_VECT_LIT_SIZE,
        len = MEM_VECT_LIT_SIZE
  vector_kernel_text :
                org = XCHAL_KERNEL_VECTOR_PADDR_IRAM,
        len = MEM_VECT_TEXT_SIZE
  vector_user_lit :
                org = XCHAL_USER_VECTOR_PADDR_IRAM - MEM_VECT_LIT_SIZE,
        len = MEM_VECT_LIT_SIZE
  vector_user_text :
                org = XCHAL_USER_VECTOR_PADDR_IRAM,
        len = MEM_VECT_TEXT_SIZE
  vector_double_lit :
                org = XCHAL_DOUBLEEXC_VECTOR_PADDR_IRAM - MEM_VECT_LIT_SIZE,
        len = MEM_VECT_LIT_SIZE
  vector_double_text :
                org = XCHAL_DOUBLEEXC_VECTOR_PADDR_IRAM,
        len = MEM_VECT_TEXT_SIZE
  iram_text_start :
                org = XCHAL_DOUBLEEXC_VECTOR_PADDR_IRAM + MEM_VECT_TEXT_SIZE,
        len = (IRAM_BASE + IRAM_SIZE) - (XCHAL_DOUBLEEXC_VECTOR_PADDR + MEM_VECT_TEXT_SIZE)
#ifdef CONFIG_GEN_ISR_TABLES
  IDT_LIST :
                org = IDT_BASE,
        len = IDT_SIZE
#endif

  static_uuid_entries_seg (!ari) :
        org = UUID_ENTRY_ELF_BASE,
        len = UUID_ENTRY_ELF_SIZE
  static_log_entries_seg (!ari) :
        org = LOG_ENTRY_ELF_BASE,
        len = LOG_ENTRY_ELF_SIZE
  fw_metadata_seg (!ari) :
        org = EXT_MANIFEST_ELF_BASE,
        len = EXT_MANIFEST_ELF_SIZE
}


ENTRY(CONFIG_KERNEL_ENTRY)

PROVIDE(_memmap_reset_vector = FLASH_START);
PROVIDE(_memmap_vecbase_reset = XCHAL_VECBASE_RESET_PADDR_IRAM);

_heap_sentry = 0x64000000;

/* Various memory-map dependent cache attribute settings: */
_memmap_cacheattr_wb_base = 0x00004200;
_memmap_cacheattr_wt_base = 0x00001200;
_memmap_cacheattr_bp_base = 0x00002200;
_memmap_cacheattr_unused_mask = 0xFFFF00FF;
_memmap_cacheattr_wb_trapnull = 0x2222422F;
_memmap_cacheattr_wba_trapnull = 0x2222422F;
_memmap_cacheattr_wbna_trapnull = 0x2222522F;
_memmap_cacheattr_wt_trapnull = 0x2222122F;
_memmap_cacheattr_bp_trapnull = 0x2222222F;
_memmap_cacheattr_wb_strict = 0xFFFF42FF;
_memmap_cacheattr_wt_strict = 0xFFFF12FF;
_memmap_cacheattr_bp_strict = 0xFFFF22FF;
_memmap_cacheattr_wb_allvalid = 0x22224222;
_memmap_cacheattr_wt_allvalid = 0x22221222;
_memmap_cacheattr_bp_allvalid = 0x22222222;
_memmap_region_map = 0x0000000C;
PROVIDE(_memmap_cacheattr_reset = _memmap_cacheattr_wb_trapnull);

_EXT_MAN_ALIGN_ = 16;

SECTIONS
{

#include <linker/rel-sections.ld>
  .ResetVector.text : ALIGN(4)
  {
    _ResetVector_text_start = ABSOLUTE(.);
    KEEP (*(.ResetVector.text))
    _ResetVector_text_end = ABSOLUTE(.);
  } >vector_reset_text

  .ResetVector.literal : ALIGN(4)
  {
    _ResetVector_literal_start = ABSOLUTE(.);
    *(.ResetVector.literal)
    _ResetVector_literal_end = ABSOLUTE(.);
  } >vector_reset_lit

   .ResetHandler.text : ALIGN(4)
  {
    _ResetHandler_text_start = ABSOLUTE(.);
    *(.ResetHandler.literal .ResetHandler.text)
    . = ALIGN (4);
    _ResetHandler_text_end = ABSOLUTE(.);
  } >vector_reset_lit

  .WindowVectors.text : ALIGN(4)
  {
    . = ALIGN (4);
    _WindowVectors_text_start = ABSOLUTE(.);
    KEEP (*(.WindowVectors.text))
    . = ALIGN (4);
    _WindowVectors_text_end = ABSOLUTE(.);
  } >vector_base_text AT > VECTORS

  .Level2InterruptVector.literal : ALIGN(4)
  {
    . = ALIGN (4);
    _Level2InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level2InterruptVector.literal)
    . = ALIGN (4);
    _Level2InterruptVector_literal_end = ABSOLUTE(.);
  } >vector_int2_lit AT > VECTORS

  .Level2InterruptVector.text : ALIGN(4)
  {
    . = ALIGN (4);
    _Level2InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level2InterruptVector.text))
    . = ALIGN (4);
    _Level2InterruptVector_text_end = ABSOLUTE(.);
  } >vector_int2_text AT > VECTORS

  .Level3InterruptVector.literal : ALIGN(4)
  {
    . = ALIGN (4);
    _Level3InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level3InterruptVector.literal)
    . = ALIGN (4);
    _Level3InterruptVector_literal_end = ABSOLUTE(.);
  } >vector_int3_lit AT > VECTORS

  .Level3InterruptVector.text : ALIGN(4)
  {
    . = ALIGN (4);
    _Level3InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level3InterruptVector.text))
    . = ALIGN (4);
    _Level3InterruptVector_text_end = ABSOLUTE(.);
  } >vector_int3_text AT > VECTORS

  .Level4InterruptVector.literal : ALIGN(4)
  {
    . = ALIGN (4);
    _Level4InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level4InterruptVector.literal)
    . = ALIGN (4);
    _Level4InterruptVector_literal_end = ABSOLUTE(.);
  } >vector_int4_lit AT > VECTORS

  .Level4InterruptVector.text : ALIGN(4)
  {
    . = ALIGN (4);
    _Level4InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level4InterruptVector.text))
    . = ALIGN (4);
    _Level4InterruptVector_text_end = ABSOLUTE(.);
  } >vector_int4_text AT > VECTORS

  .Level5InterruptVector.literal : ALIGN(4)
  {
    . = ALIGN (4);
    _Level5InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level5InterruptVector.literal)
    . = ALIGN (4);
    _Level5InterruptVector_literal_end = ABSOLUTE(.);
  } >vector_int5_lit AT > VECTORS

  .Level5InterruptVector.text : ALIGN(4)
  {
    . = ALIGN (4);
    _Level5InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level5InterruptVector.text))
    . = ALIGN (4);
    _Level5InterruptVector_text_end = ABSOLUTE(.);
  } >vector_int5_text AT > VECTORS

  .DebugExceptionVector.literal : ALIGN(4)
  {
    . = ALIGN (4);
    _DebugExceptionVector_literal_start = ABSOLUTE(.);
    *(.DebugExceptionVector.literal)
    . = ALIGN (4);
    _DebugExceptionVector_literal_end = ABSOLUTE(.);
  } >vector_debug_lit AT > VECTORS

  .DebugExceptionVector.text : ALIGN(4)
  {
    . = ALIGN (4);
    _DebugExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.DebugExceptionVector.text))
    . = ALIGN (4);
    _DebugExceptionVector_text_end = ABSOLUTE(.);
  } >vector_debug_text AT > VECTORS

  .NMIExceptionVector.literal : ALIGN(4)
  {
    . = ALIGN (4);
    _NMIExceptionVector_literal_start = ABSOLUTE(.);
    *(.NMIExceptionVector.literal)
    . = ALIGN (4);
    _NMIExceptionVector_literal_end = ABSOLUTE(.);
  } >vector_nmi_lit AT > VECTORS

  .NMIExceptionVector.text : ALIGN(4)
  {
    . = ALIGN (4);
    _NMIExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.NMIExceptionVector.text))
    . = ALIGN (4);
    _NMIExceptionVector_text_end = ABSOLUTE(.);
  } >vector_nmi_text AT > VECTORS

  .KernelExceptionVector.literal : ALIGN(4)
  {
    . = ALIGN (4);
    _KernelExceptionVector_literal_start = ABSOLUTE(.);
    *(.KernelExceptionVector.literal)
    . = ALIGN (4);
    _KernelExceptionVector_literal_end = ABSOLUTE(.);
  } >vector_kernel_lit AT > VECTORS

  .KernelExceptionVector.text :  ALIGN(4)
  {
    . = ALIGN (4);
    _KernelExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.KernelExceptionVector.text))
    . = ALIGN (4);
    _KernelExceptionVector_text_end = ABSOLUTE(.);
  } >vector_kernel_text AT > VECTORS

  .UserExceptionVector.literal : ALIGN(4)
  {
    . = ALIGN (4);
    _UserExceptionVector_literal_start = ABSOLUTE(.);
    *(.UserExceptionVector.literal)
    . = ALIGN (4);
    _UserExceptionVector_literal_end = ABSOLUTE(.);
  } >vector_user_lit AT > VECTORS

  .UserExceptionVector.text :  ALIGN(4)
  {
    . = ALIGN (4);
    _UserExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.UserExceptionVector.text))
    . = ALIGN (4);
    _UserExceptionVector_text_end = ABSOLUTE(.);
  } >vector_user_text AT > VECTORS

  .DoubleExceptionVector.literal : ALIGN(4)
  {
    . = ALIGN (4);
    _DoubleExceptionVector_literal_start = ABSOLUTE(.);
    *(.DoubleExceptionVector.literal)
    . = ALIGN (4);
    _DoubleExceptionVector_literal_end = ABSOLUTE(.);
  } >vector_double_lit AT > VECTORS

  .DoubleExceptionVector.text : ALIGN(4)
  {
    . = ALIGN (4);
    _DoubleExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.DoubleExceptionVector.text))
    . = ALIGN (4);
    _DoubleExceptionVector_text_end = ABSOLUTE(.);
  } >vector_double_text AT > VECTORS

#define XIP_COPY_DEF(name) LONG (LOADADDR(name)) LONG (ADDR(name)) LONG (SIZEOF(name))

  .copy.table :
  {
    . = ALIGN(4);
    __copy_table_start__ = .;
    XIP_COPY_DEF(.WindowVectors.text)
    XIP_COPY_DEF(.Level2InterruptVector.text)
    XIP_COPY_DEF(.Level3InterruptVector.text)
    XIP_COPY_DEF(.Level4InterruptVector.text)
    XIP_COPY_DEF(.Level5InterruptVector.text)
    XIP_COPY_DEF(.DebugExceptionVector.text)
    XIP_COPY_DEF(.NMIExceptionVector.text)
    XIP_COPY_DEF(.KernelExceptionVector.text)
    XIP_COPY_DEF(.UserExceptionVector.text)
    XIP_COPY_DEF(.DoubleExceptionVector.text)
    XIP_COPY_DEF(.iram.text)
    XIP_COPY_DEF(.clib.data)
    XIP_COPY_DEF(.clib.percpu.data)
    XIP_COPY_DEF(.fw_ready)
    XIP_COPY_DEF(.data)

    XIP_COPY_DEF(.dram1.rodata)
    XIP_COPY_DEF(.dram1.data)
    XIP_COPY_DEF(.sharedmem.text)
    XIP_COPY_DEF(.sharedmem.rodata)
    XIP_COPY_DEF(.sharedmem.data)
    XIP_COPY_DEF(.lit4)

    XIP_COPY_DEF(sw_isr_table)
    XIP_COPY_DEF(device_states)
    XIP_COPY_DEF(k_sem_area)
    XIP_COPY_DEF(_static_thread_data_area)
    XIP_COPY_DEF(k_heap_area)

    __copy_table_end__ = .;
  } > FLASH

  .rtos.rodata : ALIGN(4)
  {
    _rtos_rodata_start = ABSOLUTE(.);
    *(.rtos.rodata)
    . = ALIGN (4);
    _rtos_rodata_end = ABSOLUTE(.);
  } > FLASH


  .clib.rodata : ALIGN(4)
  {
    _clib_rodata_start = ABSOLUTE(.);
    *(.clib.rodata)
    . = ALIGN (4);
    _clib_rodata_end = ABSOLUTE(.);
  } > FLASH

  .rodata : ALIGN(4)
  {
    _rodata_start = ABSOLUTE(.);
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r.*)
    *(.rodata1)
    __XT_EXCEPTION_TABLE__ = ABSOLUTE(.);
    KEEP (*(.xt_except_table))
    KEEP (*(.gcc_except_table .gcc_except_table.*))
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    KEEP (*(.eh_frame))
    KEEP (*crtbegin.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    __XT_EXCEPTION_DESCS__ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)
    . = ALIGN(4);
    _bss_table_start = ABSOLUTE(.);
    LONG(_bss_start)
    LONG(_bss_end)
    LONG(_dram1_bss_start)
    LONG(_dram1_bss_end)
    LONG(_sharedmem_bss_start)
    LONG(_sharedmem_bss_end)
    _bss_table_end = ABSOLUTE(.);
    _rodata_end = ABSOLUTE(.);
  } > FLASH

  .module_init : ALIGN(4)
  {
   _module_init_start = ABSOLUTE(.);
    *(*.module_init)
    _module_init_end = ABSOLUTE(.);
  } > FLASH

  .text : ALIGN(4)
  {
    _stext = .;
    _text_start = ABSOLUTE(.);
    KEEP (*(.ResetVector.text))
    *(.ResetVector.literal)
    *(.entry.text)
    *(.init.literal)
    KEEP(*(.init))
    *(.literal .text .literal.* .text.* .stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.fini.literal)
    KEEP(*(.fini))
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext = .;
  } > FLASH
  
  .clib.text : ALIGN(4)
  {
    _clib_text_start = ABSOLUTE(.);
    *(.clib.literal .clib.text)
    . = ALIGN (4);
    _clib_text_end = ABSOLUTE(.);
  } > FLASH

#include <linker/common-rom.ld>

  . = ORIGIN(sdram0);
  _IRAM_TEXT_LOAD_START_ = .;
  .iram.text : ALIGN(4)
  {
    . = ALIGN (4);
    _stext = .;
    _iram_text_start = ABSOLUTE(.);
    *(.iram0.literal .iram.literal .iram.text.literal .iram1.literal .iram0.text .iram.text .iram1)
    . = ALIGN (4);
    _iram_text_end = ABSOLUTE(.);
  } >iram_text_start AT > FLASH

  .clib.data : ALIGN(4)
  {
    . = ALIGN (4);
    _clib_data_start = ABSOLUTE(.);
    *(.clib.data)
    . = ALIGN (4);
    _clib_data_end = ABSOLUTE(.);
  } >sdram0 AT > FLASH


  .clib.percpu.data : ALIGN(4)
  {
    . = ALIGN (4);
    _clib_percpu_data_start = ABSOLUTE(.);
    *(.clib.percpu.data)
    . = ALIGN (4);
    _clib_percpu_data_end = ABSOLUTE(.);
  } >sdram0 AT > FLASH


  .fw_ready : ALIGN(4)
  {
    . = ALIGN (4);
    KEEP(*(".fw_ready"));
    KEEP (*(.fw_ready_metadata))
    . = ALIGN (4);
  } >sdram0 AT > FLASH

  .noinit : ALIGN(4)
  {
    . = ALIGN (4);
    *(.noinit)
    *(.noinit.*)
    . = ALIGN (4);
  } >sdram0 AT > FLASH

  .data : ALIGN(4)
  {
    . = ALIGN (4);
    _data_start = ABSOLUTE(.);
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    KEEP(*(.gnu.linkonce.d.*personality*))
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    KEEP(*(.jcr))
    . = ALIGN(4);
    *(.gna_model)
    _data_end = ABSOLUTE(.);
    . = ALIGN(4096);
  } >sdram0 AT > FLASH

  .dram1.rodata : ALIGN(4)
  {
    . = ALIGN (4);
    _dram1_rodata_start = ABSOLUTE(.);
    *(.dram1.rodata)
    . = ALIGN (4);
    _dram1_rodata_end = ABSOLUTE(.);
  } >luna_sharedmem_dram AT > FLASH

  .dram1.data : ALIGN(4)
  {
    . = ALIGN (4);
    _dram1_data_start = ABSOLUTE(.);
    *(.dram1.data)
    . = ALIGN (4);
    _dram1_data_end = ABSOLUTE(.);
  } >luna_sharedmem_dram AT > FLASH

  .dram1.bss (NOLOAD) : ALIGN(8)
  {
    . = ALIGN (8);
    _dram1_bss_start = ABSOLUTE(.);
    *(.dram1.bss)
    . = ALIGN (8);
    _dram1_bss_end = ABSOLUTE(.);
  } >luna_sharedmem_dram AT > FLASH

  .sharedmem.text : ALIGN(4)
  {
    . = ALIGN (4);
    _sharedmem_text_start = ABSOLUTE(.);
    *(.sharedmem.literal .sharedmem.text)
    . = ALIGN (4);
    _sharedmem_text_end = ABSOLUTE(.);
  } >luna_sharedmem_data AT > FLASH

  .sharedmem.rodata : ALIGN(4)
  {
    . = ALIGN (4);
    _sharedmem_rodata_start = ABSOLUTE(.);
    *(.sharedmem.rodata)
    . = ALIGN (4);
    _sharedmem_rodata_end = ABSOLUTE(.);
  } >luna_sharedmem_data AT > FLASH

  .sharedmem.data : ALIGN(4)
  {
    . = ALIGN (4);
    _sharedmem_data_start = ABSOLUTE(.);
    *(.sharedmem.data)
    . = ALIGN (4);
    _sharedmem_data_end = ABSOLUTE(.);
  } >luna_sharedmem_data AT > FLASH

  .sharedmem.bss (NOLOAD) : ALIGN(8)
  {
    . = ALIGN (8);
    _sharedmem_bss_start = ABSOLUTE(.);
    *(.sharedmem.bss)
    . = ALIGN (8);
    _sharedmem_bss_end = ABSOLUTE(.);
    _memmap_seg_sharedmem1_end = ALIGN(0x8);
  } >luna_sharedmem_data AT > FLASH

  _memmap_mem_sharedmem_max = ABSOLUTE(.);

  .lit4 : ALIGN(4)
  {
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
  } >sdram0 AT > FLASH

#include <linker/common-ram.ld>

  .bss (NOLOAD) : ALIGN(8)
  {
    . = ALIGN (8);
    _bss_start = ABSOLUTE(.);
    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    *(.clib.bss)
    *(.clib.percpu.bss)
    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
  } >sdram0

  .heap_mem (NOLOAD) : ALIGN(8)
  {
    . = ALIGN (8);
    _heap_mem_start = ABSOLUTE(.);
    *(*.heap_mem)
    _heap_mem_end = ABSOLUTE(.);
  } >sdram1

  /* stack */
  _end = ALIGN (8);
  PROVIDE(end = ALIGN (8));

  __stack = SDRAM1_BASE + SDRAM1_SIZE;
  .comment  0 :  { *(.comment) }
  .debug  0 :  { *(.debug) }
  .line  0 :  { *(.line) }
  .debug_srcinfo  0 :  { *(.debug_srcinfo) }
  .debug_sfnames  0 :  { *(.debug_sfnames) }
  .debug_aranges  0 :  { *(.debug_aranges) }
  .debug_pubnames  0 :  { *(.debug_pubnames) }
  .debug_info  0 :  { *(.debug_info) }
  .debug_abbrev  0 :  { *(.debug_abbrev) }
  .debug_line  0 :  { *(.debug_line) }
  .debug_frame  0 :  { *(.debug_frame) }
  .debug_str  0 :  { *(.debug_str) }
  .debug_loc  0 :  { *(.debug_loc) }
  .debug_macinfo  0 :  { *(.debug_macinfo) }
  .debug_weaknames  0 :  { *(.debug_weaknames) }
  .debug_funcnames  0 :  { *(.debug_funcnames) }
  .debug_typenames  0 :  { *(.debug_typenames) }
  .debug_varnames  0 :  { *(.debug_varnames) }
  .debug_ranges  0 :  { *(.debug_ranges) }
  .xtensa.info  0 :  { *(.xtensa.info) }
  .xt.insn 0 :
  {
    KEEP (*(.xt.insn))
    KEEP (*(.gnu.linkonce.x.*))
  }
  .xt.prop 0 :
  {
    KEEP (*(.xt.prop))
    KEEP (*(.xt.prop.*))
    KEEP (*(.gnu.linkonce.prop.*))
  }
  .xt.lit 0 :
  {
    KEEP (*(.xt.lit))
    KEEP (*(.xt.lit.*))
    KEEP (*(.gnu.linkonce.p.*))
  }
  .xt.profile_range 0 :
  {
    KEEP (*(.xt.profile_range))
    KEEP (*(.gnu.linkonce.profile_range.*))
  }
  .xt.profile_ranges 0 :
  {
    KEEP (*(.xt.profile_ranges))
    KEEP (*(.gnu.linkonce.xt.profile_ranges.*))
  }
  .xt.profile_files 0 :
  {
    KEEP (*(.xt.profile_files))
    KEEP (*(.gnu.linkonce.xt.profile_files.*))
  }
#ifdef CONFIG_GEN_ISR_TABLES
#include <linker/intlist.ld>
#endif

  .static_uuid_entries (COPY) : ALIGN(1024)
  {
    *(*.static_uuids)
  } > static_uuid_entries_seg

  .static_log_entries (COPY) : ALIGN(1024)
  {
    *(*.static_log*)
  } > static_log_entries_seg

  .fw_metadata (COPY) : ALIGN(1024)
  {
    KEEP (*(.fw_metadata))
    . = ALIGN(_EXT_MAN_ALIGN_);
  } >fw_metadata_seg
}