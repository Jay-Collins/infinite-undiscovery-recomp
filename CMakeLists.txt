# infinite_undiscovery - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(infinite_undiscovery LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(INFINITEUNDISCOVERY_SOURCES
    src/main.cpp
    src/player_options.cpp
    src/save_backups.cpp
    src/asset_setup.cpp
    src/asset_dlc.cpp
    src/portable_setup.cpp
)

if(WIN32)
    list(APPEND INFINITEUNDISCOVERY_SOURCES src/infinite_undiscovery.rc)
endif()

option(IU_BUILD_SETUP_TESTS "Build local asset installer validation executable" OFF)
option(IU_ENABLE_DIAGNOSTICS "Include developer tracing and recovery tools" OFF)
option(IU_ENABLE_TIMING_EXPERIMENT "Record private frame-timing samples" OFF)
if(IU_BUILD_SETUP_TESTS)
    enable_testing()
    add_executable(iu_difficulty_unlock_tests tests/difficulty_unlock_tests.cpp src/player_options.cpp)
    target_link_libraries(iu_difficulty_unlock_tests PRIVATE rex::runtime)
    target_compile_definitions(iu_difficulty_unlock_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    rexglue_apply_target_settings(iu_difficulty_unlock_tests)
    add_test(NAME difficulty_unlock COMMAND iu_difficulty_unlock_tests)
    add_executable(iu_save_anywhere_tests tests/save_anywhere_tests.cpp)
    target_compile_definitions(iu_save_anywhere_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    add_test(NAME save_anywhere_safety COMMAND iu_save_anywhere_tests)
    add_executable(iu_save_activity_tests tests/save_activity_tests.cpp)
    add_test(NAME save_activity_exit_protection COMMAND iu_save_activity_tests)
    set_tests_properties(save_activity_exit_protection PROPERTIES TIMEOUT 20)
    add_executable(iu_menu_pause_tests tests/menu_pause_tests.cpp)
    add_test(NAME menu_pause COMMAND iu_menu_pause_tests)
    set_tests_properties(menu_pause PROPERTIES TIMEOUT 15)
    add_executable(iu_save_backups_tests tests/save_backups_tests.cpp src/save_backups.cpp)
    target_compile_definitions(iu_save_backups_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(iu_save_backups_tests PRIVATE bcrypt)
    rexglue_apply_target_settings(iu_save_backups_tests)
    add_test(NAME save_backups_and_restart COMMAND iu_save_backups_tests)
    add_executable(iu_menu_settings_tests tests/menu_settings_tests.cpp src/player_options.cpp)
    target_link_libraries(iu_menu_settings_tests PRIVATE rex::runtime)
    target_compile_definitions(iu_menu_settings_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    rexglue_apply_target_settings(iu_menu_settings_tests)
    add_test(NAME menu_settings_persistence COMMAND iu_menu_settings_tests)
    add_executable(iu_player_options_tests tests/player_options_tests.cpp src/player_options.cpp)
    target_link_libraries(iu_player_options_tests PRIVATE rex::runtime)
    target_compile_definitions(iu_player_options_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    rexglue_apply_target_settings(iu_player_options_tests)
    add_test(NAME player_options COMMAND iu_player_options_tests)
    add_executable(iu_orb_release_tests tests/orb_release_tests.cpp)
    target_compile_definitions(iu_orb_release_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX IU_ENABLE_DIAGNOSTICS=0)
    add_test(NAME orb_release_visibility COMMAND iu_orb_release_tests)
    add_executable(iu_trace_control_tests tests/trace_control_tests.cpp)
    target_compile_definitions(iu_trace_control_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX IU_ENABLE_DIAGNOSTICS=1)
    foreach(mode start stop filter legacy movement flags interaction visibility)
        add_test(NAME trace_control_${mode} COMMAND iu_trace_control_tests ${mode})
    endforeach()
    add_executable(iu_content_header_tests tests/content_header_tests.cpp)
    target_link_libraries(iu_content_header_tests PRIVATE rex::runtime)
    rexglue_apply_target_settings(iu_content_header_tests)
    add_test(NAME content_header_fallback COMMAND iu_content_header_tests)
    add_executable(iu_asset_setup_tests tests/asset_setup_tests.cpp src/asset_setup.cpp src/asset_dlc.cpp src/portable_setup.cpp)
    target_link_libraries(iu_asset_setup_tests PRIVATE rex::runtime bcrypt comctl32 ole32 shell32 uuid gdi32)
    target_include_directories(iu_asset_setup_tests PRIVATE src)
    rexglue_apply_target_settings(iu_asset_setup_tests)
endif()

if(WIN32)
    add_executable(infinite_undiscovery WIN32 ${INFINITEUNDISCOVERY_SOURCES})
else()
    add_executable(infinite_undiscovery ${INFINITEUNDISCOVERY_SOURCES})
endif()

# GPU_PLUGINS xenos makes the SDK stage rexgpu-xenos.dll next to the host. The
# GPU plugin is runtime-loaded (never linked), so TARGET_RUNTIME_DLLS misses it
# and the release build output would otherwise only contain rexruntime.dll.
rexglue_setup_target(infinite_undiscovery GPU_PLUGINS xenos)
target_compile_definitions(infinite_undiscovery PRIVATE IU_ENABLE_DIAGNOSTICS=$<BOOL:${IU_ENABLE_DIAGNOSTICS}>)
set_source_files_properties(src/main.cpp PROPERTIES COMPILE_DEFINITIONS "IU_ENABLE_TIMING_EXPERIMENT=$<BOOL:${IU_ENABLE_TIMING_EXPERIMENT}>")
# Preserve the historical PoC executable; publish this stage under its new name.
set_target_properties(infinite_undiscovery PROPERTIES OUTPUT_NAME "InfiniteUndiscoveryRecomp")

if(WIN32)
    target_link_libraries(infinite_undiscovery PRIVATE bcrypt comctl32 ole32 shell32 uuid gdi32)

    # Opt-in development builds verify the hashes recorded by build-sdk.ps1.
    # Empty by default: public/release builds retain the original exact hashes.
    set(IU_LOCAL_REXGLUE_HASH_FILE "" CACHE FILEPATH "Local source-built SDK provenance JSON")

    # Release hardening: refuse to produce a build whose staged ReXGlue DLLs are
    # the old precompiled-SDK copies. Aborts the build with a clear message.
    add_custom_command(TARGET infinite_undiscovery POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DVERIFY_DIR=$<TARGET_FILE_DIR:infinite_undiscovery>"
            "-DREQUIRE_ALL=ON"
            "-DEXPECTED_HASH_FILE=${IU_LOCAL_REXGLUE_HASH_FILE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_rexglue_dlls.cmake"
        COMMENT "Verifying staged ReXGlue DLLs (auditable v0.10.0 hashes)"
        VERBATIM)
else()
    message(FATAL_ERROR "Asset Setup currently requires Windows; do not imply cross-platform support.")
endif()
