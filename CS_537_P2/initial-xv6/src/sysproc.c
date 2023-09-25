#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "stddef.h"

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_getlastcat(void)
{
  char* ptr;
  if(argptr(0,&ptr,sizeof(&ptr)) < 0 || ptr == NULL ){
    return -1;
  }
  if (catNum == 0){
    buffer[strlen("Cat has not yet been called")] = '\0';
    strncpy(ptr, "Cat has not yet been called", strlen("Cat has not yet been called"));
  }
  else if (noArg == 1){
    buffer[strlen("No args were passed")] = '\0';
    strncpy(ptr, "No args were passed", strlen("No args were passed"));
  }
  else if(switch1 == 1){
    buffer[strlen("Invalid filename")] = '\0';
    strncpy(ptr, "Invalid filename", strlen("Invalid filename"));
  }
  else{
    strncpy(ptr,buffer,strlen(buffer));
  }
  return 0;
}
