# SPDX-License-Identifier: Apache-2.0

board_runner_args(jlink "--device=Venus")
board_runner_args(pyocd "--target=csk6")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
