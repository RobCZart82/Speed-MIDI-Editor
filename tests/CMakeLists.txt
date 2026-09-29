cmake_minimum_required(VERSION 3.21)

# A small standalone Windows build is useful when Qt is not installed.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SpeedMidiBackendTests LANGUAGES C)
    enable_testing()
    if(NOT WIN32)
        message(FATAL_ERROR "Use the main project to test non-Windows platforms")
    endif()
    set(PM ../third_party/portmidi)
    add_library(speedmidi_portmidi STATIC
        ${PM}/pm_common/portmidi.c ${PM}/pm_common/pmutil.c
        ${PM}/pm_win/pmwin.c ${PM}/pm_win/pmwinmm.c
        ${PM}/porttime/porttime.c ${PM}/porttime/ptwinmm.c)
    target_include_directories(speedmidi_portmidi PUBLIC
        ${PM}/pm_common ${PM}/pm_win ${PM}/porttime)
    target_link_libraries(speedmidi_portmidi PUBLIC winmm)
endif()

if(TARGET Qt6::Widgets)
    add_executable(smf_regression smf_regression.cpp ../src/speedymidi/smfdocument.cpp)
    target_include_directories(smf_regression PRIVATE ../src/speedymidi)
    target_link_libraries(smf_regression PRIVATE Qt6::Widgets)
    add_test(NAME smf_regression COMMAND smf_regression)
    set_tests_properties(smf_regression PROPERTIES TIMEOUT 30)

    add_executable(smf_import_regression smf_import_regression.cpp
        ../src/speedymidi/smfdocument.cpp ../src/speedymidi/smfimporter.cpp
        ../src/speedymidi/smfexporter.cpp ../src/speedymidi/doc_root.cpp
        ../src/speedymidi/doc_track.cpp ../src/speedymidi/doc_event.cpp
        ../src/speedymidi/doc_measureitem.cpp ../src/speedymidi/editorstate.cpp
        ../src/speedymidi/global.cpp ../src/speedymidi/writelengthdialog.cpp)
    target_include_directories(smf_import_regression PRIVATE ../src/speedymidi)
    target_link_libraries(smf_import_regression PRIVATE Qt6::Widgets Qt6::Xml)
    add_test(NAME smf_import_regression COMMAND smf_import_regression)
    set_tests_properties(smf_import_regression PROPERTIES TIMEOUT 30)

    add_executable(midiinterface_regression midiinterface_regression.cpp
        ../src/speedymidi/midiinterface.cpp ../src/speedymidi/midiinterface.h)
    target_compile_definitions(midiinterface_regression PRIVATE Pm_Close=test_Pm_Close)
    target_include_directories(midiinterface_regression PRIVATE ../src/speedymidi)
    target_link_libraries(midiinterface_regression PRIVATE Qt6::Widgets speedmidi_portmidi)
    if(APPLE)
        target_link_libraries(midiinterface_regression PRIVATE ${AUDIOTOOLBOX_FRAMEWORK})
    endif()
    add_test(NAME midiinterface_regression COMMAND midiinterface_regression)
    set_tests_properties(midiinterface_regression PROPERTIES TIMEOUT 30)
endif()

if(WIN32)
    add_executable(winmm_regression winmm_regression.c)
    target_include_directories(winmm_regression PRIVATE ../third_party/portmidi/pm_win)
    target_link_libraries(winmm_regression PRIVATE speedmidi_portmidi)
    add_test(NAME winmm_regression COMMAND winmm_regression)
    set_tests_properties(winmm_regression PROPERTIES TIMEOUT 30)

    add_executable(winmm_device_smoke winmm_device_smoke.c)
    target_link_libraries(winmm_device_smoke PRIVATE speedmidi_portmidi)
    add_test(NAME winmm_device_smoke COMMAND winmm_device_smoke)
    set_tests_properties(winmm_device_smoke PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77)
endif()
