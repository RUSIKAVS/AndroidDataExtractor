#include "hexviewer.h"
#include <QFont>
#include <QFontDatabase>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QTextCharFormat>

HexViewer::HexViewer(QWidget *parent)
    : QTextEdit(parent)
{
    // Устанавливаем моноширинный шрифт для правильного отображения HEX
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);
    setFont(font);

    // Делаем виджет только для чтения
    setReadOnly(true);

    // Настраиваем отступы
    document()->setDocumentMargin(10);

    // Очищаем начальное содержимое
    clear();
}

void HexViewer::setData(const QByteArray &data)
{
    clear();

    if (data.isEmpty()) {
        append("No data to display");
        return;
    }

    // Создаем формат для обычного текста
    QTextCharFormat normalFormat;
    normalFormat.setForeground(Qt::black);

    // Создаем формат для HEX значений
    QTextCharFormat hexFormat;
    hexFormat.setForeground(Qt::darkBlue);
    hexFormat.setFontWeight(QFont::Bold);

    // Создаем формат для ASCII значений
    QTextCharFormat asciiFormat;
    asciiFormat.setForeground(Qt::darkGreen);

    // Создаем формат для адресов
    QTextCharFormat addressFormat;
    addressFormat.setForeground(Qt::darkGray);

    QTextCursor cursor(document());
    QTextBlockFormat blockFormat;

    // Отображаем данные построчно по 16 байт в строке
    for (int i = 0; i < data.size(); i += 16) {
        QString line;

        // Адрес
        cursor.insertText(QString("%1: ").arg(i, 8, 16, QChar('0')), addressFormat);

        // HEX значения
        for (int j = 0; j < 16; j++) {
            if (i + j < data.size()) {
                uint8_t byte = static_cast<uint8_t>(data[i + j]);
                cursor.insertText(byteToHex(byte) + " ", hexFormat);
            } else {
                cursor.insertText("   ", normalFormat);  // Заполнитель
            }

            // Разделитель после 8 байт
            if (j == 7) {
                cursor.insertText(" ", normalFormat);
            }
        }

        // ASCII значения
        cursor.insertText(" | ", normalFormat);
        for (int j = 0; j < 16; j++) {
            if (i + j < data.size()) {
                uint8_t byte = static_cast<uint8_t>(data[i + j]);
                cursor.insertText(QString(byteToAscii(byte)), asciiFormat);
            } else {
                cursor.insertText(" ", normalFormat);
            }
        }

        // Переход на новую строку
        cursor.insertBlock(blockFormat);
    }

    // Прокручиваем к началу
    moveCursor(QTextCursor::Start);
}

void HexViewer::clearDisplay()
{
    clear();
    append("Hex viewer ready. Select a partition to view data.");
}

QString HexViewer::byteToHex(uint8_t byte)
{
    return QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
}

QChar HexViewer::byteToAscii(uint8_t byte)
{
    // Отображаем только печатные ASCII символы, остальные как точки
    if (byte >= 32 && byte <= 126) {
        return QChar(byte);
    } else {
        return '.';
    }
}
