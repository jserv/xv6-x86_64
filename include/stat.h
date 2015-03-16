#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device

struct stat {
  short type;  // Type of file
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short nlink; // Number of links to file
  short ownerid; // The owner of the file
  short groupid; // The group owner of the file
  uint mode;     // The permissions mode of the file
  uint size;   // Size of file in bytes
};
