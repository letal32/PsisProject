//#define SOCKET_NAME "cur_socket"
//#define MESSAGE_LEN 100

typedef struct message_gw{
    int type;
    int subtype;
    char address[20];
    int id;
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


