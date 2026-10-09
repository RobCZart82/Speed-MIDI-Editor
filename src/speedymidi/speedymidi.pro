# Retained legacy project; the supported Qt 6 build is top-level CMake.
error("Legacy qmake build retired: configure the repository root with CMake; see README.md")

# speedymidi.pro
# Holger Hoffmann
# created on 2009-10-08

TARGET = speedymidi
TEMPLATE = app
macx:CONFIG += x86

SOURCES += main.cpp \
    cs_common.cpp \
    cs_file.cpp \
    cs_navigation.cpp \
    cs_localmassedit.cpp \
    cs_clipboard.cpp \
    cs_write.cpp \
    cs_utilities.cpp \
    cs_playback.cpp \
    controller.cpp \
    controllersubsystem.cpp \
    doc_event.cpp \
    doc_track.cpp \
    doc_measureitem.cpp \
    doc_root.cpp \
    editorstate.cpp \
    view.cpp \
    editormapper.cpp \
    commands.cpp \
    global.cpp \
    mainwindow.cpp \
    zoomsliderwidget.cpp \
    speedymidiapp.cpp \
    launchdialog.cpp \
    preferencesdialog.cpp \
    trackwizarddialog.cpp \
    writelengthdialog.cpp \
    insertdialog.cpp \
    measurepropertiesdialog.cpp \
    trackpropertiesdialog.cpp \
    swingifydialog.cpp \
    mousepianodockwidget.cpp \
    mousepianowidget.cpp \
    smfdocument.cpp \
    smfimporter.cpp \
    smfexporter.cpp \
    midiinterface.cpp \
    qtsingleapplication/qtsingleapplication.cpp \
    qtsingleapplication/qtlocalpeer.cpp \
    settings.cpp \
    partextractiondialog.cpp \
    conversionoptionsdialog.cpp
HEADERS += cs_common.h \
    cs_file.h \
    cs_navigation.h \
    cs_localmassedit.h \
    cs_clipboard.h \
    cs_write.h \
    cs_utilities.h \
    cs_playback.h \
    controller.h \
    controllersubsystem.h \
    doc_event.h \
    doc_track.h \
    doc_measureitem.h \
    doc_root.h \
    editorstate.h \
    view.h \
    editormapper.h \
    commands.h \
    global.h \
    mainwindow.h \
    zoomsliderwidget.h \
    speedymidiapp.h \
    launchdialog.h \
    preferencesdialog.h \
    trackwizarddialog.h \
    writelengthdialog.h \
    insertdialog.h \
    trackpropertiesdialog.h \
    measurepropertiesdialog.h \
    swingifydialog.h \
    mousepianodockwidget.h \
    mousepianowidget.h \
    smfdocument.h \
    smfexporter.h \
    smfimporter.h \
    midiinterface.h \
    qtsingleapplication/qtsingleapplication.h \
    qtsingleapplication/qtlocalpeer.h \
    settings.h \
    partextractiondialog.h \
    conversionoptionsdialog.h
FORMS += mainwindow.ui \
    launchdialog.ui \
    preferencesdialog.ui \
    trackwizarddialog.ui \
    writelengthdialog.ui \
    insertdialog.ui \
    trackpropertiesdialog.ui \
    measurepropertiesdialog.ui \
    swingifydialog.ui \
    partextractiondialog.ui \
    conversionoptionsdialog.ui

CONFIG( debug, debug|release ) {
    LIBS += -L../lib -lportmidi_debug
}
CONFIG( release, debug|release ) {
    LIBS += -L../lib -lportmidi_release
}
win32:LIBS += -lwinmm
unix:!macx:LIBS += -lasound
macx:LIBS += -framework Carbon -framework CoreMIDI -framework CoreAudio -framework CoreFoundation

OTHER_FILES += 
RESOURCES += speedymidi.qrc
RC_FILE = speedymidi.rc
QT += xml network
TRANSLATIONS = translations/msg_en.ts \
    translations/music_en.ts \
    translations/msg_de.ts \
    translations/music_de.ts

