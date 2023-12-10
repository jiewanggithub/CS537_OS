#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include "wfs.h"

char *data;
int nLogEntry = 0;
uint64_t all_le[256];
off_t total_size = 0;

void print_le(struct wfs_log_entry* le){
    if(le == NULL){
        printf("Null log entry\n");
    }

    if(le->inode.mode == S_IFDIR){
        printf("Inode <%d> deleted <%d> size <%d> address <%ld> mode <%d>(dir): \n", 
        le->inode.inode_number, 
        le->inode.deleted, 
        le->inode.size,
        (uintptr_t)le - (uintptr_t)data,
        le->inode.mode
        );
        
        void *insides = le->data;

        // Iterate over the entries in the current directory
        while ((char *)insides < (char *)le + sizeof(struct wfs_inode) + le->inode.size) {
            struct wfs_dentry *entry = (struct wfs_dentry *)insides;
            printf("    inode <%ld>: %s\n", entry->inode_number, entry->name);
            insides = (char *)insides + sizeof(struct wfs_dentry);
        }
    }

    else{
        printf("Inode <%d> deleted <%d> size <%d> address <%ld> mode <%d> (file): %s\n", 
        le->inode.inode_number, 
        le->inode.deleted, 
        le->inode.size,
        (uintptr_t)le - (uintptr_t)data,
        le->inode.mode,
        le->data
        );
    }
}

struct wfs_log_entry *get_number_inode(int inode_cnm)
{
    struct wfs_sb *real_sb = (struct wfs_sb *)data;
    char *lookup = data + sizeof(struct wfs_sb);
    struct wfs_log_entry *the_entry = NULL;
    while ((uintptr_t)lookup < (uintptr_t)data + real_sb->head) {
        struct wfs_log_entry *current_entry = (struct wfs_log_entry *)lookup;
        // Check if the current entry's inode number matches the requested number
    // printf("inode num <%d> deleted <%d> mode <%d> size <%d> gid<%d> uid<%d> flags<%d>\n", 
    // current_entry->inode.inode_number, 
    // current_entry->inode.deleted, 
    // current_entry->inode.mode, 
    // current_entry->inode.size,
    // current_entry->inode.gid,
    // current_entry->inode.uid,
    // current_entry->inode.flags
    // );
        if(current_entry->inode.deleted == 0){
            all_le[current_entry->inode.inode_number] = (uintptr_t)current_entry;
        }
        else{
            all_le[current_entry->inode.inode_number] = 0;
        }
        if(current_entry->inode.inode_number > nLogEntry){
            nLogEntry = current_entry->inode.inode_number;
        }
        if (current_entry->inode.inode_number == inode_cnm) {
            // Check if the inode is marked as deleted
            the_entry = current_entry;
        }

        // Move to the next log entry
        lookup += sizeof(struct wfs_log_entry) + current_entry->inode.size;
    }
    if (the_entry->inode.deleted) {
        return NULL;  // Inode is marked as deleted
    }
    return the_entry;
}

struct wfs_log_entry *get_path_inode(const char *const_path) {

    // Starting from the root directory inode
    struct wfs_log_entry *curDir = get_number_inode(0);  // Assuming inode 0 is root
    if (curDir == NULL || !(curDir->inode.mode & S_IFDIR)) {
        return NULL;  // Root directory is not available or not a directory
    }

    // Duplicate the path to avoid modifying the original
    char *path = strdup(const_path);
    if (path == NULL) {
        perror("Failed to duplicate path");
        return NULL;
    }

    // Special case for root directory
    if (strcmp(path, "/") == 0) {
        return curDir;
    }

    // Tokenize the path
    char *token = strtok(path, "/");
    while (token != NULL) {
        int found = 0;
        void *insides = curDir->data;

        // Iterate over the entries in the current directory
        while ((char *)insides < (char *)curDir + sizeof(struct wfs_inode) + curDir->inode.size) {
            struct wfs_dentry *entry = (struct wfs_dentry *)insides;
            if (strcmp(entry->name, token) == 0) {
                curDir = get_number_inode(entry->inode_number);
                found = 1;
                break;
            }
            insides = (char *)insides + sizeof(struct wfs_dentry);
        }

        if (!found) {
            return NULL;  // Directory entry not found
        }

        token = strtok(NULL, "/");
    }

