#pragma once
#include <QtGlobal>

/**
 * @file Ext4Structs.hpp
 *
 * Полный набор структур ext4 для Android 7–10.
 *
 * Эти структуры нужны для:
 *  - чтения superblock
 *  - чтения inode
 *  - чтения extents
 *  - directory traversal
 *
 * Всё читается напрямую из userdata_decrypted.img
 * без mount и без внешних библиотек.
 */

#pragma pack(push, 1)

/**
 * @brief ext4 Superblock (частично)
 *
 * Offset всегда 1024 байта.
 */
struct Ext4Superblock
{
    quint32 s_inodes_count;
    quint32 s_blocks_count_lo;
    quint32 s_r_blocks_count_lo;
    quint32 s_free_blocks_count_lo;
    quint32 s_free_inodes_count;
    quint32 s_first_data_block;
    quint32 s_log_block_size;
    quint32 s_log_cluster_size;
    quint32 s_blocks_per_group;
    quint32 s_clusters_per_group;
    quint32 s_inodes_per_group;

    quint32 s_mtime;
    quint32 s_wtime;

    quint16 s_mnt_count;
    quint16 s_max_mnt_count;

    quint16 s_magic; // должен быть 0xEF53

    quint16 s_state;
    quint16 s_errors;
    quint16 s_minor_rev_level;

    quint32 s_lastcheck;
    quint32 s_checkinterval;
    quint32 s_creator_os;
    quint32 s_rev_level;

    quint16 s_def_resuid;
    quint16 s_def_resgid;

    quint32 s_first_ino;
    quint16 s_inode_size; // обычно 256
};

/**
 * @brief Block Group Descriptor (ext4)
 */
struct Ext4GroupDesc
{
    quint32 bg_block_bitmap_lo;
    quint32 bg_inode_bitmap_lo;
    quint32 bg_inode_table_lo;

    quint16 bg_free_blocks_count_lo;
    quint16 bg_free_inodes_count_lo;
    quint16 bg_used_dirs_count_lo;
    quint16 bg_flags;

    quint32 bg_reserved[3];
};

/**
 * @brief Inode structure (частично)
 */
struct Ext4Inode
{
    quint16 i_mode;
    quint16 i_uid;
    quint32 i_size_lo;
    quint32 i_atime;
    quint32 i_ctime;
    quint32 i_mtime;
    quint32 i_dtime;
    quint16 i_gid;
    quint16 i_links_count;
    quint32 i_blocks_lo;
    quint32 i_flags;

    quint32 i_osd1;

    quint8  i_block[60]; // extents или direct blocks

    quint32 i_generation;
    quint32 i_file_acl_lo;
    quint32 i_size_high;
};

/**
 * @brief Ext4 extent header
 */
struct Ext4ExtentHeader
{
    quint16 eh_magic;   // 0xF30A
    quint16 eh_entries;
    quint16 eh_max;
    quint16 eh_depth;
    quint32 eh_generation;
};

/**
 * @brief Ext4 extent entry
 */
struct Ext4Extent
{
    quint32 ee_block;
    quint16 ee_len;
    quint16 ee_start_hi;
    quint32 ee_start_lo;
};

/**
 * @brief Directory entry ext4
 */
struct Ext4DirEntry
{
    quint32 inode;
    quint16 rec_len;
    quint8  name_len;
    quint8  file_type;
    char    name[255];
};

#pragma pack(pop)
