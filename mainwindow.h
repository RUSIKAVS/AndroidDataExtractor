#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include "partitionanalyzer.h"
#include "hexviewer.h"
#include "filemanager.h"

// Форвард декларация классов Qt
QT_BEGIN_NAMESPACE
class QPushButton;
class QLabel;
class QListWidget;
class QTextEdit;
class QVBoxLayout;
class QHBoxLayout;
class QSplitter;
class QFileDialog;
class QProgressBar;
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief Главное окно приложения Android Extractor
 *
 * Основной интерфейс пользователя, объединяющий все компоненты приложения.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор главного окна
     * @param parent Родительский виджет
     */
    MainWindow(QWidget *parent = nullptr);

    /**
     * @brief Деструктор главного окна
     */
    ~MainWindow();

private slots:
    /**
     * @brief Открывает диалог выбора папки
     */
    void openFolderDialog();

    /**
     * @brief Анализирует выбранный файл
     */
    void analyzeSelectedFile();

    /**
     * @brief Обрабатывает выбор раздела
     * @param index Индекс выбранного раздела
     */
    void onPartitionSelected(int index);

    /**
     * @brief Отображает сообщение лога
     * @param message Сообщение для отображения
     */
    void displayLogMessage(const QString &message);

    /**
     * @brief Отображает сообщение об ошибке
     * @param errorMessage Сообщение об ошибке
     */
    void displayErrorMessage(const QString &errorMessage);

private:
    /**
     * @brief Настраивает пользовательский интерфейс
     */
    void setupUi();

    /**
     * @brief Настраивает соединения сигналов и слотов
     */
    void setupConnections();

    /**
     * @brief Сканирует папку на наличие файлов образов
     * @param folderPath Путь к папке
     */
    void scanFolderForImageFiles(const QString &folderPath);

    /**
     * @brief Обновляет список файлов образов
     * @param fileList Список файлов
     */
    void updateImageFileList(const QStringList &fileList);

    // Виджеты интерфейса
    QPushButton *m_btnOpenFolder;
    QPushButton *m_btnAnalyze;
    QLabel *m_lblFolderPath;
    QLabel *m_lblSelectedFile;
    QListWidget *m_listImageFiles;
    QListWidget *m_listPartitions;
    HexViewer *m_hexViewer;
    FileManager *m_fileManager;
    QTextEdit *m_logOutput;
    QProgressBar *m_progressBar;

    // Рабочие переменные
    QString m_currentFolder;
    QString m_selectedImageFile;
    DiskInfo m_currentDiskInfo;

    // Модули
    PartitionAnalyzer *m_partitionAnalyzer;
};

#endif // MAINWINDOW_H
