# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__video)

target_sources_ifdef(CONFIG_VIDEO_CSK6_DVP ${TARGET_LIB} PRIVATE video_csk6_dvp.c)