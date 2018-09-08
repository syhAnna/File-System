#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"
#include "helper.h"

unsigned char *disk;

/*
 * This program created a linked file from first specific file to second absolute
 * path.
 */
int main (int argc, char **argv) {
    // Check valid command line arguments
    if (argc != 4 && argc != 5) {
        printf("Usage: ext2_ln <virtual_disk> <source_file> <target_file> <-s>\n");
        exit(1);
    } else if (argc == 5 && strcmp(argv[4], "-s") != 0) {
        printf("Usage: ext2_ln <virtual_disk> <source_file> <target_file> <-s>\n");
        exit(1);
    }

    // Check valid disk
    unsigned char *disk = get_disk_loc(argv[1]);
    struct ext2_super_block *sb = get_superblock_loc(disk);
    struct ext2_inode *i_table = get_inode_table_loc(disk);

    struct ext2_inode *source_inode = trace_path(argv[2], disk);
    struct ext2_inode *target_inode = trace_path(argv[3], disk);

    // If source file does not exist -> ENOENT
    if (source_inode == NULL) {
        printf("ext2_ln: %s :Invalid path.\n", argv[2]);
        return ENOENT;
    }

    // If source file path is a directory -> EISDIR
    if (source_inode->i_mode & EXT2_S_IFDIR) {
        printf("ext2_ln: %s :Path provided is a directory.\n", argv[2]);
        return EISDIR;
    }

    // If target file exist -> EEXIST
    if (target_inode != NULL) {
        if (target_inode->i_mode & EXT2_S_IFDIR) {
            printf("ext2_ln: %s :Path provided is a directory.\n", argv[3]);
            return EISDIR;
        }
        printf("ext2_ln: %s :File already exist.\n", argv[3]);
        return EEXIST;
    }

    char *dir_path = get_dir_path(argv[3]); // for target file
    struct ext2_inode *dir_inode = trace_path(dir_path, disk);

    // Directory of target file DNE
    if (dir_inode == NULL) {
        printf("ext2_ln: %s :Invalid path.\n", dir_path);
        return ENOENT;
        // Path like /file_name/ should fail
    } else if ((argv[3])[strlen(argv[3]) - 1] == '/') {
        printf("ext2_cp: %s :Invalid path.\n", argv[3]);
        return ENOENT;
    }

    int target_inode_num;
    char *source_path = argv[2];
    char *target_name = get_file_name(argv[3]);
    if (strlen(target_name) > EXT2_NAME_LEN) { // target name too long
        printf("ext2_ln: Target file with name too long: %s\n", target_name);
        return ENOENT;
    }
    int path_len = (int) strlen(source_path);
    int blocks_needed = path_len / EXT2_BLOCK_SIZE + (path_len % EXT2_BLOCK_SIZE != 0);

    if (argc == 5) { // Create soft link
        // Check if we have enough space for path if symbolic link is created
        if ((blocks_needed <= 12 && sb->s_free_blocks_count < blocks_needed) ||
            (blocks_needed > 12 && sb->s_free_blocks_count < blocks_needed + 1)) {
            printf("ext2_ln: File system does not have enough free blocks.\n");
            return ENOSPC;
        }

        target_inode_num = init_inode(disk, path_len, 'l');
        if (target_inode_num == -1) {
            printf("ext2_ln: File system does not have enough free inodes.\n");
            return ENOSPC;
        }
        struct ext2_inode *tar_inode = &(i_table[target_inode_num - 1]);

        write_into_block(disk, tar_inode, source_path, path_len);

        if (add_new_entry(disk, dir_inode, (unsigned int) target_inode_num, target_name, 'l') == -1) {
            printf("ext2_ln: Fail to add new directory entry in directory: %s\n", dir_path);
            exit(0);
        }
    } else { // Default: create a hardlink
        if (source_inode->i_mode & EXT2_S_IFLNK) { // If create a hardlink to a softlink
            char file_path[source_inode->i_size];
            for (int k=0; k < 12; k++) {
                if (source_inode->i_block[k]) {
                    int num_to_read = EXT2_BLOCK_SIZE;
                    if (source_inode->i_size < (k + 1) * EXT2_BLOCK_SIZE) { //last read
                        num_to_read = source_inode->i_size - k * EXT2_BLOCK_SIZE;
                    }
                    char *block = (char *)disk + source_inode->i_block[k] * EXT2_BLOCK_SIZE;
                    strncpy(&file_path[k * EXT2_BLOCK_SIZE], (char *)block, num_to_read);
                }
            }
            struct ext2_inode *file_inode = trace_path(file_path, disk);
            if (file_inode == NULL) {
                printf("ext2_ln: %s :Invalid path.\n", argv[2]);
                return ENOENT;
            }
            source_inode = file_inode;
        }
        target_inode_num = get_inode_num(disk, source_inode);
        source_inode->i_links_count++;

        if (add_new_entry(disk, dir_inode, (unsigned int) target_inode_num, target_name, 'f') == -1) {
            printf("ext2_ln: Fail to add new directory entry in directory: %s\n", dir_path);
            exit(0);
        }
    }

    return 0;
}

