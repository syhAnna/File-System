#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include "ext2.h"
#include "helper.h"

/*
 * Return the disk location.
 */
unsigned char *get_disk_loc(char *disk_name) {
    int fd = open(disk_name, O_RDWR);

    // Map disk image file into memory
    unsigned char *disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    return disk;
}

/*
 * Return the super block location.
 */
struct ext2_super_block *get_superblock_loc(unsigned char *disk) {
    return (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
}

/*
 * Return the group descriptor location.
 */
struct ext2_group_desc *get_group_descriptor_loc(unsigned char *disk) {
    return (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
}

/*
 * Return the block bitmap (16*8 bits) location.
 */
unsigned char *get_block_bitmap_loc(unsigned char *disk) {
    struct ext2_group_desc *gd = get_group_descriptor_loc(disk);
    return disk + EXT2_BLOCK_SIZE * (gd->bg_block_bitmap);
}

/*
 * Return the inode bitmap (4*8 bits) location.
 */
unsigned char *get_inode_bitmap_loc(unsigned char *disk) {
    struct ext2_group_desc *gd = get_group_descriptor_loc(disk);
    return disk + EXT2_BLOCK_SIZE * (gd->bg_inode_bitmap);
}

/*
 * Return the inode table location.
 */
struct ext2_inode *get_inode_table_loc(unsigned char *disk) {
    struct ext2_group_desc *gd = get_group_descriptor_loc(disk);
    return (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
}

/*
 * Return the indirect block location.
 */
unsigned int *get_indirect_block_loc(unsigned char *disk, struct ext2_inode  *inode) {
    return (unsigned int *) (disk + EXT2_BLOCK_SIZE * inode->i_block[SINGLE_INDIRECT]);
}

/*
 * Return the directory location.
 */
struct ext2_dir_entry_2 *get_dir_entry(unsigned char *disk, int block_num) {
    return (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
}

/*
 * Return the inode of the root directory.
 */
struct ext2_inode *get_root_inode(struct ext2_inode  *inode_table) {
    return &(inode_table[EXT2_ROOT_INO - 1]);
}

/*
 * Return the file name of the given valid path.
 */
char *get_file_name(char *path) {
    char *filter = "/";
    char *file_name = malloc(sizeof(char) * (strlen(path) + 1));;

    char *full_path = malloc(sizeof(char) * (strlen(path) + 1));
    strncpy(full_path, path, strlen(path) + 1);

    char *token = strtok(full_path, filter);
    while (token != NULL) {
        strncpy(file_name, token, strlen(token) + 1);
        token = strtok(NULL, filter);
    }

    return file_name;
}

/*
 * Return the path except the final path name. With assumption that the path is
 * a valid path and it is a path of a file.
 *
 * If path is /a/bb/ccc/ this function will return /a/bb/ccc/ it will still
 * an invalid path.
 * If path is /a/bb/ccc function will give /a/bb/
 */
char *get_dir_path(char *path) {
    char *file_name = NULL;
    char *parent = NULL;
    file_name = strrchr(path, '/');
    parent = strndup(path, strlen(path) - strlen(file_name) + 1);
    return parent;
}

/*
 * Trace the given path. Return the inode of the given path.
 */
struct ext2_inode *trace_path(char *path, unsigned char *disk) {
    char *filter = "/";

    struct ext2_inode  *inode_table = get_inode_table_loc(disk);

    // Get the inode of the root
    struct ext2_inode *current_inode = get_root_inode(inode_table);

    // Get the copy of the path
    char *full_path = malloc(sizeof(char) * (strlen(path) + 1));
    strncpy(full_path, path, strlen(path) + 1);

    char *token = strtok(full_path, filter);

    while (token != NULL) {
        if (current_inode != NULL && (current_inode->i_mode & EXT2_S_IFDIR)) {
            current_inode = get_entry_with_name(disk, token, current_inode);
        }

        token = strtok(NULL, filter);

        // Handle case: in the middle of the path is not a directory
        if (current_inode != NULL && token != NULL
            && !(current_inode->i_mode & EXT2_S_IFDIR)) {
            return NULL;
        }
    }

    // Handle the case like: /a/bb/ccc/, ccc need to be a directory
    if (current_inode != NULL
        && (path[strlen(path) - 1] == '/')
        && !(current_inode->i_mode & EXT2_S_IFDIR)) {
        return NULL;
    }

