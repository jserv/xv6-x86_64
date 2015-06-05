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

/* img file structure
 *
 *    0    1    2         m-1   m         d-1   d        l-1    l         N-1
 * +----+----+----+-...-+----+----+-...-+----+----+-...-+----+----+-...-+----+
 * | BB | SB | IB | ... | IB | MB | ... | MB | DB | ... | DB | LB | ... | LB |
 * +----+----+----+-...-+----+----+-...-+----+----+-...-+----+----+-...-+----+
 *
 *           |<---- Ni ----->|<---- Nm ----->|<---- Nd ----->|<---- Nl ----->|
 *
 * BB: boot block   [0, 0]
 * SB: super block  [1, 1]
 * IB: inode block  [2, 2 - 1 + Ni]
 * MB: bitmap block [m, m - 1 + Nm]   (m = Nb + Ns + Ni)
 * DB: data block   [d, d - 1 + Nd]   (d = Nb + Ns + Ni + Nm)
 * LB: log block    [l, l - 1 + Nl]   (l = Nb + Ns + Ni + Nm + Nd = N - Nl)
 *
 * N = sb.size = Nb + Ns + Ni + Nm + Nd + Nl          (# of all blocks)
 * Nb = 1                                             (# of boot block)
 * Ns = 1                                             (# of super block)
 * Ni = sb.ninodes / IPB + 1                          (# of inode blocks)
 * Nm = N / (BSIZE * 8) + 1                           (# of bitmap blocks)
 * Nd = sb.nblocks = N - (Nb + Ns + Ni + Nm + Nl)     (# of data blocks)
 * Nl = sb.nlog                                       (# of log blocks)
 *
 * BSIZE = 512
 * IPB = BSIZE / sizeof(struct dinode) = 512 / 64 = 8
 *
 * Example: fs.img
 * BB: boot block   [0, 0]      = [0x00000000, 0x000001ff]
 * SB: super block  [1, 1]      = [0x00000200, 0x000003ff]
 * IB: inode block  [2, 27]     = [0x00000400, 0x000037ff]
 * MB: bitmap block [28, 28]    = [0x00003800, 0x000039ff]
 * DB: data block   [29, 993]   = [0x00003a00, 0x0007c3ff]
 * LB: log block    [994, 1023] = [0x0007c400, 0x0007ffff]
 *
 * N  = 1024
 * Ni = 200 / 8 + 1 = 26
 * Nm = 1024 / (512 * 8) + 1 = 1
 * Nd = sb.nblocks = 1024 - (1 + 1 + 26 + 1 + 30) = 965
 * Nl = sb.nlog = 30
 *
 */

/* dinode structure
 *
 * |<--- 32 bit ---->|
 * 
 * +--------+--------+
 * |  type  |  major |  file type, major device number [ushort * 2]
 * +--------+--------+
 * |  minor |  nlink |  minor device number, # of links [ushort * 2]
 * +--------+--------+
 * |      size       |  size of the file (bytes) [uint]
 * +-----------------+  \
 * |    addrs[0]     |  |
 * +-----------------+  |
 * |       :         |   > direct data block addresses (NDIRECT=12) [uint]
 * +-----------------+  |
 * | addrs[NDIRECT-1]|  |
 * +-----------------+  /
 * 
 */


/*
 * General mathematical functions
 */

static inline int min(int x, int y)
{
    return x < y ? x : y;
}

static inline int max(int x, int y)
{
    return x > y ? x : y;
}

// ceiling(x / y) where x >=0, y >= 0
static inline int divceil(int x, int y)
{
    return x == 0 ? 0 : (x - 1) / y + 1;
}

// the number of 1s in a 32-bit unsigned integer
uint bitcount(uint x)
{
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0f0f0f0f;
    x = x + (x >> 8);
    x = x + (x >> 16);
    return x & 0x3f;
}


/*
 * Debugging and reporting functions and macros
 */

