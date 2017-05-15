//#define SOCKET_NAME "cur_socket"
//#define MESSAGE_LEN 100

typedef struct message_gw{
    int type;
    char address[20];
    int port;
} message;

typedef struct server{
    char address[20];
    int port;
    struct server *next;
} node;


