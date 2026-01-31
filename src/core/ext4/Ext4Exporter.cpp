#include "core/Ext4Exporter.hpp"

void MainWindow::onExportPathClicked()
{
    QString img = currentDirectory + "/userdata_decrypted.img";

    QString out = QFileDialog::getExistingDirectory(
        this,
        "Выберите папку для экспорта"
    );

    if (out.isEmpty())
        return;

    Ext4Exporter::exportPath(
        img,
        "/data/media/0",
        out
    );
}
