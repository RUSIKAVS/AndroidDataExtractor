#include "Ext4InodeReader.hpp"

#include <QDebug>

Ext4InodeReader::Ext4InodeReader(Ext4BlockDevice* device)
    : m_device(device)
{
}

/**
 * @brief Чтение inode:
 *
 * inode table находится через Group Descriptor.
 *
 * Упрощённо:
 *  - inode 2 = root directory
 *
 * Сейчас реализуем минимально:
 *  - читаем inode table группы 0
 */
bool Ext4InodeReader::readInode(quint32 inodeNumber, Ext4Inode& outInode)
{
    if (!m_device || !m_device->isValid())
        return false;

    const auto& sb = m_device->superblock();

    quint32 inodeSize = sb.s_inode_size;
    quint32 inodesPerGroup = sb.s_inodes_per_group;

    // вычисляем группу inode
    quint32 groupIndex = (inodeNumber - 1) / inodesPerGroup;
    quint32 indexInside = (inodeNumber - 1) % inodesPerGroup;

    // читаем Group Descriptor Table (после superblock)
    quint64 gdBlock = (sb.s_first_data_block + 1);

    QByteArray gdData = m_device->readBlock(gdBlock);

    auto* desc = reinterpret_cast<const Ext4GroupDesc*>(gdData.constData());

    quint32 inodeTableBlock = desc[groupIndex].bg_inode_table_lo;

    // offset inode внутри inode table
    quint64 inodeOffset =
        (quint64)inodeTableBlock * m_device->blockSize()
        + (quint64)indexInside * inodeSize;

    QFile& file = *(reinterpret_cast<QFile*>(nullptr)); // placeholder

    // Прямое чтение через QFile невозможно тут,
    // поэтому inode reader будет расширен позже.

    Q_UNUSED(inodeOffset);

    qWarning() << "[Ext4InodeReader] Пока реализовано частично!";
    return false;
}
