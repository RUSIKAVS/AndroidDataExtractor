#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QWidget>
#include <QTreeWidget>
#include <QString>
#include <QByteArray>

/**
 * @class FileManager
 * @brief Виджет файлового менеджера для отображения содержимого разделов
 *
 * Этот виджет отображает файловую структуру выбранного раздела.
 * В текущей версии отображает только базовую информацию о разделе.
 */
class FileManager : public QTreeWidget
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор класса
     * @param parent Родительский виджет
     */
    explicit FileManager(QWidget *parent = nullptr);

    /**
     * @brief Отображает информацию о разделе
     * @param partitionName Имя раздела
     * @param partitionInfo Информация о разделе в виде строки
     */
    void displayPartitionInfo(const QString &partitionName, const QString &partitionInfo);

    /**
     * @brief Очищает отображение
     */
    void clearDisplay();

private:
    /**
     * @brief Настраивает виджет
     */
    void setupUi();
};

#endif // FILEMANAGER_H
