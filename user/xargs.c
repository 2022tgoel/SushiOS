#include "kernel/types.h"
#include "user/user.h"

char buf[512];

void
xargs(int fd, char *argv[], int argc)
{
  char line[512];
  int i, n;
  int j = 0;
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(i=0; i<n; i++){
      if(buf[i] == '\n'){ // reached the end of a line
        line[j++] = '\0'; j = 0;
        argv[argc] = line;
        argv[argc+1] = 0;
        // argv[argc + 2] = NULL;
        if(fork() == 0) {
          close(0);
          exec(argv[0], argv);
        }
      } else {
        line[j++] = buf[i];
      }
    }
  }
}

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "Usage: xargs...\n");
    exit(1);
  }

  xargs(0, argv+1, argc-1);
  exit(0);
}