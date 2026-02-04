#include "filemanager.h"
#include <QHeaderView>

FileManager::FileManager(QWidget *parent)
    : QTreeWidget(parent)
{
    setupUi();
}

void FileManager::setupUi()
{
    // Настраиваем заголовки колонок
    setHeaderLabels(QStringList() << "Property" << "Value");

    // Настраиваем поведение
    setColumnWidth(0, 200);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    // Очищаем начальное содержимое
    clear();
}

void FileManager::displayPartitionInfo(const QString &partitionName, const QString &partitionInfo)
{
    clear();

    if (partitionName.isEmpty()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(this);
        item->setText(0, "Status");
        item->setText(1, "No partition selected");
        return;
    }

    // Создаем корневой элемент с именем раздела
    QTreeWidgetItem *rootItem = new QTreeWidgetItem(this);
    rootItem->setText(0, partitionName);
    rootItem->setText(1, "");
    rootItem->setExpanded(true);

    // Разбиваем информацию о разделе на строки
    QStringList infoLines = partitionInfo.split("\n", Qt::SkipEmptyParts);

    for (const QString &line : infoLines) {
        // Пытаемся разбить строку на ключ и значение
        int colonIndex = line.indexOf(":");
        if (colonIndex > 0) {
            QString key = line.left(colonIndex).trimmed();
            QString value = line.mid(colonIndex + 1).trimmed();

            QTreeWidgetItem *item = new QTreeWidgetItem(rootItem);
            item->setText(0, key);
            item->setText(1, value);
        }
    }

    // Автоматически подгоняем размер колонок
    resizeColumnToContents(0);
}

void FileManager::clearDisplay()
{
    clear();
    QTreeWidgetItem *item = new QTreeWidgetItem(this);
    item->setText(0, "Status");
    item->setText(1, "Select a partition to view details");
}
