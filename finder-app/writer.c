// Name 	: Sricharan
// File 	: Writer.c
// Brief	: Perform a write function that writes a string to the 			  specified file name.
// course	: ECEN 5713 Advanced Embedded Software Development
// Date	: 4th September 2022

#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<syslog.h>
#include<stdio.h>
#include<unistd.h>
// Argument - 1 = file name with path
// Argument - 2 = string name to be added to the file name
int main(int argc, char **argv){
  // Syslog is logger statement used to log and not as time consuming as 	 printf
  syslog(LOG_USER,"File Writing");
  /* Arguments should be 3, 
     1 - current file path (default)
     2 - Expected file with the file path (1st explicit argument)
     3 - Expected string to added inside the file
  */   
  if(argc != 3){
     syslog(LOG_ERR,"Wrong number of arguments");
     return 1;
  }
  int fd;
  // the below command creates a file if it is not present there
  fd = open(argv[1],O_WRONLY | O_CREAT | O_TRUNC,0644);
  // Return on exit condition 1 if file cannot be created.
  if(fd == -1){
     syslog(LOG_ERR,"Unable to find or open the file, invalid file descriptor");
     return 1;
  }
  syslog(LOG_USER,"File opened");
  ssize_t nr;
  syslog(LOG_DEBUG,"Writing %s to file %s",argv[2],argv[1]);
  // Write the string to the file
  nr = write(fd,argv[2],strlen(argv[2]));
  if(nr == -1){
  	syslog(LOG_ERR,"String not written to the file");
  	return 1;
  }
  syslog(LOG_USER,"File Written");
  // Syslog will implicitly create an open log which is safe to be closed
  closelog();
}
