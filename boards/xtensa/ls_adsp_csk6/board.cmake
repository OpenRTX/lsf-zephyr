# SPDX-License-Identifier: Apache-2.0

board_runner_args(csk "--chip=6")

board_set_flasher_ifnset(misc-flasher)
board_finalize_runner_args(misc-flasher)

include(${ZEPHYR_BASE}/boards/common/csk.board.cmake)
