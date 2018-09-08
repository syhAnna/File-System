#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "ext2.h"
#include "helper.h"

unsigned char *disk;

void remove_dir(unsigned char *, char *);
void clear_directory_content(unsigned char *, int, char *);

/*
 * In addition to the functions in ext2_rm, this program implements
 * an additional "r" flag which allows removing directories as well.
 * If "r" is used with a regular file or link, then it should be
 * ignored (the ext2_rm operation should be carried out as if the
 * flag had not been entered).
 */
int main(int argc, char **argv) {
    // Check valid user input
    if (argc != 4) {
        printf("Usage: ext2_rm_bonus <virtual_disk> <absolute_path> <-r>\n");
        exit(1);
    }

    // Map disk image file into memory
    disk = get_disk_loc(argv[1]);

    // Get the inode of the given path
    struct ext2_inode *path_inode = trace_path(argv[2], disk);

    // The path do not exist
    if (path_inode == NULL) {
        printf("ext2_rm_bonus: The path %s do not exist.\n", argv[2]);
        return ENOENT;
    }

    // Cannot delete root
    if (strcmp(argv[2], "/") == 0) {
        printf("ext2_rm_bonus: User cannot delete the root dir.\n");
        exit(1);
    }

    // Is a file or link
    if ((path_inode->i_mode & EXT2_S_IFREG) || (path_inode->i_mode & EXT2_S_IFLNK)) {
        remove_file_or_link(disk, argv[2]);
    } else if (path_inode->i_mode & EXT2_S_IFDIR) { // Is a directory
        remove_dir(disk, argv[2]);
    }

    return 0;
}


/*
 * Remove the directory of given path.
 */
void remove_dir(unsigned char *disk, char *path) {
    struct ext2_super_block *sb = get_superblock_loc(disk);
    struct ext2_group_desc *gd = get_group_descriptor_loc(disk);
    unsigned char *block_bitmap = get_block_bitmap_loc(disk);
    struct ext2_inode *path_inode = trace_path(path, disk);

    int block_num = path_inode->i_block[0];

    // Remove all the contents inside the dir, avoid . and ..
    for (int i = 0; i < SINGLE_INDIRECT; i++) {
        if (path_inode->i_block[i]) { // Has data in the block
            clear_directory_content(disk, path_inode->i_block[i], path);
            block_num = path_inode->i_block[i];
        }
    }

    if (path_inode->i_block[SINGLE_INDIRECT]) {
        unsigned int *indirect = get_indirect_block_loc(disk, path_inode);

        for (int j = 0; j < EXT2_BLOCK_SIZE / sizeof(unsigned int); j++) {
            if (indirect[j]) {
                clear_directory_content(disk, indirect[j], path);
                block_num = indirect[j];
            }
        }
    }

    // Update fields and zero the block bitmap and inode bitmap
    path_inode->i_blocks = 0;
    sb->s_free_blocks_count++;
    gd->bg_free_blocks_count++;
    zero_bitmap(block_bitmap, block_num);
    clear_inode_bitmap(disk, path_inode);

    // Get the parent directory
    char *parent_path = get_dir_parent_path(path);
    struct ext2_inode *parent_dir = trace_path(parent_path, disk);
    parent_dir->i_links_count--;
    // Remove current directory's name but keep the inode
    remove_name(disk, path);
    gd->bg_used_dirs_count--;

    // Update the field of removed dir inode
    for (int i = 0; i < 15; i++) {
        path_inode->i_block[i] = 0;
    }
    path_inode->i_dtime = (unsigned int) time(NULL);
    path_inode->i_size = 0;
    path_inode->i_blocks = 0;
}

/*
 * Clear all the entries in one block.
 */
void clear_directory_content(unsigned char *disk, int block_num, char *path) {
    struct ext2_dir_entry_2 *dir = get_dir_entry(disk, block_num);
    struct ext2_dir_entry_2 *curr_dir = dir;
    int curr_pos = 0;

    while (curr_pos < EXT2_BLOCK_SIZE) {
        curr_dir->name[curr_dir->name_len] = '\0';

        if (strcmp(curr_dir->name, ".") == 0) {
            curr_dir->name_len = 0;
            curr_dir->name[0] = '\0';

        } else if (strcmp(curr_dir->name, "..") == 0) {
            curr_dir->name_len = 0;
            curr_dir->name[0] = '\0';
            dir->rec_len += curr_dir->rec_len;
        } else {
            if ((dir->file_type == EXT2_FT_REG_FILE)
                || (dir->file_type == EXT2_FT_SYMLINK)) {
                remove_file_or_link(disk, combine_name(path, dir));
            } else if (dir->file_type == EXT2_FT_DIR) {
                remove_dir(disk, combine_name(path, dir));
            }
        }

        curr_pos += curr_dir->rec_len;
        curr_dir = (void*) curr_dir + curr_dir->rec_len;
    }
}

