#
# Copyright (c) 2019 Peter Bigot Consulting, LLC
#
# SPDX-License-Identifier: Apache-2.0
#
zephyr_include_directories(include)
zephyr_library_sources(src/lisa_vcapture.c)
zephyr_library_sources(src/lisa_acapture.c)
zephyr_library_sources(src/lisa_buttons.c)
zephyr_library_sources(src/lisa_translate.c)
zephyr_library_sources(src/lisa_tts.c)
zephyr_library_sources(src/lisa_evs_cp_data_parser.c)

# dictionary inc
zephyr_include_directories(src/dictdemo)
zephyr_include_directories(src/dictquery)
zephyr_include_directories(src/dict)
# dictionary lib
zephyr_library_sources(src/dictdemo/dict_demo.c)
zephyr_library_sources(src/dictquery/dict_search.c)
zephyr_library_sources(src/dictquery/rule.c)
zephyr_library_sources(src/dict/b+tree_org.c)
zephyr_library_sources(src/dict/dict.c)
zephyr_library_sources(src/dict/pdict.c)
