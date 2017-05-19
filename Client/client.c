#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "API.h"

int s_tcp_fd;


int main(int argc, char *argv[]){

    if (argc < 3){
        printf("[Gateway IP] [Gateway port]");
        exit(1);
    }
    
    char *gw_ip = argv[1];
    int gw_port = atoi(argv[2]);
    
    /* Setup UDP socket and send a request to gateway for a server address */
    
    int c_stat = gallery_connect(gw_ip, gw_port);

    if (c_stat < 0){
        printf("Connection with gateway/peer failed\n");
        exit(1);
    } else if (c_stat == 0){
        printf("No peers available\n");
        exit(1);
    } else {
        s_tcp_fd = c_stat;
        printf("%d\n", c_stat );
    }

    uint32_t ret = gallery_add_photo(s_tcp_fd, "./falls.jpg");

    printf("%d\n", ret);

    
    exit(0);
        

}
