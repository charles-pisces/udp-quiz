#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define BUF_SIZE                255
// Exponential Backoff Algorithm parameters
#define EB_BASE                 500             // 500ms
#define EB_MULTIPLIER           2
#define EB_MAX_WAIT_TIME        8000000         // 8s


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


static int init_udp_socket() 
{	
    int sock = -1;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
        perror("socket create fail!\n");
    else
        printf("socket fd = %d\n", sock);
    
    return sock;
}


static int udp_select_check(int sock, 
                            int echo_wait_time) 
{	
    struct timeval timeout; 
    fd_set r_fd;
    
    if (echo_wait_time == 0) {
        timeout.tv_sec = 0; 
        timeout.tv_usec = 10;
    } else {
        timeout.tv_sec = echo_wait_time / 1000000; 
        timeout.tv_usec = echo_wait_time % 1000000;
    }
     
    FD_ZERO(&r_fd); 
    FD_SET(sock, &r_fd); 
    
    int rc = select(sock + 1, &r_fd, NULL, NULL, &timeout); 
    if (rc < 0)
        printf("select error! socket : %d, rc : %d, errno : %d\n", sock, rc, errno);
        
    // timeout
    if (rc == 0)
        printf("timeout!! \n");

    if (FD_ISSET(sock, &r_fd))
        rc = 1;

    return rc;
}


static int udp_send(int sock, 
                    struct sockaddr_in *client, 
                    char* buf)
{
    int rc = -1;
    
    rc = sendto(sock, buf, BUF_SIZE, 0, (struct sockaddr *)client, sizeof(struct sockaddr_in));
    if (rc == -1)
        perror("sendto call failed");
    
    return rc;
}


static int udp_recv(int sock, 
                    char* recv_buf, 
                    char* send_buf,
                    int retry_cnt)
{
    int rc = -1;
    struct sockaddr_in from;
    socklen_t sock_len = sizeof(struct sockaddr_in);

    memset(recv_buf, 0, BUF_SIZE);  
    rc = recvfrom(sock, recv_buf, BUF_SIZE, 0, (struct sockaddr*)&from, &sock_len);
    if (rc > 0) {
        printf("Received Echo[r:%d]: %s\n", retry_cnt, recv_buf);
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


static int udp_send_delay(int *retry_cnt, int max_retry_cnt)
{
    /*
     * Exponential Backoff Algorithm:
     * wait_interval = base * multiplier^n
    */
    int wait_time =  0;

    (*retry_cnt)++;

    if (*retry_cnt > max_retry_cnt)
        return -1;

    wait_time = (EB_BASE * (1 << *retry_cnt));
    if (wait_time > EB_MAX_WAIT_TIME)
        wait_time = EB_MAX_WAIT_TIME;
    
    printf("retry_cnt: %d, wait_time: %d (ms)\n", *retry_cnt, wait_time);
    
    return wait_time;
}


static bool is_valid_ip(char *ip)
{
    struct sockaddr_in sa;
    int rc = inet_pton(AF_INET, ip, &(sa.sin_addr));
    return rc != 0;
}


static int is_param_valid(int argc, 
                          char *argv[], 
                          struct sockaddr_in *client,
                          int *max_retry_cnt)
{
    int server_port = 0;
    
    if (argc != 5) {
		printf("[Usage] ./udp_client <server_iP> <server_port> <message> <max_retry> <echo_wait_time>\n\n");
        printf("<server_iP>: The server ip \n");
        printf("<server_port>: The server port \n");
        printf("<message>: The message sent to server. 0 < Message length < 255. \n");
        printf("<max_retry>: The max retry count that no echo data received from server. Range: 0 < Max-Retry <= 10.\n");
    
        return -1;
	}

    if (!is_valid_ip(argv[1])) {
        printf("Invalid <server_ip>. \n");
		return -1;
    }

    server_port = atoi(argv[2]);
	if (server_port <= 0) {
        printf("Invalid <server_port>. \n");
		return -1;
    }

    if (strlen(argv[3]) > 255) {
        printf("Invalid <message>. Message length > 255.\n");
		return -1;
    }

    *max_retry_cnt = atoi(argv[4]);
	if (*max_retry_cnt <= 0 || *max_retry_cnt > 10) {
        printf("Invalid <max_retry>. Range: 0 < Max-Retry <= 10.\n");
		return -1;
    }

    client->sin_family = AF_INET;
    client->sin_addr.s_addr = inet_addr(argv[1]);
    client->sin_port = htons(server_port);    

    return 0;
}


int main (int argc, char *argv[])
{
    int rc = -1;
    int sock = -1;
    struct sockaddr_in client;
    struct timeval timeout;
    char send_buf[BUF_SIZE], recv_buf[BUF_SIZE];
    int max_retry_cnt = 0, retry_cnt = 0;
    int echo_wait_time = 0;

    CHECK_FUNC_ERR(is_param_valid(argc, argv, &client, &max_retry_cnt), rc)
    
    printf("UDP Client Start.\n");
    CHECK_FUNC_ERR(init_udp_socket(), sock)
    
    for (int i = 0; i <= 50000; ) {
        sprintf(send_buf, "%s[%d]", argv[3], i);
        CHECK_FUNC_ERR(udp_send(sock, &client, send_buf), rc)
        CHECK_FUNC_ERR(udp_select_check(sock, echo_wait_time), rc)
        if (rc == 0) {
            // Adjust the wait time of select() since server might be busy.
            CHECK_FUNC_ERR(udp_send_delay(&retry_cnt, max_retry_cnt), echo_wait_time)
        }

        CHECK_FUNC_ERR(udp_recv(sock, recv_buf, send_buf, retry_cnt), rc)
        if (rc > 0) {
            // comfirm recv. the right echo from server
            if (strcmp(send_buf, recv_buf) == 0) 
                i++;
        }        
    }
    
    close(sock);
    return 0;

ERR:
    close(sock);
    return 1;
}
