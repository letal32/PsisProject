//#define SOCKET_NAME "cur_socket"
//#define MESSAGE_LEN 100

typedef struct message_gw{
    int type;
    int subtype;
    char address[20];
    int port;
    int port_gw;
    int port_pr;
    char up[20];
} message;

typedef struct server{
    char address[20];
    int port;
    int port_gw;
    int port_pr;
    struct server *next;
} node;


