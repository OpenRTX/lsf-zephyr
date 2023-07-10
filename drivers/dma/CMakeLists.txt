# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__dma)

target_sources_ifdef(CONFIG_DMA_CSK6 ${TARGET_LIB} PRIVATE dma_csk6.c)
