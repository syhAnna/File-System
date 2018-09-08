#include <stdio.h>
#include <stdlib.h>

#ifndef CSC369A3_HELPER_H
#define CSC369A3_HELPER_H

#include "ext2.h"

#define SINGLE_INDIRECT 12
#define NUM_BLOCKS 2

/*
 * Return the disk location.
 */
unsigned char *get_disk_loc(char *disk_name);

/*
 * Return the super block location.
 */
struct ext2_super_block *get_superblock_loc(unsigned char *disk);

/*
 * Return the group descriptor location.
 */
struct ext2_group_desc *get_group_descriptor_loc(unsigned char *disk);

/*
 * Return the block bitmap (16*8 bits) location.
 */
unsigned char *get_block_bitmap_loc(unsigned char *disk);

/*
 * Return the inode bitmap (4*8 bits) location.
 */
unsigned char *get_inode_bitmap_loc(unsigned char *disk);

/*
 * Return the inode table location.
 */
struct ext2_inode *get_inode_table_loc(unsigned char *disk);

/*
 * Return the indirect block location.
 */
unsigned int *get_indirect_block_loc(unsigned char *disk, struct ext2_inode  *inode);

/*
 * Return the directory location.
 */
struct ext2_dir_entry_2 *get_dir_entry(unsigned char *disk, int block_num);

/*
 * Return the inode of the root directory.
 */
struct ext2_inode *get_root_inode(struct ext2_inode  *inode_table);

/*
 * Return the file name of the given valid path.
 */
char *get_file_name(char *path);

/*
 * Return the path except the final path name. With assumption that the path is
 * a valid path and it is a path of a file.
 */
char *get_dir_path(char *path);

/*
 * Trace the given path. Return the inode of the given path.
 */
struct ext2_inode *trace_path(char *path, unsigned char *disk);

/*
 * Return the inode of the directory/file/link with a particular name if it is in the given
 * parent directory, otherwise, return NULL.
 */
struct ext2_inode *get_entry_with_name(unsigned char *disk, char *name, struct ext2_inode *parent);

/*
 * Return the inode of a directory/file/link with a particular name if it is in a block,
 * otherwise return NULL.
 */
struct ext2_inode *get_entry_in_block(unsigned char *disk, char *name, int block_num);

/*
 * Zero block [inode / block] bitmap of given block number.
 */
void zero_bitmap(unsigned char *block, int block_num);

/*
 * Clear all the entries in the blocks of given inode and
 * zero the block bitmap of given inode.
 */
void clear_block_bitmap(unsigned char *disk, char *path);

/*
 * Zero the given inode from the inode bitmap
 */
void clear_inode_bitmap(unsigned char *disk, struct ext2_inode *remove);

/*
 * Get inode number of given inode if exist, otherwise return 0.
 */
int get_inode_num(unsigned char *disk, struct ext2_inode *target);

/*
 * Remove the file name in the given inode.
 */
void remove_name(unsigned char *disk, char *path);

/*
 * Return 1 if successfully remove the directory entry with given name,
 * otherwise, return 0.
 */
int remove_name_in_block(unsigned char *disk, char *file_name, int block_num);

/*
 * Remove a file or link in the given path.
 */
void remove_file_or_link(unsigned char *disk, char *path);

/*
 * Get parent dir of a directory, exclude root dir.
 */
char *get_dir_parent_path(char *path);

/*
 * Combine the parent path and the file/link/directory name.
 * Example: /a/bb (or /a/bb/) and ccc outputs /a/bb/ccc
 */
char *combine_name(char *parent_path, struct ext2_dir_entry_2 *dir_entry);

/*
 * Add new entry into the directory.
 */
int add_new_entry(unsigned char *disk, struct ext2_inode *dir_inode, unsigned int new_inode, char *f_name, char type);

/*
 * Return the first inode number that is free.
 */
int get_free_inode(unsigned char *disk, unsigned char *inode_bitmap);

/*
 * Return the first block number that is free.
 */
int get_free_block(unsigned char *disk, unsigned char *block_bitmap);

/*
 * Find a new unused inode and initialize. Return inode number. Return -1 if
 * could not find such inode.
 */
int init_inode(unsigned char *disk, int size, char type);

/*
 * Write buf into blocks of the target inode.
 */
int write_into_block(unsigned char *disk, struct ext2_inode *tar_inode, char *buf, int buf_size);

#endif

