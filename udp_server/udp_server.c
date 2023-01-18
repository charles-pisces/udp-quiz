#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define SERVER_PORT             12345
#define BUF_SIZE                255

#define CHECK_FUNC_ERR(func, rc) {									\
			rc = func; 											\
			if(rc < 0) { 											\
				printf("%s failed!, error code: %d\n", #func, rc); \
				goto ERR; 											\
			}                                                       \
        } 


static int set_sock_nonblock(int sock) 
{
	int rc = -1;
	int flags;

	if ((flags = fcntl(sock, F_GETFL)) < 0) {
		rc = flags;
		printf("F_GETFL error!\n");
	} else {
		flags |= O_NONBLOCK;
		rc = fcntl(sock, F_SETFL, flags);
		if (rc < 0) 
            printf("F_SETFL error!\n");
	}

	return rc;
}


static int udp_bind_port(int sock, 
                         int port) 
{
    int rc = -1;
    struct sockaddr_in server;
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( port );

    rc = set_sock_nonblock(sock); 
    if (rc < 0) {
		printf("set_udp_nonblock failed. errno = %d\n", errno);
		return rc;
	}

    rc = bind(sock, (struct sockaddr *)&server , sizeof(server));
	if (rc < 0) {
        printf("bind failed. errno = %d\n", errno);
		return rc;
	}

    return rc;
}


static int udp_recv(int sock, 
                    char* buf, 
                    struct sockaddr_in* from)
{
    int rc = -1;
    socklen_t sock_len = sizeof(struct sockaddr_in);

    memset(buf, 0, BUF_SIZE);  
    rc = recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr *)from, &sock_len);
    if (rc > 0) {
        printf("Received : %s\n", buf);
    } else {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            rc = 0;
        } else if (rc == 0) {
            printf("The connection is closed by peer\n");
        } else {
            printf("udp_recv error. rc : %d, errno : %d\n", rc, errno);
        }
    }

    return rc;
}


static int udp_send(int sock, 
                    char* buf, 
                    struct sockaddr_in *to)
{
    int rc = -1;
    
    rc = sendto(sock, buf, BUF_SIZE, 0, (struct sockaddr *)to, sizeof(struct sockaddr_in));
    if (rc == -1)
        perror("sendto call failed\n");
        
    return rc;
}


static int udp_select_check(int sock) 
{	
    struct timeval timeout; 
    fd_set r_fd;
    
    timeout.tv_sec = 0; 
    timeout.tv_usec = 50; 
    FD_ZERO(&r_fd); 
    FD_SET(sock, &r_fd); 
    
    int rc = select(sock + 1, &r_fd, NULL, NULL, &timeout); 
    if (rc < 0)
        printf("select error! socket : %d, rc : %d, errno : %d\n", sock, rc, errno);
        
    // timeout
    if (rc == 0) {

    }

    if (FD_ISSET(sock, &r_fd))
        rc = 1;

    return rc;
}


int main (int argc, char *argv[])
{
    int rc = -1;
    int sock = -1;
    char buf[BUF_SIZE];
    struct sockaddr_in from;
    socklen_t sock_len = sizeof(struct sockaddr_in);
    
    printf("UDP Server Start.\n");

    CHECK_FUNC_ERR(socket(AF_INET, SOCK_DGRAM, 0), sock)
    CHECK_FUNC_ERR(udp_bind_port(sock, SERVER_PORT), rc)
    
    while (true) {
        CHECK_FUNC_ERR(udp_select_check(sock), rc)
        CHECK_FUNC_ERR(udp_recv(sock, buf, &from), rc)
        if (rc > 0)
            CHECK_FUNC_ERR(udp_send(sock, buf, &from), rc)
    }

    close(sock);
    return 0;
ERR:
    close(sock);
    return -1;
}
