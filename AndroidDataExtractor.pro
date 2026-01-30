QT = core gui widgets concurrent
CONFIG += c++17
TARGET = AndroidDataExtractor
TEMPLATE = app

# ============================================================================
# OpenSSL Configuration
# ============================================================================

QMAKE_CXXFLAGS += -v
CONFIG += debug_and_release
QMAKE_CFLAGS_RELEASE += -v
QMAKE_CXXFLAGS_RELEASE += -v


HASHCAT_ROOT = "C:/dev/qt/hashcat"
exists($$HASHCAT_ROOT/include/hashcat.h) {
    message("✓ Found Hashcat at: $$HASHCAT_ROOT")
    INCLUDEPATH += $$HASHCAT_ROOT/include
    DEFINES += WITH_HASHCAT
} else {
    message("✗ Hashcat not found, building without hashcat support")
}

# Путь к OpenSSL
OPENSSL_ROOT = "C:/OpenSSL-Win64/include"

# Проверяем наличие OpenSSL
exists($$OPENSSL_ROOT/include/openssl/evp.h) {
    message("✓ Found OpenSSL at: $$OPENSSL_ROOT")

    INCLUDEPATH += $$OPENSSL_ROOT/include
    LIBS += -L$$OPENSSL_ROOT/lib

    # Для MinGW
    win32:g++ {
        LIBS += -llibcrypto -llibssl -lws2_32 -lgdi32 -lcrypt32
        DEFINES += WITH_OPENSSL OPENSSL_MINGW
        message("Using MinGW OpenSSL libraries")
    }

    # Для MSVC
    win32:msvc {
        LIBS += libcrypto.lib libssl.lib crypt32.lib ws2_32.lib
        DEFINES += WITH_OPENSSL OPENSSL_MSVC
        message("Using MSVC OpenSSL libraries")
    }

} else {
    message("✗ OpenSSL not found at $$OPENSSL_ROOT")
    DEFINES += NO_OPENSSL
    message("Building without OpenSSL (crypto features disabled)")
}

# ============================================================================
# Файлы проекта
# ============================================================================

SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp \
    src/core/PartitionParser.cpp \
    src/core/KeyParser.cpp \
    src/core/AndroidPartition.cpp \
    src/core/PasswordBruteforcer.cpp \
    src/core/MtkCrypto.cpp \
    src/core/AndroidCrypto.cpp \
    src/core/MaskGenerator.cpp \
    src/core/FileSystemExplorer.cpp \
    src/core/DeviceInfoParser.cpp \
    src/core/ForceLogger.cpp \
    src/core/HashcatBruteforcer.cpp

HEADERS += \
    src/MainWindow.hpp \
    src/core/PartitionParser.hpp \
    src/core/KeyParser.hpp \
    src/core/AndroidPartition.hpp \
    src/core/PasswordBruteforcer.hpp \
    src/core/MtkCrypto.hpp \
    src/core/AndroidCrypto.hpp \
    src/core/MaskGenerator.hpp \
    src/core/FileSystemExplorer.hpp \
    src/core/DeviceInfoParser.hpp \
    src/core/ForceLogger.hpp \
    src/core/HashcatBruteforcer.hpp

FORMS += src/ui/MainWindow.ui
RESOURCES += resources/resources.qrc

# ============================================================================
# Компилятор и линковка
# ============================================================================

# Для больших файлов
QMAKE_CXXFLAGS += -D_FILE_OFFSET_BITS=64
QMAKE_CXXFLAGS += -D_LARGEFILE64_SOURCE

# Предупреждения
QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable

# Пути включения
INCLUDEPATH += $$PWD/src \
               $$PWD/src/core \
               $$PWD/src/ui

# Автоматическая генерация
CONFIG += uitools

# Для отладки
CONFIG(debug, debug|release) {
    DESTDIR = debug
    TARGET = $$join(TARGET,,,_debug)
    QMAKE_CXXFLAGS += -g -O0 -DDEBUG
} else {
    DESTDIR = release
    QMAKE_CXXFLAGS += -O2 -DNDEBUG
}

# Копируем DLL при сборке
win32 {
    # Копируем OpenSSL DLL если есть
    exists($$OPENSSL_ROOT/bin/libcrypto-*.dll) {
        QMAKE_POST_LINK += $$quote(cmd /c if exist \"$$OPENSSL_ROOT/bin/libcrypto-*.dll\" copy /Y \"$$OPENSSL_ROOT/bin/libcrypto-*.dll\" \"$${DESTDIR}\" $$escape_expand(\n\t))
        QMAKE_POST_LINK += $$quote(cmd /c if exist \"$$OPENSSL_ROOT/bin/libssl-*.dll\" copy /Y \"$$OPENSSL_ROOT/bin/libssl-*.dll\" \"$${DESTDIR}\" $$escape_expand(\n\t))
    }
}
