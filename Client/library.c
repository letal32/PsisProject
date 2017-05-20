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

char *serialize_cmd(cmd_add m){

    char *buffer;
    buffer = malloc(sizeof(m));
    memset(buffer, 0, sizeof(m));
    memcpy(buffer, &m, sizeof(m));
    
    return buffer;

}


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
        return -1;
    }    

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(gw_mess.port);
        
    if (!inet_aton(gw_mess.address, &server_addr.sin_addr)){
        perror("Gateway IP not valid");
        return -1;
    }
    
    if (connect(s_tcp_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("TCP connection failed");
        return -1;
    }

    return s_tcp_fd;
}

uint32_t gallery_add_photo(int peer_socket, char *file_name){

    FILE *picture;
    picture = fopen(file_name, "r");

    if (picture == NULL){
        perror("File not found");
        return 0;
    }

    int size;
    fseek(picture, 0, SEEK_END);
    size = ftell(picture);
    fseek(picture, 0, SEEK_SET);
    //printf("Size: %d\n", size);
    cmd_add request;
    request.code = 10;
    request.type = 0;
    request.size = size;
    strcpy(request.name, file_name);

    char * buffer = serialize_cmd(request);

    send(peer_socket, buffer, sizeof(cmd_add), 0);

    /* Send image */
    char send_buffer[size];
    while(!feof(picture)) {
        fread(send_buffer, 1, sizeof(send_buffer), picture);
        send(peer_socket, send_buffer, sizeof(send_buffer),0);
        bzero(send_buffer, sizeof(send_buffer));
    }

    /* Receive identifier */
    char * recv_id = malloc(sizeof(cmd_add));
    cmd_add response;

    if (recv(peer_socket, recv_id, sizeof(cmd_add), 0) < 0){
        perror("ID receive errors");
        return 0;
    }

    memcpy(&response, recv_id, sizeof(cmd_add));
    //printf("type: %d, id: %d\n", response.type, response.id);
    if (response.type == 1){
        return response.id;
    }

    free(recv_id);


    return 0;
}

int gallery_add_keyword(int peer_socket, uint32_t id_photo, char *keyword){

    cmd_add request;
    request.code = 11;
    request.type = 0;
    strcpy(request.keyword, keyword);
    request.id = id_photo;

    char * buffer = serialize_cmd(request);

    if (send(peer_socket, buffer, sizeof(cmd_add), 0) < 0){
        perror("Keyword request failed");
        return 0;
    }

    if (recv(peer_socket, buffer, sizeof(cmd_add), 0) < 0){
        perror("Keyword reply failed");
        return 0;
    }

    cmd_add response;
    memcpy(&response, buffer, sizeof(cmd_add));

    if (response.type == 2){
        return 0;
    }

    if (response.type == 1){
        return 1;
    }

    return 0;


}

int gallery_delete_photo(int peer_socket, uint32_t id_photo){

    cmd_add request;
    request.code = 12;
    request.type = 0;
    request.id = id_photo;

    char * buffer = serialize_cmd(request);

    if (send(peer_socket, buffer, sizeof(cmd_add), 0) < 0){
        perror("Delete photo request failed");
        return -1;
    }

    if (recv(peer_socket, buffer, sizeof(cmd_add), 0) < 0){
        perror("Delete photo reply failed");
        return -1;
    }

    cmd_add response;
    memcpy(&response, buffer, sizeof(cmd_add));

    if (response.type == 2){
        return 0;
    }

    if (response.type == 1){
        return 1;
    }

    return -1;
}


