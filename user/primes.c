#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void launch_sieve(int p, int* leftpipe){
    int rightpipe[2];
    pipe(rightpipe);
    int first = 1;
    int value;
    int childpid = 0;
    // int c = 0;
    close(leftpipe[1]); // never going to write here 
    while (read(leftpipe[0], (char*) &value, 4) > 0){ // getting stuff from left pipe, if it is 0 the pipe was closed and there was not 4 bytes to be read
        if (value % p){
            if (first){
                childpid = fork();
                if (childpid < 0){
                    fprintf(2, "fork did not work");
                    break;
                }
                else if (childpid == 0){
                    // fprintf(1, "%d\n", value);
                    if (close(leftpipe[0]) < 0){ // never going to use at all 
                        fprintf(2, "problem closing1 %d\n", value);
                        break;
                    }
                    leftpipe[0] = rightpipe[0];
                    leftpipe[1] = rightpipe[1];
                    if (close(leftpipe[1]) < 0){
                        fprintf(2, "problem closing2");
                        break;
                    } 
                    pipe(rightpipe);
                    p = value;
                }
                else first = 0;
            }
            else {
                int x = write(rightpipe[1], (char*) &value, 4);
                if (x < 0){
                    fprintf(2, "problem writing");
                    break;
                }
            }
        }
    }
    fprintf(1, "prime %d\n", p);
    close(leftpipe[0]);
    if (childpid){
        close(rightpipe[1]);
        wait(&childpid);
    }
}

int
main(int argc, char *argv[]) // argv is an array of pointers to string arguments 
{
    int p1[2];
    int p = 2;
    pipe(p1);
    int first = 1;
    int childpid;
    for (int i = p+1; i <= 35; i++){
        if (i % p){
            if (first){
                childpid = fork();
                if (childpid == 0) {
                    launch_sieve(i, p1);
                    break;
                }
                else first = 0;
            }
            else {
                // send along p1
                write(p1[1], (char*) &i, 4); // not an async function, will copy the bytes here into a buffer no matter how long it takes
                // do not have to worry about that address chanigng value stored later 
            }
            
        }
    }
    if (childpid){
        fprintf(1, "prime %d\n", p); // important you run this before closing the file descriptor
        // child process will definitely wait until you do, but might continue executing after 
        close(p1[1]);
        wait(&childpid);
    }
    exit(0);
}
