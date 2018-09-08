#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include "ext2.h"
#include "helper.h"

unsigned char *disk;


/*
 * This program works like mkdir, creating the final directory on the
 * specified path on the disk.
 */
int main (int argc, char **argv) {

    // Check valid user inputs
    if (argc != 3) {
        printf("Usage: ext2_mkdir <virtual_disk> <absolute_path>\n");
        exit(1);
    }

    // Map disk image file into memory， read superblock and group descriptor
    unsigned char *disk = get_disk_loc(argv[1]);
    struct ext2_group_desc *gd = get_group_descriptor_loc(disk);
    struct ext2_super_block *sb = get_superblock_loc(disk);
    struct ext2_inode *i_table = get_inode_table_loc(disk);

    // Check valid target absolute_path
    struct ext2_inode *target_inode = trace_path(argv[2], disk);
    if (target_inode != NULL) { // Target path exists
        printf("ext2_mkdir: %s :Directory exists.\n", argv[2]);
        return EEXIST;
    }
    char *parent_path = get_dir_parent_path(argv[2]);
    struct ext2_inode *parent_inode = trace_path(parent_path, disk);
    if (parent_inode == NULL) { //parent directory not exist
        printf("ext2_mkdir: %s :Invalid path.\n", argv[2]);
        return ENOENT;
    }

    if (strlen(get_file_name(argv[2])) > EXT2_NAME_LEN) { // target name too long
        printf("ext2_mkdir: Target directory with name too long: %s\n", get_file_name(argv[2]));
        return ENOENT;
    }

    // Check if there is enough inode (require 1) and free blocks
    if (sb->s_free_inodes_count <= 0 || sb->s_free_blocks_count <= 0) {
        printf("ext2_mkdir: File system does not have enough free inodes or blocks.\n");
        return ENOSPC;
    }

    int i_num = init_inode(disk, EXT2_BLOCK_SIZE, 'd');
    struct ext2_inode *tar_inode = &(i_table[i_num - 1]);

    // Create a new entry in directory
    if (add_new_entry(disk, parent_inode, (unsigned int)i_num, get_file_name(argv[2]), 'd') == -1) {
        printf("ext2_mkdir: Fail to add new directory entry in directory: %s\n", argv[3]);
        exit(0);
    }

    // Add . and .. into block
    if (add_new_entry(disk, tar_inode, (unsigned int)i_num, ".", 'd') == -1) {
        printf("ext2_mkdir: Fail to add new directory entry in directory: %s\n", argv[3]);
        exit(0);
    }

    if (add_new_entry(disk, tar_inode, (unsigned int)get_inode_num(disk, parent_inode), "..", 'd') == -1) {
        printf("ext2_mkdir: Fail to add new directory entry in directory: %s\n", argv[3]);
        exit(0);
    }

    //update directories count of blocks group descriptor
    gd->bg_used_dirs_count ++;
    return 0;
}

