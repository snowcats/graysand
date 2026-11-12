add_library(echo_codec STATIC
    alac.c
    flac.c
    flac_bits.c
)
target_include_directories(echo_codec PUBLIC include)
if(CMAKE_CROSSCOMPILING AND NOT ECHO_RELEASE)
    set_source_files_properties(flac.c flac_bits.c PROPERTIES COMPILE_OPTIONS -Os)
endif()
target_link_libraries(echo_codec
    PUBLIC echo_core
    PRIVATE echo_options
)
