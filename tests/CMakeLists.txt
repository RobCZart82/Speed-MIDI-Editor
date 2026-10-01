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
    # Native undo commands plus the document model, with isolated settings and
    # no MIDI device initialization. Keep the application entry point out.
    set(LOSSLESS_REGRESSION_SOURCES ${SPEED_MIDI_SOURCES} ${SPEED_MIDI_HEADERS} ${SPEED_MIDI_FORMS})
    list(REMOVE_ITEM LOSSLESS_REGRESSION_SOURCES src/speedymidi/main.cpp)
    list(TRANSFORM LOSSLESS_REGRESSION_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
    add_executable(lossless_regression lossless_regression.cpp ${LOSSLESS_REGRESSION_SOURCES})
    target_include_directories(lossless_regression PRIVATE ../src/speedymidi)
    target_link_libraries(lossless_regression PRIVATE
        Qt6::Widgets Qt6::Xml Qt6::Network Qt6::Svg speedmidi_portmidi)
    if(APPLE)
        target_link_libraries(lossless_regression PRIVATE ${AUDIOTOOLBOX_FRAMEWORK})
    endif()
    add_test(NAME lossless_regression COMMAND lossless_regression)
    set_tests_properties(lossless_regression PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # Exercise the real editor/controller and undo stack without opening MIDI
    # devices. Keep the normal executable entry point out of this test target.
    set(EDITOR_REGRESSION_SOURCES ${SPEED_MIDI_SOURCES} ${SPEED_MIDI_HEADERS} ${SPEED_MIDI_FORMS})
    list(REMOVE_ITEM EDITOR_REGRESSION_SOURCES src/speedymidi/main.cpp)
    list(TRANSFORM EDITOR_REGRESSION_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
    add_executable(editor_regression editor_regression.cpp ${EDITOR_REGRESSION_SOURCES})
    target_include_directories(editor_regression PRIVATE ../src/speedymidi)
    target_link_libraries(editor_regression PRIVATE
        Qt6::Widgets Qt6::Xml Qt6::Network Qt6::Svg speedmidi_portmidi)
    if(APPLE)
        target_link_libraries(editor_regression PRIVATE ${AUDIOTOOLBOX_FRAMEWORK})
    endif()
    add_test(NAME editor_regression COMMAND editor_regression)
    set_tests_properties(editor_regression PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
    target_compile_definitions(smf_import_regression PRIVATE SMF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/smf")
    target_include_directories(smf_import_regression PRIVATE ../src/speedymidi)
    target_link_libraries(smf_import_regression PRIVATE Qt6::Widgets Qt6::Xml)
    add_test(NAME smf_import_regression COMMAND smf_import_regression)
    set_tests_properties(smf_import_regression PROPERTIES TIMEOUT 30)

    add_executable(validation_regression validation_regression.cpp
        ../src/speedymidi/smfdocument.cpp ../src/speedymidi/smfimporter.cpp
        ../src/speedymidi/smfexporter.cpp ../src/speedymidi/doc_root.cpp
        ../src/speedymidi/doc_track.cpp ../src/speedymidi/doc_event.cpp
        ../src/speedymidi/doc_measureitem.cpp ../src/speedymidi/editorstate.cpp
        ../src/speedymidi/global.cpp ../src/speedymidi/writelengthdialog.cpp)
    target_include_directories(validation_regression PRIVATE ../src/speedymidi)
    target_link_libraries(validation_regression PRIVATE Qt6::Widgets Qt6::Xml)
    add_test(NAME validation_regression COMMAND validation_regression)
    set_tests_properties(validation_regression PROPERTIES TIMEOUT 30)

    add_executable(localpeer_regression localpeer_regression.cpp
        ../src/speedymidi/qtsingleapplication/qtlocalpeer.cpp
        ../src/speedymidi/qtsingleapplication/qtlocalpeer.h
        ../src/speedymidi/qtsingleapplication/qtlockedfile.h)
    target_include_directories(localpeer_regression PRIVATE ../src/speedymidi)
    target_link_libraries(localpeer_regression PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME localpeer_regression COMMAND localpeer_regression)
    set_tests_properties(localpeer_regression PROPERTIES TIMEOUT 15 SKIP_RETURN_CODE 77)

    add_executable(smf_compatibility_regression smf_compatibility_regression.cpp
        ../src/speedymidi/smfdocument.cpp ../src/speedymidi/smfimporter.cpp
        ../src/speedymidi/smfexporter.cpp ../src/speedymidi/doc_root.cpp
        ../src/speedymidi/doc_track.cpp ../src/speedymidi/doc_event.cpp
        ../src/speedymidi/doc_measureitem.cpp ../src/speedymidi/editorstate.cpp
        ../src/speedymidi/global.cpp ../src/speedymidi/writelengthdialog.cpp)
    target_include_directories(smf_compatibility_regression PRIVATE ../src/speedymidi)
    target_link_libraries(smf_compatibility_regression PRIVATE Qt6::Widgets Qt6::Xml)
    add_test(NAME smf_compatibility_regression COMMAND smf_compatibility_regression)
    set_tests_properties(smf_compatibility_regression PROPERTIES TIMEOUT 30)

    add_executable(event_ordering_regression event_ordering_regression.cpp
        ../src/speedymidi/smfdocument.cpp ../src/speedymidi/smfimporter.cpp
        ../src/speedymidi/smfexporter.cpp ../src/speedymidi/doc_root.cpp
        ../src/speedymidi/doc_track.cpp ../src/speedymidi/doc_event.cpp
        ../src/speedymidi/doc_measureitem.cpp ../src/speedymidi/editorstate.cpp
        ../src/speedymidi/global.cpp ../src/speedymidi/writelengthdialog.cpp)
    target_include_directories(event_ordering_regression PRIVATE ../src/speedymidi)
    target_link_libraries(event_ordering_regression PRIVATE Qt6::Widgets Qt6::Xml)
    add_test(NAME event_ordering_regression COMMAND event_ordering_regression)
    set_tests_properties(event_ordering_regression PROPERTIES TIMEOUT 30)

    add_executable(midiinterface_regression midiinterface_regression.cpp
        ../src/speedymidi/midiinterface.cpp ../src/speedymidi/midiinterface.h)
    target_compile_definitions(midiinterface_regression PRIVATE Pm_Close=test_Pm_Close
        Pm_WriteShort=test_Pm_WriteShort Pt_Time=test_Pt_Time)
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

if(APPLE)
    add_executable(coremidi_regression coremidi_regression.c)
    target_include_directories(coremidi_regression PRIVATE ../third_party/portmidi/pm_mac)
    target_link_libraries(coremidi_regression PRIVATE speedmidi_portmidi)
    add_test(NAME coremidi_regression COMMAND coremidi_regression)
    set_tests_properties(coremidi_regression PROPERTIES TIMEOUT 30)
endif()
