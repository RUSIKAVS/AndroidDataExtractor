#include "hexviewer.h"
#include <QScrollBar>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QPalette>
#include <QApplication>
#include <QThread>
#include <QVBoxLayout>      // Добавьте эту строку
#include <QHBoxLayout>      // Добавьте эту строку (если нужно)
#include <QLabel>           // Добавьте эту строку (если нужно)


/**
 * @brief Конструктор Hex Viewer
 * @param parent Родительский виджет
 */
HexViewer::HexViewer(QWidget *parent)
    : QWidget(parent)
    , m_bytesPerLine(16)
    , m_showAddress(true)
    , m_showAscii(true)
{
    setupUi();
    qDebug() << "HexViewer: Конструктор вызван";
}

/**
 * @brief Инициализирует пользовательский интерфейс
 */
void HexViewer::setupUi()
{
    // Создаем вертикальный layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Создаем текстовый редактор
    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setLineWrapMode(QTextEdit::NoWrap);
    m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_textEdit->setAcceptRichText(false);

    // Настраиваем шрифт (моноширинный для правильного выравнивания)
    m_font = QFont("Courier New", 10);
    m_font.setStyleHint(QFont::TypeWriter);
    m_textEdit->setFont(m_font);

    // Настраиваем цвета
    QPalette palette = m_textEdit->palette();
    palette.setColor(QPalette::Base, QColor(240, 240, 240));
    palette.setColor(QPalette::Text, Qt::black);
    m_textEdit->setPalette(palette);

    layout->addWidget(m_textEdit);

    // Устанавливаем layout
    setLayout(layout);

    qDebug() << "HexViewer: UI инициализирован";
}

/**
 * @brief Загружает данные из файла для отображения
 * @param filePath Путь к файлу
 * @param offset Смещение в файле (в байтах)
 * @param size Количество байт для чтения (0 = все)
 * @return true если данные успешно загружены
 */
bool HexViewer::loadDataFromFile(const QString &filePath, qint64 offset, qint64 size)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "HexViewer: Не удалось открыть файл" << filePath;
        return false;
    }

    if (!file.seek(offset)) {
        qDebug() << "HexViewer: Не удалось переместиться к смещению" << offset;
        file.close();
        return false;
    }

    qint64 fileSize = file.size();
    qint64 bytesToRead = size > 0 ? qMin(size, fileSize - offset) : fileSize - offset;

    if (bytesToRead <= 0) {
        qDebug() << "HexViewer: Нет данных для чтения";
        file.close();
        return false;
    }

    // Читаем данные
    m_data = file.read(bytesToRead);
    file.close();

    if (m_data.isEmpty()) {
        qDebug() << "HexViewer: Не удалось прочитать данные из файла";
        return false;
    }

    qDebug() << "HexViewer: Загружено" << m_data.size() << "байт из файла" << filePath;

    // Обновляем отображение
    updateDisplay();

    return true;
}

/**
 * @brief Загружает данные из памяти для отображения
 * @param data Данные для отображения
 */
void HexViewer::loadDataFromMemory(const QByteArray &data)
{
    m_data = data;
    qDebug() << "HexViewer: Загружено" << m_data.size() << "байт из памяти";
    updateDisplay();
}

/**
 * @brief Очищает отображаемые данные
 */
void HexViewer::clear()
{
    m_data.clear();
    m_textEdit->clear();
    qDebug() << "HexViewer: Данные очищены";
}

/**
 * @brief Устанавливает количество байт в строке
 * @param bytesPerLine Количество байт (16 по умолчанию)
 */
void HexViewer::setBytesPerLine(int bytesPerLine)
{
    if (bytesPerLine > 0 && bytesPerLine <= 64) {
        m_bytesPerLine = bytesPerLine;
        updateDisplay();
        qDebug() << "HexViewer: Установлено" << bytesPerLine << "байт в строке";
    }
}

/**
 * @brief Возвращает текущее количество байт в строке
 * @return Количество байт в строке
 */
int HexViewer::bytesPerLine() const
{
    return m_bytesPerLine;
}

/**
 * @brief Включает/отключает отображение адресов
 * @param enabled true для включения адресов
 */
void HexViewer::setAddressEnabled(bool enabled)
{
    m_showAddress = enabled;
    updateDisplay();
    qDebug() << "HexViewer: Отображение адресов" << (enabled ? "включено" : "выключено");
}

/**
 * @brief Включает/отключает отображение ASCII
 * @param enabled true для включения ASCII
 */
void HexViewer::setAsciiEnabled(bool enabled)
{
    m_showAscii = enabled;
    updateDisplay();
    qDebug() << "HexViewer: Отображение ASCII" << (enabled ? "включено" : "выключено");
}

/**
 * @brief Обновляет отображение данных
 */
void HexViewer::updateDisplay()
{
    if (m_data.isEmpty()) {
        m_textEdit->setPlainText("Нет данных для отображения");
        return;
    }

    QString hexDisplay;
    QTextStream stream(&hexDisplay);

    // Форматируем данные
    for (int i = 0; i < m_data.size(); i += m_bytesPerLine) {
        // Адрес
        if (m_showAddress) {
            stream << QString("%1: ").arg(i, 8, 16, QChar('0')).toUpper();
        }

        // Hex байты
        for (int j = 0; j < m_bytesPerLine; ++j) {
            if (i + j < m_data.size()) {
                quint8 byte = static_cast<quint8>(m_data[i + j]);
                stream << formatByte(byte) << " ";
            } else {
                stream << "   "; // Заполнитель для неполных строк
            }

            // Разделитель после 8 байт
            if (j == 7) {
                stream << " ";
            }
        }

        // ASCII представление
        if (m_showAscii) {
            stream << " |";
            for (int j = 0; j < m_bytesPerLine && i + j < m_data.size(); ++j) {
                stream << formatAsciiChar(m_data[i + j]);
            }
            stream << "|";
        }

        stream << "\n";
    }

    m_textEdit->setPlainText(hexDisplay);

    // Прокручиваем в начало
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_textEdit->setTextCursor(cursor);

    qDebug() << "HexViewer: Отображение обновлено, показано" << m_data.size() << "байт";
}

/**
 * @brief Форматирует байт в шестнадцатеричную строку
 * @param byte Байт для форматирования
 * @return Строка в формате "XX"
 */
QString HexViewer::formatByte(quint8 byte) const
{
    return QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
}

/**
 * @brief Форматирует символ для ASCII отображения
 * @param ch Символ для форматирования
 * @return Отформатированный символ или точка для непечатаемых символов
 */
QChar HexViewer::formatAsciiChar(char ch) const
{
    uchar uch = static_cast<uchar>(ch);
    if (uch >= 32 && uch <= 126) {
        return QChar(ch);
    } else {
        return '.';
    }
}
