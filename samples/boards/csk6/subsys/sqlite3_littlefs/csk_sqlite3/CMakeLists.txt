# zephyr_library_named(csk_sqlite)

# target_include_directories(csk_sqlite PRIVATE
zephyr_include_directories(
                          ./include
                          ./private_include
                          )

zephyr_library_sources(
    ./sqlite3.c
    ./shox96_0_2.c
    ./csk6_sqlite3.c
    )
