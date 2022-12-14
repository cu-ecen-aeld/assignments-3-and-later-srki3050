/* Name	:	Sricharan Kidambi S
   File	:	aesdsocket.c
   Brief	:	Accept multiple socket connections using multithreading and include a timestamp function to add time stamp every 10 seconds
   References	:	Linux System Programming By Robert Love Chapter 10 - Signal Handler
   			https://github.com/stockrt/queue.h/blob/master/sample.c - Queue.h functions handbook
   			https://man7.org/linux/man-pages/man3/strftime.3.html   - Add timestamp
   			Discussed with Swapnil Ghonge on how to develop ideas to setup linked list functions in queue.h
   Notes	:	Understanding Changes for Assignment 9
   			1.	String sent to Socket AESDCHAR_IOCSEEKTO:X,Y where X and Y are unsigned decimal integer values.
   				X - Write Command to seek into, Y - Offset within write command
   			2.	These values are sent to AESDCHAR_SEEKTO ioctl
   				Then IOCTL command will perform before writes to device
   			3.	Read file and return to socket uses same file descriptor used to send to ioctl. So that file offset is honored read command.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>
#include <syslog.h>
#include <net/if.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/queue.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include "../aesd-char-driver/aesd_ioctl.h"
#define USE_AESD_CHAR_DEVICE 1
#define TIMESTAMP_SIZE 100
#ifdef USE_AESD_CHAR_DEVICE
	#define STORE_IN_THIS_FILE ("/dev/aesdchar")
#else
	#define STORE_IN_THIS_FILE ("/var/tmp/aesdsocketdata")
	#define TIMESTAMP_SIZE 50
#endif

#if USE_AESD_CHAR_DEVICE
const char *perform_ioctl = "AESDCHAR_IOCSEEKTO:";
#endif

int fd;
int sockfd;
pthread_mutex_t mutex_lock;
// To perform multithreaded applications, 
typedef struct node
{
	pthread_t thread;
	int clientfd;
	bool thread_complete_status;
	char client_ipaddress[INET6_ADDRSTRLEN];
	TAILQ_ENTRY(node) entries;
}thread_data_t;

typedef TAILQ_HEAD(head_s, node) head_t;
head_t head;
// Assignment instruction 2b - Append timestamp in 24hours format
// parameters 	: 	None
// Returns    	:	None
// References	:	https://geeksforgeeks.org/strftime-function-in-c/
void append_time_stamp()
{
	time_t t;
	struct tm *tmp;
	char MY_TIME[TIMESTAMP_SIZE];
	time(&t);
	tmp = localtime(&t);
	strftime(MY_TIME, sizeof(MY_TIME), "timestamp: %Y-%m-%d %H:%M:%S\r\n", tmp);
	printf("%s", MY_TIME);
	lseek(fd, 0, SEEK_END);						// Move the current file to the end
	int write_bytes_count = write(fd, MY_TIME, strlen(MY_TIME));
	if (write_bytes_count != strlen(MY_TIME)) {
		printf("write unsuccessful\n");
	}
}
// Remove all the memory to avoid memory leaks from valgrind checks, in this program, that occurs only during SIGINT, SIGTERM
// queue.h functions handbook line 187 - 192
void delete_all_the_memory()
{
	thread_data_t *datap;
	while(!TAILQ_EMPTY(&head))
	{
		datap = TAILQ_FIRST(&head);
		TAILQ_REMOVE(&head, datap, entries);
		free(datap);
	}
	pthread_mutex_destroy(&mutex_lock);
	exit(0);
}
// Signal handler function to terminate during SIGINT and SIGTERM and add timestamp every 10 seconds
void signal_handler(int signo)
{
	printf("%d signal caught", signo);
	// Call timestamp every 10 seconds despite other operations happening in parallel, discussed in robert love linux system programming chapter 10, Page 387
	if (signo == SIGALRM)
	{
		printf("SIGALRM ALERT\n");
		#ifndef USE_AESD_CHAR_DEVICE
			append_time_stamp();
		#endif
		alarm(10);
	}
	// In case a SIGINT or SIGTERM happens (pressing ctrl + c, graceful cleanup operation)
	// Perform program exit - As per assignment instructions 1.c
	if ((signo == SIGINT) || (signo == SIGTERM)) {
		printf("Caught signal %d\n", signo);
		close(fd);
		close(sockfd);
		remove(STORE_IN_THIS_FILE);
		delete_all_the_memory();
		exit (0);
	}
}
// Perform threading function which we will be handling upon every connection.
void * thread_function(void* thread_param)
{
	thread_data_t* data = (thread_data_t *)thread_param;
	bool packet_in_progress = true;
	bool need_to_realloc = false;
	char read_data, write_data;
	char *write_buffer = (char*)malloc(sizeof(char));
	int current_bytes = 0;
	data->thread_complete_status=false;
	fd = open(STORE_IN_THIS_FILE,O_RDWR|O_CREAT|O_APPEND, 0777);
	if(fd == -1){
		perror("Unable to open the file");
	}
	while(packet_in_progress)
	{
		if(need_to_realloc)
		{
			write_buffer = realloc(write_buffer, current_bytes+1);
			need_to_realloc = false;
		}
		int receive_bytes = recv(data->clientfd, &write_data, 1, 0);
		if(receive_bytes < 1)
		{
			printf("Error receiving bytes\n");
			perror("recv error\n");
		}
		if(receive_bytes == 1)
		{
			need_to_realloc = true;
			*(write_buffer + current_bytes) = write_data;
			current_bytes++;
		}
		if(write_data == '\n')
		{
			packet_in_progress = false;
		}
	}
	if(strncmp(write_buffer, perform_ioctl, strlen(perform_ioctl)) == 0) {
        	struct aesd_seekto seekto;
        	sscanf(write_buffer, "AESDCHAR_IOCSEEKTO:%d,%d", &seekto.write_cmd, &seekto.write_cmd_offset);
        	if(ioctl(fd, AESDCHAR_IOCSEEKTO, &seekto)) {
            		perror("ioctl failed.");
        	}
    	}
    	else{
	// Lock the program to prevent mutual sharing of resources by multiple threads simoultaneously
	pthread_mutex_lock(&mutex_lock);
	int write_bytes = write(fd, write_buffer, current_bytes);
	if(write_bytes != current_bytes){
		perror("write failed\n");
	}
	printf("write success\n");
	pthread_mutex_unlock(&mutex_lock);
	}
	// Once file is written move the file to current position
	//lseek(fd, 0, SEEK_SET);
	
	while(read(fd, &read_data, 1) != 0) {
	pthread_mutex_lock(&mutex_lock);
	int sent_status = send(data->clientfd, &read_data, 1, 0);
	if (sent_status == 0)
	{
		printf("send failed\n");
	}
	pthread_mutex_lock(&mutex_lock);
    }
	printf("send complete\n");
	packet_in_progress = true;
	int close_fd = close(data->clientfd);
	if(close_fd == 0){
		syslog(LOG_DEBUG, "Closed connection from %s\n", data->client_ipaddress);
	}
	current_bytes = 0;
	free(write_buffer);
	data->thread_complete_status=true;
	return thread_param;
}
// Driver Function
int main(int argc, char **argv) {
/************************************************************************************************Signal Handler Invoke********************************************************************************/
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGALRM, signal_handler);
/*************************************************************************************************** Create the Socket *******************************************************************************/
	syslog(LOG_USER, "Socket Creation");
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd == -1){
   		perror("Socket Not created Properly");
   		exit(-1);
	}
