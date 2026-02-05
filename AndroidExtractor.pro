QT       += core gui widgets

CONFIG   += c++20

SOURCES += \
    hexviewer.cpp \
    main.cpp \
    mainwindow.cpp \
    partitionanalyzer.cpp \
    superanalyzer.cpp \
    guidmanager.cpp \
    filemanager.cpp

HEADERS += \
    hexviewer.h \
    mainwindow.h \
    partitionanalyzer.h \
    superanalyzer.h \
    guidmanager.h \
    filemanager.h

FORMS += \
    mainwindow.ui

# Отключаем устаревшие функции
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

# Режим релиза
CONFIG(release, debug|release) {
    DESTDIR = release
    OBJECTS_DIR = release/.obj
    MOC_DIR = release/.moc
    RCC_DIR = release/.rcc
    UI_DIR = release/.ui
}

# Режим отладки
CONFIG(debug, debug|release) {
    DESTDIR = debug
    OBJECTS_DIR = debug/.obj
    MOC_DIR = debug/.moc
    RCC_DIR = debug/.rcc
    UI_DIR = debug/.ui
}

# Предупреждения
QMAKE_CXXFLAGS_WARN_ON += -Wall -Wextra
QMAKE_CXXFLAGS += -std=c++20

# Ресурсы (если есть иконки)
# RESOURCES += resources.qrc
