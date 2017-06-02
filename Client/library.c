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


int get_new_peer_address(struct sockaddr_in* peer_addr, char* host, in_port_t gw_port){
    message new_addr;
    new_addr.type = 1;
    strncpy(new_addr.address, inet_ntoa((*peer_addr).sin_addr), 20);
    new_addr.port = ntohs((*peer_addr).sin_port);

    char buffer[sizeof(message)];
    memcpy(buffer, &new_addr, sizeof(message));

    struct sockaddr_in gw_addr;
    gw_addr.sin_family = AF_INET;
    gw_addr.sin_port = htons(gw_port);
        
    if (!inet_aton(host, &gw_addr.sin_addr)){
        perror("Gateway IP not valid");
        exit(1);
    }

    int udp_tmp = socket(AF_INET, SOCK_DGRAM, 0);

    if (sendto(udp_tmp, buffer, sizeof(message), 0, (struct sockaddr*)&gw_addr, sizeof(gw_addr)) < 0){
        perror("Failed UDP connection with gateway");
        exit(1);
    }

    printf("\nSENT REQUEST FOR NEW ADDRESS WITH OLD ADDRESS: (%s, %d) \n", new_addr.address, new_addr.port);

    if (recvfrom(udp_tmp, buffer, sizeof(message), 0, NULL, NULL) < 0){
        perror("Address UP reception failed");
        exit(1);
    }    



    message response;
    memcpy(&response, buffer, sizeof(message));

    printf("\nRECEIVED NEW ADDRESS : (%s, %d)\n", response.address, response.port );

    if (response.type == 0){
        return -1;
    }

    (*peer_addr).sin_family = AF_INET;
    (*peer_addr).sin_port = htons(response.port);
                            
    if (!inet_aton(response.address, &((*peer_addr).sin_addr))){
        perror("UP peer IP not valid");
        exit(1);
    }

    close(udp_tmp);

    return 0;
}

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

    free(buffer_rcv);

    printf("\nRECEIVED ADDRESS OF PEER\n");

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
        perror("Gateway IP 2 not valid");
        return -1;
    }
    
    while (connect(s_tcp_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("TCP connection failed with peer. Getting new address");
        int status = get_new_peer_address(&server_addr, host, port);

        if (status == -1){
            return 0;
        }
        
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

    free(buffer);

    /* Send image */
    //printf("SIZE OF PICTURE: %d\n", size);
    char* send_buffer = malloc(size);
    while(!feof(picture)) {
        int read = fread(send_buffer, 1, sizeof(send_buffer), picture);
        if (read > 0){
            int sent = send(peer_socket, send_buffer, sizeof(send_buffer),0);
            //printf("SENT: %d\n", sent);
        }

        bzero(send_buffer, sizeof(send_buffer));
    }

    fclose(picture);
    free(send_buffer);

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

    free(buffer);

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

    free(buffer);

    if (response.type == 2){
        return 0;
    }

    if (response.type == 1){
        return 1;
    }

    return -1;
}

int gallery_get_photo(int peer_socket, uint32_t id_photo, char *file_name){

    cmd_add request;
    request.code = 15;
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

    free(buffer);

    int photo_size = response.size;

    char* p_array = malloc(photo_size);
    int nbytes = 1;
    int cur_index = 0;
    FILE *image;
    image = fopen(file_name, "w");

    while(cur_index < photo_size){
        nbytes = recv(peer_socket, p_array, photo_size,0);
        cur_index = cur_index + nbytes;
        fwrite(p_array, 1, nbytes, image);
        //printf("%d\n", nbytes);
    }
            
    fclose(image);
    free(p_array);

    return 1;

}

int gallery_search_photo(int peer_socket, char * keyword, uint32_t ** id_photos){

    cmd_add request;
    request.code = 13;
    request.type = 0;
    strncpy(request.keyword, keyword, MAX_KEYWORD_LEN);

    char * buffer = serialize_cmd(request);

    if (send(peer_socket, buffer, sizeof(cmd_add), 0) < 0){
        perror("Search keyword request failed");
        return -1;
    }

    if (recv(peer_socket, buffer, sizeof(cmd_add), 0) < 0){
        perror("Search keyword reply failed");
        return -1;
    }

    cmd_add response;
    memcpy(&response, buffer, sizeof(cmd_add));

    if (response.type == 2){
        free(buffer);
        return 0;
    } else if (response.type == 1){

        int num_keywords = response.size;
        
        uint32_t* ids = (uint32_t*) calloc(num_keywords, sizeof(uint32_t));

        for (int i = 0; i < num_keywords; i++){

            if (recv(peer_socket, buffer, sizeof(cmd_add), 0) < 0){
                perror("Search keyword reply failed");
                return -1;
            }

            memcpy(&response, buffer, sizeof(cmd_add));
            ids[i] = response.id;
            //printf("NUM KEYWORDS: %d\n", ids[i]);
        }

        *id_photos = ids;

        free(buffer);

        return num_keywords;
    }

    free(buffer);

    return -1;
    
}

int gallery_get_photo_name(int peer_socket, uint32_t id_photo, char **photo_name){

    cmd_add request;
    request.code = 14;
    request.type = 0;
    request.id = id_photo;

    char * buffer = serialize_cmd(request);

    if (send(peer_socket, buffer, sizeof(cmd_add), 0) < 0){
        perror("Get name request failed");
        return -1;
    }

    if (recv(peer_socket, buffer, sizeof(cmd_add), 0) < 0){
        perror("Get name reply failed");
        return -1;
    }

    cmd_add response;
    memcpy(&response, buffer, sizeof(cmd_add));    

    if (response.type == 2){
        free(buffer);
        return 0;
    }

    if (response.type == 1){
        char* name = malloc(MAX_NAME_LEN);
        strncpy(name, response.name, MAX_NAME_LEN);

        *photo_name = name;

        free(buffer);

        return 1;

    }

    free(buffer);

    return -1;


}


