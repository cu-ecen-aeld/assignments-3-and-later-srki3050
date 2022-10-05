/*
	Name		: Sricharan Kidambi S
	Subject	: Advanced Embedded Software Development
	File		: aesdsocket.c
	date		: 2nd October 2022
	references	: Discussed with Swapnil Ghonge in devicing a method to write bytes into the file.
			  beej's guide to socket programming	
*/

// Open a stream socket bound to port 9000, fail and exit(-1) on any failure.
// Listens for and accepts connection
// close the file descriptor both from the socket and accept to avoid memory leaks.
// consider shutting down

#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<errno.h>
#include<signal.h>
#include<syslog.h>
#include<arpa/inet.h>
#include<stdbool.h>

#define BUFFER_SIZE 1024

int sockfd;
int fd;
/* Perform Signal handler on SIGINT and SIGTERM */
// Obtained guidance from Swapnil Ghonge on how to device this handler function
void signal_handler(int signo)
{
    printf("%d signal caught", signo);
    if ((signo == SIGINT) || (signo == SIGTERM)) {
        close(fd);
        close(sockfd);
        remove("/var/tmp/aesdsocketdata");
        exit(0);
    }
}

int main(int argc, char **argv){

char buffer[BUFFER_SIZE] = {0};

signal(SIGINT, signal_handler);
signal(SIGTERM, signal_handler);
/******************************************************************************************** Create the Socket **************************************************************************************/
syslog(LOG_USER, "Socket Creation");
sockfd = socket(AF_INET,SOCK_STREAM,0);
if(sockfd == -1){
   perror("Socket Not created Properly");
   exit(-1);
}
/******************************************************************************************** Fill in the Data Structure *****************************************************************************/
struct sockaddr_in server;
memset((void *)&server, 0, sizeof(server));
server.sin_family = AF_INET;
server.sin_addr.s_addr = INADDR_ANY;
server.sin_port = htons(9000);
/***************************************************************************************** Bind the client with the server ***************************************************************************/
int yes = 1;
if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
	perror("Unable to Setup reusing ability of the socket port and ip");
	exit(-1);
}

int sockbind = bind(sockfd, (struct sockaddr *)&server, sizeof(server));
if(sockbind == -1){
	close(sockfd);
	perror("Unable to bind Properly");
	exit(-1);
}
syslog(LOG_USER, "Binded Connection from 9000");
/*********************************************************************************** Perform the Daemonising Process *********************************************************************************/
// The daemon process by accepting a statement -d as a command line argument to this file.
/* Daemon Creation Process
	Create the child process using fork
	setsid() - set an Id and remove as a parent
	change directory to /dev/null using chdir()
	redirect to STDIN, STDOUT, and STDERR
*/
if(argc > 1){
	if(strcmp(argv[1],"-d") == 0){
		pid_t pid = fork();	//Create a child process using fork
		if(pid < 0){
			perror("Child Process not created: Daemon Process failed to create in step 1");
			exit(-1);
		}
		else if(pid == 0){
			if(setsid() < 0){
				perror("Unable to set Id");
				exit(-1);
			}
			
			if(chdir("/") == -1){
				perror("Unable to change directory");
				exit(-1);
			}
			open("/dev/null",O_RDWR);
			dup(0);
			dup(0);
			syslog(LOG_USER,"Daemon Created Successfully");
		}
		else{
			exit(0);
		}
	}
}
/***************************************************************************************** Listening to the Server ***********************************************************************************/
int listener = listen(sockfd, 10);
if(listener < 0){											// Backlog value set as 10 as prescribed in the Beej's user guide on sockets.
	perror("Failed to execute in the listening process");
	return -1;
}
syslog(LOG_USER, "Listening phase completed");
fd = open("/var/tmp/aesdsocketdata",O_RDWR|O_CREAT|O_APPEND, 0777);
if(fd == -1){
	perror("Unable to open the file");
}
syslog(LOG_USER,"File successfully opened");
/*************************************************************************************************** Accepting packets *******************************************************************************/
int bytes_received;
int buffer_size = BUFFER_SIZE;
struct sockaddr_in addr_client;
socklen_t length;
int newfd;
int total_bytes_written_so_far = 0;
while(1){
	length = sizeof(addr_client);
	newfd = accept(sockfd, (struct sockaddr *)&addr_client,&length);
	if(newfd == -1){
		perror("Unable to Accept connection from 9000");
		exit(-1);
	}
	syslog(LOG_USER, "Accepted connection from 9000");
	
	bool packet_in_progress = true;
	int space_occupied_buffer = 0;					// Store the total occupied bytes in memory
	char *write_buffer = malloc(sizeof(char) * BUFFER_SIZE);		// allocate space from the heap memory

        while(packet_in_progress)
        {
            bytes_received = recv(newfd, buffer, BUFFER_SIZE, 0);
            if (bytes_received == 0)
            {
                packet_in_progress = false;
            }
            int bytes = 0;
            while (bytes ++ < bytes_received)
            {
                if (buffer[bytes] == '\n')
                {
                    packet_in_progress = false;
                }
            }
            if ((buffer_size - space_occupied_buffer) < bytes_received)
            {
                buffer_size += bytes_received;
                write_buffer = (char *) realloc(write_buffer, sizeof(char) * buffer_size);
            }
            memcpy(write_buffer + space_occupied_buffer, buffer, bytes_received);
            space_occupied_buffer += bytes_received;
        }
        
        if(packet_in_progress == false)
        {
            packet_in_progress = true;
          
            int nbytes = write(fd, write_buffer, space_occupied_buffer);
            if (nbytes == -1)
            {
                perror("error writing to file");
                exit(-1);
            }
            total_bytes_written_so_far += nbytes;

            lseek(fd, 0, SEEK_SET);

            char *read_buffer = (char *) malloc(sizeof(char) * total_bytes_written_so_far);
           
            ssize_t read_bytes = read(fd, read_buffer, total_bytes_written_so_far);
            if (read_bytes == -1)
            {
                perror("error reading from file");
                exit(-1);
            }

            int send_bytes = send(newfd, read_buffer, read_bytes, 0);
            if (send_bytes == -1)
            {
                perror("error sending to client");
                exit(-1);
            }
            
            free(read_buffer);
            free(write_buffer);

        }
        close(newfd);
        syslog(LOG_DEBUG,"Connection Shutdown %s\n",inet_ntoa(addr_client.sin_addr));  
    }
    close(sockbind);
    return 0;
}
