#include "mainwindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Настройка приложения
    a.setApplicationName("Android Data Extractor");
    a.setApplicationVersion("1.0.0");
    a.setOrganizationName("AndroidExtractor");

    MainWindow w;
    w.show();

    return a.exec();
}
