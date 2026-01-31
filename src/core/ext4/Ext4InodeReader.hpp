#pragma once

#include "Ext4BlockDevice.hpp"

/**
 * @brief Ext4InodeReader
 *
 * Читает inode по номеру из inode table.
 *
 * Android ext4 использует inode size обычно 256.
 */
class Ext4InodeReader
{
public:
    explicit Ext4InodeReader(Ext4BlockDevice* device);

    /**
     * @brief Читает inode по номеру (начиная с 1)
     */
    bool readInode(quint32 inodeNumber, Ext4Inode& outInode);

private:
    Ext4BlockDevice* m_device;
};
