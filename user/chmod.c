#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if (argc < 3) exit();

  int fd;
  struct stat st;
  int mode;

  char *path = argv[2];
  if ((fd = open(path, 0)) < 0) {
    printf(2, "chmod: cannot open %s\n", path);
    exit();
  }
  if (fstat(fd, &st) < 0) {
    printf(2, "chmod: cannot stat %s\n", path);
    close(fd);
    exit();
  }
  mode = st.mode;
  close(fd);

  if (strcmp(argv[1], "-x") == 0)
    chmod(path, 0x677 & mode);
  else if (strcmp(argv[1], "+x") == 0)
    chmod(path, 0x777 & mode);

  exit();
}
