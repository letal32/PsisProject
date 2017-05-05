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

#include <sys/un.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>


int s_udp_cl;
int s_udp_pr;
node *head = NULL;
int num_servers = 0;

void * fromclient (void * arg);
void * frompeers (void * arg);
static void handler(int signum);
void gw_udp_setup();
node* insert(node *head, char *address, int port);
node get_server(node *head, int index);
char *serialize_msg(message m);
int mod(int a, int b);


/*
    All peers connections are received on port 4000
    All client connections are received on port 5000
*/


void * fromclient (void * arg){
   
   int cur_server_index = 0; 
        
   while(1)
   {
        
         char buffer[sizeof(message)]; 
                
         /* Listen for incoming connections and retrieve message */
         
         printf("Thread listening for incoming client connections\n");
         fflush(stdout);
         
         struct sockaddr_in recv_addr;
         socklen_t recv_size = sizeof(recv_addr);
         int r = recvfrom(s_udp_cl, buffer, sizeof(message), 0, (struct sockaddr *)&recv_addr, &recv_size);
         
         if (r < 0){
            perror("Receive Error");
            exit(1);
         }
         
         /* Copy buffer in a struct */
         
         message mess;
         memcpy(&mess, buffer, sizeof(mess));
         
         /* Send address of next server to client*/
         
         message assigned_server;
         if (num_servers == 0){
             assigned_server.type = 0;
             char *buffer = serialize_msg(assigned_server);
                
             if (sendto(s_udp_cl, buffer, sizeof(message), 0, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
                  perror("Failed UDP connection with client");
                  exit(1);
              }
              
          } else {
            
             node server = get_server(head, cur_server_index);
             cur_server_index = mod(cur_server_index + 1, num_servers);
             assigned_server.type = 1;
             strcpy(assigned_server.address, server.address);
             assigned_server.port = server.port;
                
             char *buffer = serialize_msg(assigned_server);
                
             if (sendto(s_udp_cl, buffer, sizeof(message), 0, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
                    perror("Failed UDP connection with client");
                    exit(1);
             }
          }
            
     }
}

void * frompeers (void * arg){

    while(1)
    {
        
         char buffer[sizeof(message)]; 
                
         /* Listen for incoming connections and retrieve message */
         printf("Thread listening for incoming peers connections\n");
         fflush(stdout);
         
         struct sockaddr_in recv_addr;
         socklen_t recv_size = sizeof(recv_addr);
         int r = recvfrom(s_udp_pr, buffer, sizeof(message), 0, (struct sockaddr *)&recv_addr, &recv_size);
         
         if (r < 0){
            perror("Receive Error");
            exit(1);
         }
         
         /* Copy buffer in a struct */
         
         message mess;
         memcpy(&mess, buffer, sizeof(mess));
         
         /* Save the address in linked list*/
         
         head = insert(head, inet_ntoa(recv_addr.sin_addr), mess.port);
         num_servers++;
       
    }

}

static void handler(int signum)
{
    if (close(s_udp_cl) < 0)
        perror("UDP socket not closed");
        
    if (close(s_udp_pr) < 0)
        perror("UDP socket not closed");    
        
    exit(0);
}

void gw_udp_setup(){

    s_udp_cl = socket(AF_INET, SOCK_DGRAM, 0);
    if ( s_udp_cl == -1) {
        perror("DGRAM socket error");
        exit(1);
    }
    
    struct sockaddr_in gw_addr;
    gw_addr.sin_family = AF_INET;
    gw_addr.sin_port = htons(4000);
    gw_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        
    if (bind(s_udp_cl, (struct sockaddr *)&gw_addr, sizeof(gw_addr)) < 0){
        perror("Binding error");
        exit(1);
    }
    
    s_udp_pr = socket(AF_INET, SOCK_DGRAM, 0);
    if ( s_udp_pr == -1) {
        perror("DGRAM socket error");
        exit(1);
    }
    
    gw_addr.sin_family = AF_INET;
    gw_addr.sin_port = htons(5000);
    gw_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        
    if (bind(s_udp_pr, (struct sockaddr *)&gw_addr, sizeof(gw_addr)) < 0){
        perror("Binding error");
        exit(1);
    }   
}

node* insert(node *head, char *address, int port){
    
    if (head == NULL){
        node *new_head = malloc(sizeof(node));
        strncpy(new_head->address, address, 20);
        new_head->port = port;
        new_head->next = NULL;
        return new_head;
    }
    
    node* cur_node = head;
    
    while (cur_node->next != NULL){
        cur_node = cur_node->next;
    }
    
    cur_node->next = malloc(sizeof(node));
    strncpy(cur_node->next->address, address, 20); 
    cur_node->next->port = port;
    cur_node->next->next = NULL;
    
    return head;
    
}

node get_server(node *head, int index){
    
    node *cur_node = head;
    int i = 0;
    
    while (cur_node->next != NULL && i < index){
        
        cur_node = cur_node->next;
        i++;
    }
    
    if (i < index) {
        perror("Index out of bounds");
        exit(1);
    }
    
    return *cur_node;

}

char *serialize_msg(message m){

    char *buffer;
    buffer = malloc(sizeof(m));
    memset(buffer, 0, sizeof(m));
    memcpy(buffer, &m, sizeof(m));
    
    return buffer;

}

int mod(int a, int b)
{
    int r = a % b;
    return r < 0 ? r + b : r;
}


int main(){
        
    /* Signal handling */
    
    struct sigaction sa;
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) < 0){
        perror("Error in handling SIGINT");
        exit(1);
    }
    
    /* Creates socket to receive datagrams  */
    
    gw_udp_setup();
    
    /* Create one thread to handle requests from clients and one to handle requests from peers */
    
    pthread_t thr_cl;
	pthread_t thr_pr;

	int error;
	error = pthread_create(&thr_cl, NULL, fromclient, NULL);
	if(error != 0){
		perror("Thread creation error");
		exit(-1);
	}

	error = pthread_create(&thr_pr, NULL, frompeers, NULL);
	if(error != 0){
		perror("Thread creation error");
		exit(-1);
	}
	
	pthread_join(thr_cl, NULL);
	pthread_join(thr_pr, NULL);

    exit(0);
    
}
