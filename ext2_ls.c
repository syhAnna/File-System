#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"
#include "helper.h"

unsigned char *disk;

void print_entries(unsigned char *, struct ext2_inode *, char *);
void print_one_block_entries(struct ext2_dir_entry_2 *, char *);

/*
 * This program takes two command line arguments. The first is the name
 * of an ext2 formatted virtual disk. The second is an absolute path on
 * the ext2 formatted disk. The program should work like ls -1, printing
 * each directory entry on a separate line. If the flag "a" is specified
 * , program should also print the . and .. entries.
 */
int main(int argc, char **argv) {
    // Check valid user input
    if (argc != 3 && argc != 4) {
        printf("Usage: ext2_ls <virtual_disk> <absolute_path> <-a>\n");
        exit(1);
    } else if (argc == 4 && strcmp(argv[3], "-a") != 0) {
        printf("Usage: ext2_ls <virtual_disk> <absolute_path> <-a>\n");
        exit(1);
    }

    // Map disk image file into memory
    disk = get_disk_loc(argv[1]);

    // Get the inode of the given path
    struct ext2_inode *path_inode = trace_path(argv[2], disk);

    char type = '\0';
    if (path_inode != NULL) { // the given path exists
        // Check the type of inode
        if (path_inode->i_mode & EXT2_S_IFREG || path_inode->i_mode & EXT2_S_IFLNK) { // File or link
            type = 'f';
        } else if (path_inode->i_mode & EXT2_S_IFDIR) { // Directory
            type = 'd';
        }

        if (argc == 3) {
            if (type == 'f') { // Only print file or link name
                printf("%s\n", get_file_name(argv[2]));
            } else if (type == 'd') { // Print all entries in the directory
                print_entries(disk, path_inode, NULL);
            }

        } else { // "-a" case
            if (type == 'f') { // Refrain from printing the . and ..
                printf("%s\n", get_file_name(argv[2]));
            } else if (type == 'd') { // Print all entries in the directory as well as . and ..
                print_entries(disk, path_inode, argv[3]);
            }
        }

    } else { // The given path does not exist
        printf("exts_ls: No such file or directory.\n");
        return ENOENT;
    }

    return 0;
}


/*
 * Print all the entries of a given directory.
 */
void print_entries(unsigned char *disk, struct ext2_inode *directory, char *flag) {
    // Print all the entries of a given directory in the direct blocks.
    for (int i = 0; i < SINGLE_INDIRECT; i++) {
        if (directory->i_block[i]) {
            print_one_block_entries(get_dir_entry(disk, directory->i_block[i]), flag);
        }
    }

    // Print all the entries of given directory in the indirect blocks.
    if (directory->i_block[SINGLE_INDIRECT]) {
        unsigned int *indirect = get_indirect_block_loc(disk, directory);

        for (int j = 0; j < EXT2_BLOCK_SIZE / sizeof(unsigned int); j++) {
            if (indirect[j]) {
                print_one_block_entries(get_dir_entry(disk, indirect[j]), flag);
            }
        }
    }
}

/*
 * Print the entries in one block
 */
void print_one_block_entries(struct ext2_dir_entry_2 *dir, char *flag) {
    int curr_pos = 0; // used to keep track of the dir entries in each block
    while (curr_pos < EXT2_BLOCK_SIZE) {
        char *entry_name = malloc(sizeof(char) * dir->name_len + 1);

        for (int u = 0; u < dir->name_len; u++) {
            entry_name[u] = dir->name[u];
        }
        entry_name[dir->name_len] = '\0';

        if (flag == NULL) { // Refrain from printing the . and ..
            if (strcmp(entry_name, ".") != 0 && strcmp(entry_name, "..") != 0) {
                printf("%s\n", entry_name);
            }
        } else if (strcmp(flag, "-a") == 0) {
            printf("%s\n", entry_name);
        }

        free(entry_name);

        /* Moving to the next directory */
        curr_pos = curr_pos + dir->rec_len;
        dir = (void*) dir + dir->rec_len;
    }
}
