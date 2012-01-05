#-------------------------------------------------
#
# Project created by QtCreator 2011-11-23T08:52:01
#
#-------------------------------------------------

QT       -= core gui

TARGET = gc
TEMPLATE = lib
CONFIG += staticlib

#QMAKE_CXX = clang

SOURCES += \
	../../../src/gc.cpp

HEADERS += \
	../../../include/gc.hpp

INCLUDEPATH += ../../../include/

unix:!symbian {
	maemo5 {
		target.path = /opt/usr/lib
	} else {
		target.path = /usr/lib
	}
	INSTALLS += target
}
