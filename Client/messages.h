#ifndef messages
#define messages

typedef struct message_gw{
    int type;
    char address[20];
    int port;
} message;

#endif
