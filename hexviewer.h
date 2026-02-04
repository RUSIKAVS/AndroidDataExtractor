#ifndef HEXVIEWER_H
#define HEXVIEWER_H

#include <QWidget>
#include <QTextEdit>
#include <QByteArray>

/**
 * @class HexViewer
 * @brief Виджет для отображения данных в HEX формате
 *
 * Этот виджет отображает бинарные данные в формате HEX с ASCII представлением,
 * аналогично классическим HEX редакторам.
 */
class HexViewer : public QTextEdit
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор класса
     * @param parent Родительский виджет
     */
    explicit HexViewer(QWidget *parent = nullptr);

    /**
     * @brief Устанавливает данные для отображения
     * @param data Данные для отображения
     */
    void setData(const QByteArray &data);

    /**
     * @brief Очищает отображение
     */
    void clearDisplay();

private:
    /**
     * @brief Преобразует байт в HEX строку
     * @param byte Байт для преобразования
     * @return HEX строка (два символа)
     */
    QString byteToHex(uint8_t byte);

    /**
     * @brief Преобразует байт в ASCII символ
     * @param byte Байт для преобразования
     * @return ASCII символ или точка для непечатных символов
     */
    QChar byteToAscii(uint8_t byte);
};

#endif // HEXVIEWER_H
