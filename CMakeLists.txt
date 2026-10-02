cmake_minimum_required(VERSION 3.21)

project(SpeedMIDIEditor VERSION 0.1.2 LANGUAGES C CXX)

configure_file(src/speedymidi/buildversion.h.in generated/buildversion.h @ONLY)
include_directories("${PROJECT_BINARY_DIR}/generated")

include(CTest)

option(SPEED_MIDI_SANITIZERS "Run with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(SPEED_MIDI_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "SPEED_MIDI_SANITIZERS requires Clang or GCC")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src/speedymidi")

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Xml Network Svg LinguistTools)

set(SPEED_MIDI_SOURCES
    src/speedymidi/main.cpp
    src/speedymidi/cs_common.cpp
    src/speedymidi/cs_file.cpp
    src/speedymidi/cs_navigation.cpp
    src/speedymidi/cs_localmassedit.cpp
    src/speedymidi/cs_clipboard.cpp
    src/speedymidi/cs_write.cpp
    src/speedymidi/cs_utilities.cpp
    src/speedymidi/cs_playback.cpp
    src/speedymidi/controller.cpp
    src/speedymidi/controllersubsystem.cpp
    src/speedymidi/doc_event.cpp
    src/speedymidi/doc_track.cpp
    src/speedymidi/doc_measureitem.cpp
    src/speedymidi/doc_root.cpp
    src/speedymidi/editorstate.cpp
    src/speedymidi/view.cpp
    src/speedymidi/editormapper.cpp
    src/speedymidi/commands.cpp
    src/speedymidi/global.cpp
    src/speedymidi/mainwindow.cpp
    src/speedymidi/zoomsliderwidget.cpp
    src/speedymidi/speedymidiapp.cpp
    src/speedymidi/launchdialog.cpp
    src/speedymidi/preferencesdialog.cpp
    src/speedymidi/trackwizarddialog.cpp
    src/speedymidi/writelengthdialog.cpp
    src/speedymidi/insertdialog.cpp
    src/speedymidi/measurepropertiesdialog.cpp
    src/speedymidi/trackpropertiesdialog.cpp
    src/speedymidi/swingifydialog.cpp
    src/speedymidi/mousepianodockwidget.cpp
    src/speedymidi/mousepianowidget.cpp
    src/speedymidi/smfdocument.cpp
    src/speedymidi/smfimporter.cpp
    src/speedymidi/smfexporter.cpp
    src/speedymidi/midiinterface.cpp
    src/speedymidi/qtsingleapplication/qtsingleapplication.cpp
    src/speedymidi/qtsingleapplication/qtlocalpeer.cpp
    src/speedymidi/settings.cpp
    src/speedymidi/partextractiondialog.cpp
    src/speedymidi/conversionoptionsdialog.cpp
    src/speedymidi/speedymidi.qrc
)

set(SPEED_MIDI_HEADERS
    src/speedymidi/cs_common.h
    src/speedymidi/cs_file.h
    src/speedymidi/cs_navigation.h
    src/speedymidi/cs_localmassedit.h
    src/speedymidi/cs_clipboard.h
    src/speedymidi/cs_write.h
    src/speedymidi/cs_utilities.h
    src/speedymidi/cs_playback.h
    src/speedymidi/controller.h
    src/speedymidi/controllersubsystem.h
    src/speedymidi/doc_event.h
    src/speedymidi/doc_track.h
    src/speedymidi/doc_measureitem.h
    src/speedymidi/doc_root.h
    src/speedymidi/editorstate.h
    src/speedymidi/view.h
    src/speedymidi/editormapper.h
    src/speedymidi/commands.h
    src/speedymidi/global.h
    src/speedymidi/mainwindow.h
    src/speedymidi/zoomsliderwidget.h
    src/speedymidi/speedymidiapp.h
    src/speedymidi/launchdialog.h
    src/speedymidi/preferencesdialog.h
    src/speedymidi/trackwizarddialog.h
    src/speedymidi/writelengthdialog.h
    src/speedymidi/insertdialog.h
    src/speedymidi/measurepropertiesdialog.h
    src/speedymidi/trackpropertiesdialog.h
    src/speedymidi/swingifydialog.h
    src/speedymidi/mousepianodockwidget.h
    src/speedymidi/mousepianowidget.h
    src/speedymidi/smfdocument.h
    src/speedymidi/smfimporter.h
    src/speedymidi/smfexporter.h
    src/speedymidi/midiinterface.h
    src/speedymidi/qtsingleapplication/qtsingleapplication.h
    src/speedymidi/qtsingleapplication/qtlocalpeer.h
    src/speedymidi/qtsingleapplication/qtlockedfile.h
    src/speedymidi/settings.h
    src/speedymidi/partextractiondialog.h
    src/speedymidi/conversionoptionsdialog.h
)

set(SPEED_MIDI_FORMS
    src/speedymidi/mainwindow.ui
    src/speedymidi/launchdialog.ui
    src/speedymidi/preferencesdialog.ui
    src/speedymidi/trackwizarddialog.ui
    src/speedymidi/writelengthdialog.ui
    src/speedymidi/insertdialog.ui
    src/speedymidi/trackpropertiesdialog.ui
    src/speedymidi/measurepropertiesdialog.ui
    src/speedymidi/swingifydialog.ui
    src/speedymidi/partextractiondialog.ui
    src/speedymidi/conversionoptionsdialog.ui
)

