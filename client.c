#include "server.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/un.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <arpa/inet.h>
#include <arpa/inet.h>

#define MAXSTRLEN 101


int s_udp_fd;
int s_tcp_fd;

void client_tcp_setup(){
    
    s_tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (s_tcp_fd == -1) {
        perror("STREAM socket error");
        exit(1);
    }    
}


char *serialize_msg(message m){

    char *buffer;
    buffer = malloc(sizeof(m));
    memset(buffer, 0, sizeof(m));
    memcpy(buffer, &m, sizeof(m));
    
    return buffer;

}

void client_udp_setup(){
    
    s_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_udp_fd == -1) {
        perror("DGRAM socket error");
        exit(1);
    }
    
}

int main(int argc, char *argv[]){

    if (argc < 3){
        printf("[Gateway IP] [Gateway port]");
        exit(1);
    }
    
    char *gw_ip = argv[1];
    int gw_port = atoi(argv[2]);
    
    /* Setup UDP socket and send a request to gateway for a server address */
    
    client_udp_setup();
    message serv_req;
    serv_req.type = 0;
    char *buffer_snd = serialize_msg(serv_req);
    
    struct sockaddr_in gw_addr;
    gw_addr.sin_family = AF_INET;
    gw_addr.sin_port = htons(gw_port);
        
    if (!inet_aton(gw_ip, &gw_addr.sin_addr)){
        perror("Gateway IP not valid");
        exit(1);
    }

    if (sendto(s_udp_fd, buffer_snd, sizeof(message), 0, (struct sockaddr*)&gw_addr, sizeof(gw_addr)) < 0){
        perror("Failed UDP connection with gateway");
        exit(1);
    }
    
    /* Receive the response from the gateway*/
    
    char *buffer_rcv = malloc(sizeof(message));
    
    if(recv(s_udp_fd, buffer_rcv, sizeof(message), 0) < 0){
        perror("Reception error UDP");
        exit(1);
    }
    
    message gw_mess;
    memcpy(&gw_mess, buffer_rcv, sizeof(message));
    
    if (gw_mess.type == 0){
        printf("No servers Availables\n");
        exit(1);
    }
    
    /* Establish a connection to the server */
    
    client_tcp_setup();
    
    char string[MAXSTRLEN];
    char upper_string[MAXSTRLEN];
    printf("message: ");
    fgets(string, MAXSTRLEN, stdin);
    
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
    
    if (send(s_tcp_fd, string, strlen(string)+1, 0) < 0){
        perror("TCP send string failed");
        exit(1);
    }
    
    if (recv(s_tcp_fd, upper_string, MAXSTRLEN, 0) < 0){
       perror("Receive problem");
    }
    
    printf("%s\n", upper_string);
    
    exit(0);
        

}
