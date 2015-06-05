// xv6 header files
#include "../include/types.h"
#include "../include/fs.h"

// file types (copied from xv6/stat.h)
// Including xv6/stat.h causes a name clash (with struct stat)
#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device

#define MAXFILESIZE (MAXFILE * BSIZE)
#define BUFSIZE 1024

#define ddebug(...) debug_message("DEBUG", __VA_ARGS__)
#define derror(...) debug_message("ERROR", __VA_ARGS__)
#define dwarn(...) debug_message("WARNING", __VA_ARGS__)

extern char *progname;
extern jmp_buf fatal_exception_buf;

uint bitcount(uint x);

void debug_message(const char *tag, const char *fmt, ...);
void error(const char *fmt, ...);
void fatal(const char *fmt, ...);
char *typename(int type);

// a disk image as an array of blocks
// a block is a uchar array of size BSIZE
typedef uchar (*img_t)[BSIZE];

// super block
#define SBLK(img) ((struct superblock *)(img)[1])

bool valid_data_block(img_t img, uint b);
uint balloc(img_t img);
int bfree(img_t img, uint b);

// inode
typedef struct dinode *inode_t;

// inode of the root directory
extern const uint root_inode_number;
extern inode_t root_inode;

inode_t iget(img_t img, uint inum);
uint geti(img_t img, inode_t ip);

inode_t ialloc(img_t img, uint type);
int ifree(img_t img, uint inum);
uint bmap(img_t img, inode_t ip, uint n);
int iread(img_t img, inode_t ip, uchar *buf, uint n, uint off);
int iwrite(img_t img, inode_t ip, uchar *buf, uint n, uint off);
int itruncate(img_t img, inode_t ip, uint size);

bool is_empty(char *s);
bool is_sep(char c);
char *skipelem(char *path, char *name);
char *splitpath(char *path, char *dirbuf, uint size);

inode_t dlookup(img_t img, inode_t dp, char *name, uint *offp);
int daddent(img_t img, inode_t dp, char *name, inode_t ip);
int dmkparlink(img_t img, inode_t pip, inode_t cip);
inode_t ilookup(img_t img, inode_t rp, char *path);
inode_t icreat(img_t img, inode_t rp, char *path, uint type, inode_t *dpp);
bool emptydir(img_t img, inode_t dp);
int iunlink(img_t img, inode_t rp, char *path);
