# P7
## 2023/12/05
### done
Done with the regulars. 
### errors
1. error after deleting and then creating: nLogEntries increased, gap between inode numbers

## 2023/12/05
### most tests done
(*DONE*)only unlink has errors
### errors
1. (*DONE*)the get_path_inode is incorrect.

## 2023/12/01
### Done something
1. finished writing get inode from path and number
2. wrote wfs_read, untested  

### Note
1. (*DONE*)still have compiling error  
2. (*DONE*)untested

## 2023/11/30
### Getting started
1. (*DONE*)get inode from inode number
2. (*DONE*)get inode from path
3. (*DONE*)actual size of log entry is sizeof() + the len of data (size param in inode)  

### Unclear problems
1. (*DONE*)Unknown utilization of "struct fuse_file_info* fi" in function wfs_read() in mount.wfs.c -> not needed
2. (*DONE*)Don't know how to use mmap (specifically in mkfs.wfs.c)
