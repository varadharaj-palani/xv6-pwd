#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "fcntl.h"
#include "fs.h"
#include "stat.h"
#define DIRSIZ 14

struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.
};
struct sleeplock {
  uint locked;       // Is the lock held?
  struct spinlock lk; // spinlock protecting this sleep lock
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};

struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};


int
 sys_hello(void) {
    cprintf("\nSurprise\n\n");
    return 7;
 }

int
name_of_inode(struct inode *ip, struct inode *parent, char buf[DIRSIZ]) {
    uint off;
    struct dirent de;
    for (off = 0; off < parent->size; off += sizeof(de)) {
        if (readi(parent, (char*)&de, off, sizeof(de)) != sizeof(de))
            panic("couldn't read dir entry");
        if (de.inum == ip->inum) {
            safestrcpy(buf, de.name, DIRSIZ);
            return 0;
        }
    }
    return -1;
}

int
name_for_inode(char* buf, int n, struct inode *ip) {
    int path_offset;
    struct inode *parent;
    char node_name[DIRSIZ];
    if (ip->inum == namei("/")->inum) { //namei is inefficient but iget isn't exported for some reason
        buf[0] = '/';
        return 1;
    } else if (ip->type == T_DIR) {
        parent = dirlookup(ip, "..", 0);
        ilock(parent);
        if (name_of_inode(ip, parent, node_name)) {
            panic("could not find name of inode in parent!");
        }
        path_offset = name_for_inode(buf, n, parent);
        safestrcpy(buf + path_offset, node_name, n - path_offset);
        path_offset += strlen(node_name);
        if (path_offset == n - 1) {
            buf[path_offset] = '\0';
            return n;
        } else {
            buf[path_offset++] = '/';
        }
        iput(parent); //free
        return path_offset;
    } else if (ip->type == T_DEV || ip->type == T_FILE) {
        panic("process cwd is a device node / file, not a directory!");
    } else {
        panic("unknown inode type");
    }
    return -1;
}

int
sys_getcwd(void)
{
    char *p;
    int n;
    struct proc *curproc = myproc();
    if(argint(1, &n) < 0 || argptr(0, &p, n) < 0)
        return -1;
    return name_for_inode(p, n, curproc->cwd);
}

 
int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
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
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_cps (void)
{
  return cps();
}