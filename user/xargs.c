#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAX 512

int main(int argc, char* argv[]) {
  char* args[MAXARG];
  char buf[MAX];
  int arg_index = 0;

  for(int i = 1; i < argc; ++i) {
    args[arg_index++] = argv[i];
  }

  int n;

  while( (n = read(0, buf, MAX) > 0) ){
    int pid = fork();
    if(pid == 0) {
	char* para = (char*) malloc(sizeof(buf));
	int index = 0;
	for(int i = 0; i < n; i++) {
	  if(buf[i] == ' ' || buf[i] == '\n') {
	    para[index] = 0;
	    args[arg_index++] = para;
	    index = 0;
	    para = (char*)malloc(sizeof(buf));
	  }else {
 	    para[index++] = buf[i];
	  }
	}
	args[arg_index] = 0;
	exec(args[0], args);
    }else {
      wait(0);
    }

  }
  exit(0);
}
