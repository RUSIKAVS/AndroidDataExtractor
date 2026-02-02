QT += core gui widgets concurrent

CONFIG += c++20

TARGET = AndroidExtractorTester
TEMPLATE = app

SOURCES += \
    main.cpp \
    AndroidExtractor.cpp \
    TesterWidget.cpp

HEADERS += \
    AndroidExtractor.h \
    TesterWidget.h

# Настройки компилятора
QMAKE_CXXFLAGS += -std=c++20
