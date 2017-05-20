
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

int s_tcp_fd;
int s_udp_fd;
int tcp_port;
char *gw_ip;
int gw_port;
node *head = NULL;

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

        int * new_tcp_fd = (int *)socket;

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
                new_image->identifier = clock() * getpid();

                insert(new_image);
                printlist();

                cmd_add resp;

                resp.type = 1;
                resp.id = new_image->identifier;

                //printf("%d\n", cmd.id );

                char * response = serialize_cmd(resp);

                send(*new_tcp_fd, response, sizeof(cmd_add), 0);

            } else if (cmd.code == 11){

                //printf("IM HERE\n");

                node * photo_info = search(cmd.id);

                if (photo_info == NULL){
                    cmd_add resp;
                    resp.type = 2;
                    char * response = serialize_cmd(resp);
                    if (send(*new_tcp_fd, response, sizeof(cmd_add), 0) <= 0){
                        perror("Keyword send response error");
                        break;
                    }

                } else {

                    strncpy(photo_info->keywords + strlen(photo_info->keywords), cmd.keyword, MAX_KEYWORD_LEN);
                    printlist();

                    cmd_add resp;
                    resp.type = 1;
                    char * response = serialize_cmd(resp);
                    if (send(*new_tcp_fd, response, sizeof(cmd_add), 0) <= 0){
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
    
    //printf("(Name : %s , keywords: %s, identifier: %d, )\n", new_node->name, new_node->keywords, new_node->identifier );
    if (head == NULL){
        head = new_node;
        head->next = NULL;
   
    } else {
        new_node->next = head;
        head = new_node;
    }
    
}

node* search(uint32_t id){

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

/*
node* remove_node(node *head, char *address, int port){

    if (head == NULL)
        return head;

    if (strcmp(address, head->address) == 0 && port == head->port){

        node *new_head = head->next;
        free(head);

        return new_head;
    }

    node* cur_node = head;
    
    while (cur_node->next != NULL){

        if (strcmp(address, cur_node-> next-> address) == 0 && port == cur_node-> next-> port){

            node *temp = cur_node->next->next;
            free(cur_node->next);
            cur_node->next = temp;
            return head;
        }

        cur_node = cur_node->next;
    }    

    return head;
}
*/

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
