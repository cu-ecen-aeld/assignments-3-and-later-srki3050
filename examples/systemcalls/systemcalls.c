/*Name		: Sricharan Kidambi
  Course	: ECEN 5713 Advanced Embedded Software Development
  File		: systemcalls.c
  Brief	: perform system calls and test it in multiple ways
  	          so as to create a child process, execute commands in it 			  and wait for termination.
  Date		: 11th September 2022
  References	: Linux system programming by robert love page 160 and 161
  		  https://stackoverflow.com/a/13784315/1446624
*/

#define _XOPEN_SOURCE
#include "systemcalls.h"
#include "sys/types.h"
#include "sys/wait.h"
#include "unistd.h"
#include "stdlib.h"
#include "string.h"
#include "fcntl.h"
#include "stdio.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    //int system(const char *cmd) - used to do fork(), exec() and wait() 	call in a single statement.
    int sys_call = system(cmd);
    if(sys_call == -1){
    	return false;
    }
    else{
    	return true;
    }	
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    // fork() - creates a new process and returns its process id
    pid_t pid = fork (); 
    if (pid == -1){
	return false;
    }
    // execv() - Loads the binary image to the created process and starts execution
    else if (pid == 0){
	if (execv (command[0], command) == -1){
		perror("Exec call failed");
		exit(-1);
	}
    }
    int status;
    // wait() = waits until the child process gets terminated
    int call_wait = waitpid(pid,&status,0);
    if(call_wait == -1){
    	perror("waitpid call failed");
    	return false;
    }
    // WIFEXITED returns true if process terminated normally
    if (WIFEXITED(status)){
	if (WEXITSTATUS(status) != 0)
		return false;
    else
	return true;
    }
    va_end(args);
    
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    int kidpid;
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) { perror("open"); abort(); }
    switch (kidpid = fork()) {
  	case -1: perror("fork"); abort();
  	case 0:
    		if (dup2(fd, 1) < 0) { perror("dup2"); abort(); }
    		close(fd);
    		int call_to_exec = execvp(command[0], command);
    		if(call_to_exec == -1){
    			perror("execvp"); abort();
    		}	
  	default:
    		close(fd);
    		int status;
   		int call_wait = waitpid(kidpid,&status,0);
    		if(call_wait == -1){
    			perror("waitpid");
    			return false;
    		}
    		else if(WIFEXITED(status)){
    			return false;
    		}
    }
    va_end(args);

    return true;
}