    return current_inode;
}

/*
 * Return the inode of the directory/file/link with a particular name if it is in the given
 * parent directory, otherwise, return NULL.
 */
struct ext2_inode *get_entry_with_name(unsigned char *disk, char *name, struct ext2_inode *parent) {
    struct ext2_inode *target = NULL;

    // Search through the direct blocks
    for (int i = 0; i < SINGLE_INDIRECT; i++) {
        if (parent->i_block[i]) {
            target = get_entry_in_block(disk, name, parent->i_block[i]);
        }
    }

    // Search through the indirect blocks if not find the target inode
    if (target == NULL && parent->i_block[SINGLE_INDIRECT]) {
        unsigned int *indirect = get_indirect_block_loc(disk, parent);

        for (int i = 0; i < EXT2_BLOCK_SIZE / sizeof(unsigned int); i++) {
            if (indirect[i]) {
                target = get_entry_in_block(disk, name, indirect[i]);
            }
        }
    }

    return target;
}

/*
 * Return the inode of a directory/file/link with a particular name if it is in a block,
 * otherwise return NULL.
 */
struct ext2_inode *get_entry_in_block(unsigned char *disk, char *name, int block_num) {
    struct ext2_dir_entry_2 *dir = get_dir_entry(disk, block_num);
    struct ext2_inode *target = NULL;
    struct ext2_inode  *inode_table = get_inode_table_loc(disk);

    int curr_pos = 0; // Used to keep track of the dir entry in each block
    while (curr_pos < EXT2_BLOCK_SIZE) {
        char *entry_name = malloc(sizeof(char) * dir->name_len + 1);

        for (int u = 0; u < dir->name_len; u++) {
            entry_name[u] = dir->name[u];
        }
        entry_name[dir->name_len] = '\0';

        if (strcmp(entry_name, name) == 0) {
            target = &(inode_table[dir->inode - 1]);
        }

        free(entry_name);

        /* Moving to the next directory */
        curr_pos = curr_pos + dir->rec_len;
        dir = (void*) dir + dir->rec_len;
    }

