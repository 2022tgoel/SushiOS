#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
find(const char* path, const char* file) {
  int len = 512;
  char *buf = malloc(len);
  char *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type != T_DIR) {
    fprintf(2, "find: argument is not a directory");
  }

  if(strlen(path) + 1 + DIRSIZ + 1 > len){
    fprintf(2, "find: path too long, %d\n", strlen(path));
  }

  strcpy(buf, path);
  p = buf+strlen(buf);
  *p++ = '/';
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      continue;
    if(de.inum == 0)
      continue;
    
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;

    if(stat(buf, &st) < 0){
      fprintf(2, "find: cannot stat %s\n", buf);
      continue;
    }
    if (st.type == T_DIR) {
      find(buf, file);
    } else if (st.type == T_FILE) {
      if (strcmp(de.name, file) == 0) {
        printf("%s\n", buf);
      }
    }
  }
  free(buf);
}

int
main(int argc, char *argv[])
{

  if(argc !=3){
    fprintf(2, "Usage: find...\n");
    exit(1);
  }
  char *dir = argv[1];
  char *file = argv[2];
  find(dir, file);
  exit(0);
}
