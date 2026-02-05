#ifndef HEXVIEWER_H
#define HEXVIEWER_H

#include <QObject>
#include <QWidget>
#include <QTextEdit>
#include <QScrollBar>
#include <QFont>
#include <QByteArray>
#include <QFile>
#include <QDebug>

/**
 * @brief Класс для просмотра данных в шестнадцатеричном формате
 *
 * Предоставляет виджет для отображения бинарных данных в формате
 * hex dump с адресами, hex значениями и ASCII представлением.
 */
class HexViewer : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор Hex Viewer
     * @param parent Родительский виджет
     */
    explicit HexViewer(QWidget *parent = nullptr);

    /**
     * @brief Деструктор
     */
    ~HexViewer() override = default;

    /**
     * @brief Загружает данные из файла для отображения
     * @param filePath Путь к файлу
     * @param offset Смещение в файле (в байтах)
     * @param size Количество байт для чтения (0 = все)
     * @return true если данные успешно загружены
     */
    bool loadDataFromFile(const QString &filePath, qint64 offset = 0, qint64 size = 0);

    /**
     * @brief Загружает данные из памяти для отображения
     * @param data Данные для отображения
     */
    void loadDataFromMemory(const QByteArray &data);

    /**
     * @brief Очищает отображаемые данные
     */
    void clear();

    /**
     * @brief Устанавливает количество байт в строке
     * @param bytesPerLine Количество байт (16 по умолчанию)
     */
    void setBytesPerLine(int bytesPerLine);

    /**
     * @brief Возвращает текущее количество байт в строке
     * @return Количество байт в строке
     */
    int bytesPerLine() const;

    /**
     * @brief Включает/отключает отображение адресов
     * @param enabled true для включения адресов
     */
    void setAddressEnabled(bool enabled);

    /**
     * @brief Включает/отключает отображение ASCII
     * @param enabled true для включения ASCII
     */
    void setAsciiEnabled(bool enabled);

private:
    /**
     * @brief Инициализирует пользовательский интерфейс
     */
    void setupUi();

    /**
     * @brief Обновляет отображение данных
     */
    void updateDisplay();

    /**
     * @brief Форматирует байт в шестнадцатеричную строку
     * @param byte Байт для форматирования
     * @return Строка в формате "XX"
     */
    QString formatByte(quint8 byte) const;

    /**
     * @brief Форматирует символ для ASCII отображения
     * @param ch Символ для форматирования
     * @return Отформатированный символ или точка для непечатаемых символов
     */
    QChar formatAsciiChar(char ch) const;

private:
    QTextEdit *m_textEdit;          ///< Текстовый редактор для отображения данных
    QByteArray m_data;              ///< Данные для отображения
    int m_bytesPerLine;             ///< Количество байт в строке
    bool m_showAddress;             ///< Флаг отображения адресов
    bool m_showAscii;               ///< Флаг отображения ASCII
    QFont m_font;                   ///< Шрифт для отображения
};

#endif // HEXVIEWER_H
