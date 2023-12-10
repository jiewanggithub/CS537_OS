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

int main(int argc, char *argv[]){
      if(argc != 2){
            return -1;
      }
      get_number_inode(0);

      struct wfs_sb *real_sb = (struct wfs_sb *)data;

      real_sb->head = sizeof(struct wfs_sb);
      for(int i = 0; i < 256; i++){
            if(all_le[i] != 0){
                  struct wfs_log_entry *cur = (struct wfs_log_entry *)all_le[i];
                  struct wfs_log_entry *new = (struct wfs_log_entry *)(data + real_sb->head);

                  wfsincpy(&(cur->inode), &(new->inode));
                  memcpy(new->data, cur->data, cur->inode.size);
                  real_sb->head += sizeof(struct wfs_log_entry) + cur->inode.size;

                  all_le[i] = (uintptr_t)new;
            }
      }
      
      return 0;
}