    return curDir;
}

void le_print(const char *const_path){
    struct wfs_log_entry *le = get_path_inode(const_path);
    if(le == NULL){
        printf("null log entry\n");
        return;
    }

    printf("inode num <%d>\ndeleted <%d>\nmode <%d>\nsize <%d>\n", le->inode.inode_number, le->inode.deleted, le->inode.mode, le->inode.size);

    // print all the directory entries in data[]
}

void wfsincpy(struct wfs_inode* orig_inode, struct wfs_inode* copied_inode){
    unsigned int cur_time = (unsigned int)time(NULL);
    copied_inode->inode_number = orig_inode->inode_number;
    copied_inode->deleted = orig_inode->deleted;
    copied_inode->mode = orig_inode->mode;
    copied_inode->uid = orig_inode->uid;
    copied_inode->gid = orig_inode->gid;
    copied_inode->flags = orig_inode->flags;
    copied_inode->size = orig_inode->size;  // will change
    copied_inode->atime = cur_time;
    copied_inode->mtime = cur_time;
    copied_inode->ctime = cur_time;
    copied_inode->links = orig_inode->links;
}

static int wfs_getattr(const char *path, struct stat *stbuf)
{
    struct wfs_log_entry* file_entry;
    if((file_entry = get_path_inode(path)) == NULL){
        return -ENOENT;
    }
    
    stbuf->st_uid = file_entry->inode.uid;
    stbuf->st_gid = file_entry->inode.gid;
    stbuf->st_mtime = file_entry->inode.mtime;
    stbuf->st_mode = file_entry->inode.mode;
    stbuf->st_nlink = file_entry->inode.links;
    stbuf->st_size = file_entry->inode.size;
    return 0;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    struct wfs_log_entry* test_path;
    if((test_path = get_path_inode(path)) != NULL && test_path->inode.deleted != 0){
        printf("%s still exists\n", path);
        return -EEXIST;
    }

    // Find the parent directory path
    char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        printf("No parent directory found\n");
        return -ENOENT; // No parent directory found
    }

    size_t dir_path_len = last_slash - path;
    char *dir_path = strndup(path, dir_path_len);
    if(dir_path_len == 0){
        dir_path = "/";
    }
    if (!dir_path) {
        printf("Memory allocation failure\n");
        return -1; // Memory allocation failure
    }

    struct wfs_log_entry *dir = get_path_inode(dir_path);

    if (!dir || !(dir->inode.mode & S_IFDIR)) {
        printf("Parent directory does not exist or is not a directory\n");
        return -ENOENT; // Parent directory does not exist or is not a directory
    }

    // Check if a node with the same name already exists
    const char *node_name = last_slash + 1;

    struct wfs_sb *real_sb = (struct wfs_sb *)data;

    // Check for sufficient disk space
    if (real_sb->head + sizeof(struct wfs_log_entry) > total_size) {
        printf("Insufficient disk space\n");
        return -ENOSPC; // Insufficient disk space
    }



    struct wfs_log_entry *new_dir = (struct wfs_log_entry *)(data + real_sb->head);
    wfsincpy(&(dir->inode), &(new_dir->inode));
    memcpy(new_dir->data, dir->data, dir->inode.size);
    // update the node first;
    real_sb->head += sizeof(struct wfs_log_entry) + dir->inode.size + sizeof(struct wfs_dentry); 
    
    struct wfs_log_entry *new_node = (struct wfs_log_entry *)(data + real_sb->head);
    struct wfs_inode *new_inode = &(new_node->inode);

    // Initialize the new inode
    for(int i = 0; i < 256; i++){
        if(all_le[i] == 0){
            new_inode->inode_number = i;
            break;
        }
    }
    new_inode->deleted = 0;
    new_inode->mode = S_IFREG;
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->flags = 0;
    new_inode->size = 0;  // New file size is 0
    new_inode->atime = new_inode->mtime = new_inode->ctime = (unsigned int)time(NULL);
    new_inode->links = 1;

    // update the head
    real_sb->head += sizeof(struct wfs_log_entry);

    // Update the directory entry in the parent directory new_dir
    struct wfs_dentry *new_dentry = (struct wfs_dentry *)(new_dir->data + new_dir->inode.size);
    new_dentry->inode_number = new_inode->inode_number;
    strncpy(new_dentry->name, node_name, MAX_FILE_NAME_LEN - 1);
    new_dentry->name[MAX_FILE_NAME_LEN - 1] = '\0';  // Ensure null termination
    // Update the directory size and superblock head
    new_dir->inode.size += sizeof(struct wfs_dentry);

    all_le[new_inode->inode_number] = (uintptr_t)new_node;
    print_le(new_dir);
    print_le(new_node);
    printf("successfully added %s\n", path);
    return 0;
}

