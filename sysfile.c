//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//
#include "e1000.h"

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "x86.h"
#include "memlayout.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  uint off;
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, &off)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int
sys_arp(void)
{
  char *ipAddr, *interface, *arpResp;
  int size;

  if(argstr(0, &interface) < 0 || argstr(1, &ipAddr) < 0 || argint(3, &size) < 0 || argptr(2, &arpResp, size) < 0) {
    cprintf("ERROR:sys_createARP:Failed to fetch arguments");
    return -1;
  }

  if(send_arpRequest(interface, ipAddr, arpResp) < 0) {
    cprintf("ERROR:sys_createARP:Failed to send ARP Request for IP:%s", ipAddr);
    return -1;
  }

  return 0;
}

// static uint32_t e1000_reg_read(uint32_t reg_addr, struct e1000 *the_e1000)
// {
//   uint32_t value = *(uint32_t*)(the_e1000->membase + reg_addr);
//   //cprintf("Read value 0x%x from E1000 I/O port 0x%x\n", value, reg_addr);

//   return value;
// }

int
sys_checknic(void)
{
  int HEAD;
  int TAIL;

  if(argint(0,&HEAD)<0 || argint(1,&TAIL)<0)
  {
    cprintf("Error: invalid parameter");
    return -1;
  }

  // void* driver=0;
  // uint16_t length=0;
  // uint8_t* p=(uint8_t*)kalloc();
  // while(1)
  // {
  //   e1000_recv(driver,p,&length);
  //   if(length>0)
  //   {
  //     for(int i=0;i<length;++i)
  //     {
  //       cprintf("%x",*p);
  //       ++p;
  //     }
  //     break;
  //   }
  // }

  //>>>>>>>>>>>>>>>>
  // struct e100* e100p=(struct e100*)nic_devices[0].driver;
  // uint8_t* pkt=(uint8_t*)kalloc();
  // uint16_t* length=0;
  // e1000_recv(e100p,pkt,length);
  // uint8_t old_status=inb(e100p->iobase);
  // while(1)
  // {
  //   uint8_t new_status=inl(e100p->iobase);
  //   udelay(10);
  //   if(new_status!=old_status)
  //   {
  //     cprintf("new: 0x%x\n",new_status);
  //     cprintf("old: 0x%x\n",old_status);
  //     e1000_recv(e100p,pkt,length);
  //     for(int i=0;i<100;++i)
  //     {
  //       cprintf("%x",*pkt);
  //       pkt++;
  //     }
  //     break;
  //   }
  // }
  //>>>>>>>>>>>>>>>


  struct e1000* e1000p=(struct e1000*)nic_devices[0].driver;
  //TAIL=e1000_reg_read(E1000_RDT,nic_devices[0].driver);
  // cprintf("TAIL : %x\n",TAIL);
  //HEAD=e1000_reg_read(E1000_RDH,nic_devices[0].driver);
  // cprintf("HEAD : %x\n",HEAD);
  uint8_t* p=P2V((uint8_t*)(uint32_t)e1000p->rbd[0]->addr);
  for(int i=0;i<60;++i)
  {
    cprintf("%x",*p);
    ++p;
  }
  //>>>>>>>>>>>>>>>
  // uint8_t* p=(uint8_t*)kalloc();
  // uint16_t length=0;
  // while(1)
  // {
  //   e1000_recv(e1000p,p,&length);
  //   if(length!=0)
  //   {
  //     for(int i=0;i<length;++i)
  //     {
  //       cprintf("%x",*p);
  //       ++p;
  //     }
  //     cprintf("\n");
  //   }
  //>>>>>>>>>>>>>>
    // HEAD=e1000_reg_read(E1000_RDH,nic_devices[0].driver);
    // if((TAIL+1)%128==HEAD)
    //   continue;
    // TAIL=(TAIL+1)%128;
    // while(!(e1000p->rbd[TAIL]->status&E1000_RXD_STAT_DD )||!(e1000p->rbd[TAIL]->status&E1000_RXD_STAT_EOP)){}
    // cprintf("TAIL : %x\n",TAIL);
    // cprintf("HEAD : %x\n",HEAD);
    // struct e1000_rbd* rbdp=e1000p->rbd[TAIL];
    // cprintf("LENGTH : %x\n",rbdp->length);
    // cprintf("STATUS : %x\n",rbdp->status);
    // cprintf("ERROR : %x\n",rbdp->errors);
    // cprintf("CHECKSUM : %x\n",rbdp->checksum);
    // struct packet_buf* pbp=e1000p->rx_buf[TAIL];
    // for(int i=0;i<2046;++i)
    // {
    //   cprintf("%x",pbp->buf[i]);
    // }
  


  // struct e1000* e1000p=(struct e1000*)nic_devices[0].driver;
  // cprintf("HEAD: %x\n",e1000p->rbd_head);
  // cprintf("TAIL: %x\n",e1000p->rbd_tail);

  // ddreg=e1000_reg_read(E1000_TDT, nic_devices[0].driver)&0x00000001;
  // old_reg=ddreg;
  // cprintf("%x",&ddreg);
  // while(1)
  // {
  //     ddreg=e1000_reg_read(E1000_TDT, nic_devices[0].driver)&0x00000001;
  //     if(old_reg!=ddreg)
  //     {
  //         if(ddreg==1)
  //             cprintf("1");
  //         else
  //             cprintf("0");
  //     }
  // }
  return 0;
}