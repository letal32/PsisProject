//#define SOCKET_NAME "cur_socket"
#define MAX_NAME_LEN 100

typedef struct message_gw{
    int type;
    char address[20];
    int port;
} message;

typedef struct photo{
    char name[100];
    char keywords[100];
    uint32_t identifier;
    struct photo *next;
} node;

typedef struct cmd_add{
	int code;
	int type;
	int size;
	char name[MAX_NAME_LEN];

} cmd_add;


void insert(node* new_node);
void printlist();