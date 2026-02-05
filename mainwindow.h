#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidgetItem>
#include <QTableWidgetItem>
#include <QLabel>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <memory>

// Предварительное объявление классов
class PartitionAnalyzer;
class SuperAnalyzer;
class GuidManager;
class FileManager;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /**
     * @brief Добавить сообщение в лог
     * @param message Сообщение для логирования
     * @param type Тип сообщения (info, warning, error)
     */
    void logMessage(const QString &message, const QString &type = "info");

private slots:
    // Слоты для меню
    void on_actionOpenImage_triggered();
    void on_actionOpenFolder_triggered();
    void on_actionExtractPartition_triggered();
    void on_actionAnalyzeSuper_triggered();
    void on_actionDarkTheme_triggered();
    void on_actionLightTheme_triggered();
    void on_actionExit_triggered();
    void on_actionAbout_triggered();

    // Слоты для логов
    void on_clearLogsButton_clicked();
    void on_saveLogsButton_clicked();

    // Слоты для виджетов
    void on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_treeWidget_customContextMenuRequested(const QPoint &pos);
    void on_guidTableWidget_itemDoubleClicked(QTableWidgetItem *item);

    // Слот для прогресса файловых операций
    void onFileProgressChanged(int current, int total, const QString &message);

private:
    // Инициализация
    void initUI();
    void setupTreeWidget();
    void setupGuidTable();
    void setupHexViewer();
    void setupLogsViewer();
    void setupStatusBar();
    void setupMenuAndToolbar();
    void connectSignalsSlots();

    // Темы
    void applyDarkTheme();
    void applyLightTheme();

    // Вспомогательные методы
    void updateStatusBar(const QString &message, int timeout = 3000);
    void showProgress(bool show, int maximum = 100);
    void updateProgress(int value);

    // Работа с разделами
    void addPartitionToTree(const QString &name, const QString &guid,
                            quint64 size, const QString &type,
                            const QString &path, quint64 offset = 0);
    QString formatSize(quint64 bytes) const;
    void analyzeImageFile(const QString &filePath);
    void analyzeFolder(const QString &folderPath);
    void scanFolderForImages(const QString &folderPath);
    void checkAndAddSuperPartition(const QString &filePath);
    void autoSuperPartitionAnalysis();
    void showPartitionInfo(const QVariantMap &partData);
    QString getGuidUsage(const QString &guid) const;

    // Члены класса
    Ui::MainWindow *ui;
    std::unique_ptr<PartitionAnalyzer> m_partitionAnalyzer;
    std::unique_ptr<SuperAnalyzer> m_superAnalyzer;
    std::unique_ptr<GuidManager> m_guidManager;
    std::unique_ptr<FileManager> m_fileManager;

    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;

    QString m_currentImagePath;
    QString m_currentFolderPath;
    QMap<QString, QVariantMap> m_partitionMap;
};

#endif // MAINWINDOW_H
