#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<syslog.h>
#include<stdio.h>
#include<unistd.h>
int main(int argc, char **argv){
  syslog(LOG_USER,"File Writing");
  if(argc != 3){
     syslog(LOG_ERR,"Wrong number of arguments");
     return 1;
  }
  int fd;
  fd = open(argv[1],O_WRONLY | O_CREAT | O_TRUNC,0644);
  if(fd == -1){
     syslog(LOG_ERR,"Unable to find or open the file, invalid file descriptor");
     return 1;
  }
  syslog(LOG_USER,"File opened");
  ssize_t nr;
  syslog(LOG_DEBUG,"Writing %s to file %s",argv[2],argv[1]);
  nr = write(fd,argv[2],strlen(argv[2]));
  if(nr == -1){
  	syslog(LOG_ERR,"String not written to the file");
  	return 1;
  }
  syslog(LOG_USER,"File Written");
  closelog();
}
