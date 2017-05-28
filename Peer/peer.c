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
#include <stdint.h>

#include <sys/un.h>          
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "server.h"

#define MAXCONN 50
#define MAXSTRLEN 101

int s_tcp_fd; //TCP socket for accepting client requests
int s_udp_fd; //UDP socket for communicating with the gateway
int s_tcp_peer_fd; //TCP socket for accepting connections from other peers


int tcp_port;
int tcp_port_pr;
int udp_port_gw;

int peer_id = -1;
int counter = 0;


char *gw_ip;
int gw_port;
node *head = NULL;
int num_nodes = 0;

char peer_up[20];
int port_peer_up = -1;


int index_s = 0;
int index_t = 0;

char *serialize_cmd(cmd_add m){

    char *buffer;
    buffer = malloc(sizeof(m));
    memset(buffer, 0, sizeof(m));
    memcpy(buffer, &m, sizeof(m));
    
    return buffer;

}

static void handler(int signum)
{   
    message m;
    m.type = 2;
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

    struct sockaddr_in server_udp_addr;
    server_udp_addr.sin_family = AF_INET;
    server_udp_addr.sin_port = htons(udp_port_gw);
    server_udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(s_udp_fd, (struct sockaddr *)&server_udp_addr, sizeof(server_udp_addr)) < 0){
        perror("Binding error");
        exit(1);
    }  
    
}

void * listen_to_gw(){
    
    message m;
    m.type = 0;
    m.port = tcp_port;
    m.port_gw = udp_port_gw;
    m.port_pr = tcp_port_pr;

    //printf("%d\n", m.port);
    
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

    if (recvfrom(s_udp_fd, buffer, sizeof(m), 0, NULL, NULL) < 0){
        perror("Address UP reception failed");
        exit(1);
    }

    message mess;
    memcpy(&mess, buffer, sizeof(m));
    peer_id = mess.id;

    printf("PEER ID: %d\n", peer_id );

    if (mess.port_pr > 0){
        strncpy(peer_up, mess.up, 20);
        port_peer_up = mess.port_pr;

        printf("ADDRESS UP: %s\n", peer_up);
        printf("PORT UP: %d\n", port_peer_up );
    }

    while(1){

        if (recvfrom(s_udp_fd, buffer, sizeof(m), 0, NULL, NULL) < 0){
            perror("Address UP reception failed");
            exit(1);
        }

        memcpy(&m, buffer, sizeof(m));
        strncpy(peer_up, m.up, 20);
        port_peer_up = m.port_pr;

        printf("ADDRESS UP: %s\n", peer_up);
        printf("PORT UP: %d\n", port_peer_up );

        //If new peer added start uploading pictures
        pthread_t up_pic;

        if (num_nodes > 0 && m.subtype == 1){

            int err = pthread_create(&up_pic, NULL, upload_pic, NULL);
            if (err < 0){
                perror("Thread creation error");
                break;
            }

        }

    }

}


