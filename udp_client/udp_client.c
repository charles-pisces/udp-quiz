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
// Exponential Backoff Algorithm parameters
#define EB_BASE                 500     // 500ms
#define EB_MULTIPLIER           2
#define EB_MAX_WAIT_TIME        8       // 8s
#define MAX_RETRY_CNT           10


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


static int udp_select_check(int sock) 
{	
    struct timeval timeout; 
    fd_set r_fd;
    
    timeout.tv_sec = 1; 
    timeout.tv_usec = 0; 
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
                    int port, 
                    char* buf)
{
    int rc = -1;
    struct sockaddr_in client;
    
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr("127.0.0.1");
    client.sin_port = htons( port );    

    rc = sendto(sock, buf, BUF_SIZE, 0, (struct sockaddr *)&client, sizeof(client));
    if (rc == -1)
        perror("sendto call failed");
    
    return rc;
}


static int udp_recv(int sock, 
                    char* recv_buf, 
                    char* send_buf)
{
    int rc = -1;
    struct sockaddr_in from;
    socklen_t sock_len = sizeof(struct sockaddr_in);

    memset(recv_buf, 0, BUF_SIZE);  
    rc = recvfrom(sock, recv_buf, BUF_SIZE, 0, (struct sockaddr*)&from, &sock_len);
    if (rc > 0) {
        printf("Received Echo: %s\n", recv_buf);
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


static int udp_send_delay(int *retry_cnt)
{
    /*
     * Exponential Backoff Algorithm:
     * wait_interval = base * multiplier^n
    */
    int wait_time =  0;

    (*retry_cnt)++;

    if (*retry_cnt > MAX_RETRY_CNT)
        return -1;

    wait_time = (int)((EB_BASE * (1 << *retry_cnt)) / 1000);
    if (wait_time > EB_MAX_WAIT_TIME)
        wait_time = EB_MAX_WAIT_TIME;
    
    printf("retry_cnt: %d, wait_time: %d\n", *retry_cnt, wait_time);
    
    return wait_time;
}


int main (int argc, char *argv[])
{
    int rc = -1;
    int sock = -1;
    char send_buf[BUF_SIZE], recv_buf[BUF_SIZE];
    int retry_cnt = 0;

    CHECK_FUNC_ERR(init_udp_socket(), sock)
    
    for (int i = 0; i <= 20000; ) {
        sprintf(send_buf, "packet_id[%d]\n", i);
        CHECK_FUNC_ERR(udp_send(sock, SERVER_PORT, send_buf), rc)
        CHECK_FUNC_ERR(udp_select_check(sock), rc)
        if (rc > 0) {
            CHECK_FUNC_ERR(udp_recv(sock, recv_buf, send_buf), rc)
            if (rc > 0) {
                if (strcmp(send_buf, recv_buf) == 0)
                    // comfirm recv. the right echo from server
                    i++;
            }
        } else if (rc == 0) {
            // Delay for a while since server might be busy.
            CHECK_FUNC_ERR(udp_send_delay(&retry_cnt), rc)
            sleep(rc);
        }
    }
    
    close(sock);
    return 0;

ERR:
    close(sock);
    return -1;
}