if(APPLE)
    find_library(COREMIDI_FRAMEWORK CoreMIDI REQUIRED)
    find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(CORESERVICES_FRAMEWORK CoreServices REQUIRED)
    find_library(CARBON_FRAMEWORK Carbon REQUIRED)
    set(PORTMIDI_PLATFORM_SOURCES
        third_party/portmidi/pm_mac/finddefault.c
        third_party/portmidi/pm_mac/pmmac.c
        third_party/portmidi/pm_mac/pmmacosxcm.c
        third_party/portmidi/pm_mac/readbinaryplist.c
        third_party/portmidi/porttime/ptmacosx_cf.c
    )
    set(PORTMIDI_PLATFORM_LIBRARIES
        ${COREMIDI_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${CORESERVICES_FRAMEWORK}
        ${CARBON_FRAMEWORK}
    )
elseif(WIN32)
    set(PORTMIDI_PLATFORM_SOURCES
        third_party/portmidi/pm_win/pmwinmm.c
        third_party/portmidi/pm_win/pmwin.c
        third_party/portmidi/porttime/ptwinmm.c
    )
    set(PORTMIDI_PLATFORM_LIBRARIES winmm)
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
    set(PORTMIDI_PLATFORM_SOURCES
        third_party/portmidi/pm_linux/finddefault.c
        third_party/portmidi/pm_linux/pmlinux.c
        third_party/portmidi/pm_linux/pmlinuxalsa.c
        third_party/portmidi/porttime/ptlinux.c
    )
    set(PORTMIDI_PLATFORM_LIBRARIES PkgConfig::ALSA)
else()
    message(FATAL_ERROR "No PortMidi backend is configured for this platform")
endif()

add_library(speedmidi_portmidi STATIC
    third_party/portmidi/pm_common/portmidi.c
    third_party/portmidi/pm_common/pmutil.c
    third_party/portmidi/porttime/porttime.c
    ${PORTMIDI_PLATFORM_SOURCES}
)
target_include_directories(speedmidi_portmidi PUBLIC
    third_party/portmidi/pm_common
    third_party/portmidi/porttime
)
if(APPLE)
    target_include_directories(speedmidi_portmidi PRIVATE third_party/portmidi/pm_mac)
elseif(WIN32)
    target_include_directories(speedmidi_portmidi PRIVATE third_party/portmidi/pm_win)
elseif(UNIX)
    target_include_directories(speedmidi_portmidi PRIVATE third_party/portmidi/pm_linux)
    target_compile_definitions(speedmidi_portmidi PRIVATE PMALSA)
endif()
target_link_libraries(speedmidi_portmidi PUBLIC ${PORTMIDI_PLATFORM_LIBRARIES})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(WIN32)
    list(APPEND SPEED_MIDI_SOURCES src/speedymidi/speedymidi.rc)
endif()

if(APPLE)
    add_executable(SpeedMIDIEditor MACOSX_BUNDLE
        ${SPEED_MIDI_SOURCES} ${SPEED_MIDI_HEADERS} ${SPEED_MIDI_FORMS})
    set_target_properties(SpeedMIDIEditor PROPERTIES
        OUTPUT_NAME "Speed MIDI Editor"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.speedymidieditor.SpeedMIDIEditor"
        MACOSX_BUNDLE_BUNDLE_NAME "Speed MIDI Editor"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
elseif(WIN32)
    add_executable(SpeedMIDIEditor WIN32
        ${SPEED_MIDI_SOURCES} ${SPEED_MIDI_HEADERS} ${SPEED_MIDI_FORMS})
else()
    add_executable(SpeedMIDIEditor
        ${SPEED_MIDI_SOURCES} ${SPEED_MIDI_HEADERS} ${SPEED_MIDI_FORMS})
endif()

target_include_directories(SpeedMIDIEditor PRIVATE
    src/speedymidi
    third_party/portmidi/pm_common
)
target_link_libraries(SpeedMIDIEditor PRIVATE
    Qt6::Widgets Qt6::Xml Qt6::Network Qt6::Svg speedmidi_portmidi)
if(APPLE)
    target_link_libraries(SpeedMIDIEditor PRIVATE ${AUDIOTOOLBOX_FRAMEWORK})
endif()

qt_add_translations(SpeedMIDIEditor
    TS_FILES
        src/speedymidi/translations/msg_en.ts
        src/speedymidi/translations/msg_de.ts
        src/speedymidi/translations/music_en.ts
        src/speedymidi/translations/music_de.ts
    QM_FILES_OUTPUT_VARIABLE SPEED_MIDI_QM_FILES
)

# Keep non-code resources out of Contents/MacOS so bundle signing succeeds.
if(APPLE)
    set(SPEED_MIDI_TRANSLATIONS_DIR "$<TARGET_BUNDLE_CONTENT_DIR:SpeedMIDIEditor>/Resources/translations")
else()
    set(SPEED_MIDI_TRANSLATIONS_DIR "$<TARGET_FILE_DIR:SpeedMIDIEditor>/translations")
endif()
add_custom_command(TARGET SpeedMIDIEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SPEED_MIDI_TRANSLATIONS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SPEED_MIDI_QM_FILES}
        "${SPEED_MIDI_TRANSLATIONS_DIR}"
    VERBATIM
)

if(APPLE)
    install(TARGETS SpeedMIDIEditor BUNDLE DESTINATION .)
    install(FILES ${SPEED_MIDI_QM_FILES}
        DESTINATION "Speed MIDI Editor.app/Contents/Resources/translations")
else()
    install(TARGETS SpeedMIDIEditor RUNTIME DESTINATION .)
    install(FILES ${SPEED_MIDI_QM_FILES} DESTINATION translations)
endif()
