# portmidi.pro
# Holger 2011-03-03: Now also tested on linux. Mac config added, but not tested.

QT       -= core gui

DESTDIR = ../lib

CONFIG( debug, debug|release ) {
    TARGET = portmidi_debug
}
CONFIG( release, debug|release ) {
    TARGET = portmidi_release
}

TEMPLATE = lib
CONFIG += staticlib # replace this with DLL for dynamic link on Windows

INCLUDEPATH = pm_common/ porttime/

win32 {
	INCLUDEPATH += pm_win/
	LIBS += -lwinmm
	SOURCES += pm_win/pmwinmm.c \
            pm_win/pmwin.c \
            porttime/ptwinmm.c
        HEADERS += pm_win/pmwinmm.h
}

unix:!macx {
	DEFINES += PMALSA
	INCLUDEPATH += pm_linux/
	LIBS += -lasound
	SOURCES += pm_linux/finddefault.c \
	    pm_linux/pmlinux.c \
            pm_linux/pmlinuxalsa.c \
            porttime/ptlinux.c
	HEADERS += pm_linux/pmlinux.h pm_linux/pmlinuxalsa.h
}

macx {
        CONFIG += x86
        INCLUDEPATH += pm_mac/
        LIBS += /System/Library/Frameworks/CoreMIDI.framework /System/Library/Frameworks/CoreFoundation.framework
        SOURCES += pm_mac/finddefault.c \
            pm_mac/pmmac.c \
            pm_mac/pmmacosxcm.c \
            pm_mac/readbinaryplist.c \
            porttime/ptmacosx_cf.c #alternatively: porttime/ptmacosx_mach.c
        HEADERS += pm_mac/pmmac.h pm_mac/pmmacosxcm.h pm_mac/readbinaryplist.h
}

DEFINES -= UNICODE

SOURCES += \
    pm_common/portmidi.c \
    pm_common/pmutil.c \
    porttime/porttime.c

HEADERS += \
    pm_common/pmutil.h \
    pm_common/pminternal.h \
    pm_common/portmidi.h \
    porttime/porttime.h
