#include "MainWindow.hpp"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Android Data Extractor");
    app.setOrganizationName("YourCompany");

    MainWindow window;
    window.show();

    return app.exec();
}
