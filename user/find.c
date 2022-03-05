#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void helper(char* path, char* target) {
  char buf[512], *p;
  int fd;
  struct dirent d;
  struct stat s;
  
  fd = open(path, 0);
  fstat(fd, &s);
  switch(s.type) {
   case T_FILE:
  	printf("should be used to find dir");  
        exit(1);

   case T_DIR:
     strcpy(buf, path);
     p = buf + strlen(buf);
     *p++ = '/';
    while(read(fd, &d, sizeof(d)) == sizeof(d)) {
	if(d.inum == 0 || strcmp(d.name, ".")==0 || strcmp(d.name, "..") ==0)
	  continue;
	memmove(p, d.name, DIRSIZ);
 	p[DIRSIZ] = 0;
	if(stat(buf, &s) < 0) {
	  continue; // cann st
	}
    
    if(s.type == T_DIR) {
	helper(buf, target);
    }else if(s.type == T_FILE) {
	if(strcmp(target, d.name) == 0)
	  printf("%s\n", buf);
    }
   }
    break;
  }
  close(fd);
}

int 
main(int argc, char* argv[]) 
{
  if(argc != 3){
	printf("invalid arg\n");
	exit(1);
  }

  char* path = argv[1];
  char* t = argv[2];
  helper(path, t);
  exit(0);
}