static int wfs_mkdir(const char *path, mode_t mode)
{    
    struct wfs_log_entry* test_path;
    if((test_path = get_path_inode(path)) != NULL && test_path->inode.deleted != 0){
        return -EEXIST;
    }
    // Find the parent directory path
    char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        return -ENOENT; // No parent directory found
    }

    size_t dir_path_len = last_slash - path;
    char *dir_path = strndup(path, dir_path_len);
    if(dir_path_len == 0){
        dir_path = "/";
    }
    if (!dir_path) {
        return -1; // Memory allocation failure
    }

    struct wfs_log_entry *dir = get_path_inode(dir_path);

    if (!dir || !(dir->inode.mode & S_IFDIR)) {
        return -ENOENT; // Parent directory does not exist or is not a directory
    }

    // Check if a directory with the same name already exists
    const char *dir_name = last_slash + 1;

    struct wfs_sb *real_sb = (struct wfs_sb *)data;

    // Check for sufficient disk space
    if (real_sb->head + sizeof(struct wfs_log_entry) > total_size) {
        return -ENOSPC; // Insufficient disk space
    }

    struct wfs_log_entry *new_dir = (struct wfs_log_entry *)(data + real_sb->head);
    wfsincpy(&(dir->inode), &(new_dir->inode));
    memcpy(new_dir->data, dir->data, dir->inode.size);
    // update the node first;
    real_sb->head += sizeof(struct wfs_log_entry) + dir->inode.size + sizeof(struct wfs_dentry); 
    
    struct wfs_log_entry *new_node = (struct wfs_log_entry *)(data + real_sb->head);
    struct wfs_inode *new_inode = &(new_node->inode);

    // Initialize the new inode
    for(int i = 0; i < 256; i++){
        if(all_le[i] == 0){
            new_inode->inode_number = i;
            break;
        }
    }
    new_inode->deleted = 0;
    new_inode->mode = S_IFDIR;
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->flags = 0;
    new_inode->size = 0;  // New file size is 0
    new_inode->atime = new_inode->mtime = new_inode->ctime = (unsigned int)time(NULL);
    new_inode->links = 1;

    // update the head
    real_sb->head += sizeof(struct wfs_log_entry);

    // Update the directory entry in the parent directory new_dir
    struct wfs_dentry *new_dentry = (struct wfs_dentry *)(new_dir->data + new_dir->inode.size);
    new_dentry->inode_number = new_inode->inode_number;
    strncpy(new_dentry->name, dir_name, MAX_FILE_NAME_LEN - 1);
    new_dentry->name[MAX_FILE_NAME_LEN - 1] = '\0';  // Ensure null termination
    all_le[new_inode->inode_number] = (uintptr_t)new_node;
    // Update the directory size and superblock head
    new_dir->inode.size += sizeof(struct wfs_dentry);

    return 0;
}

