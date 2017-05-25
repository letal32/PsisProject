#ifndef messages
#define messages
#define MAX_NAME_LEN 100
#define MAX_KEYWORD_LEN 50
#define MAX_KEYWORD_TOT_LEN 100

typedef struct message_gw{
    int type;
    int subtype;
    char address[20];
    int port;
    int port_gw;
    int port_pr;
    char up[20];
} message;


typedef struct cmd_add{
	int code;
	int type;
	int size;
	char name[MAX_NAME_LEN];
	char keyword[MAX_KEYWORD_TOT_LEN];
	uint32_t id;

} cmd_add;

#endif
