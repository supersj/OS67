#define __LOG_ON 1
// std
#include <type.h>
#include <dbg.h>
#include <syscall.h>
// libs
#include <printk.h>
#include <string.h>
// proc
#include <proc.h>
// fs
#include <minix.h>
#include <stat.h>
#include <inode.h>
#include <dir.h>
#include <p2i.h>
#include <file.h>
#include <sysfile.h>

/* get syscall param as file descrpitor */
static int argfd(int n, int *pfd, struct file **pf){
    int fd;
    struct file *f;

    if (argint(n, &fd) < 0){
        goto bad;
    }
    if (fd < 0 || fd >= NOFILE || (f = proc->ofile[fd]) == 0){
        goto bad;
    }
    if (pfd){
        *pfd = fd;
    }
    if (pf){
        *pf = f;
    }
    return 0;
bad:
    printl("argfd: failed %d\n", n);
    return -1;
}

/* alloc a file descrpitor for given file */
static int fdalloc(struct file *f){
    int fd;
    
    for (fd = 0; fd < NOFILE; fd++){
        if (proc->ofile[fd] == 0){
            proc->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

int sys_dup(){
    struct file *f;
    int fd;

    if (argfd(0, 0, &f) < 0){
        return -1;
    }
    if ((fd = fdalloc(f)) < 0){
        return -1;
    }
    fdup(f);

    return fd;
}

int sys_read(){
    struct file *f;
    int n;
    char *p;

    if (argfd(0, 0, &f) < 0  || argint(2, &n) < 0 || argptr(1, &p, n) < 0){
        return -1;
    }

    return fread(f, p, n);
}

int sys_write(){
    struct file *f;
    int n;
    char *p;

    if (argfd(0, 0, &f) < 0  || argint(2, &n) < 0 || argptr(1, &p, n) < 0){
        return -1;
    }

    return fwrite(f, p, n);
}

int sys_close(){
    int fd;
    struct file *f;

    if (argfd(0, &fd, &f) < 0){
        return -1;
    }
    proc->ofile[fd] = 0;
    fclose(f);

    return 0;
}

int sys_fstat(){
    struct file *f;
    struct stat *st;

    if (argfd(0, 0, &f) < 0 || argptr(1, (void *)&st, sizeof(st)) < 0){
        return -1;
    }
    return fstat(f, st);
}

/* int _link(char *old, char *new); */
int sys_link(){
    char name[NAME_LEN], *new, *old;
    struct inode *dp, *ip;

    if (argstr(0, &old) < 0 || argstr(1, &new) < 0){
        return -1;
    }
    if ((ip = p2i(old)) == 0){
        return -1;
    }

    printl("sys_link: old: [%s] new: [%s]\n", old, new);

    ilock(ip);

    if (S_ISDIR(ip->mode)){     // don't create link for directory
        printl("sys_link: failed, can not link directory\n");
        iunlockput(ip);
        return -1;
    }
    
    ip->nlinks++;
    iupdate(ip);

    if ((dp = p2ip(new, name)) == 0){
        printl("sys_link: failed, parent directory not found\n");
        goto bad;
    }
    ilock(dp);
    /* don't worry about dev number */
    if (dir_link(dp, name, ip) < 0){
        printl("sys_link: failed, dir_link\n");
        iunlockput(dp);
        goto bad;
    }
    iunlockput(dp);
    iunlockput(ip);

    return 0;

bad:
    printl("sys_link: bad\n");
    ip->nlinks--;
    iupdate(ip);
    iunlockput(ip);

    return -1;
}

/* int _unlink(char *path) */
int sys_unlink(){
    uint32_t off;
    char name[NAME_LEN], *path;
    struct inode *ip, *dip;
    struct dir_entry de;

    if (argstr(0, &path) < 0){
        return -1;
    }
    
    printl("sys_unlink: unlink [%s]\n", path);

    if ((dip = p2ip(path, name)) == 0){
        return -1;
    }

    ilock(dip);
    print_i(dip);

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0){
        return -1;
    }

    if ((ip = dir_lookup(dip, name, &off)) == 0){
        goto bad;
    }

    ilock(ip);
    print_i(ip);

    assert(ip->nlinks >= 1, "sys_unlink: nlinks < 1");

    /* we do not link any directory, so directory's nlinks always be 2 (self and ".") */
    if (S_ISDIR(ip->mode) && !dir_isempty(ip)){
        iunlockput(ip);
        goto bad;
    }

    memset(&de, 0, sizeof(de));
    if (iwrite(dip, (char *)&de, off, sizeof(de)) != sizeof(de)){
        panic("sys_unlink: wirte fault");
    }

    /* empty directory */
    if (S_ISDIR(ip->mode)){
        dip->nlinks--;   // for ip/..
        iupdate(dip);
    }
    iunlockput(dip);

    ip->nlinks--;
    iupdate(ip);
    iunlockput(ip);

    return 0;

bad:
    iunlockput(dip);

    return -1;
}

int sys_open(){
    return 0;
}

int sys_mkdir(){
    return 0;
}
int sys_chdir(){
    return 0;
}
