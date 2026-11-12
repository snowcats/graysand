add_library(echo_gfx STATIC
    canvas.c
    color.c
    font.c
    font_pack.c
    font_sized.c
    frame_diff.c
    image.c
    rect.c
    strip_renderer.c
    text.c
)
target_include_directories(echo_gfx PUBLIC include)
if(CMAKE_CROSSCOMPILING)
    set_source_files_properties(font_pack.c PROPERTIES COMPILE_OPTIONS -Os)
endif()
if(CMAKE_CROSSCOMPILING AND NOT ECHO_RELEASE)
    set_source_files_properties(image.c text.c canvas.c rect.c strip_renderer.c color.c font.c font_sized.c frame_diff.c PROPERTIES COMPILE_OPTIONS -Os)
    target_compile_options(echo_gfx PRIVATE -flto)
endif()
target_link_libraries(echo_gfx
    PUBLIC echo_core
    PRIVATE echo_options
)
