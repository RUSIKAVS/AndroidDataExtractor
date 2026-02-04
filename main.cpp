#include "mainwindow.h"
#include <QApplication>

/**
 * @brief Точка входа в приложение
 * @param argc Количество аргументов командной строки
 * @param argv Массив аргументов командной строки
 * @return Код возврата приложения
 */
int main(int argc, char *argv[])
{
    // Создаем приложение Qt
    QApplication app(argc, argv);

    // Устанавливаем информацию о приложении
    app.setApplicationName("Android Extractor");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("AndroidExtractor");

    // Создаем и показываем главное окно
    MainWindow mainWindow;
    mainWindow.show();

    // Запускаем главный цикл приложения
    return app.exec();
}