// program name
char *progname;
jmp_buf fatal_exception_buf;

void debug_message(const char *tag, const char *fmt, ...)
{
#ifndef NDEBUG
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s: ", tag);
    vfprintf(stderr, fmt, args);
    va_end(args);
#endif
}

void error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void fatal(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "FATAL: ");
    vfprintf(stderr, fmt, args);
    va_end(args);
    longjmp(fatal_exception_buf, 1);
}

char *typename(int type)
{
    switch (type) {
    case T_DIR:
        return "directory";
    case T_FILE:
        return "file";
    case T_DEV:
        return "device";
    default:
        return "unknown";
    }
}


/*
 * Basic operations on blocks
 */

// checks if b is a valid data block number
bool valid_data_block(img_t img, uint b)
{
    const uint Ni = SBLK(img)->ninodes / IPB + 1;       // # of inode blocks
    const uint Nm = SBLK(img)->size / (BSIZE * 8) + 1;  // # of bitmap blocks
    const uint Nd = SBLK(img)->nblocks;                 // # of data blocks
    const uint d = 2 + Ni + Nm;                         // 1st data block number
    return d <= b && b <= d + Nd - 1;
}

// allocates a new data block and returns its block number
uint balloc(img_t img)
{
    for (int b = 0; b < SBLK(img)->size; b += BPB) {
        uchar *bp = img[BBLOCK(b, SBLK(img)->ninodes)];
        for (int bi = 0; bi < BPB && b + bi < SBLK(img)->size; bi++) {
            int m = 1 << (bi % 8);
            if ((bp[bi / 8] & m) == 0) {
                bp[bi / 8] |= m;
                if (!valid_data_block(img, b + bi)) {
                    fatal("balloc: %u: invalid data block number\n", b + bi);
                    return 0; // dummy
                }
                memset(img[b + bi], 0, BSIZE);
                return b + bi;
            }
        }
    }
    fatal("balloc: no free blocks\n");
    return 0; // dummy
}

// frees the block specified by b
int bfree(img_t img, uint b)
{
    if (!valid_data_block(img, b)) {
        derror("bfree: %u: invalid data block number\n", b);
        return -1;
    }
    uchar *bp = img[BBLOCK(b, SBLK(img)->ninodes)];
    int bi = b % BPB;
    int m = 1 << (bi % 8);
    if ((bp[bi / 8] & m) == 0)
        dwarn("bfree: %u: already freed block\n", b);
    bp[bi / 8] &= ~m;
    return 0;
}


/*
 * Basic operations on files (inodes)
 */

// inode of the root directory
const uint root_inode_number = 1;
inode_t root_inode;

// returns the pointer to the inum-th dinode structure
inode_t iget(img_t img, uint inum)
{
    if (0 < inum && inum < SBLK(img)->ninodes)
        return (inode_t)img[IBLOCK(inum)] + inum % IPB;
    derror("iget: %u: invalid inode number\n", inum);
    return NULL;
}

// retrieves the inode number of a dinode structure
uint geti(img_t img, inode_t ip)
{
    uint Ni = SBLK(img)->ninodes / IPB + 1;       // # of inode blocks
    for (int i = 0; i < Ni; i++) {
        inode_t bp = (inode_t)img[i + 2];
        if (bp <= ip && ip < bp + IPB)
            return ip - bp + i * IPB;
    }
    derror("geti: %p: not in the inode blocks\n", ip);
    return 0;
}

// allocate a new inode structure
inode_t ialloc(img_t img, uint type)
{
    for (int inum = 1; inum < SBLK(img)->ninodes; inum++) {
        inode_t ip = (inode_t)img[IBLOCK(inum)] + inum % IPB;
        if (ip->type == 0) {
            memset(ip, 0, sizeof(struct dinode));
            ip->type = type;
            return ip;
        }
    }
    fatal("ialloc: cannot allocate\n");
    return NULL;
}

