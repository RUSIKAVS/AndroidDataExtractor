#include "Ext4InodeReader.hpp"

#include <QDebug>
#include <cstring>

Ext4InodeReader::Ext4InodeReader(Ext4BlockDevice* device)
    : m_device(device)
{
}

bool Ext4InodeReader::readInode(quint32 inodeNumber, Ext4Inode& outInode)
{
    if (!m_device || !m_device->isValid())
        return false;

    const auto& sb = m_device->superblock();

    quint32 inodeSize       = sb.s_inode_size;
    quint32 inodesPerGroup  = sb.s_inodes_per_group;

    if (inodeNumber == 0)
        return false;

           // ---- вычисляем группу inode ----
    quint32 groupIndex   = (inodeNumber - 1) / inodesPerGroup;
    quint32 indexInside  = (inodeNumber - 1) % inodesPerGroup;

           // ---- читаем таблицу group descriptors ----
    quint64 gdBlock = sb.s_first_data_block + 1;

    QByteArray gdData = m_device->readBlock(gdBlock);

    if (gdData.size() < (int)sizeof(Ext4GroupDesc))
        return false;

    const Ext4GroupDesc* desc =
        reinterpret_cast<const Ext4GroupDesc*>(gdData.constData());

    quint32 inodeTableBlock = desc[groupIndex].bg_inode_table_lo;

           // ---- вычисляем offset inode ----
    quint64 inodeOffset =
        (quint64)inodeTableBlock * m_device->blockSize()
        + (quint64)indexInside * inodeSize;

           // ---- читаем inode ----
    QByteArray raw = m_device->readBytes(inodeOffset, inodeSize);

    if (raw.size() != (int)inodeSize)
        return false;

           // копируем в структуру inode
    memset(&outInode, 0, sizeof(Ext4Inode));
    memcpy(&outInode, raw.constData(),
           qMin((int)sizeof(Ext4Inode), raw.size()));

    return true;
}
