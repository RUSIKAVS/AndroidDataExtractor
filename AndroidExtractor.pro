QT       = core gui widgets

CONFIG   += c++20

SOURCES += \
    filemanager.cpp \
    main.cpp \
    mainwindow.cpp \
    partitionanalyzer.cpp \
    guidmanager.cpp \
    hexviewer.cpp

HEADERS += \
    filemanager.h \
    mainwindow.h \
    partitionanalyzer.h \
    guidmanager.h \
    hexviewer.h

FORMS +=

# Настройки компиляции
QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
CONFIG += debug_and_release
CONFIG(debug, debug|release) {
    TARGET = AndroidExtractor-debug
} else {
    TARGET = AndroidExtractor
}