// frees inum-th inode
int ifree(img_t img, uint inum)
{
    inode_t ip = iget(img, inum);
    if (ip == NULL)
        return -1;
    if (ip->type == 0)
        dwarn("ifree: inode #%d is already freed\n", inum);
    if (ip->nlink > 0)
        dwarn("ifree: nlink of inode #%d is not zero\n", inum);
    ip->type = 0;
    return 0;
}

// returns n-th data block number of the file specified by ip
uint bmap(img_t img, inode_t ip, uint n)
{
    if (n < NDIRECT) {
        uint addr = ip->addrs[n];
        if (addr == 0) {
            addr = balloc(img);
            ip->addrs[n] = addr;
        }
        return addr;
    }
    else {
        uint k = n - NDIRECT;
        if (k >= NINDIRECT) {
            derror("bmap: %u: invalid index number\n", n);
            return 0;
        }
        uint iaddr = ip->addrs[NDIRECT];
        if (iaddr == 0) {
            iaddr = balloc(img);
            ip->addrs[NDIRECT] = iaddr;
        }
        uint *iblock = (uint *)img[iaddr];
        if (iblock[k] == 0)
            iblock[k] = balloc(img);
        return iblock[k];
    }
}

// reads n byte of data from the file specified by ip
int iread(img_t img, inode_t ip, uchar *buf, uint n, uint off)
{
    if (ip->type == T_DEV)
        return -1;
    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > ip->size)
        n = ip->size - off;
    // t : total bytes that have been read
    // m : last bytes that were read
    uint t = 0;
    for (uint m = 0; t < n; t += m, off += m, buf += m) {
        uint b = bmap(img, ip, off / BSIZE);
        if (!valid_data_block(img, b)) {
            derror("iread: %u: invalid data block\n", b);
            break;
        }
        m = min(n - t, BSIZE - off % BSIZE);
        memmove(buf, img[b] + off % BSIZE, m);
    }
    return t;
}

// writes n byte of data to the file specified by ip
int iwrite(img_t img, inode_t ip, uchar *buf, uint n, uint off)
{
    if (ip->type == T_DEV)
        return -1;
    if (off > ip->size || off + n < off || off + n > MAXFILESIZE)
        return -1;
    // t : total bytes that have been written
    // m : last bytes that were written
    uint t = 0;
    for (uint m = 0; t < n; t += m, off += m, buf += m) {
        uint b = bmap(img, ip, off / BSIZE);
        if (!valid_data_block(img, b)) {
            derror("iwrite: %u: invalid data block\n", b);
            break;
        }
        m = min(n - t, BSIZE - off % BSIZE);
        memmove(img[b] + off % BSIZE, buf, m);
    }
    if (t > 0 && off > ip->size)
        ip->size = off;
    return t;
}

// truncate the file specified by ip to size
int itruncate(img_t img, inode_t ip, uint size)
{
    if (ip->type == T_DEV)
        return -1;
    if (size > MAXFILESIZE)
        return -1;

    if (size < ip->size) {
        int n = divceil(ip->size, BSIZE);  // # of used blocks
        int k = divceil(size, BSIZE);      // # of blocks to keep
        int nd = min(n, NDIRECT);          // # of used direct blocks
        int kd = min(k, NDIRECT);          // # of direct blocks to keep
        for (int i = kd; i < nd; i++) {
            bfree(img, ip->addrs[i]);
            ip->addrs[i] = 0;
        }

        if (n > NDIRECT) {
            uint iaddr = ip->addrs[NDIRECT];
            assert(iaddr != 0);
            uint *iblock = (uint *)img[iaddr];
            int ni = max(n - NDIRECT, 0);  // # of used indirect blocks
            int ki = max(k - NDIRECT, 0);  // # of indirect blocks to keep
            for (uint i = ki; i < ni; i++) {
                bfree(img, iblock[i]);
                iblock[i] = 0;
            }
            if (ki == 0) {
                bfree(img, iaddr);
                ip->addrs[NDIRECT] = 0;
            }
        }
    }
    else {
        int n = size - ip->size; // # of bytes to be filled
        for (uint off = ip->size, t = 0, m = 0; t < n; t += m, off += m) {
            uchar *bp = img[bmap(img, ip, off / BSIZE)];
            m = min(n - t, BSIZE - off % BSIZE);
            memset(bp + off % BSIZE, 0, m);
        }
    }
    ip->size = size;
    return 0;
}


