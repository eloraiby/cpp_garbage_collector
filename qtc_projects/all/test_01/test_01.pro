#-------------------------------------------------
#
# Project created by QtCreator 2011-11-23T09:07:01
#
#-------------------------------------------------

QT       -= core gui

TARGET = test_01
CONFIG   += console
CONFIG   -= app_bundle

#QMAKE_CXX = clang

TEMPLATE = app

INCLUDEPATH += ../../../include/

SOURCES += \
	../../../test/test_01.cpp

DEPENDENCY_LIBRARIES = gc

OTHER_FILES +=

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../gc/release/ -lgc
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../gc/debug/ -lgc
else:unix:!symbian: LIBS += -L$$OUT_PWD/../gc/ -lgc

INCLUDEPATH += $$PWD/../gc
DEPENDPATH += $$PWD/../gc

win32:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gc/release/gc.lib
else:win32:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gc/debug/gc.lib
else:unix:!symbian: PRE_TARGETDEPS += $$OUT_PWD/../gc/libgc.a
