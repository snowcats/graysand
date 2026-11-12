add_library(echo_app STATIC
    app.c
    card_files.c
    app_screens.c
    bluetooth_link.c
    bluetooth_store.c
    board_layout.c
    browse_screen.c
    cover_cache.c
    default_theme.c
    favorites_store.c
    diagnostics_values.c
    diagnostics_rows.c
    $<IF:$<BOOL:${ECHO_RELEASE}>,diagnostics_menu_release.c,diagnostics_menu.c>
    draw_profile.c
    frame_overlay.c
    home_art.c
    album_page.c
    idle_track.c
    motion.c
    art_slots.c
    info_screen.c
    cassette.c
    volume_blur.c
    home_emblem.c
    library_rescan.c
    music_check.c
    library_store.c
    lyrics.c
    player.c
    settings_screens.c
    settings_store.c
    spoken_store.c
    screens/bluetooth_screen.c
    screens/charge_screen.c
    screens/font_sample_screen.c
    screens/now_playing_screen.c
    screens/files_screen.c
    screens/up_next_screen.c
    screens/stopwatch_screen.c
    screens/test_pattern_screen.c
)
if(CMAKE_CROSSCOMPILING)
    set_source_files_properties(
        info_screen.c
        volume_blur.c
        screens/font_sample_screen.c
        screens/now_playing_screen.c
        screens/stopwatch_screen.c
        screens/test_pattern_screen.c
        PROPERTIES COMPILE_OPTIONS -fno-lto
    )
endif()
target_include_directories(echo_app PUBLIC include PRIVATE .)
target_link_libraries(echo_app
    PUBLIC echo_core echo_gfx echo_ui echo_fonts echo_library echo_playback echo_settings echo_hal
    PRIVATE echo_options echo_compact
)