void * upload_pic(){

    struct sockaddr_in peer_addr;
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port_peer_up);
        
    if (!inet_aton(peer_up, &peer_addr.sin_addr)){
        perror("UP peer IP not valid");
        pthread_exit(NULL);
    }

    int tcp_tmp = socket(AF_INET, SOCK_STREAM, 0);
    
    if (tcp_tmp == -1) {
        perror("STREAM socket error");
        pthread_exit(NULL);
    }   

    if (connect(tcp_tmp, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0){
        perror("TCP connection failed");
        pthread_exit(NULL);
    }

    cmd_add command;
    command.code = 20;
    command.size = num_nodes;

    char buffer[sizeof(cmd_add)];
    memcpy(buffer, &command, sizeof(cmd_add));

    if (send(tcp_tmp, buffer, sizeof(cmd_add), 0) < 0){
        perror("Send error in replication");
        pthread_exit(NULL);
    }

    //TODO: Sync!!!!

    node* cur_node = head;
    for (int i = 0; i < num_nodes; i++){

        cmd_add pic_info;
        strncpy(pic_info.name, cur_node->name, MAX_NAME_LEN);
        strncpy(pic_info.keyword, cur_node->keywords, MAX_KEYWORD_TOT_LEN);
        pic_info.id = cur_node->identifier;


        FILE *picture;
        picture = fopen(pic_info.name, "r");
        if (picture == NULL){
            perror("File not found");
            pthread_exit(NULL);
        }

        int size;
        fseek(picture, 0, SEEK_END);
        size = ftell(picture);
        fseek(picture, 0, SEEK_SET);

        pic_info.size = size;

        memcpy(buffer, &pic_info, sizeof(cmd_add));
        if (send(tcp_tmp, buffer, sizeof(cmd_add), 0) < 0){
            perror("Send error in replication while sending pic info");
            pthread_exit(NULL);
        }

        /* Send image */
        char send_buffer[size];
        while(!feof(picture)) {
            int read = fread(send_buffer, 1, sizeof(send_buffer), picture);
            if (read > 0){
                int sent = send(tcp_tmp, send_buffer, sizeof(send_buffer),0);
                printf("SENT: %d\n", sent);
            }

            bzero(send_buffer, sizeof(send_buffer));
        }

        fclose(picture);

        if (cur_node->next != NULL)
            cur_node = cur_node->next;

        cmd_add response;
        if (recv(tcp_tmp, buffer, sizeof(cmd_add), 0) < 0){
            perror("Confirmation picture downloaded success");
            break;
        }
        memcpy(&response, buffer, sizeof(cmd_add));

        if (response.type != 1){
            break;
        }

    }

    close(tcp_tmp);
    pthread_exit(NULL);

}

