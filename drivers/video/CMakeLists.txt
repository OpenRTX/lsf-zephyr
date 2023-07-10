# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_VIDEO_CSK6_DVP	video_csk6_dvp.c)