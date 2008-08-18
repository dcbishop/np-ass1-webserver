#include <stdio.h> /* Required for printf() */
#include <stdlib.h> /* exit() */
#include <sys/types.h> /* Doesn't seem to be required? Possibly pulled in by another lib? */
#include <sys/socket.h> /* same as above */
#include <string.h> /* memset() */
#include <netdb.h> /* struct sockaddr_in */
#include <netinet/in.h> /* another not required? */
#include <sys/stat.h> /* S_* declarations */
#include <fcntl.h> /* O_RDONLY declaration */

#define PORTNUM 50023
#define BACKLOG 10
#define MAXDATASIZE 1000

#define DOCUMENTROOT "./files"
#define SEPERATOR "/"

#define STATE_NONE 0
#define STATE_GET 1
#define STATE_IGNORE 2

void logmsg(char* message) {
	printf("Server: %s\n", message);
}

int main(int argc, char* argv[]) {
	const int on = 1;
	int sockfd, fd_new;
	struct sockaddr_in my_addr;
	struct sockaddr_in their_addr;
	int connected = 1;
	
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
	my_addr.sin_port = htons(PORTNUM);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	
	memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);
	
	if(bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
		perror("bind");
		exit(1);
	}
	
	if(listen(sockfd, 10) < 0) {
		perror("listen");
		exit(1);
	}
	
	while(connected) {
		int clen = sizeof(their_addr);
		fd_new = accept(sockfd, (struct sockaddr *)&their_addr, &clen);
		if(fd_new < 0) {
			perror("accept");
			exit(1);
		}
		
		char data[MAXDATASIZE];
		char message[MAXDATASIZE];
		snprintf(message, MAXDATASIZE-1,"New connection from %s on port %s", inet_ntoa(their_addr.sin_addr.s_addr), inet_ntoa(their_addr.sin_port));
		logmsg(message);
		
		char buf[MAXDATASIZE];
		int numbytes;
		if ((numbytes=recv(fd_new, buf, MAXDATASIZE-1, 0)) == -1) {
			perror("recv");
			close(fd_new);
			exit(1);
		}
		printf("Recieved %d\n", numbytes);
		
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
			snprintf(realfile, MAXDATASIZE, "%s%s%s", DOCUMENTROOT, SEPERATOR, filename);
			printf("Opening file '%s'...\n", realfile);
			fd = open(realfile, flags, mode);
			
			/* Display a 404 page if it exists */
			if(fd == -1) {
				snprintf(realfile, MAXDATASIZE, "%s%s%s", DOCUMENTROOT, SEPERATOR, "404.html"); 
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
				//printf("Dumping file to socket...\n");
				write(fd_new, data, nbytes);
			}
			printf("Closing file...\n");
			//fclose(filep);
			printf("Closed file.\n");
		}
		
		buf[0]='\0'; /* Ensure blank data doesn't case a repeat */
		filename = NULL;
		
		/*if (send(fd_new, "Hello, world!\n", 14, 0) == -1) {
			perror("send");
			close(fd_new);
			exit(1);
		}*/
		
		close(fd_new);
	}
	
	exit(0);
}
