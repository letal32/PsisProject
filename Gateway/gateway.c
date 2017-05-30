#include "gateway.h"

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
pthread_rwlock_t rwlock;
int counter;

void * fromclient (void * arg);
void * frompeers (void * arg);
static void handler(int signum);
void gw_udp_setup();
node* insert(char *address, int port, int port_gw, int port_pr);
node get_server(node *head, int index);
char *serialize_msg(message m);
int mod(int a, int b);
node* remove_node(char *address, int port);
void printlist();


/*
    All peers connections are received on port 5000
    All client connections are received on port 4000
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
         if (mess.type == 0){
           message assigned_server;
           if (num_servers == 0){
               assigned_server.type = 0;
               char *buffer = serialize_msg(assigned_server);
                  
               if (sendto(s_udp_cl, buffer, sizeof(message), 0, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
                    perror("Failed UDP connection with client");
                    exit(1);
                }
                
            } else {
              
               pthread_rwlock_rdlock(&rwlock);
               node server;
               if (cur_server_index < num_servers){
                  server = get_server(head, cur_server_index);
                  cur_server_index++;
               } else {
                  server = *head;
                  cur_server_index = 0;
               }

               //cur_server_index = mod(cur_server_index + 1, num_servers);
               assigned_server.type = 1;
               strncpy(assigned_server.address, server.address,20);
               assigned_server.port = server.port;

               pthread_rwlock_unlock(&rwlock);

               char *buffer = serialize_msg(assigned_server);
                  
               if (sendto(s_udp_cl, buffer, sizeof(message), 0, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
                      perror("Failed UDP connection with client");
                      exit(1);
               }
            }

        } else if (mess.type == 1){

            //Remove the server and inform the old peer of the new UP address, if any
            node *down = remove_node(mess.address, mess.port);
            printf("NUM SERVERS: %d\n", num_servers);

            if (down != NULL){
              printf("ERROR NULL\n");
            

            message to_old_peer;
            if (num_servers >= 1){
                to_old_peer.type = 3;
                to_old_peer.subtype = 0;
                if (num_servers == 1){
                    to_old_peer.port_pr = -1;
                } else{
                    if (down->next != NULL){
                      to_old_peer.port_pr = down->next->port_pr;
                      strncpy(to_old_peer.up, down->next->address, 20);
                    } else {
                      to_old_peer.port_pr = head->port_pr;
                      strncpy(to_old_peer.up, head->address, 20);
                    }
                    
                }

            }

            struct sockaddr_in old_peer_addr;
            old_peer_addr.sin_family = AF_INET;
            old_peer_addr.sin_port = htons(down->port_gw);
        
            if (!inet_aton(down->address, &old_peer_addr.sin_addr)){
                perror("Old peer IP not valid");
                exit(1);
            }

            memcpy(buffer, &to_old_peer, sizeof(message));

            printf("GIVING TO OLD PEER A NEW ADDRESS\n");

            if (sendto(s_udp_cl, buffer, sizeof(to_old_peer), 0, (struct sockaddr*)&old_peer_addr, sizeof(old_peer_addr)) < 0){
                perror("Failed UDP connection with peer");
                exit(1);
            }

            printf("PEER GOT NEW ADDRESS\n");
          }

            //Give a new address to the client
             message assigned_server;
             if (num_servers == 0){
                 assigned_server.type = 0;
                 char *buffer = serialize_msg(assigned_server);

                 printf("NO SERVERS AVAILABLE... SENDING RESPONSE\n");
                    
                 if (sendto(s_udp_cl, buffer, sizeof(message), 0, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
                      perror("Failed UDP connection with client");
                      exit(1);
                  }
                  
              } else {
                
                 pthread_rwlock_rdlock(&rwlock);
                 node server;
                 if (cur_server_index < num_servers){
                    server = get_server(head, cur_server_index);
                    cur_server_index++;
                 } else {
                    server = *head;
                    cur_server_index = 0;
                 }

                 cur_server_index = mod(cur_server_index + 1, num_servers);
                 assigned_server.type = 1;
                 strncpy(assigned_server.address, server.address,20);
                 assigned_server.port = server.port;

                 pthread_rwlock_unlock(&rwlock);

                 char *buffer = serialize_msg(assigned_server);

                 printf("SERVER AVAILABLE SENDING TO CLIENT... (%s, %d)\n", assigned_server.address, assigned_server.port );
                    
                 if (sendto(s_udp_cl, buffer, sizeof(message), 0, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
                        perror("Failed UDP connection with client");
                        exit(1);
                 }



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
         
         if (mess.type == 0){

            /* Save the address in linked list*/
            pthread_rwlock_wrlock(&rwlock);
            node* node_down = insert(inet_ntoa(recv_addr.sin_addr), mess.port, mess.port_gw, mess.port_pr);
            num_servers++;

            message to_new_peer;
            to_new_peer.type = 1;
            to_new_peer.subtype = 0;
            to_new_peer.id = counter++; 
            
            if (num_servers > 1){
                to_new_peer.port_pr = head->port_pr;
                strncpy(to_new_peer.up, head->address, 20);
            } else {
                to_new_peer.port_pr = -1;
            }

            message to_old_peer;
            char old_peer[20];
            int old_peer_port;
            if (num_servers > 1){
                to_old_peer.type = 1;
                to_old_peer.subtype = 1;
                to_old_peer.port_pr = node_down->next->port_pr;
                strncpy(to_old_peer.up, node_down->next->address, 20);

                strncpy(old_peer, node_down->address, 20);
                old_peer_port = node_down->port_gw;
            }

            pthread_rwlock_unlock(&rwlock);

            memcpy(buffer, &to_new_peer, sizeof(to_new_peer));

            if (sendto(s_udp_pr, buffer, sizeof(to_new_peer), 0, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
                perror("Failed UDP connection with peer");
                exit(1);
            }

            if (num_servers > 1){

                    struct sockaddr_in old_peer_addr;
                    int status = 0;
                    int tmp_udp = socket(AF_INET, SOCK_DGRAM, 0);
                    while (1){
                      old_peer_addr.sin_family = AF_INET;
                      old_peer_addr.sin_port = htons(old_peer_port);

          
                      if (!inet_aton(old_peer, &old_peer_addr.sin_addr)){
                          perror("Old peer IP not valid");
                          exit(1);
                      }



                      message ping;
                      ping.type = 4;
                      memcpy(buffer, &ping, sizeof(ping));
                      if (sendto(tmp_udp, buffer, sizeof(ping), 0, (struct sockaddr*)&old_peer_addr, sizeof(old_peer_addr)) < 0){
                        perror("Failed UDP connection with peer");
                        exit(1);
                      }

                      struct timeval tv;
                      tv.tv_sec = 2;
                      if (setsockopt(tmp_udp, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
                          perror("Error");
                      }

                      if (recvfrom(tmp_udp, buffer, sizeof(ping), 0, NULL, NULL) < 0){
                        perror("Ping problem!");
                        node* down = remove_node(old_peer, old_peer_port);
                        if (num_servers == 1){
                          status = 1;
                          break;
                        } else {
                          strncpy(old_peer, down->address, 20);
                          old_peer_port = down->port_gw;
                        }
                      } else {
                        break;
                      }

                    }
                    close(tmp_udp);

                    if (status == 1)
                      continue;

                    memcpy(buffer, &to_old_peer, sizeof(to_old_peer));
                    if (sendto(s_udp_pr, buffer, sizeof(to_old_peer), 0, (struct sockaddr*)&old_peer_addr, sizeof(old_peer_addr)) < 0){
                        perror("Failed UDP connection with peer");
                        exit(1);
                    }
            }


         } else if (mess.type == 2){

            /* Remove the address from the linked list*/
            pthread_rwlock_wrlock(&rwlock);
            node* node_down = remove_node(inet_ntoa(recv_addr.sin_addr), mess.port);
            printf("NUM SERVERS: %d\n", num_servers);

            message to_old_peer;
            char old_peer[20];
            int old_peer_port;
            if (num_servers >= 1){
                to_old_peer.type = 1;
                to_old_peer.subtype = 0;
                if (num_servers == 1){
                    to_old_peer.port_pr = -1;
                } else{
                    if (node_down->next != NULL){
                      to_old_peer.port_pr = node_down->next->port_pr;
                      strncpy(to_old_peer.up, node_down->next->address, 20);
                    } else {
                      to_old_peer.port_pr = head->port_pr;
                      strncpy(to_old_peer.up, head->address, 20);
                    }
                    
                }

                strncpy(old_peer, node_down->address, 20);
                old_peer_port = node_down->port_gw;

            }

            pthread_rwlock_unlock(&rwlock);

            if (num_servers >= 1){

                    struct sockaddr_in old_peer_addr;
                    old_peer_addr.sin_family = AF_INET;
                    old_peer_addr.sin_port = htons(old_peer_port);
        
                    if (!inet_aton(old_peer, &old_peer_addr.sin_addr)){
                        perror("Old peer IP not valid");
                        exit(1);
                    }

                    memcpy(buffer, &to_old_peer, sizeof(to_old_peer));
                    if (sendto(s_udp_pr, buffer, sizeof(to_old_peer), 0, (struct sockaddr*)&old_peer_addr, sizeof(old_peer_addr)) < 0){
                        perror("Failed UDP connection with peer");
                        exit(1);
                    }
            }



         } else if (mess.type == 3){

            node *down = remove_node(mess.up, mess.port_pr);
            printf("NUM SERVERS: %d\n", num_servers);

            message to_old_peer;
            if (num_servers >= 1){
                to_old_peer.type = 3;
                to_old_peer.subtype = 0;
                if (num_servers == 1){
                    to_old_peer.port_pr = -1;
                } else{
                    if (down->next != NULL){
                      to_old_peer.port_pr = down->next->port_pr;
                      strncpy(to_old_peer.up, down->next->address, 20);
                    } else {
                      to_old_peer.port_pr = head->port_pr;
                      strncpy(to_old_peer.up, head->address, 20);
                    }
                    
                }

            }

            memcpy(buffer, &to_old_peer, sizeof(message));

            if (sendto(s_udp_pr, buffer, sizeof(to_old_peer), 0, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
                perror("Failed UDP connection with peer");
                break;
            }

         } 

         printlist();
       
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

//Return the node before the one added
node* insert(char *address, int port, int port_gw, int port_pr){
    
    if (head == NULL){
        head = malloc(sizeof(node));
        strncpy(head->address, address, 20);
        head->port = port;
        head->port_gw = port_gw;
        head->port_pr = port_pr;
        head->next = NULL;
        return NULL;
    }
    
    node* cur_node = head;
    
    while (cur_node->next != NULL){
        cur_node = cur_node->next;
    }
    
    cur_node->next = malloc(sizeof(node));
    strncpy(cur_node->next->address, address, 20); 
    cur_node->next->port = port;
    cur_node->next->port_gw = port_gw;
    cur_node->next->port_pr = port_pr;
    cur_node->next->next = NULL;
    
    return cur_node;
    
}

//Return the node before the one removed
node* remove_node(char *address, int port){

    if (head == NULL)
        return NULL;


    if (strcmp(address, head->address) == 0 && (port == head->port || port == head->port_pr || port == head->port_gw)){


        if (head->next == NULL){
          free(head);
          head = NULL;
          num_servers--;
          return NULL;
        }

        node *new_head = head->next;
        free(head);

        head = new_head;

        node* cur_node = head;
        
        while (cur_node->next != NULL){
            cur_node = cur_node->next;
        }

        num_servers--;


        return cur_node;


    }

    node* cur_node = head;
    
    while (cur_node->next != NULL){


        if (strcmp(address, cur_node-> next-> address) == 0 && (port == cur_node-> next-> port || port == cur_node->next->port_pr || port == cur_node->next->port_gw)){

            node *temp;

            if (cur_node->next->next != NULL)
              temp = cur_node->next->next;
            else
              temp = NULL;

            free(cur_node->next);
            cur_node->next = temp;

            num_servers--;

            return cur_node;
        }

        cur_node = cur_node->next;
    }    

    return NULL;
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

    //printf("%s\n", cur_node->address);
    
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

void printlist(){

    if (head == NULL)
        return;

    int k = 0;
    node * cur = head;
    while (cur->next != NULL){
        printf("Entry %d: (%s), (%d), (%d), (%d)\n", k, cur->address, cur->port, cur->port_gw, cur->port_pr);
        fflush(stdout);
        k++;
        cur = cur->next;
    }

    if (cur->next == NULL){
        printf("Entry %d: (%s), (%d), (%d), (%d)\n", k, cur->address, cur->port, cur->port_gw, cur->port_pr);
        fflush(stdout);
        k++;
    }

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
    pthread_rwlock_init(&rwlock,NULL);

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
