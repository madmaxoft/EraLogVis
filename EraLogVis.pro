#-------------------------------------------------
#
# Project created by QtCreator 2017-02-21T15:21:38
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = EraLogVis
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

unix {
	LIBS += -lz
}

SOURCES += \
	main.cpp\
	MainWindow.cpp \
	Session.cpp \
	LogFile.cpp \
	FileParser.cpp \
	SessionSourcesModel.cpp \
	SessionMessagesModel.cpp \
	Stopwatch.cpp \
	MessageView.cpp \
	BackgroundParser.cpp

HEADERS  += \
	MainWindow.h \
	Session.h \
	LogFile.h \
	Exceptions.h \
	FileParser.h \
	SessionSourcesModel.h \
	SessionMessagesModel.h \
	Stopwatch.h \
	MessageView.h \
	BackgroundParser.h

FORMS    += \
	MainWindow.ui

RESOURCES += \
	Resources/Resources.qrc
