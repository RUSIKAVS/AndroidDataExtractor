#include "Ext4BlockDevice.hpp"

#include <QDebug>

/**
 * ext4 superblock расположен всегда на offset 1024.
 */
static constexpr quint64 SUPERBLOCK_OFFSET = 1024;

Ext4BlockDevice::Ext4BlockDevice(const QString& imagePath)
    : m_file(imagePath)
{
}

/**
 * @brief Открываем userdata_decrypted.img и читаем superblock.
 *
 * Здесь происходит автоопределение block size:
 *
 * block_size = 1024 << s_log_block_size
 *
 * Обычно:
 *  log=0 -> 1024
 *  log=1 -> 2048
 *  log=2 -> 4096
 */
bool Ext4BlockDevice::open()
{
    if (!m_file.open(QIODevice::ReadOnly))
    {
        qWarning() << "[Ext4] Cannot open image:" << m_file.fileName();
        return false;
    }

    // читаем superblock
    m_file.seek(SUPERBLOCK_OFFSET);

    if (m_file.read(reinterpret_cast<char*>(&m_sb), sizeof(m_sb)) != sizeof(m_sb))
    {
        qWarning() << "[Ext4] Failed to read superblock";
        return false;
    }

    // проверяем magic
    if (m_sb.s_magic != 0xEF53)
    {
        qWarning() << "[Ext4] Invalid magic, not ext4!";
        return false;
    }

    // вычисляем block size
    m_blockSize = 1024u << m_sb.s_log_block_size;

    qDebug() << "======================================";
    qDebug() << "[Ext4] ext4 filesystem detected!";
    qDebug() << "[Ext4] Block size:" << m_blockSize;
    qDebug() << "[Ext4] Inodes:" << m_sb.s_inodes_count;
    qDebug() << "[Ext4] Inode size:" << m_sb.s_inode_size;
    qDebug() << "======================================";

    m_valid = true;
    return true;
}

bool Ext4BlockDevice::isValid() const
{
    return m_valid;
}

quint32 Ext4BlockDevice::blockSize() const
{
    return m_blockSize;
}

const Ext4Superblock& Ext4BlockDevice::superblock() const
{
    return m_sb;
}

/**
 * @brief Читает один block по номеру.
 *
 * Offset вычисляется как:
 *
 * offset = blockNumber * blockSize
 */
QByteArray Ext4BlockDevice::readBlock(quint64 blockNumber)
{
    if (!m_valid)
        return {};

    quint64 offset = blockNumber * m_blockSize;

    m_file.seek(offset);

    return m_file.read(m_blockSize);
}


QByteArray Ext4BlockDevice::readBytes(quint64 offset, quint64 size)
{
    if (!m_valid)
        return {};

    m_file.seek(offset);
    return m_file.read(size);
}
