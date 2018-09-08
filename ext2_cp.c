#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <memory.h>
#include "ext2.h"
#include "helper.h"

unsigned char *disk;

/*
 * This program copies the file on local file system on to the the specified
 * location on the disk. The program works similar to cp.
 */
int main (int argc, char **argv) {
    // Check valid command line arguments
    if (argc != 4) {
        printf("Usage: ext2_cp <virtual_disk> <source_file> <absolute_path>\n");
        exit(1);
    }

    // Check valid disk
    unsigned char *disk = get_disk_loc(argv[1]);
    struct ext2_super_block *sb = get_superblock_loc(disk);
    struct ext2_inode *i_table = get_inode_table_loc(disk);

    // Check valid path on native file system
    // Open source file
    int fd;
    if ((fd = open(argv[2], O_RDONLY)) < 0) {
        fprintf(stderr, "Cannot open source file path.\n");
        exit(1);
    }
    // Get source file size.
    struct stat st;
    fstat(fd, &st);
    int file_size = (int) st.st_size;
    int blocks_needed = file_size / EXT2_BLOCK_SIZE + (file_size % EXT2_BLOCK_SIZE != 0);

    char *name_var = NULL;
    struct ext2_inode *dir_inode = NULL;

    // Check valid target absolute_path
    struct ext2_inode *target_inode = trace_path(argv[3], disk);
    char *parent_path = NULL;
    struct ext2_inode *parent_inode = NULL;
    if (target_inode != NULL) { // Target path exists
        name_var = get_file_name(argv[2]);
        // If source path is a directory
        if (target_inode->i_mode & EXT2_S_IFDIR) {
            // test whether the directory exist the same file name
            struct ext2_inode *check = get_entry_with_name(disk, name_var, target_inode);
            if (check == NULL) { // no file with same name exist
                dir_inode = target_inode;
            } else {
                printf("ext2_cp: %s :File exists.\n", name_var);
                return EEXIST;
            }

            // If such file exist -> EEXIST, no overwrite
        } else { // Source path is a file or a link
            printf("ext2_cp: %s :File exists.\n", name_var);
            return EEXIST;
        }
    } else {
        parent_path = get_dir_parent_path(argv[3]);
        parent_inode = trace_path(parent_path, disk);

        if (parent_inode == NULL) { // Parent path does not exist
            printf("ext2_cp: %s :Invalid path.\n", argv[3]);
            return ENOENT;
            // Path like /file_name/ should fail
        } else if ((argv[3])[strlen(argv[3]) - 1] == '/') {
            printf("ext2_cp: %s :Invalid path.\n", argv[3]);
            return ENOENT;
        } else { // File path with file DNE (yet) in a valid directory path.
            name_var = get_file_name(argv[3]);
            dir_inode = parent_inode;
        }
    }

    if (strlen(name_var) > EXT2_NAME_LEN) { // target name too long
        printf("ext2_cp: Target file with name too long: %s\n", name_var);
        return ENOENT;
    }

    // Check if there is enough inode (require 1)
    if (sb->s_free_inodes_count <= 0) {
        printf("ext2_cp: File system does not have enough free inodes.\n");
        return ENOSPC;
    }

    // Check if there is enough blocks for Data
    if ((blocks_needed <= 12 && sb->s_free_blocks_count < blocks_needed) ||
        (blocks_needed > 12 && sb->s_free_blocks_count < blocks_needed + 1)) {
        printf("ext2_cp: File system does not have enough free blocks.\n");
        return ENOSPC;
    } // If indirected block needed, one more indirect block is required to store pointers

    // Require a free inode
    int i_num = init_inode(disk, file_size, 'f');
    struct ext2_inode *tar_inode = &(i_table[i_num - 1]);

    // Write into target file (data blocks
    char buf[file_size];
    if (read(fd, buf, file_size) < 0) {
        perror("Read");
        exit(1);
    }

    write_into_block(disk, tar_inode, buf, file_size);

    // Create a new entry in directory
    if (add_new_entry(disk, dir_inode, (unsigned int) i_num, name_var, 'f') == -1) {
        printf("ext2_cp: Fail to add new directory entry in directory: %s\n", argv[3]);
        exit(0);
    }
    return 0;
}

