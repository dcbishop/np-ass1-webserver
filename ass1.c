#include <stdio.h> /* Required for printf() */
#include <stdlib.h> /* exit() */
#include <sys/types.h> /* Doesn't seem to be required? Possibly pulled in by another lib? */
#include <sys/socket.h> /* same as above */
#include <string.h> /* memset() */
#include <netdb.h> /* struct sockaddr_in */
#include <netinet/in.h> /* another not required? */
#include <sys/stat.h> /* S_* declarations */
#include <fcntl.h> /* O_RDONLY declaration */
#include <getopt.h> /* For command line handling */
#include <pthread.h>

#define PORTNUM 50023 /* Default port, override with -p # */
#define BACKLOG 10
#define MAXDATASIZE 1000

#define DOCUMENTROOT "./files" /* Default location of the files to be servered, override with '-d directory' */
#define SEPERATOR "/" /* Backslash for windows or other odd systems */

#define STATE_NONE 0 /* Defines used for simple state machine later */
#define STATE_GET 1
#define STATE_IGNORE 2

void* thread_proc(void *arg);

char documentRoot[255]; /* Directory holding the files served */

void logmsg(char* message) {
	printf("Server: %s\n", message);
}

int main(int argc, char* argv[]) {
	const int on = 1;
	int sockfd, fd_new;
	struct sockaddr_in my_addr;
	int connected = 1;
	int portnum = PORTNUM;
	int name, result;
	pthread_t thread_id;
	char usage[255]; /* For holding the error message */
	
	char address[255]; /* For holding an overiding bind address */
	address[0]='\0';

	strncpy(documentRoot, DOCUMENTROOT, 255);
	
	snprintf(usage, 255, "%s: [-p port] [-a address] [-d directory]", (char*)argv[0]);
	
	while ((name = getopt(argc, argv, "p:a:d:")) != -1) {
		switch(name) {
			case 'p':
				portnum = atoi(optarg);
				break;
			case 'a':
				strncpy(address, optarg, 255);
				break;
			case 'd':
				strncpy(documentRoot, optarg, 255);
				break;
			case '?':
				fprintf(stderr, "%s\n", usage);
				exit(1);
		}
	}
	argc -= optind-1;
	
	sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd == -1) {
		perror("socket");
		exit(1);
	}
	
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		perror("setsockopt");
		exit(1);
	}
	
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(portnum);
	if(address[0] == '\0') {
		my_addr.sin_addr.s_addr = INADDR_ANY;
	} else {
		inet_aton(address, my_addr.sin_addr.s_addr);
	}
		
	memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);
	
	printf("Starting server on port %d.\n", portnum);
	
	if(bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
		perror("bind");
		exit(1);
	}
	
	if(listen(sockfd, 10) < 0) {
		perror("listen");
		exit(1);
	}
	
	while(connected) {
		fd_new = accept(sockfd, NULL, NULL);
		if(fd_new < 0) {
			perror("accept");
			exit(1);
		}
		result = pthread_create(&thread_id, NULL, thread_proc, (void*)fd_new);
		if (result != 0) {
				perror("pthread");
				exit(1);
			}
			pthread_detach(thread_id);
	}
	
	exit(0);
}

void *thread_proc(void *arg) {
		int fd_new = (int)arg;
		char data[MAXDATASIZE];
		char message[MAXDATASIZE];
		
		char buf[MAXDATASIZE];
		int numbytes;
		if ((numbytes=recv(fd_new, buf, MAXDATASIZE-1, 0)) == -1) {
			perror("recv");
			close(fd_new);
			exit(1);
		}
		printf("Recieved %d bytes of data.\n", numbytes);
		
		char* filename = NULL;
		
		char *line = strtok(buf, "\n");
		while(line != NULL) {
			char *tok = strtok(buf, " ");
			int state = STATE_NONE;
			int stateIgnoreCount = 0;
			while (tok != NULL) {
				switch(state) {
					/* Looks for a valid HTML request */
					case STATE_NONE:
						if(strncmp("GET", tok, 3) == 0) {
							printf("GET REQUEST\n");
							state = STATE_GET;
						} else {
							printf("Skipping unknown token '%s'\n", tok);
						}
						break;
					/* processes a get request */
					case STATE_GET:
						printf("Filename: %s\n", tok);
						state = STATE_NONE;
						
						if(strcmp(tok, "/") == 0) {
							filename = "index.html";
						} else if (tok[0] == '/') { /* nuke leading '/' */
							printf("Nuke leading /\n");
							filename = tok+1;
						} else {
							filename = tok;
						}
						
						break;
					/* Ignores trailng data */
					case STATE_IGNORE:
						stateIgnoreCount--;
						if(stateIgnoreCount < 1) {
							state = STATE_NONE;
						}
						printf("Skipping token '%s'...\n", tok);
						break;
				}
				
				tok = strtok(NULL," ");
			}
			line = strtok(NULL, "\n");
		}
		
		if(filename != NULL) {
			int fd;
			int flags = O_RDONLY;
			mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
			
			char realfile[MAXDATASIZE];
			snprintf(realfile, MAXDATASIZE, "%s%s%s", documentRoot, SEPERATOR, filename);
			printf("Opening file '%s'...\n", realfile);
			fd = open(realfile, flags, mode);
			
			/* Display a 404 page if it exists */
			if(fd == -1) {
				snprintf(realfile, MAXDATASIZE, "%s%s%s", documentRoot, SEPERATOR, "404.html"); 
				printf("Loading 404 page: '%s'\n", realfile);
				fd = open(realfile, flags, mode);
			}
			
			/* Display a real basic plain text 404 message if no html */
			if (fd == -1) {
				perror("file");
				snprintf(data, MAXDATASIZE-1,"404!\n");
				if (send(fd_new, data, strlen(data), 0) == -1) {
					perror("send");
					close(fd_new);
					exit(1);
				}
			}
			int nbytes = 100;
			while( (nbytes = read(fd, data, MAXDATASIZE)) > 0) {
				write(fd_new, data, nbytes);
			}
			printf("Closing file...\n");
			close(fd);
		}
		
		buf[0]='\0'; /* Ensure blank data doesn't case a repeat */
		filename = NULL;
		
		close(fd_new);
}
