#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "API.h"

int s_tcp_fd;


void add_photo(){
    printf("\nInsert the name of the picture you want to upload : \n");

    char *line = NULL;
    size_t size;
    if (getline(&line, &size, stdin) == -1) {
            printf("\nInvalid Command!\n");
            return;
    }

    line[strlen(line)-1] = '\0';

    //printf("%s\n", line);
    uint32_t ret = gallery_add_photo(s_tcp_fd, line);

    if (ret == 0){
        printf("\nSomething went wrong. Please retry!\n");
    } else {
        printf("\nThe picture have been successfully added. The ID of %s is %u\n\n", line, ret);
    }


}

void add_keyword(){
    printf("\nInsert the picture id: \n");

    char *line = NULL;
    size_t size;
    if (getline(&line, &size, stdin) == -1) {
            printf("\nInvalid ID!\n");
            return;
    }

    line[strlen(line)-1] = '\0';
    char ** endptr;

    uint32_t id = strtoul(line, endptr, 0);

    if (strlen(*endptr) != 0){
        printf("Invalid ID!\n");
        return;
    }


    printf("\nInsert the keyword you want to add: \n");
    line = NULL;

    if (getline(&line, &size, stdin) == -1) {
            printf("\nInvalid keyword!\n");
            return;
    }

    line[strlen(line)-1] = '\0';

    int ret = gallery_add_keyword(s_tcp_fd, id, line);

    if (ret == 1){
        printf("The keyword was successfully inserted in picture %d\n", id);
    } else{
        printf("Something went wrong. Try again!\n");
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
    
    int c_stat = gallery_connect(gw_ip, gw_port);

    if (c_stat < 0){
        printf("Connection with gateway/peer failed\n");
        exit(1);
    } else if (c_stat == 0){
        printf("No peers available\n");
        exit(1);
    } else {
        s_tcp_fd = c_stat;
        //printf("%d\n", c_stat );
    }

    while (1){
        printf("What do you want to do?\n");
        printf("1 - Add a picture to the gallery\n");
        printf("2 - Add a keyword to a picture\n");
        printf("3 - Remove a picture from the gallery\n");
        printf("4 - Search a picture in the gallery\n");
        printf("5 - Get the name of a picture in the gallery\n");
        printf("6 - Download a picture\n\n");

        char *line = NULL;
        size_t size;
        if (getline(&line, &size, stdin) == -1) {
            printf("\nInvalid Command!\n");
            continue;
        }

        int selection = atoi(line);

        if (selection == 1){
            add_photo();
        } else if (selection == 2){
            add_keyword();
        } else{
            printf("\nInvalid command!\n");
        }

    }

    
    
    exit(0);
        

}
