/*
 * opfs: a simple utility for manipulating xv6 file system images
 *
 * Copyright (c) 2015 Takuo Watanabe
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* usage: opfs img_file command [arg...]
 * command
 *     diskinfo
 *     info path
 *     ls path
 *     get path
 *     put path
 *     rm path
 *     cp spath dpath
 *     mv spath dpath
 *     ln spath dpath
 *     mkdir path
 *     rmdir path
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <assert.h>

#include "libfs.h"

/*
 * Command implementations
 */

// diskinfo
int do_diskinfo(img_t img, int argc, char *argv[]) {
    if (argc != 0) {
        error("usage: %s img_file diskinfo\n", progname);
        return EXIT_FAILURE;
    }

    uint N = SBLK(img)->size;
    uint Ni = SBLK(img)->ninodes / IPB + 1;
    uint Nm = N / (BSIZE * 8) + 1;
    uint Nd = SBLK(img)->nblocks;
    uint Nl = SBLK(img)->nlog;

    printf("total blocks: %d (%d bytes)\n", N, N * BSIZE);
    printf("inode blocks: #%d-#%d (%d blocks, %d inodes)\n",
           2, Ni + 1, Ni, SBLK(img)->ninodes);
    printf("bitmap blocks: #%d-#%d (%d blocks)\n", Ni + 2, Ni + Nm + 1, Nm);
    printf("data blocks: #%d-#%d (%d blocks)\n",
           Ni + Nm + 2, Ni + Nm + Nd + 1, Nd);
    printf("log blocks: #%d-#%d (%d blocks)\n",
           Ni + Nm + Nd + 2, Ni + Nm + Nd + Nl + 1, Nl);
    printf("maximum file size (bytes): %zu\n", MAXFILESIZE);

    int nblocks = 0;
    for (uint b = Ni + 2; b <= Ni + Nm + 1; b++)
        for (int i = 0; i < BSIZE; i++)
            nblocks += bitcount(img[b][i]);
    printf("# of used blocks: %d\n", nblocks);

    int n_dirs = 0, n_files = 0, n_devs = 0;
    for (uint b = 2; b <= Ni + 1; b++)
        for (int i = 0; i < IPB; i++)
            switch (((inode_t)img[b])[i].type) {
            case T_DIR:
                n_dirs++;
                break;
            case T_FILE:
                n_files++;
                break;
            case T_DEV:
                n_devs++;
                break;
            }
    printf("# of used inodes: %d (dirs: %d, files: %d, devs: %d)\n",
           n_dirs + n_files + n_devs, n_dirs, n_files, n_devs);

    return EXIT_SUCCESS;
}

// info path
int do_info(img_t img, int argc, char *argv[]) {
    if (argc != 1) {
        error("usage: %s img_file info path\n", progname);
        return EXIT_FAILURE;
    }
    char *path = argv[0];

    inode_t ip = ilookup(img, root_inode, path);
    if (ip == NULL) {
        error("info: no such file or directory: %s\n", path);
        return EXIT_FAILURE;
    }
    printf("inode: %d\n", geti(img, ip));
    printf("type: %d (%s)\n", ip->type, typename(ip->type));
    printf("nlink: %d\n", ip->nlink);
    printf("size: %d\n", ip->size);
    if (ip->size > 0) {
        printf("data blocks:");
        int bcount = 0;
        for (uint i = 0; i < NDIRECT && ip->addrs[i] != 0; i++, bcount++)
            printf(" %d", ip->addrs[i]);
        uint iaddr = ip->addrs[NDIRECT];
        if (iaddr != 0) {
            bcount++;
            printf(" %d", iaddr);
            uint *iblock = (uint *)img[iaddr];
            for (int i = 0; i < BSIZE / sizeof(uint) && iblock[i] != 0;
                 i++, bcount++)
                printf(" %d", iblock[i]);
        }
        printf("\n");
        printf("# of data blocks: %d\n", bcount);
    }
    return EXIT_SUCCESS;
}