/*
 * Pathname handling functions
 */

// check if s is an empty string
bool is_empty(char *s)
{
    return *s == 0;
}

// check if c is a path separator
bool is_sep(char c)
{
    return c == '/';
}

// adapted from skipelem in xv6/fs.c
char *skipelem(char *path, char *name)
{
    while (is_sep(*path))
        path++;
    char *s = path;
    while (!is_empty(path) && !is_sep(*path))
        path++;
    int len = min(path - s, DIRSIZ);
    memmove(name, s, len);
    if (len < DIRSIZ)
        name[len] = 0;
    return path;
}

// split the path into directory name and base name
char *splitpath(char *path, char *dirbuf, uint size)
{
    char *s = path, *t = path;
    while (!is_empty(path)) {
        while (is_sep(*path))
            path++;
        s = path;
        while (!is_empty(path) && !is_sep(*path))
            path++;
    }
    if (dirbuf != NULL) {
        int n = min(s - t, size - 1);
        memmove(dirbuf, t, n);
        dirbuf[n] = 0;
    }
    return s;
}

/*
 * Operations on directories
 */

// search a file (name) in a directory (dp)
inode_t dlookup(img_t img, inode_t dp, char *name, uint *offp)
{
    assert(dp->type == T_DIR);
    struct dirent de;
    for (uint off = 0; off < dp->size; off += sizeof(de)) {
        if (iread(img, dp, (uchar *)&de, sizeof(de), off) != sizeof(de)) {
            derror("dlookup: %s: read error\n", name);
            return NULL;
        }
        if (strncmp(name, de.name, DIRSIZ) == 0) {
            if (offp != NULL)
                *offp = off;
            return iget(img, de.inum);
        }
    }
    return NULL;
}

// add a new directory entry in dp
int daddent(img_t img, inode_t dp, char *name, inode_t ip)
{
    struct dirent de;
    uint off;
    // try to find an empty entry
    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (iread(img, dp, (uchar *)&de, sizeof(de), off) != sizeof(de)) {
            derror("daddent: %u: read error\n", geti(img, dp));
            return -1;
        }
        if (de.inum == 0)
            break;
        if (strncmp(de.name, name, DIRSIZ) == 0) {
            derror("daddent: %s: exists\n", name);
            return -1;
        }
    }
    strncpy(de.name, name, DIRSIZ);
    de.inum = geti(img, ip);
    if (iwrite(img, dp, (uchar *)&de, sizeof(de), off) != sizeof(de)) {
        derror("daddent: %u: write error\n", geti(img, dp));
        return -1;
    }
    if (strncmp(name, ".", DIRSIZ) != 0)
        ip->nlink++;
    return 0;
}

// create a link to the parent directory
int dmkparlink(img_t img, inode_t pip, inode_t cip)
{
    if (pip->type != T_DIR) {
        derror("dmkparlink: %d: not a directory\n", geti(img, pip));
        return -1;
    }
    if (cip->type != T_DIR) {
        derror("dmkparlink: %d: not a directory\n", geti(img, cip));
        return -1;
    }
    uint off;
    dlookup(img, cip, "..", &off);
    struct dirent de;
    de.inum = geti(img, pip);
    strncpy(de.name, "..", DIRSIZ);
    if (iwrite(img, cip, (uchar *)&de, sizeof(de), off) != sizeof(de)) {
        derror("dmkparlink: write error\n");
        return -1;
    }
    pip->nlink++;
    return 0;
}


