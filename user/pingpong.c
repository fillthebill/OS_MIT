#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main () {

  char b[512];
  int fd1[2], fd2[2];
  
  pipe(fd1);
  pipe(fd2);

  int pid = fork();
  if(pid < 0) {
    fprintf(2, "fork error\n");
    exit(-1);
  }

  if(pid == 0) {
    int chpid = getpid();
    read(fd1[1], b, 1);
    close(fd1[1]);
    printf("%d, received ping\n", chpid);
    write(fd2[0], b, 1);
    exit(0);
  }else {
    int papid = getpid();
    write(fd1[0], b, 1);
    close(fd1[0]);
    read(fd2[1], b, 1);
    wait(0);
    printf("%d, received pong\n", papid);
    
  exit(0);
  }



}
