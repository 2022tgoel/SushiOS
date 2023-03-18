#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int fd[2]; // both file descriptors point to the same pipe object, which has the data buffer in it
    // int fd2[2];
    int x = pipe(fd);
    int y = 0;
    // int y = pipe(fd2); 
    if (x < 0 || y < 0) {
        fprintf(2, "pipes could not open");
        exit(1);
    }
    int childpid = fork(); // when the process is forked, so is the ofile object that still has pointers to the same two file objects in the file table
    
    if (childpid == 0){
        // char smth = 'x';
        char c = 'a';
        read(fd[0], &c, 1); // read a single byte 
        fprintf(1, "%d: received ping\n", getpid());
        write(fd[1], "c", 1);
        
    }
    else {
        // char *c;
        char c = 'a';
        write(fd[1], "b", 1);
        read(fd[0], &c, 1);
        fprintf(1, "%d: received pong\n", getpid());
    }
    exit(0);
}