static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("reading %s\n", path);
    // Retrieve the inode corresponding to the path
    struct wfs_log_entry* file_entry = get_path_inode(path);
    if (file_entry == NULL) {
        return -ENOENT;  // File does not exist
    }

    if (file_entry->inode.mode & S_IFDIR) {
        return -1;  // Cannot read from a directory
    }

    if (offset >= file_entry->inode.size) {
        return 0;  // Offset is beyond the end of the file, nothing to read
    }

    // Calculate the actual size to read
    size_t actual_size = size;
    if (offset + size > file_entry->inode.size) {
        actual_size = file_entry->inode.size - offset; // Adjust size to available data
    }

    // Check if the buffer is valid
    if (buf == NULL) {
        return -1;  // Invalid buffer
    }

    // Perform the read operation
    memcpy(buf, file_entry->data + offset, actual_size);

    // Update the last access time
    file_entry->inode.atime = (unsigned int)time(NULL);

    return actual_size;  // Return the number of bytes read
}

static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // Retrieve the inode corresponding to the path
    struct wfs_log_entry* original_file_entry = get_path_inode(path);
    if (original_file_entry == NULL) {
        return -ENOENT;  // File does not exist
    }

    if (original_file_entry->inode.mode & S_IFDIR) {
        return -1;  // Cannot write to a directory
    }

    if (offset > original_file_entry->inode.size) {
        return -EFBIG;  // Offset is beyond the end of the file
    }

    // Check if the buffer is valid
    if (buf == NULL) {
        return -1;  // Invalid buffer
    }

    struct wfs_sb *real_sb = (struct wfs_sb *)data;
    struct wfs_log_entry* modified_file_entry = (struct wfs_log_entry*)(data + real_sb->head);
    struct wfs_inode* modified_inode = &(modified_file_entry->inode);

    // Copy the original inode data to the modified inode
    wfsincpy(&(original_file_entry->inode), modified_inode);

    // Check for sufficient disk space
    if (real_sb->head + sizeof(struct wfs_log_entry) + original_file_entry->inode.size + size > total_size) {
        return -ENOSPC; // Insufficient disk space
    }

    // Prepare the buffer for modified data
    char *modified_data = modified_file_entry->data;
    if (original_file_entry->inode.size > 0) {
        memcpy(modified_data, original_file_entry->data, original_file_entry->inode.size);
    }

    // Perform the write operation
    memcpy(modified_data + offset, buf, size);

    // Update inode information
    if(offset + size > original_file_entry->inode.size){
        modified_inode->size = offset + size;
    }
    else{
        modified_inode->size = original_file_entry->inode.size;
    }
    modified_inode->mtime = modified_inode->ctime = (unsigned int)time(NULL);

    // Update the superblock's head pointer
    real_sb->head += sizeof(struct wfs_inode) + modified_inode->size;
    printf("written to %s\n", path);

    return size;  // Return the number of bytes written
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{

    // Retrieve the directory entry corresponding to the path
    struct wfs_log_entry *dir = get_path_inode(path);
    if (dir == NULL || !(dir->inode.mode & S_IFDIR)) {
        return -ENOENT;  // Path is not a directory or does not exist
    }

    // Iterate over the entries in the directory
    int dentry_num = 0;
    while (dentry_num * sizeof(struct wfs_dentry) < dir->inode.size) {
        struct wfs_dentry *cur_dentry = (struct wfs_dentry *)(dir->data + dentry_num * sizeof(struct wfs_dentry));

        // Call the filler function to add the entry to the buffer
        if (filler(buf, cur_dentry->name, NULL, 0) != 0) {
            break; // Buffer is full, no more entries can be added
        }
        dentry_num++;
    }

    return 0; // Success
}

