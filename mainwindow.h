#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenu>
#include <QTableWidgetItem>
#include <QTreeWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Предварительные объявления классов
class PartitionAnalyzer;
class HexViewer;
class GuidManager;
class FileManager;
class SuperAnalyzer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // ========== Настройка интерфейса ==========
    void setupUi();
    void setupPartitionsTable();
    void setupFileTree();
    void setupConnections();
    void loadSettings();
    void saveSettings();

    // ========== Обработка кнопок ==========
    void onBrowseClicked();
    void onScanClicked();
    void onExtractClicked();
    void onExtractAllClicked();
    void onRefreshFilesClicked();
    void onExtractFileClicked();
    void onFindFilesClicked();
    void onAnalyzeSuperClicked();
    void onBrowseExtractClicked();
    void onSaveSettingsClicked();
    void onDefaultSettingsClicked();
    void onSaveReportClicked();

    // ========== Обработка результатов ==========
    void onAnalysisComplete(bool success);
    void onPartitionDoubleClicked(QTableWidgetItem* item);
    void onFileDoubleClicked(QTreeWidgetItem* item, int column);

    // ========== Контекстное меню ==========
    void showPartitionContextMenu(const QPoint& pos);
    void copyPartitionName();
    void copyPartitionGuid();
    void copyPartitionOffset();
    void showInHexViewer();

    // ========== Управление логами ==========
    void onLogMessage(const QString& message);
    void onClearLogClicked();
    void onSaveLogClicked();
    void onCopyLogClicked();
    void onAutoScrollChanged(int state);

    // ========== Диалоги и сообщения ==========
    void showError(const QString& message);

private:
    // ========== Вспомогательные методы ==========
    QString formatFileSize(qint64 size);
    void setUiEnabled(bool enabled);
    void logMessage(const QString &message);  // Добавьте эту строку!

private:
    // ========== Члены данных ==========
    Ui::MainWindow *ui;                         ///< Указатель на сгенерированный UI

    // Указатели на управляющие классы
    PartitionAnalyzer *m_partitionAnalyzer;     ///< Анализатор разделов
    HexViewer *m_hexViewer;                     ///< Hex-просмотрщик
    GuidManager *m_guidManager;                 ///< Менеджер GUID
    FileManager *m_fileManager;                 ///< Менеджер файлов
    SuperAnalyzer *m_superAnalyzer;             ///< Анализатор суперблока

    QMenu *m_contextMenu;                       ///< Контекстное меню для таблицы разделов
    int m_selectedPartitionRow = -1;            ///< Индекс выбранной строки в таблице разделов
};

#endif // MAINWINDOW_H