// ls path
int do_ls(img_t img, int argc, char *argv[]) {
    if (argc != 1) {
        error("usage: %s img_file ls path\n", progname);
        return EXIT_FAILURE;
    }
    char *path = argv[0];
    inode_t ip = ilookup(img, root_inode, path);
    if (ip == NULL) {
        error("ls: %s: no such file or directory\n", path);
        return EXIT_FAILURE;
    }
    if (ip->type == T_DIR) {
        struct dirent de;
        for (uint off = 0; off < ip->size; off += sizeof(de)) {
            if (iread(img, ip, (uchar *)&de, sizeof(de), off) != sizeof(de)) {
                error("ls: %s: read error\n", path);
                return EXIT_FAILURE;
            }
            if (de.inum == 0)
                continue;
            char name[DIRSIZ + 1];
            name[DIRSIZ] = 0;
            strncpy(name, de.name, DIRSIZ);
            inode_t p = iget(img, de.inum);
            printf("%s %d %d %d\n", name, p->type, de.inum, p->size);
        }
    }
    else
        printf("%s %d %d %d\n", path, ip->type, geti(img, ip), ip->size);

    return EXIT_SUCCESS;
}

// get path
int do_get(img_t img, int argc, char *argv[]) {
    if (argc != 1) {
        error("usage: %s img_file get path\n", progname);
        return EXIT_FAILURE;
    }
    char *path = argv[0];

    // source
    inode_t ip = ilookup(img, root_inode, path);
    if (ip == NULL) {
        error("get: no such file or directory: %s\n", path);
        return EXIT_FAILURE;
    }

    uchar buf[BUFSIZE];
    for (uint off = 0; off < ip->size; off += BUFSIZE) {
        int n = iread(img, ip, buf, BUFSIZE, off);
        if (n < 0) {
            error("get: %s: read error\n", path);
            return EXIT_FAILURE;
        }
        write(1, buf, n);
    }

    return EXIT_SUCCESS;
}

// put path
int do_put(img_t img, int argc, char *argv[]) {
    if (argc != 1) {
        error("usage: %s img_file put path\n", progname);
        return EXIT_FAILURE;
    }
    char *path = argv[0];

    // destination
    inode_t ip = ilookup(img, root_inode, path);
    if (ip == NULL) {
        ip = icreat(img, root_inode, path, T_FILE, NULL);
        if (ip == NULL) {
            error("put: %s: cannot create\n", path);
            return EXIT_FAILURE;
        }
    }
    else {
        if (ip->type != T_FILE) {
            error("put: %s: directory or device\n", path);
            return EXIT_FAILURE;
        }
        itruncate(img, ip, 0);
    }
    
    uchar buf[BUFSIZE];
    for (uint off = 0; off < MAXFILESIZE; off += BUFSIZE) {
        int n = read(0, buf, BUFSIZE);
        if (n < 0) {
            perror(NULL);
            return EXIT_FAILURE;
        }
        if (iwrite(img, ip, buf, n, off) != n) {
            error("put: %s: write error\n", path);
            return EXIT_FAILURE;
        }
        if (n < BUFSIZE)
            break;
    }
    return EXIT_SUCCESS;
}

