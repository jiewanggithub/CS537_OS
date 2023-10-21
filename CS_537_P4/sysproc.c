#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "psched.h"

int getschedstate(struct pschedinfo *psi);
int nice(int n);
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
  myproc()->sleep_ticks = n;
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

int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_nice(void)
{
  int nice_new_;
  if(argint(0, &nice_new_) < 0){
    return -1;
  }
  return nice(nice_new_);
}

int
sys_getschedstate(void)
{
  struct pschedinfo *psi;
  if(argptr(0, (void*)&psi, sizeof(*psi)) < 0){
    return -1;
  }
  return getschedstate(psi);
}