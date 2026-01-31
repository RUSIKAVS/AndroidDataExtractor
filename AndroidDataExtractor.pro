QT += core gui widgets concurrent
CONFIG += c++17

TARGET = AndroidDataExtractor
TEMPLATE = app

# ===============================
# SOURCES
# ===============================
SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp \
    \
    src/core/AndroidCrypto.cpp \
    src/core/AndroidPartition.cpp \
    src/core/DeviceInfoParser.cpp \
    src/core/FileSystemExplorer.cpp \
    src/core/KeyParser.cpp \
    src/core/MtkCrypto.cpp \
    src/core/PartitionParser.cpp \
    \
    src/core/ext4/Ext4BlockDevice.cpp \
    src/core/ext4/Ext4Directory.cpp \
    src/core/ext4/Ext4Exporter.cpp \
    src/core/ext4/Ext4InodeReader.cpp \
    src/core/ext4/Ext4PathResolver.cpp

# ===============================
# HEADERS
# ===============================
HEADERS += \
    src/MainWindow.hpp \
    \
    src/core/AndroidCrypto.hpp \
    src/core/AndroidPartition.hpp \
    src/core/DeviceInfoParser.hpp \
    src/core/FileSystemExplorer.hpp \
    src/core/KeyParser.hpp \
    src/core/MtkCrypto.hpp \
    src/core/PartitionParser.hpp \
    \
    src/core/ext4/Ext4BlockDevice.hpp \
    src/core/ext4/Ext4Directory.hpp \
    src/core/ext4/Ext4Exporter.hpp \
    src/core/ext4/Ext4InodeReader.hpp \
    src/core/ext4/Ext4PathResolver.hpp \
    src/core/ext4/Ext4Progress.hpp \
    src/core/ext4/Ext4Structs.hpp

# ===============================
# UI FORMS
# ===============================
FORMS += \
    src/ui/MainWindow.ui

# ===============================
# INCLUDE PATHS
# ===============================
INCLUDEPATH += \
    $$PWD/src \
    $$PWD/src/core \
    $$PWD/src/core/ext4 \
    $$PWD/src/ui

# ===============================
# FLAGS
# ===============================
QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter
