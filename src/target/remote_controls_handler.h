#include "messages.h"

typedef struct thread_params_
{
    int hSocket;
    int tid;
    char clientname[2048];
    char clientip[32];
    keymng * key;
}thread_params;

void *controls_handler(void *params);