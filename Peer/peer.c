#include "server.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

#include <sys/un.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXCONN 50
#define MAXSTRLEN 101

int s_tcp_fd;
int s_udp_fd;
int tcp_port;
char *gw_ip;
int gw_port;

int index_s = 0;
int index_t = 0;

static void handler(int signum)
{   
    message m;
    m.type = 1;
    m.port = tcp_port;
    
    char *buffer;
    buffer = malloc(sizeof(m));
    memset(buffer, 0, sizeof(m));
    memcpy(buffer, &m, sizeof(m));

    
    struct sockaddr_in gw_addr;
    gw_addr.sin_family = AF_INET;
    gw_addr.sin_port = htons(gw_port);
        
    if (!inet_aton(gw_ip, &gw_addr.sin_addr)){
        perror("Gateway IP not valid");
        exit(1);
    }

    if (sendto(s_udp_fd, buffer, sizeof(m), 0, (struct sockaddr*)&gw_addr, sizeof(gw_addr)) < 0){
        perror("Failed UDP connection with gateway");
        exit(1);
    }
    
    if (close(s_udp_fd) < 0)
        perror("UDP socket not closed");
    
    if (close(s_tcp_fd) < 0)
        perror("TCP socket not closed");
        
    exit(0);
}

void server_tcp_setup(){
    
    s_tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (s_tcp_fd == -1) {
        perror("STREAM socket error");
        exit(1);
    }
    
    struct sockaddr_in server_tcp_addr;
    server_tcp_addr.sin_family = AF_INET;
    server_tcp_addr.sin_port = htons(tcp_port);
    server_tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(s_tcp_fd, (struct sockaddr *)&server_tcp_addr, sizeof(server_tcp_addr)) < 0){
        perror("Binding error");
        exit(1);
    }  
    
}

void server_udp_setup(){
    
    s_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_udp_fd == -1) {
        perror("DGRAM socket error");
        exit(1);
    }
    
}

void server_to_gw(){
    
    message m;
    m.type = 0;
    m.port = tcp_port;

    printf("%d\n", m.port);
    
    char *buffer;
    buffer = malloc(sizeof(m));
    memset(buffer, 0, sizeof(m));
    memcpy(buffer, &m, sizeof(m));

    
    struct sockaddr_in gw_addr;
    gw_addr.sin_family = AF_INET;
    gw_addr.sin_port = htons(gw_port);
        
    if (!inet_aton(gw_ip, &gw_addr.sin_addr)){
        perror("Gateway IP not valid");
        exit(1);
    }

    if (sendto(s_udp_fd, buffer, sizeof(m), 0, (struct sockaddr*)&gw_addr, sizeof(gw_addr)) < 0){
        perror("Failed UDP connection with gateway");
        exit(1);
    }

}

int accept_connection(){

    if (listen(s_tcp_fd, MAXCONN) < 0){
            perror("Listen problem");
            exit(1);
    }

    int new_tcp_fd;
    new_tcp_fd = accept(s_tcp_fd, NULL, NULL);
    if ( new_tcp_fd < 0){
        perror("Accept problem");
        exit(1);
    }

    return new_tcp_fd;
}

void * serve_client (void * socket){

        char str_buf[MAXSTRLEN];
        int * new_tcp_fd = (int *)socket;

        printf("Client is being served on socket %d\n", *new_tcp_fd);
        
        if (recv(*new_tcp_fd, str_buf, MAXSTRLEN, 0) < 0){
            perror("Receive problem");
        }
        
        printf("%s\n", str_buf);
        
        /* String to upper case */

        int i = 0;
        char upper_string[MAXSTRLEN];
        while(str_buf[i])
        {
            upper_string[i] = toupper(str_buf[i]);
            i++;
        }
        
        upper_string[i] = '\0';
        
        
        if (send(*new_tcp_fd, upper_string, strlen(upper_string)+1, 0) < 0){
            perror("Send problem");
        }

        if (close(*new_tcp_fd) < 0)
            perror("TCP socket not closed");

        *new_tcp_fd = -1;

        pthread_exit(NULL);
}



int main(int argc, char *argv[]){
    
    if (argc < 3){
        printf("[IP gateway] [Port Gateway]\n");
        exit(1);
    }
    
    tcp_port = 3000 + getpid();
    
    gw_ip = argv[1];
    gw_port = atoi(argv[2]);
    
    /* SIGINT handling */
    struct sigaction sa;
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) < 0){
        perror("Error in handling SIGINT");
        exit(1);
    }
    
    /* Creates socket to send datagrams to gw  */
    
    server_udp_setup();
    
    /* Creates socket of type stream for connections with clients */
    
    server_tcp_setup();
    
    /* Send sock_stream address to gw */
    
    server_to_gw();

    pthread_t threads[MAXCONN];
    int sockets[MAXCONN];
    memset(&sockets, -1, MAXCONN*sizeof(int));
    
    while(1){

        while (sockets[index_s] != -1){
            index_s++;
        }

        sockets[index_s] = accept_connection();

        printf("%d, %d\n", index_s, sockets[index_s]);

        int error;
        error = pthread_create(&threads[index_t++], NULL, serve_client, &sockets[index_s]);
        if(error != 0){
            perror("Thread creation error");
            exit(-1);
        }

        index_s++;

        if (index_t >= MAXCONN)
            index_t = 0;
    
    }
        
    exit(0);
    
}