    return target;
}

/*
 * Zero the block [inode / block] bitmap of the given block number.
 */
void zero_bitmap(unsigned char *block, int block_num) {
    int bitmap_byte = block_num/ 8;
    int bit_order = block_num % 8;
    if (bit_order != 0) {
        block[bitmap_byte] &= ~(1 << (bit_order - 1));
    } else { // bit_order == 0
        block[bitmap_byte - 1] &= ~(1 << (8 - 1));
    }
}

/*
 * Clear all the entries in the blocks of given inode and
 * zero the block bitmap of given inode.
 *
 * Clear the block bitmap of the given inode [remove].
 */
void clear_block_bitmap(unsigned char *disk, char *path) {
    struct ext2_inode *remove = trace_path(path, disk);
    struct ext2_super_block *sb = get_superblock_loc(disk);
    struct ext2_group_desc *gd = get_group_descriptor_loc(disk);
    unsigned char *block_bitmap = get_block_bitmap_loc(disk);

    // Zero through the blocks on the first level
    for (int i = 0; i < SINGLE_INDIRECT; i++) {
        if (remove->i_block[i]) { // Check has data, not points to 0
            zero_bitmap(block_bitmap, remove->i_block[i]);
            remove->i_block[i] = 0; // Points to "boot" block

            sb->s_free_blocks_count++;
            gd->bg_free_blocks_count++;

            remove->i_blocks -= NUM_BLOCKS;
        }
    }

    // Zero through the blocks on the second level
    if (remove->i_block[SINGLE_INDIRECT]) {
        unsigned int *indirect = get_indirect_block_loc(disk, remove);

        for (int j = 0; j < EXT2_BLOCK_SIZE / sizeof(unsigned int); j++) {
            if (indirect[j]) {
                zero_bitmap(block_bitmap, indirect[j]);
                indirect[j] = 0; // Each indirect block points to "boot" block

                sb->s_free_blocks_count++;
                gd->bg_free_blocks_count++;

                remove->i_blocks -= NUM_BLOCKS;
            }
        }
        zero_bitmap(block_bitmap, remove->i_block[SINGLE_INDIRECT]);

        sb->s_free_blocks_count++;
        gd->bg_free_blocks_count++;

        remove->i_blocks -= NUM_BLOCKS;
    }
}

/*
 * Zero the given inode from the inode bitmap.
 */
void clear_inode_bitmap(unsigned char *disk, struct ext2_inode *remove) {
    struct ext2_super_block *sb = get_superblock_loc(disk);
    struct ext2_group_desc *gd = get_group_descriptor_loc(disk);
    unsigned char *inode_bitmap = get_inode_bitmap_loc(disk);

    int inode_number = get_inode_num(disk, remove);
    zero_bitmap(inode_bitmap, inode_number);

    sb->s_free_inodes_count++;
    gd->bg_free_inodes_count++;
}

/*
 * Get inode number of given inode if exist, otherwise return 0.
 */
int get_inode_num(unsigned char *disk, struct ext2_inode *target) {
    struct ext2_super_block *sb = get_superblock_loc(disk);
    struct ext2_inode  *inode_table = get_inode_table_loc(disk);

    int inode_num = 0;

    for (int i = 0; i < sb->s_inodes_count; i++) {
        if (inode_table[i].i_size && (&(inode_table[i]) == target)) { // There is data
            inode_num = i + 1;
        }
    }

    return inode_num;
}

/*
 * Remove the file's or directory's name of the given path.
 */
void remove_name(unsigned char *disk, char *path) {
    char *file_name = get_file_name(path);
    char *parent_path = get_dir_parent_path(path);
    struct ext2_inode *parent_dir = trace_path(parent_path, disk);
    int remove = 0;

    // Check through the direct blocks
    for (int i = 0; i < SINGLE_INDIRECT; i++) {
        if (parent_dir->i_block[i]) { // check has data, not points to 0
            remove = remove_name_in_block(disk, file_name, parent_dir->i_block[i]);
        }
    }

    // Check through the single indirect block's blocks
    if (parent_dir->i_block[SINGLE_INDIRECT] && (remove == 0)) {
        unsigned int *indirect = get_indirect_block_loc(disk, parent_dir);

        for (int j = 0; j < EXT2_BLOCK_SIZE / sizeof(unsigned int); j++) {
            if (indirect[j]) {
                remove_name_in_block(disk, file_name, indirect[j]);
            }
        }
    }
}

/*
 * Return 1 if successfully remove the directory entry with given name,
 * otherwise, return 0.
 */
int remove_name_in_block(unsigned char *disk, char *file_name, int block_num) {
    struct ext2_dir_entry_2 *dir = get_dir_entry(disk, block_num);

    int curr_pos = 0; // Used to keep track of the dir entry in each block
    struct ext2_dir_entry_2 *prev_dir = NULL;

    while (curr_pos < EXT2_BLOCK_SIZE) {
        char *entry_name = malloc(sizeof(char) * dir->name_len + 1);

        for (int u = 0; u < dir->name_len; u++) {
            entry_name[u] = dir->name[u];
        }
        entry_name[dir->name_len] = '\0';

        if (strcmp(entry_name, file_name) == 0) { // Find the dir entry with given name
            for (int i = 0; i < dir->name_len; i++) {
                dir->name[i] = '\0';
            }

            if (prev_dir != NULL) { // Need to update of the rec_len of the previous dir entry
                prev_dir->rec_len += dir->rec_len;
            }
            dir->rec_len = 0;
            free(entry_name);
            return 1;
        }

        free(entry_name);

        /* Moving to the next directory */
        curr_pos = curr_pos + dir->rec_len;
        prev_dir = dir;
        dir = (void*) dir + dir->rec_len;
    }

    for (int i = 0; i < dir->name_len; i++) {
        dir->name[i] = '\0';
    }
    dir->rec_len = 0;

    return 0;
}

/*
 * Remove a file or link in the given path.
 */
void remove_file_or_link(unsigned char *disk, char *path) {
    struct ext2_inode *path_inode = trace_path(path, disk);

    // Get the inode of a file/link which need to be removed
    if (path_inode->i_links_count > 1) {
        // Remove current file's name but keep the inode
        remove_name(disk, path);
        // Decrement links_count
        path_inode->i_links_count--;

    } else { // links_count == 1, need to remove the actual file/link
        // Clear and zero the block bitmap
        clear_block_bitmap(disk, path);
        // Clear and zero the inode bitmap
        clear_inode_bitmap(disk, path_inode);

        // Remove current file's name but keep the inode
        remove_name(disk, path);

        // Set delete time, in order to reuse inode
        path_inode->i_dtime = (unsigned int) time(NULL);
        path_inode->i_size = 0;
    }
}

/*
 * Get parent dir of a directory, exclude root dir.
 */
char *get_dir_parent_path(char *path) {
    char *file_name = NULL;
    char *parent = NULL;
    char *full_path = malloc(sizeof(char) * (strlen(path) + 1));

    if (path[strlen(path) - 1] == '/') { // remove last '/'
        for (int i = 0; i < strlen(path) - 1; i++) {
            full_path[i] = path[i];
        }

        full_path[strlen(path) - 1] = '\0';
    } else {
        strncpy(full_path, path, strlen(path) + 1);
    }

    file_name = strrchr(full_path, '/');
    parent = strndup(full_path, strlen(full_path) - strlen(file_name) + 1);

    return parent;
}

/*
 * Combine the parent path and the file/link/directory name.
 * Example: /a/bb (or /a/bb/) and ccc outputs /a/bb/ccc
 */
char *combine_name(char *parent_path, struct ext2_dir_entry_2 *dir_entry) {
    char *full_path = malloc(sizeof(char) * (strlen(parent_path) + 1 + dir_entry->name_len + 1));

    if (parent_path[strlen(parent_path) - 1] == '/') {
        strncpy(full_path, parent_path, strlen(parent_path) + 1);
        strncat(full_path, dir_entry->name, dir_entry->name_len + 1);
    } else {
        strncpy(full_path, parent_path, strlen(parent_path) + 1);
        strncat(full_path, "/", 2);
        strncat(full_path, dir_entry->name, dir_entry->name_len + 1);
    }

    return full_path;
}

/*
 * Add new entry into the directory.
 */
int add_new_entry(unsigned char *disk, struct ext2_inode *dir_inode, unsigned int new_inode, char *f_name, char type) {
    // Recalls that there are 12 direct blocks.
    struct ext2_group_desc *gd = get_group_descriptor_loc(disk);

    unsigned char *b_bitmap =  disk + EXT2_BLOCK_SIZE * (gd->bg_block_bitmap);
    int block_num;
    int length = (int)(strlen(f_name) + sizeof(struct ext2_dir_entry_2 *));
    struct ext2_dir_entry_2 *dir = NULL;
    for (int k = 0; k < 12; k++) {
        // If the block does not exist yet i.e. block number = 0
        if ((block_num = dir_inode->i_block[k]) == 0) {
            int free_block_num = get_free_block(disk, b_bitmap);
            if (free_block_num == -1) { // No extra free blocks for new entry
                return -1;
            }
            dir_inode->i_block[k] = (unsigned int)free_block_num;
            dir_inode->i_blocks += 2;
            dir_inode->i_size += EXT2_BLOCK_SIZE;
            dir = get_dir_entry(disk, free_block_num);
            length = EXT2_BLOCK_SIZE;
            break;
        }

        dir = get_dir_entry(disk, block_num);
        int curr_pos = 0;

        /* Total size of the directories in a block cannot exceed a block size */
        while (curr_pos < EXT2_BLOCK_SIZE) {
            int true_len = sizeof(struct ext2_dir_entry_2 *) + dir->name_len;
            while (true_len % 4 != 0) {
                true_len ++;
            }
            if ((dir->rec_len - true_len) >= length) {
                int orig_rec_len = dir->rec_len;
                dir->rec_len = (unsigned char) true_len;
                dir = (void *) dir + true_len;
                length = orig_rec_len - true_len;
                k = 12; // Also terminate the for loop
                break;
            }
            // Moving to the next directory
            curr_pos = curr_pos + dir->rec_len;
            dir = (void *) dir + dir->rec_len;
        }
    }
    if (dir == NULL) {
        return -1;
    }
    dir->inode = new_inode;
    dir->name_len = (unsigned char) strlen(f_name);
    memcpy(dir->name, f_name, dir->name_len);
    if (type == 'd') {
        dir->file_type = EXT2_FT_DIR;
        dir_inode->i_links_count ++;
    } else if (type == 'f') {
        dir->file_type = EXT2_FT_REG_FILE;
    } else if (type == 'l') {
        dir->file_type = EXT2_FT_SYMLINK;
    } else {
        dir->file_type = EXT2_FT_UNKNOWN;
    }
    dir->rec_len = (unsigned short)length;
    return 0;
}

/*
 * Return the first inode number that is free.
 */
int get_free_inode(unsigned char *disk, unsigned char *inode_bitmap) {
    // Loop over the inodes that are not reserved, index i
    struct ext2_group_desc *gd = get_group_descriptor_loc(disk);
    struct ext2_super_block *sb = get_superblock_loc(disk);
    for (int i = EXT2_GOOD_OLD_FIRST_INO; i < sb->s_inodes_count; i++) {
        int bitmap_byte = i / 8;
        int bit_order = i % 8;
        if (!(1 & (inode_bitmap[bitmap_byte] >> bit_order))) {
            // Such bit is 0, which is a free inode
            if ((i + 1) % 8 == 0) {
                inode_bitmap[((i + 1) / 8) - 1] |= 1 << bit_order;
            } else {
                inode_bitmap[(i + 1) / 8] |= 1 << bit_order;
            }
            sb->s_free_inodes_count --;
            gd->bg_free_inodes_count --;
            return i + 1;
        }
    }

    return -1;
}

/*
 * Return the first block number that is free.
 */
int get_free_block(unsigned char *disk, unsigned char *block_bitmap) {
    // Loop over the inodes that are not reserved
    struct ext2_group_desc *gd = get_group_descriptor_loc(disk);
    struct ext2_super_block *sb = get_superblock_loc(disk);
    for (int i = 0; i < sb->s_blocks_count; i++) {
        int bitmap_byte = i / 8;
        int bit_order = i % 8;
        if (!(1 & (block_bitmap[bitmap_byte] >> bit_order))) {
            // Such bit is 0, which is a free block
            if ((i + 1) % 8 == 0) {
                block_bitmap[((i + 1) / 8) - 1] |= 1 << bit_order;
            } else {
                block_bitmap[(i + 1) / 8] |= 1 << bit_order;
            }
            sb->s_free_blocks_count --;
            gd->bg_free_blocks_count --;
            return i + 1;
        }
    }

    return -1;
}

/*
 * Find a new unused inode and initialize. Return inode number. Return -1 if
 * could not find such inode.
 */
int init_inode(unsigned char *disk, int size, char type) {
    int inode_num;
    unsigned char *i_bitmap = get_inode_bitmap_loc(disk);
    struct ext2_inode *i_table = get_inode_table_loc(disk);
    if ((inode_num = get_free_inode(disk, i_bitmap)) == -1) {
        return -1;
    }

    struct ext2_inode *tar_inode = &(i_table[inode_num - 1]);

    // Init the inode
    if (type == 'f') {
        tar_inode->i_mode = EXT2_S_IFREG;
        tar_inode->i_links_count = 1;
    } else if (type == 'd') {
        tar_inode->i_mode = EXT2_S_IFDIR;
        tar_inode->i_links_count = 0;
    } else if (type == 'l') {
        tar_inode->i_mode = EXT2_S_IFLNK;
        tar_inode->i_links_count = 1;
    }
    tar_inode->i_size = (unsigned int)size;
    tar_inode->i_blocks = 0;
    tar_inode->i_dtime = 0;

    return inode_num;
}

/*
 * Write buf into blocks of the target inode.
 */
int write_into_block(unsigned char *disk, struct ext2_inode *tar_inode, char *buf, int buf_size) {
    // Write path into target file
    int block_index = 0;
    int indirect_b = -1;
    unsigned char *b_bitmap = get_block_bitmap_loc(disk);
    while (block_index * EXT2_BLOCK_SIZE < buf_size) { // While not write all into blocks
        int b_num;
        if (block_index < SINGLE_INDIRECT) {
            b_num = get_free_block(disk, b_bitmap);
            tar_inode->i_block[block_index] = (unsigned int) b_num;
        } else {
            if (block_index == SINGLE_INDIRECT) { // First time access indirect blocks
                indirect_b = get_free_block(disk, b_bitmap);
                tar_inode->i_block[SINGLE_INDIRECT] = (unsigned int) indirect_b;
                tar_inode->i_blocks += 2;
            }
            b_num = get_free_block(disk, b_bitmap);
            unsigned int *indirect_block = (unsigned int *) (disk + indirect_b * EXT2_BLOCK_SIZE);
            indirect_block[block_index - SINGLE_INDIRECT] = (unsigned int) b_num;
        }
        unsigned char *block = disk + b_num * EXT2_BLOCK_SIZE;
        strncpy((char *) block, &buf[block_index * EXT2_BLOCK_SIZE], EXT2_BLOCK_SIZE);
        tar_inode->i_blocks += 2;
        block_index++;
    }
    return 0;
}