// returns the inode number of a file (rp/path)
inode_t ilookup(img_t img, inode_t rp, char *path)
{
    char name[DIRSIZ + 1];
    name[DIRSIZ] = 0;
    while (true) {
        assert(path != NULL && rp != NULL && rp->type == T_DIR);
        path = skipelem(path, name);
        // if path is empty (or a sequence of path separators),
        // it should specify the root direcotry (rp) itself
        if (is_empty(name))
            return rp;

        inode_t ip = dlookup(img, rp, name, NULL);
        if (ip == NULL)
            return NULL;
        if (is_empty(path))
            return ip;
        if (ip->type != T_DIR) {
            derror("ilookup: %s: not a directory\n", name);
            return NULL;
        }
        rp = ip;
    }
}

// create a file
inode_t icreat(img_t img, inode_t rp, char *path, uint type, inode_t *dpp)
{
    char name[DIRSIZ + 1];
    name[DIRSIZ] = 0;
    while (true) {
        assert(path != NULL && rp != NULL && rp->type == T_DIR);
        path = skipelem(path, name);
        if (is_empty(name)) {
            derror("icreat: %s: empty file name\n", path);
            return NULL;
        }

        inode_t ip = dlookup(img, rp, name, NULL);
        if (is_empty(path)) {
            if (ip != NULL) {
                derror("icreat: %s: file exists\n", name);
                return NULL;
            }
            ip = ialloc(img, type);
            daddent(img, rp, name, ip);
            if (ip->type == T_DIR) {
                daddent(img, ip, ".", ip);
                daddent(img, ip, "..", rp);
            }
            if (dpp != NULL)
                *dpp = rp;
            return ip;
        }
        if (ip == NULL || ip->type != T_DIR) {
            derror("icreat: %s: no such directory\n", name);
            return NULL;
        }
        rp = ip;
    }
}

// checks if dp is an empty directory
bool emptydir(img_t img, inode_t dp)
{
    int nent = 0;
    struct dirent de;
    for (uint off = 0; off < dp->size; off += sizeof(de)) {
        iread(img, dp, (uchar *)&de, sizeof(de), off);
        if (de.inum != 0)
            nent++;
    }
    return nent == 2;
}

// unlinks a file (dp/path)
int iunlink(img_t img, inode_t rp, char *path)
{
    char name[DIRSIZ + 1];
    name[DIRSIZ] = 0;
    while (true) {
        assert(path != NULL && rp != NULL && rp->type == T_DIR);
        path = skipelem(path, name);
        if (is_empty(name)) {
            derror("iunlink: empty file name\n");
            return -1;
        }
        uint off;
        inode_t ip = dlookup(img, rp, name, &off);
        if (ip != NULL && is_empty(path)) {
            if (strncmp(name, ".", DIRSIZ) == 0 ||
                strncmp(name, "..", DIRSIZ) == 0) {
                derror("iunlink: cannot unlink \".\" or \"..\"\n");
                return -1;
            }
            // erase the directory entry
            uchar zero[sizeof(struct dirent)];
            memset(zero, 0, sizeof(zero));
            if (iwrite(img, rp, zero, sizeof(zero), off) != sizeof(zero)) {
                derror("iunlink: write error\n");
                return -1;
            }
            if (ip->type == T_DIR && dlookup(img, ip, "..", NULL) == rp)
                rp->nlink--;
            ip->nlink--;
            if (ip->nlink == 0) {
                if (ip->type != T_DEV)
                    itruncate(img, ip, 0);
                ifree(img, geti(img, ip));
            }
            return 0;
        }
        if (ip == NULL || ip->type != T_DIR) {
            derror("iunlink: %s: no such directory\n", name);
            return -1;
        }
        rp = ip;
    }
}