void * listen_to_peer(){

    printf("IM HERE GW\n");
    fflush(stdout);

    s_tcp_peer_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (s_tcp_peer_fd == -1) {
        perror("STREAM socket error");
        exit(1);
    }
    
    struct sockaddr_in server_tcp_peer_addr;
    server_tcp_peer_addr.sin_family = AF_INET;
    server_tcp_peer_addr.sin_port = htons(tcp_port_pr);
    server_tcp_peer_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(s_tcp_peer_fd, (struct sockaddr *)&server_tcp_peer_addr, sizeof(server_tcp_peer_addr)) < 0){
        perror("Binding error");
        exit(1);
    } 

    while (1){

        if (listen(s_tcp_peer_fd, MAXCONN) < 0){
            perror("Listen problem");
        }

        int new_tcp_fd;
        new_tcp_fd = accept(s_tcp_peer_fd, NULL, NULL);
        if ( new_tcp_fd < 0){
            perror("Accept problem");
            exit(1);
        }

        cmd_add command;
        char buffer[sizeof(cmd_add)];
        int nby;
        nby=recv(new_tcp_fd, buffer, sizeof(cmd_add), 0);
        //printf("NBY: %d\n", nby );
        //fflush(stdout);

        memcpy(&command, buffer, sizeof(cmd_add));

        //Download all pictures from above peer
        if (command.code == 20){
            //printf("I'M HERE PLEASE\n");
            fflush(stdout);

            int num_pictures = command.size;
            //printf("NUM PIC: %d\n", num_pictures );
            fflush(stdout);

            for (int i = 0; i < num_pictures; i++){

                cmd_add pic_info;
                if (recv(new_tcp_fd, buffer, sizeof(cmd_add),0) < 0){
                    perror("Receive problem from peer in download all pictures");
                    break;
                }

                memcpy(&pic_info, buffer, sizeof(cmd_add));
                node * picture = malloc(sizeof(node));
                strncpy(picture->name, pic_info.name, MAX_NAME_LEN);
                strncpy(picture->keywords, pic_info.keyword, MAX_KEYWORD_TOT_LEN);
                picture->identifier = pic_info.id;

                int photo_size = pic_info.size;
                //printf("PIC SIZE: %d\n", photo_size);
                char p_array[photo_size];
                int nbytes = 0;
                int cur_index = 0;
                FILE *image;
                image = fopen(pic_info.name, "w");

                while(cur_index < photo_size){
                    nbytes = recv(new_tcp_fd, p_array, photo_size,0);
                    cur_index = cur_index + nbytes;
                    fwrite(p_array, 1, nbytes, image);
                    printf("%d\n", nbytes);
                    printf("%d\n", cur_index );
                    fflush(stdout);
                }
            
                fclose(image);

                //printf("INSERTING PICTURE\n");
                insert(picture);
                //printf("END INSERTING PICTURE\n");
                printlist();

                cmd_add response;
                response.code = 20;
                response.type = 1;
                memcpy(buffer, &response, sizeof(cmd_add));
                if (send(new_tcp_fd, buffer, sizeof(cmd_add),0) < 0){
                    perror("Receive problem from peer in download all pictures");
                    break;
                }

            }

            

        } else if (command.code == 21) {

            if (command.source == peer_id){

                cmd_add response;
                response.type = 2;

                memcpy(buffer, &response, sizeof(cmd_add));

                if (send(new_tcp_fd, buffer, sizeof(cmd_add), 0) < 0){
                    perror("Send error in replication");
                    break;
                }

            } else {

                cmd_add response;
                response.type = 1;

                memcpy(buffer, &response, sizeof(cmd_add));

                if (send(new_tcp_fd, buffer, sizeof(cmd_add), 0) < 0){
                    perror("Send error in replication");
                    break;
                }

                node * picture = malloc(sizeof(node));
                strncpy(picture->name, command.name, MAX_NAME_LEN);
                strncpy(picture->keywords, command.keyword, MAX_KEYWORD_TOT_LEN);
                picture->identifier = command.id;
                int photo_size = command.size;
                int cur_index = 0;
                int nbytes = 0;

                char p_array[photo_size];
                FILE *image;
                image = fopen(command.name, "w");

                while(cur_index < photo_size){
                    nbytes = recv(new_tcp_fd, p_array, photo_size,0);
                    cur_index = cur_index + nbytes;
                    fwrite(p_array, 1, nbytes, image);
                    printf("%d\n", nbytes);
                    printf("%d\n", cur_index );
                    fflush(stdout);
                }
            
                fclose(image);

                insert(picture);
                printlist();

                //Replicate the picture to the peer down
                struct sockaddr_in peer_addr;
                peer_addr.sin_family = AF_INET;
                peer_addr.sin_port = htons(port_peer_up);
                            
                if (!inet_aton(peer_up, &peer_addr.sin_addr)){
                    perror("UP peer IP not valid");
                    break;
                }

                int tcp_tmp = socket(AF_INET, SOCK_STREAM, 0);
                        
                if (tcp_tmp == -1) {
                    perror("STREAM socket error");
                    break;
                }   

                int status = 0;
                while (connect(tcp_tmp, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0){
                    perror("TCP connection failed adding picture");
                    status = get_new_peer_address(&peer_addr);

                    if (status == -1){
                        break;
                    }
                }

                if (status == -1){
                    continue;
                }

                image = fopen(command.name, "r");
                if (image == NULL){
                        perror("File not found");
                        exit(1);
                }

                fseek(image, 0, SEEK_END);
                photo_size = ftell(image);
                fseek(image, 0, SEEK_SET);


                cmd_add upload;
                upload.code = 21;
                upload.size = photo_size;
                upload.source = command.source;
                strncpy(upload.name, command.name, MAX_NAME_LEN);
                upload.id = command.id;

                memcpy(buffer, &upload, sizeof(cmd_add));

                if (send(tcp_tmp, buffer, sizeof(cmd_add), 0) < 0){
                    perror("Send error in replication");
                    break;
                }

                if (recv(tcp_tmp, buffer, sizeof(cmd_add), 0) < 0){
                    perror("Send error in replication");
                    break;
                }

                memcpy(&upload, buffer, sizeof(cmd_add));

                if (upload.type == 1){

                    /* Send image */
                    char send_buffer[photo_size];
                    while(!feof(image)) {
                        int read = fread(send_buffer, 1, sizeof(send_buffer), image);
                        if (read > 0){
                            int sent = send(tcp_tmp, send_buffer, sizeof(send_buffer),0);
                            printf("SENT: %d\n", sent);
                        }

                        bzero(send_buffer, sizeof(send_buffer));
                    }

                    fclose(image);

                }                      

            }
        //ADD KEYWORD REPLICATION
        } else if (command.code == 22){

            if (command.source != peer_id){
                node * photo_info = search_by_id(command.id);
                strncpy(photo_info->keywords + strlen(photo_info->keywords), command.keyword, MAX_KEYWORD_TOT_LEN);
                printlist();

                struct sockaddr_in peer_addr;
                peer_addr.sin_family = AF_INET;
                peer_addr.sin_port = htons(port_peer_up);
                            
                if (!inet_aton(peer_up, &peer_addr.sin_addr)){
                    perror("UP peer IP not valid");
                    break;
                }

                int tcp_tmp = socket(AF_INET, SOCK_STREAM, 0);
                        
                if (tcp_tmp == -1) {
                    perror("STREAM socket error");
                    break;
                }   

                int status = 0;
                while (connect(tcp_tmp, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0){
                    perror("TCP connection failed");
                    status = get_new_peer_address(&peer_addr);
                    if (status == -1)
                        break;
                }

                if (status == -1)
                    continue;

                memcpy(buffer, &command, sizeof(cmd_add));

                if (send(tcp_tmp, buffer, sizeof(cmd_add), 0) < 0){
                    perror("Send error in replication");
                    break;
                }

            }
        } else if (command.code == 23) {

            if (command.source != peer_id){
                remove_node(command.id);
                printlist();

                struct sockaddr_in peer_addr;
                peer_addr.sin_family = AF_INET;
                peer_addr.sin_port = htons(port_peer_up);
                            
                if (!inet_aton(peer_up, &peer_addr.sin_addr)){
                    perror("UP peer IP not valid");
                    break;
                }

                int tcp_tmp = socket(AF_INET, SOCK_STREAM, 0);
                        
                if (tcp_tmp == -1) {
                    perror("STREAM socket error");
                    break;
                }   

                int status = 0;
                while (connect(tcp_tmp, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0){
                    perror("TCP connection failed");
                    status = get_new_peer_address(&peer_addr);
                    if (status == -1)
                        break;
                    
                }

                if (status == -1)
                    continue;

                memcpy(buffer, &command, sizeof(cmd_add));

                if (send(tcp_tmp, buffer, sizeof(cmd_add), 0) < 0){
                    perror("Send error in replication");
                    break;
                }

            }

        }

        close(new_tcp_fd);
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

void * serve_client (void * sock){

        int * new_tcp_fd = (int *)sock;

        printf("Client is being served on socket %d\n", *new_tcp_fd);
        
        while (1){

            //printf("Waiting for command...\n");
            char *buffer = malloc(sizeof(cmd_add));

            if (recv(*new_tcp_fd, buffer, sizeof(cmd_add), 0) <= 0){
                break;
            }
            
            cmd_add cmd;
            memcpy(&cmd, buffer, sizeof(cmd));

            //printf("%d\n", cmd.code );

            /* Add photo command */

            if (cmd.code == 10){

                int photo_size = cmd.size;
                char * photo_name = cmd.name;

                char p_array[photo_size];
                int nbytes = 0;
                int cur_index = 0;
                FILE *image;
                image = fopen(cmd.name, "w");

                while(cur_index < photo_size){
                    nbytes = recv(*new_tcp_fd, p_array, photo_size,0);
                    cur_index = cur_index + nbytes;
                    fwrite(p_array, 1, nbytes, image);
                    printf("%d\n", nbytes);
                }
            
                fclose(image);

                node *new_image = malloc(sizeof(node));
                strncpy(new_image->name, cmd.name,100);
                strncpy(new_image->keywords, "\0", 100);
                uint32_t pic_id = 100000*peer_id + counter;
                counter++;
                new_image->identifier = pic_id;

                insert(new_image);
                printlist();

                cmd_add resp;

                resp.type = 1;
                resp.id = new_image->identifier;

                //printf("%d\n", cmd.id );

                char * response = serialize_cmd(resp);

                if (send(*new_tcp_fd, response, sizeof(cmd_add), 0) < 0){
                    perror("Add photo response problem");
                    break;
                }

                if (port_peer_up > 0){

                        struct sockaddr_in peer_addr;
                        peer_addr.sin_family = AF_INET;
                        peer_addr.sin_port = htons(port_peer_up);
                            
                        if (!inet_aton(peer_up, &peer_addr.sin_addr)){
                            perror("UP peer IP not valid");
                            break;
                        }

                        int tcp_tmp = socket(AF_INET, SOCK_STREAM, 0);
                        
                        if (tcp_tmp == -1) {
                            perror("STREAM socket error");
                            break;
                        }   

                        int status = 0;
                        while (connect(tcp_tmp, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
                            perror("TCP connection failed, retrieving new peer address");
                            status = get_new_peer_address(&peer_addr);
                            if (status == -1)
                                break;
                        }

                        if (status == -1)
                            continue;

                        FILE *picture;
                        picture = fopen(cmd.name, "r");
                        if (picture == NULL){
                            perror("File not found");
                            exit(1);
                        }

                        fseek(picture, 0, SEEK_END);
                        photo_size = ftell(picture);
                        fseek(picture, 0, SEEK_SET);


                        cmd_add upload;
                        upload.code = 21;
                        upload.type = 0;
                        upload.size = photo_size;
                        upload.source = peer_id;
                        strncpy(upload.name, cmd.name, MAX_NAME_LEN);
                        upload.id = pic_id;

                        memcpy(buffer, &upload, sizeof(cmd_add));

                        if (send(tcp_tmp, buffer, sizeof(cmd_add), 0) < 0){
                            perror("Send error in replication");
                            break;
                        }

                        if (recv(tcp_tmp, buffer, sizeof(cmd_add), 0) < 0){
                            perror("Send error in replication");
                            break;
                        }

                        memcpy(&upload, buffer, sizeof(cmd_add));

                        if (upload.type == 1){

                            /* Send image */
                            char send_buffer[photo_size];
                            while(!feof(picture)) {
                                int read = fread(send_buffer, 1, sizeof(send_buffer), picture);
                                if (read > 0){
                                    int sent = send(tcp_tmp, send_buffer, sizeof(send_buffer),0);
                                    printf("SENT: %d\n", sent);
                                }

                                bzero(send_buffer, sizeof(send_buffer));
                            }

                            fclose(picture);
                        }                      

                }



            } else if (cmd.code == 11){

                //printf("IM HERE\n");

                node * photo_info = search_by_id(cmd.id);

                if (photo_info == NULL){
                    cmd_add resp;
                    resp.type = 2;
                    char * response = serialize_cmd(resp);
                    if (send(*new_tcp_fd, response, sizeof(cmd_add), 0) < 0){
                        perror("Keyword send response error");
                        break;
                    }

                    free(response);

                } else {

                    strncpy(photo_info->keywords + strlen(photo_info->keywords), cmd.keyword, MAX_KEYWORD_LEN);
                    printlist();

                    cmd_add resp;
                    resp.type = 1;
                    char * response = serialize_cmd(resp);
                    if (send(*new_tcp_fd, response, sizeof(cmd_add), 0) < 0){
                        perror("Keyword send response error");
                        break;
                    }

                    if (port_peer_up > 0){
                        //Add keyword to all other peers

                        struct sockaddr_in peer_addr;
                        peer_addr.sin_family = AF_INET;
                        peer_addr.sin_port = htons(port_peer_up);
                            
                        if (!inet_aton(peer_up, &peer_addr.sin_addr)){
                            perror("UP peer IP not valid");
                            break;
                        }

                        int tcp_tmp = socket(AF_INET, SOCK_STREAM, 0);
                        
                        if (tcp_tmp == -1) {
                            perror("STREAM socket error");
                            break;
                        }   

                        int status = 0;
                        while (connect(tcp_tmp, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0){
                            perror("TCP connection failed");
                            status = get_new_peer_address(&peer_addr);
                            if (status == -1){
                                break;
                            }
                            
                        }

                        if (status == -1)
                            continue;

                        cmd_add upload;
                        upload.code = 22;
                        upload.type = 0;
                        upload.source = peer_id;
                        strncpy(upload.keyword, cmd.keyword, MAX_KEYWORD_TOT_LEN);
                        upload.id = cmd.id;

                        printf("Upload keyword: %d, %d, %s, %d\n", upload.code, upload.source, upload.keyword, upload.id);
                        fflush(stdout);

                        memcpy(response, &upload, sizeof(cmd_add));
                        if (send(tcp_tmp, response, sizeof(cmd_add), 0) < 0){
                            perror("Keyword send response error");
                            break;
                        }

                    }

                }

            } else if (cmd.code == 12) {

                int status = remove_node(cmd.id);
                printlist();

                if (status == 0){
                    cmd_add resp;
                    resp.type = 2;
                    char * response = serialize_cmd(resp);
                    if (send(*new_tcp_fd, response, sizeof(cmd_add), 0) < 0){
                        break;
                    }
                } else if (status == 1){
                    cmd_add resp;
                    resp.type = 1;
                    char * response = serialize_cmd(resp);
                    if (send(*new_tcp_fd, response, sizeof(cmd_add), 0) < 0){
                        perror("Photo delete response error");
                        break;
                    }


                    //Replicate the delete
                    if (port_peer_up > 0){
                        //Add keyword to all other peers

                        struct sockaddr_in peer_addr;
                        peer_addr.sin_family = AF_INET;
                        peer_addr.sin_port = htons(port_peer_up);
                            
                        if (!inet_aton(peer_up, &peer_addr.sin_addr)){
                            perror("UP peer IP not valid");
                            break;
                        }

                        int tcp_tmp = socket(AF_INET, SOCK_STREAM, 0);
                        
                        if (tcp_tmp == -1) {
                            perror("STREAM socket error");
                            break;
                        }   

                        int stat = 0;
                        while (connect(tcp_tmp, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0){
                            perror("TCP connection failed");
                            stat = get_new_peer_address(&peer_addr);
                            
                            if (stat == -1)
                                break;
                        }

                        if (stat == -1)
                            continue;

                        cmd_add upload;
                        upload.code = 23;
                        upload.type = 0;
                        upload.source = peer_id;
                        upload.id = cmd.id;

                        memcpy(response, &upload, sizeof(cmd_add));
                        if (send(tcp_tmp, response, sizeof(cmd_add), 0) < 0){
                            perror("Keyword send response error");
                            break;
                        }

                    }                   

                }

            } else if (cmd.code == 15){

                node * photo_info = search_by_id(cmd.id);

                if (photo_info == NULL){
                    cmd_add resp;
                    resp.type = 2;
                    char * response = serialize_cmd(resp);
                    if (send(*new_tcp_fd, response, sizeof(cmd_add), 0) < 0){
                        perror("Download picture response error");
                        break;
                    }                   
                } else {

                    FILE *picture;
                    picture = fopen(photo_info->name, "r");
                    if (picture == NULL){
                        perror("File not found");
                        exit(1);
                    }

                    int size;
                    fseek(picture, 0, SEEK_END);
                    size = ftell(picture);
                    fseek(picture, 0, SEEK_SET);


                    cmd_add resp;
                    resp.code = 15;
                    resp.type = 1;
                    resp.size = size;

                    char * buffer = serialize_cmd(resp);

                    if (send(*new_tcp_fd, buffer, sizeof(cmd_add), 0) < 0){
                        perror("Download picture response failed");
                        break;
                    }

                    /* Send image */
                    char send_buffer[size];
                    while(!feof(picture)) {
                        int read = fread(send_buffer, 1, sizeof(send_buffer), picture);
                        if (read > 0){
                            int sent = send(*new_tcp_fd, send_buffer, sizeof(send_buffer),0);
                            printf("SENT: %d\n", sent);
                        }

                        bzero(send_buffer, sizeof(send_buffer));
                    }

                    fclose(picture);


                }

            } else if (cmd.code == 13){

                node* cur_node = head;
                int num_keywords = 0;
                int size = 100;
                int index = 0;

                uint32_t* ids = malloc(size*sizeof(uint32_t));

                while ((cur_node = search_by_keyword(cur_node, cmd.keyword)) != NULL){

                    num_keywords++;

                    if (index < size){ 
                        ids[index++] = cur_node->identifier;

                    } else {
                        uint32_t* new_ids = malloc(index*2*sizeof(uint32_t));
                        size = 2*size;

                        for (int i = 0; i < index; i++){
                            new_ids[i] = ids[i];
                        }

                        free(ids);
                        ids = new_ids;
                        ids[index++] = cur_node->identifier;
                    }

                    cur_node = cur_node->next;
                }


                if (num_keywords == 0){

                    cmd_add resp;
                    resp.code = 15;
                    resp.type = 2;

                    char * buffer = serialize_cmd(resp);

                    if (send(*new_tcp_fd, buffer, sizeof(cmd_add), 0) < 0){
                        perror("Search keyword response failed");
                        break;
                    }

                } else {

                    cmd_add resp;
                    resp.code = 15;
                    resp.type = 1;
                    resp.size = num_keywords;

                    char * buffer = serialize_cmd(resp);

                    if (send(*new_tcp_fd, buffer, sizeof(cmd_add), 0) < 0){
                        perror("Search keyword response failed");
                        break;
                    }

                    int i;
                    for (i = 0; i < num_keywords; i++){
                        resp.id = ids[i];
                        //printf("identifier: %u\n", resp.id);
                        buffer = serialize_cmd(resp);
                        if (send(*new_tcp_fd, buffer, sizeof(cmd_add), 0) < 0){
                            perror("Search keyword response failed");
                            break;
                        }

                    }

                    free(ids);

                    if (i < num_keywords){
                        break;
                    }

                }

            } else if (cmd.code == 14){

                node * photo_info = search_by_id(cmd.id);

                if (photo_info == NULL){
                    cmd_add resp;
                    resp.type = 2;
                    char * response = serialize_cmd(resp);
                    if (send(*new_tcp_fd, response, sizeof(cmd_add), 0) < 0){
                        perror("Photo name response error");
                        break;
                    }

                } else {

                    cmd_add resp;
                    resp.type = 1;
                    strncpy(resp.name, photo_info->name, MAX_NAME_LEN);

                    char * response = serialize_cmd(resp);
                    if (send(*new_tcp_fd, response, sizeof(cmd_add), 0) < 0){
                        perror("Keyword send response error");
                        break;
                    }
                }


            }

        }

        //printf("IM HERE2\n");
        if (close(*new_tcp_fd) < 0)
            perror("TCP socket not closed");

        *new_tcp_fd = -1;

        pthread_exit(NULL);
}

void insert(node* new_node){
    
    if (head == NULL){
        head = new_node;
        head->next = NULL;
   
    } else {
        new_node->next = head;
        head = new_node;
    }

    num_nodes++;
    
}

node* search_by_id(uint32_t id){

    if (head == NULL)
        return NULL;

    if (head->identifier == id){
        return head;
    }

    node* cur_node = head;

    while (cur_node->next != NULL){

        if (cur_node->identifier == id){

            return cur_node;
        }

        cur_node = cur_node->next;
    }

    if (cur_node->identifier == id){
        return cur_node;
    }

    return NULL;
}

node* search_by_keyword(node* start, char* keyword){

    if (start == NULL)
        return NULL;

    if (strstr(start->keywords, keyword) != NULL){
        return start;
    }

    node* cur_node = start;

    while (cur_node->next != NULL){

        if (strstr(start->keywords, keyword) != NULL){

            return cur_node;
        }

        cur_node = cur_node->next;
    }

    if (strstr(cur_node->keywords, keyword) != NULL){
        return cur_node;
    }

    return NULL;
}


int remove_node(uint32_t id){

    if (head == NULL)
        return 0;

    if (head->identifier == id){

        int rem = remove(head->name);
        if (rem == 0){
            node *new_head = head->next;
            free(head);
            head = new_head;
            num_nodes--;
            return 1;
        }else{
            return 0;
        }
    }

    node* cur_node = head;
    
    while (cur_node->next != NULL){

        if (cur_node->next->identifier == id){

            int rem = remove(cur_node->next->name);
            if (rem == 0){
                node *temp = cur_node->next->next;
                free(cur_node->next);
                cur_node->next = temp;
                num_nodes--;
                return 1;
            } else {
                return 0;
            }
        }

        cur_node = cur_node->next;
    }    

    return 0;
}


void printlist(){

    node * cur_node = head;

    if (cur_node ==  NULL){
        return;
    }

    while (cur_node->next != NULL){
        printf("(Name : %s , keywords: %s, identifier: %u)\n", cur_node->name, cur_node->keywords, cur_node->identifier );
        cur_node = cur_node->next;
    }

    printf("(Name : %s , keywords: %s, identifier: %u)\n", cur_node->name, cur_node->keywords, cur_node->identifier );
}

int get_new_peer_address(struct sockaddr_in* peer_addr){
    message new_addr;
    new_addr.type = 3;
    strncpy(new_addr.up, peer_up, 20);
    new_addr.port_pr = port_peer_up;

    char buffer[sizeof(message)];
    memcpy(buffer, &new_addr, sizeof(message));

    struct sockaddr_in gw_addr;
    gw_addr.sin_family = AF_INET;
    gw_addr.sin_port = htons(gw_port);
        
    if (!inet_aton(gw_ip, &gw_addr.sin_addr)){
        perror("Gateway IP not valid");
        exit(1);
    }

    int udp_tmp = socket(AF_INET, SOCK_DGRAM, 0);

    if (sendto(udp_tmp, buffer, sizeof(message), 0, (struct sockaddr*)&gw_addr, sizeof(gw_addr)) < 0){
        perror("Failed UDP connection with gateway");
        exit(1);
    }

    if (recvfrom(udp_tmp, buffer, sizeof(message), 0, NULL, NULL) < 0){
        perror("Address UP reception failed");
        exit(1);
    }    

    message response;
    memcpy(&response, buffer, sizeof(message));
    strncpy(peer_up, response.up, 20);
    port_peer_up = response.port_pr;

    if (port_peer_up == -1){
        return -1;
    }

    (*peer_addr).sin_family = AF_INET;
    (*peer_addr).sin_port = htons(port_peer_up);
                            
    if (!inet_aton(peer_up, &((*peer_addr).sin_addr))){
        perror("UP peer IP not valid");
        exit(1);
    }

    close(udp_tmp);

    return 0;
}



int main(int argc, char *argv[]){
    
    if (argc < 3){
        printf("[IP gateway] [Port Gateway]\n");
        exit(1);
    }
    
    printf("PID: %d\n", getpid());

    tcp_port = 3000 + getpid();
    tcp_port_pr = 2000 + getpid();
    udp_port_gw = 1000 + getpid();
    
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
    pthread_t gw_msg;

    int err = pthread_create(&gw_msg, NULL, listen_to_gw, NULL);
    if(err != 0){
        perror("Thread creation error");
        exit(-1);
    }

    pthread_t pr_mess;

    err = pthread_create(&pr_mess, NULL, listen_to_peer, NULL);
    if (err != 0){
        perror("Thread creation error");
        exit(-1);
    }


    pthread_t threads[MAXCONN];
    int sockets[MAXCONN];
    memset(&sockets, -1, MAXCONN*sizeof(int));
    
    while(1){

        while (sockets[index_s] != -1){
            index_s++;
        }

        sockets[index_s] = accept_connection();

        //printf("%d, %d\n", index_s, sockets[index_s]);

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
