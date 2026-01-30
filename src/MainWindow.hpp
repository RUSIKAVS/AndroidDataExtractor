#pragma once

#include "AndroidPartition.hpp"
#include "FileSystemExplorer.hpp"
#include <QMainWindow>
#include <QString>
#include <QTreeWidget>
#include <QProgressBar>
#include <QList>

// Forward declarations
class QAction;
class QMenu;
class QTreeWidgetItem;
struct AndroidPartition;
class KeyParser;

struct PinSettings {
    bool enabled = false;
    QList<int> lengths; // 4, 5, 6, 8
    int start = 0;
    int end = 9999;
    bool leadingZeros = true;
    bool repeatedDigits = true;
    bool sequential = true;
};

struct PatternSettings {
    bool enabled = false;
    QList<int> gridSizes; // 3, 4, 5, 6
    int minLength = 4;
    int maxLength = 6;
    bool allowOverlap = true;
    bool simpleOnly = false;
};

struct AlphanumSettings {
    bool enabled = false;
    int minLength = 6;
    int maxLength = 8;
    bool useLowercase = true;
    bool useUppercase = true;
    bool useDigits = true;
    bool useSpecial = false;
    QString mask;
};

struct DictSettings {
    bool enabled = false;
    QString filePath;
    bool caseSensitive = false;
    bool tryVariations = true;
    bool appendNumbers = true;
    bool prependNumbers = true;
};

struct BruteForceSettings {
    AndroidPartition partition;
    PinSettings pinSettings;
    PatternSettings patternSettings;
    AlphanumSettings alphanumSettings;
    DictSettings dictSettings;
    QString method = "CPU";
    int threadCount = 1;
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    // Инициализация
    void initUI();
    void initMenu();
    void initToolbar();
    void initConnections();
    void restoreSettings();
    void saveSettings();

    // Основные функции
    void analyzeDirectory(const QString& path);
    void loadEncryptionKeys(const QString& path);
    void updateStatistics();

    // Отображение разделов
    void displayPartitions(const QList<AndroidPartition>& partitions);
    void addPartitionToTree(const AndroidPartition& partition, QTreeWidgetItem* parent = nullptr);
    void showPartitionInfo(const AndroidPartition& partition);

    // Работа с разделами
    AndroidPartition* getSelectedPartition();
    AndroidPartition* getPartitionFromItem(QTreeWidgetItem* item);

    // Логирование и отчеты
    QString generateAnalysisReport() const;
    void saveReport(const QString& report);
    void extractUserdataPartition();

    // Подбор паролей
    void startSimpleBruteForce(const AndroidPartition& partition, int method, const QString& dictPath);

    // Вспомогательные методы
    void clearDisplay();
    void updateStatus(const QString& message);
    void updateProgress(int value);
    void cleanupTreeWidget();

private slots:
    // Основные слоты из .ui файла
    void onSelectDirectory();
    void onAnalyzeClicked();
    void onExtractPartition();
    void onExtractAllPartitions();
    void showLog();
    void displayKeyInfo();
    void testDecryption();
    void onForceRawParse();
    void onShowMemoryMap();
    void testWithKnownPassword();
    void createTestScenario();

    // Дополнительные слоты
    void onTreeItemClicked(QTreeWidgetItem* item, int column);

    // Слоты для кнопок
    void onGenerateReport();
    void onQuickExtract();
    void onViewKeys();
    void onRefresh();
    void onToggleDarkMode();
    void onCheckUpdates();
    void onDocumentation();

    // Простые слоты
    void onAbout();

private:
    Ui::MainWindow *ui;
    KeyParser* keyParser;
    FileSystemExplorer* fileExplorer;
    QProgressBar* progressBar;
    QString currentDirectory;
    QList<AndroidPartition> currentPartitions;
    void displayDeviceInfo(const QString& path);
    void showFileSystemExplorer();
    QString formatFileSize(qint64 bytes);

};
