#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "ext2.h"
#include "helper.h"

unsigned char *disk;

/*
 * This program takes two command line arguments. The first is the
 * name of an ext2 formatted virtual disk, and the second is an
 * absolute path to a file or link (not a directory) on that disk.
 * The program should work like rm, removing the specified file from
 * the disk.
 */
int main(int argc, char **argv) {
    // Check valid user input
    if (argc != 3) {
        printf("Usage: ext2_rm <virtual_disk> <absolute_path>\n");
        exit(1);
    }

    // Map disk image file into memory
    disk = get_disk_loc(argv[1]);

    // Get the inode of the given path
    struct ext2_inode *path_inode = trace_path(argv[2], disk);

    // The file/lin do not exist
    if (path_inode == NULL) {
        printf("ext2_rm: The path %s do not exist.\n", argv[2]);
        return ENOENT;
    }

    // Is a directory
    if (path_inode->i_mode & EXT2_S_IFDIR) {
        printf("ext2_rm: The path %s is a directory.\n", argv[2]);
        return EISDIR;
    }

    // Remove the file or link in their parent directory
    remove_file_or_link(disk, argv[2]);

    return 0;
}

