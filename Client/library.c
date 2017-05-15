#include "API.h"
#include "messages.h"
#include <sys/types.h>          
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


char *serialize_msg(message m){

    char *buffer;
    buffer = malloc(sizeof(m));
    memset(buffer, 0, sizeof(m));
    memcpy(buffer, &m, sizeof(m));
    
    return buffer;

}

// What should I return in case of a general error?

int gallery_connect(char * host, in_port_t port){

	int s_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_udp_fd == -1) {
        perror("DGRAM socket creation error");
        return -1;   
    }

    message serv_req;
    serv_req.type = 0;
    char *buffer_snd = serialize_msg(serv_req);

    struct sockaddr_in gw_addr;
    gw_addr.sin_family = AF_INET;
    gw_addr.sin_port = htons(port);
        
    if (!inet_aton(host, &gw_addr.sin_addr)){
        perror("Gateway IP not valid");
        return -1;
    }

    if (sendto(s_udp_fd, buffer_snd, sizeof(message), 0, (struct sockaddr*)&gw_addr, sizeof(gw_addr)) < 0){
        perror("Failed UDP connection with gateway");
        return -1;
    }
    
    /* Receive the response from the gateway*/
    
    char *buffer_rcv = malloc(sizeof(message));
    
    if(recv(s_udp_fd, buffer_rcv, sizeof(message), 0) < 0){
        perror("Reception error UDP");
        return -1;
    }

    if (close(s_udp_fd) < 0)
        perror("UDP socket not closed");
    
    message gw_mess;
    memcpy(&gw_mess, buffer_rcv, sizeof(message));
    
    if (gw_mess.type == 0){
    	return 0;
    }

    /* Establish a connection with the peer */

    int s_tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (s_tcp_fd == -1) {
        perror("STREAM socket error");
        exit(1);
    }    

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(gw_mess.port);
        
    if (!inet_aton(gw_mess.address, &server_addr.sin_addr)){
        perror("Gateway IP not valid");
        exit(1);
    }
    
    if (connect(s_tcp_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("TCP connection failed");
        exit(1);
    }

    return s_tcp_fd;
}

