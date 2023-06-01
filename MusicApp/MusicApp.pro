QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    musicplayer.cpp

HEADERS += \
    mainwindow.h \
    musicplayer.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    MusicApp_zh_CN.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    MusicLists/1.wav \
    MusicLists/Justin-Hurwitz-Mia-_-Sebastian’s-Theme.wav \
    MusicLists/Ryan-Gosling_Emma-Stone-City-Of-Stars.wav

#INCLUDEPATH += /opt/st/myir/3.1-snapshot/sysroots/cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi/mnt/usb/ffmpeg/include
#LIBS += -L/opt/st/myir/3.1-snapshot/sysroots/cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi/mnt/usb/ffmpeg/lib -lavcodec -lavformat -lavutil
#LIBS += -lasound -lpthread -lswscale -lavdevice -lavfilter -lswresample -ldl -lm -lz -lbz2 -llzma
LIBS += -lasound