// rm path
int do_rm(img_t img, int argc, char *argv[]) {
    if (argc != 1) {
        error("usage: %s img_file rm path\n", progname);
        return EXIT_FAILURE;
    }
    char *path = argv[0];
    
    inode_t ip = ilookup(img, root_inode, path);
    if (ip == NULL) {
        error("rm: %s: no such file or directory\n", path);
        return EXIT_FAILURE;
    }
    if (ip->type == T_DIR) {
        error("rm: %s: a directory\n", path);
        return EXIT_FAILURE;
    }
    if (iunlink(img, root_inode, path) < 0) {
        error("rm: %s: cannot unlink\n", path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// cp src_path dest_path
int do_cp(img_t img, int argc, char *argv[]) {
    if (argc != 2) {
        error("usage: %s img_file cp spath dpath\n", progname);
        return EXIT_FAILURE;
    }
    char *spath = argv[0];
    char *dpath = argv[1];

    // source
    inode_t sip = ilookup(img, root_inode, spath);
    if (sip == NULL) {
        error("cp: %s: no such file or directory\n", spath);
        return EXIT_FAILURE;
    }
    if (sip->type != T_FILE) {
        error("cp: %s: directory or device file\n", spath);
        return EXIT_FAILURE;
    }

    // destination
    inode_t dip = ilookup(img, root_inode, dpath);
    char ddir[BUFSIZE];
    char *dname = splitpath(dpath, ddir, BUFSIZE);
    if (dip == NULL) {
        if (is_empty(dname)) {
            error("cp: %s: no such directory\n", dpath);
            return EXIT_FAILURE;
        }
        inode_t ddip = ilookup(img, root_inode, ddir);
        if (ddip == NULL) {
            error("cp: %s: no such directory\n", ddir);
            return EXIT_FAILURE;
        }
        if (ddip->type != T_DIR) {
            error("cp: %s: not a directory\n", ddir);
            return EXIT_FAILURE;
        }
        dip = icreat(img, ddip, dname, T_FILE, NULL);
        if (dip == NULL) {
            error("cp: %s/%s: cannot create\n", ddir, dname);
            return EXIT_FAILURE;
        }
    }
    else {
        if (dip->type == T_DIR) {
            char *sname = splitpath(spath, NULL, 0);
            inode_t fp = icreat(img, dip, sname, T_FILE, NULL);
            if (fp == NULL) {
                error("cp: %s/%s: cannot create\n", dpath, sname);
                return EXIT_FAILURE;
            }
            dip = fp;
        }
        else if (dip->type == T_FILE) {
            itruncate(img, dip, 0);
        }
        else if (dip->type == T_DEV) {
            error("cp: %s: device file\n", dpath);
            return EXIT_FAILURE;
        }
    }

    // sip : source file inode, dip : destination file inode
    uchar buf[BUFSIZE];
    for (uint off = 0; off < sip->size; off += BUFSIZE) {
        int n = iread(img, sip, buf, BUFSIZE, off);
        if (n < 0) {
            error("cp: %s: read error\n", spath);
            return EXIT_FAILURE;
        }
        if (iwrite(img, dip, buf, n, off) != n) {
            error("cp: %s: write error\n", dpath);
            return EXIT_FAILURE;
        }
    }
    
    return EXIT_SUCCESS;
}

// mv src_path dest_path
int do_mv(img_t img, int argc, char *argv[]) {
    if (argc != 2) {
        error("usage: %s img_file mv spath dpath\n", progname);
        return EXIT_FAILURE;
    }
    char *spath = argv[0];
    char *dpath = argv[1];

    // source
    inode_t sip = ilookup(img, root_inode, spath);
    if (sip == NULL) {
        error("mv: %s: no such file or directory\n", spath);
        return EXIT_FAILURE;
    }
    if (sip == root_inode) {
        error("mv: %s: root directory\n", spath);
        return EXIT_FAILURE;
    }

    inode_t dip = ilookup(img, root_inode, dpath);
    char ddir[BUFSIZE];
    char *dname = splitpath(dpath, ddir, BUFSIZE);
    if (dip != NULL) {
        if (dip->type == T_DIR) {
            char *sname = splitpath(spath, NULL, 0);
            inode_t ip = dlookup(img, dip, sname, NULL);
            // ip : inode of dpath/sname
            if (ip != NULL) {
                if (ip->type == T_DIR) {
                    // override existing empty directory
                    if (sip->type != T_DIR) {
                        error("mv: %s: not a directory\n", spath);
                        return EXIT_FAILURE;
                    }
                    if (!emptydir(img, ip)) {
                        error("mv: %s/%s: not empty\n", ddir, sname);
                        return EXIT_FAILURE;
                    }
                    iunlink(img, dip, sname);
                    daddent(img, dip, sname, sip);
                    iunlink(img, root_inode, spath);
                    dmkparlink(img, dip, sip);
                    return EXIT_SUCCESS;
                }
                else if (ip->type == T_FILE) {
                    // override existing file
                    if (sip->type != T_FILE) {
                        error("mv: %s: directory or device\n", spath);
                        return EXIT_FAILURE;
                    }
                    iunlink(img, dip, sname);
                    daddent(img, dip, sname, sip);
                    iunlink(img, root_inode, spath);
                    return EXIT_SUCCESS;
                }
                else {
                    error("mv: %s: device\n", dpath);
                    return EXIT_FAILURE;
                }
            }
            else { // ip == NULL
                daddent(img, dip, sname, sip);
                iunlink(img, root_inode, spath);
                if (sip->type == T_DIR)
                    dmkparlink(img, dip, sip);
            }
        }
        else if (dip->type == T_FILE) {
            // override existing file
            if (sip->type != T_FILE) {
                error("mv: %s: not a file\n", spath);
                return EXIT_FAILURE;
            }
            iunlink(img, root_inode, dpath);
            inode_t ip = ilookup(img, root_inode, ddir);
            assert(ip != NULL && ip->type == T_DIR);
            daddent(img, ip, dname, sip);
            iunlink(img, root_inode, spath);
        }
        else { // dip->type == T_DEV
            error("mv: %s: device\n", dpath);
            return EXIT_FAILURE;
        }
    }
    else { // dip == NULL
        if (is_empty(dname)) {
            error("mv: %s: no such directory\n", dpath);
            return EXIT_FAILURE;
        }
        inode_t ip = ilookup(img, root_inode, ddir);
        if (ip == NULL) {
            error("mv: %s: no such directory\n", ddir);
            return EXIT_FAILURE;
        }
        if (ip->type != T_DIR) {
            error("mv: %s: not a directory\n", ddir);
            return EXIT_FAILURE;
        }
        daddent(img, ip, dname, sip);
        iunlink(img, root_inode, spath);
        if (sip->type == T_DIR)
            dmkparlink(img, ip, sip);
    }
    return EXIT_SUCCESS;
}

// ln src_path dest_path
int do_ln(img_t img, int argc, char *argv[]) {
    if (argc != 2) {
        error("usage: %s img_file ln spath dpath\n", progname);
        return EXIT_FAILURE;
    }
    char *spath = argv[0];
    char *dpath = argv[1];

    // source
    inode_t sip = ilookup(img, root_inode, spath);
    if (sip == NULL) {
        error("ln: %s: no such file or directory\n", spath);
        return EXIT_FAILURE;
    }
    if (sip->type != T_FILE) {
        error("ln: %s: is a directory or a device\n", spath);
        return EXIT_FAILURE;
    }

    // destination
    char ddir[BUFSIZE];
    char *dname = splitpath(dpath, ddir, BUFSIZE);
    inode_t dip = ilookup(img, root_inode, ddir);
    if (dip == NULL) {
        error("ln: %s: no such directory\n", ddir);
        return EXIT_FAILURE;
    }
    if (dip->type != T_DIR) {
        error("ln: %s: not a directory\n", ddir);
        return EXIT_FAILURE;
    }
    if (is_empty(dname)) {
        dname = splitpath(spath, NULL, 0);
        if (dlookup(img, dip, dname, NULL) != NULL) {
            error("ln: %s/%s: file exists\n", ddir, dname);
            return EXIT_FAILURE;
        }
    }
    else {
        inode_t ip = dlookup(img, dip, dname, NULL);
        if (ip != NULL) {
            if (ip->type != T_DIR) {
                error("ln: %s/%s: file exists\n", ddir, dname);
                return EXIT_FAILURE;
            }
            dname = splitpath(spath, NULL, 0);
            dip = ip;
        }
    }
    if (daddent(img, dip, dname, sip) < 0) {
        error("ln: %s/%s: cannot create a link\n", ddir, dname);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// mkdir path
int do_mkdir(img_t img, int argc, char *argv[]) {
    if (argc != 1) {
        error("usage: %s img_file mkdir path\n", progname);
        return EXIT_FAILURE;
    }
    char *path = argv[0];
    
    if (ilookup(img, root_inode, path) != NULL) {
        error("mkdir: %s: file exists\n", path);
        return EXIT_FAILURE;
    }
    if (icreat(img, root_inode, path, T_DIR, NULL) == NULL) {
        error("mkdir: %s: cannot create\n", path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// rmdir path
int do_rmdir(img_t img, int argc, char *argv[]) {
    if (argc != 1) {
        error("usage: %s img_file rmdir path\n", progname);
        return EXIT_FAILURE;
    }
    char *path = argv[0];
    
    inode_t ip = ilookup(img, root_inode, path);
    if (ip == NULL) {
        error("rmdir: %s: no such file or directory\n", path);
        return EXIT_FAILURE;
    }
    if (ip->type != T_DIR) {
        error("rmdir: %s: not a directory\n", path);
        return EXIT_FAILURE;
    }
    if (!emptydir(img, ip)) {
        error("rmdir: %s: non-empty directory\n", path);
        return EXIT_FAILURE;
    }
    if (iunlink(img, root_inode, path) < 0) {
        error("rmdir: %s: cannot unlink\n", path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


struct cmd_table_ent {
    char *name;
    char *args;
    int (*fun)(img_t, int, char **);
};

struct cmd_table_ent cmd_table[] = {
    { "diskinfo", "", do_diskinfo },
    { "info", "path", do_info },
    { "ls", "path", do_ls },
    { "get", "path", do_get },
    { "put", "path", do_put },
    { "rm", "path", do_rm },
    { "cp", "spath dpath", do_cp },
    { "mv", "spath dpath", do_mv },
    { "ln", "spath dpath", do_ln },
    { "mkdir", "path", do_mkdir },
    { "rmdir", "path", do_rmdir },
    { NULL, NULL }
};

static int exec_cmd(img_t img, char *cmd, int argc, char *argv[])
{
    for (int i = 0; cmd_table[i].name != NULL; i++) {
        if (strcmp(cmd, cmd_table[i].name) == 0)
            return cmd_table[i].fun(img, argc, argv);
    }
    error("unknown command: %s\n", cmd);
    return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
    progname = argv[0];
    if (argc < 3) {
        error("usage: %s img_file command [arg...]\n", progname);
        error("Commands are:\n");
        for (int i = 0; cmd_table[i].name != NULL; i++)
            error("    %s %s\n", cmd_table[i].name, cmd_table[i].args);
        return EXIT_FAILURE;
    }
    char *img_file = argv[1];
    char *cmd = argv[2];

    int img_fd = open(img_file, O_RDWR);
    if (img_fd < 0) {
        perror(img_file);
        return EXIT_FAILURE;
    }

    struct stat img_sbuf;
    if (fstat(img_fd, &img_sbuf) < 0) {
        perror(img_file);
        close(img_fd);
        return EXIT_FAILURE;
    }
    size_t img_size = (size_t)img_sbuf.st_size;

    img_t img = mmap(NULL, img_size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, img_fd, 0);
    if (img == MAP_FAILED) {
        perror(img_file);
        close(img_fd);
        return EXIT_FAILURE;
    }

    root_inode = iget(img, root_inode_number);

    // shift argc and argv to point the first command argument
    int status = EXIT_FAILURE;
    if (setjmp(fatal_exception_buf) == 0)
        status = exec_cmd(img, cmd, argc - 3, argv + 3);

    munmap(img, img_size);
    close(img_fd);

    return status;
}
