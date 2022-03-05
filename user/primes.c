#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void child(int fd[2]) {
  int prime;
  close(fd[1]);
  read(fd[0], &prime, 4);
  printf("prime %d\n", prime);
  
  int t;
  int f = read(fd[0], &t, 4);

  if(f) {
    int nfd[2];
    pipe(nfd);
    int tpid = fork();
    if(tpid == 0){
      child(nfd);
    }else {
      close(nfd[0]);
      if(t%prime) 
        write(nfd[1], &t, 4);
      while(read(fd[0], &t, 4)) 
	if(t%prime) write(nfd[1], &t, 4);
    
      close(fd[0]);
      close(nfd[1]);
      wait(0);
    }

  }
  exit(0);
}


int main(int argc, char* argv[]) {
  int fd[2];
  pipe(fd);

  int pid = fork();
  if(pid == 0) {
	child(fd);
  }else {
    close(fd[0]);
    for(int i = 2; i < 35; i++) {
	write(fd[1], &i, 4);
    }
    close(fd[1]);
    wait(0);
    exit(0);
  }

  return 0;
}
