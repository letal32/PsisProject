//#define SOCKET_NAME "cur_socket"
#define MAX_NAME_LEN 100
#define MAX_KEYWORD_LEN 50
#define MAX_KEYWORD_TOT_LEN 100

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

typedef struct photo{
    char name[MAX_NAME_LEN];
    char keywords[MAX_KEYWORD_TOT_LEN];
    uint32_t identifier;
    struct photo *next;
} node;

typedef struct cmd_add{
    int code;
    int type;
    int size;
    int source; //Added, pay attention if errors appear
    char name[MAX_NAME_LEN];
    char keyword[MAX_KEYWORD_TOT_LEN];
    uint32_t id;

} cmd_add;



void insert(node* new_node);
void printlist();
void * upload_pic();

node* search_by_id(uint32_t id);
int remove_node(uint32_t id);
node* search_by_keyword(node* start, char* keyword);
int get_new_peer_address(struct sockaddr_in* peer_addr);