/*************************************************************************************************** Bind the Socket *********************************************************************************/
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
syslog(LOG_USER,"File successfully opened");
int listener = listen(sockfd, 10);
if(listener < 0){											// Backlog value set as 10 as prescribed in the Beej's user guide on sockets.
	perror("Failed to execute in the listening process");
	return -1;
}
syslog(LOG_USER, "Listening phase completed");
/***************************************************************************************** Accepting the Packets *************************************************************************************/
socklen_t clientfd;
struct sockaddr_in clientadd;
bool alarm_flag = false;
pthread_mutex_init(&mutex_lock, NULL);
TAILQ_INIT(&head);
	while (1) {
	// Spin a new thread on every connection accept
		thread_data_t *datap = (thread_data_t *) malloc(sizeof(thread_data_t));
		clientfd = sizeof(clientadd);
		datap->clientfd = accept(sockfd, (struct sockaddr *) &clientadd, &clientfd);

		if (datap->clientfd == -1) {
			perror("Error in accepting\n");
			return -1;
		}
	// Convert IPv4 and IPv6 address from binary to text form
	inet_ntop(clientadd.sin_family,(struct sockaddr *) &clientadd, datap->client_ipaddress, sizeof(datap->client_ipaddress));
	syslog(LOG_DEBUG, "Accepted a connection from %s\n", datap->client_ipaddress);
	// Once successful connection accept, create a thread to handle the thread_function
	pthread_create(&(datap->thread), NULL, &thread_function, (void *)datap);
	// Source: Queue.h functions handbook line 253
	TAILQ_INSERT_TAIL(&head, datap, entries);
	datap = NULL;
	free(datap);
	// Trigger an alarm for 10 seconds to efficiently call SIGALRM to append a timestamp
	if(!STORE_IN_THIS_FILE){
	if (!alarm_flag) {
		alarm_flag = true;
		printf("alarm set\n");
		alarm(10);
	}
	}
	thread_data_t *entry = NULL;
	// Source: Queue.h functions handbook line 181
	TAILQ_FOREACH(entry, &head, entries)
	{
		printf("Join all the pthreads to prevent detached threads\n");
		pthread_join(entry->thread, NULL);
		if (entry->thread_complete_status) {
			TAILQ_REMOVE(&head, entry, entries);
			free(entry);
			break;
		}
	}

	}
	close(sockfd);
	close(fd);
}
