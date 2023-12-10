#include "wfs.h"
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

void initialize_file_system(int fd) {
    // Initialize and write the superblock
    struct wfs_sb sb = {
        .magic = WFS_MAGIC,
        .head = sizeof(struct wfs_sb)  // Assuming the head starts right after the superblock
    };

    // Create and write the root directory inode
    struct wfs_inode root_inode = {
        .inode_number = 0,
        .deleted = 0,
        .mode = S_IFDIR,    // Root is a directory
        .uid = 0,           // Root owned by superuser (typically)
        .gid = 0,           // Group of superuser
        .flags = 0,         // No specific flags
        .size = 0,          // Initial size is 0
        .atime = time(NULL),
        .mtime = time(NULL),
        .ctime = time(NULL),
        .links = 1          // Usually, the root inode has one link
    };

    struct wfs_log_entry log_entry = {
        .inode = root_inode
    };

    sb.head += sizeof(struct wfs_log_entry);
    write(fd, &sb, sizeof(sb));
    write(fd, &log_entry, sizeof(log_entry));
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s disk_path\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *disk_path = argv[1];
    int fd = open(disk_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    if (fd == -1) {
        perror("Error opening disk image file");
        return EXIT_FAILURE;
    }

    initialize_file_system(fd);

    close(fd);
    return EXIT_SUCCESS;
}