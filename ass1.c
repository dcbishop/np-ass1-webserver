#include <stdio.h> /* Required for printf() */
#include <stdlib.h> /* exit() */
#include <sys/types.h> /* Doesn't seem to be required? Possibly pulled in by another lib? */
#include <sys/socket.h> /* same as above */
#include <string.h> /* memset() */
#include <netdb.h> /* struct sockaddr_in */
#include <netinet/in.h> /* another not required? */

#define PORTNUM 50023
#define BACKLOG 10
#define MAXDATASIZE 100

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
		
		char buf[MAXDATASIZE];
		int numbytes;
		if ((numbytes=recv(fd_new, buf, 100-1, 0)) == -1) {
			perror("recv");
			exit(1);
		}
		
		if (send(fd_new, "Hello, world!\n", 14, 0) == -1) {
			perror("send");
			close(fd_new);
			exit(1);
		}
		close(fd_new);
		printf("%s\n", buf);
	}
	exit(0);
}