static int wfs_unlink(const char *path) {

    // Retrieve the file entry corresponding to the path
    struct wfs_log_entry *file_entry = get_path_inode(path);
    if (file_entry == NULL) {
        return -ENOENT;  // File does not exist
    }

    // Find the parent directory of the file
    char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        return -ENOENT; // No parent directory found
    }

    size_t dir_path_len = last_slash - path;
    char *dir_path = strndup(path, dir_path_len);    
    if(dir_path_len == 0){
        dir_path = "/";
    }
    if (!dir_path) {
        return -1; // Memory allocation failure
    }

    struct wfs_log_entry *dir = get_path_inode(dir_path);
    if (!dir || !(dir->inode.mode & S_IFDIR)) {
        return -ENOENT; // Parent directory does not exist
    }

    struct wfs_sb *real_sb = (struct wfs_sb *)data;
    struct wfs_log_entry *new_dir = (struct wfs_log_entry *)(data + real_sb->head);
    wfsincpy(&(dir->inode), &(new_dir->inode));

    // Remove the file entry from the directory
    const char *filename = last_slash + 1;
    void *insides = dir->data, *new_insides = new_dir->data;
    int found = 0;

    while ((char *)insides < (char *)dir->data + dir->inode.size) {
        struct wfs_dentry *dentry = (struct wfs_dentry *)insides;
        struct wfs_dentry *new_dentry = (struct wfs_dentry *)new_insides;
        
        // skip if the to be deleted entry is encountered
        if (strcmp(dentry->name, filename) == 0) {
            insides = (char *)insides + sizeof(struct wfs_dentry);
            found = 1;
            continue;
        }

        // copy other entries
        new_dentry->inode_number = dentry->inode_number;  
        strncpy(new_dentry->name, dentry->name, MAX_FILE_NAME_LEN - 1);
        new_dentry->name[MAX_FILE_NAME_LEN - 1] = '\0';  // Ensure null termination
        printf("file <%ld> copied\n", dentry->inode_number);

        insides = (char *)insides + sizeof(struct wfs_dentry);
        new_insides = (char *)new_insides + sizeof(struct wfs_dentry);
    }

    if (found) {
        // Update the size of the directory to reflect the removed entry
        new_dir->inode.size -= sizeof(struct wfs_dentry);
    } else {
        return -ENOENT; // File entry not found in parent directory
    }


    // update the node later;
    real_sb->head += sizeof(struct wfs_log_entry) + new_dir->inode.size;

    file_entry->inode.deleted = 1;

    all_le[file_entry->inode.inode_number] = 0;
    
    print_le(new_dir);
    printf("%s removed\n", path);

    return 0; // Success
}

static struct fuse_operations ops = {
    .getattr = wfs_getattr,
    .mknod = wfs_mknod,
    .mkdir = wfs_mkdir,
    .read = wfs_read,
    .write = wfs_write,
    .readdir = wfs_readdir,
    .unlink = wfs_unlink,
};

int main(int argc, char *argv[])
{
    int fd = open(argv[argc - 2], O_RDONLY);
    if (fd == -1)
    {
        exit(-1);
    }

    struct stat file_info;
    if (fstat(fd, &file_info) == -1)
    {
        exit(-1);
    }

    data = mmap(NULL, file_info.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED)
    {
        exit(-1);
    }
    total_size = file_info.st_size;
    
    // // initialize super block, change the pointer head to the bottom of super block
    // struct wfs_sb *real_sb = (struct wfs_sb *)data;
    // real_sb->head += sizeof(struct wfs_sb);
    // real_sb->magic = WFS_MAGIC;

    close(fd);

    char **new_argv = malloc(sizeof(char*) * argc);
    if (!new_argv) {
        close(fd);
        munmap(data, file_info.st_size);
        return -1;
    }

    for (int i = 0, j = 0; i < argc; ++i) {
        if (i != argc - 2) {
            new_argv[j++] = argv[i];
        }
    }

    // le_print("/");
    for(int i = 0; i < 256; i++){
        all_le[i] = 0;
    }
    printf("total size %ld\n", total_size);
    int fuse_status = fuse_main(argc - 1, new_argv, &ops, NULL);

    munmap(data, file_info.st_size);
    return fuse_status;
}
