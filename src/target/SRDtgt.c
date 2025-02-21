///////////////////////////////////////////////////////////////////////////////////
// File : SRDtgt.c
// Contains: SimpleRemoteDesktop remote capture process.
//
// This file is part of SimpleRemoteDesktop.
//
// Written by: Jean-François DEL NERO
//
// Copyright (C) 2025 Jean-François DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <jpeglib.h>

#include <pthread.h>
#include <sys/time.h>

#include <sys/types.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#define SOCKET_ERROR        -1
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "screenshot.h"

#include "messages.h"
#include "jpeg.h"
#include "utils.h"
#include "cmd_param.h"

#if defined(__WIN32__)
    const char* inet_ntop(int af, const void* src, char* dst, size_t size);
    int inet_pton(int af, const char* src, void* dst);
#endif

typedef struct thread_params_
{
    int hSocket;
    int tid;
    char clientname[2048];
    char clientip[32];
    keymng * key;
}thread_params;

void *NetRx_Thread(void *threadid)
{
    int i,ret;
    unsigned char rx_buffer[NET_PACKET_SIZE];
    int hSocket;
    struct timeval curtime;
    time_t start_time;
    int recverror;
    struct timeval tv;
    int ofs;

    int err_loop_cnt;
    message_header header;
    char systemcmd[1024];

    scancodekeybevent * kevt;
    mouseevent * mevt;
    execcmd * execevt;

    thread_params * tp;

    tp = (thread_params*)threadid;

    pthread_detach(pthread_self());

    hSocket=tp->hSocket;
    recverror = 0;

    start_time = time(NULL);

    // wait until either socket has data ready to be recv()d (timeout 10.5 secs)

    tv.tv_sec = 20;
    tv.tv_usec = 0;
    setsockopt(hSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

    if(1)
    {
        printf("[%s] Connected\r\n",curdatestr());

        gettimeofday(&curtime,NULL);

        do
        {
            err_loop_cnt = 0;

            memset((char*)tp->key->lastrx,0,128/8);

            recverror = waitandreceivepacket(hSocket, tp->key, rx_buffer,NET_PACKET_SIZE);

            if(!recverror)
            {
                memcpy((void*)&header, (void*)&rx_buffer, sizeof(message_header) );
                if( header.sign == 0x41544144 )
                {
                    unsigned char * buf;

                    buf = malloc(header.user_size);
                    i = sizeof(message_header);
                    ofs = 0;

                    while(ofs < header.user_size && !recverror )
                    {
                        while(ofs < header.user_size && i < NET_PACKET_SIZE )
                        {
                            buf[ofs] = rx_buffer[i];
                            ofs++;
                            i++;
                        }
                        i = 0;

                        if( ofs < header.user_size )
                            recverror = waitandreceivepacket(hSocket, tp->key, rx_buffer, NET_PACKET_SIZE);
                    }

                    switch(header.type)
                    {
                        case MSGTYPE_PING:
                            //printf("ping received ! : %s \n",buf);
                        break;

                        case MSGTYPE_JPEGFULLSCREEN: // JPEG full screen
/*                          xsize = 0;
                            ysize = 0;
                            pic_ptr = NULL;
                            jpeg_decompress(buf, header.user_size, &pic_ptr, &xsize, &ysize);

                            if(tp->videobuf)
                            {
                                memcpy(tp->videobuf,pic_ptr,xsize*ysize*3);
                                tp->xsize = xsize;
                                tp->ysize = ysize;
                            }

                            free(pic_ptr);*/
                        break;

                        case MSGTYPE_SCANCODE_KEYBOARD_EVENT: // Keyboard scancode(s)
                            kevt = (scancodekeybevent *)buf;

                            switch(kevt->state)
                            {
                                case 0x00:
                                    for(int i=0;i<kevt->cnt;i++)
                                    {
                                        sprintf(systemcmd,"xdotool key 0x%X", kevt->buf[i]);
                                        ret = system(systemcmd);
                                        if(ret)
                                            printf("system : %s failed (%d)\n",systemcmd, ret);
                                    }
                                break;
                                case 0x01:
                                    for(int i=0;i<kevt->cnt;i++)
                                    {
                                        sprintf(systemcmd,"xdotool keydown 0x%X", kevt->buf[i]);
                                        ret = system(systemcmd);
                                        if(ret)
                                            printf("system : %s failed (%d)\n",systemcmd, ret);
                                    }
                                break;
                                case 0x02:
                                    for(int i=0;i<kevt->cnt;i++)
                                    {
                                        sprintf(systemcmd,"xdotool keyup 0x%X", kevt->buf[i]);
                                        ret = system(systemcmd);
                                        if(ret)
                                            printf("system : %s failed (%d)\n",systemcmd, ret);
                                    }
                                break;
                            }
                        break;

                        case MSGTYPE_MOUSE_EVENT: // Mouse position / buttons state

                            mevt = (mouseevent *)buf;

                            sprintf(systemcmd,"xdotool mousemove %d %d", mevt->xpos, mevt->ypos);
                            ret = system(systemcmd);
                            if(ret)
                                printf("system : %s failed (%d)\n",systemcmd, ret);

                            if(mevt->click)
                            {
                                if( mevt->click & 0x01 )
                                {
                                    sprintf(systemcmd,"xdotool mousedown 1" );

                                    if(mevt->click & 0x10)
                                        sprintf(systemcmd,"xdotool click --repeat 1 1" );

                                    ret = system(systemcmd);
                                    if(ret)
                                        printf("system : %s failed (%d)\n",systemcmd, ret);

                                }

                                if( mevt->click & 0x02 )
                                {
                                    sprintf(systemcmd,"xdotool mouseup 1" );

                                    ret = system(systemcmd);
                                    if(ret)
                                        printf("system : %s failed (%d)\n",systemcmd, ret);
                                }

                                if( mevt->click & 0x04 )
                                {
                                    sprintf(systemcmd,"xdotool mousedown 3" );

                                    if(mevt->click & 0x10)
                                        sprintf(systemcmd,"xdotool click --repeat 1 3" );

                                    ret = system(systemcmd);
                                    if(ret)
                                        printf("system : %s failed (%d)\n",systemcmd, ret);
                                }

                                if( mevt->click & 0x08 )
                                {
                                    sprintf(systemcmd,"xdotool mouseup 3" );

                                    ret = system(systemcmd);
                                    if(ret)
                                        printf("system : %s failed (%d)\n",systemcmd, ret);
                                }
                            }

                        break;

                        case MSGTYPE_EXEC:
                            execevt = (execcmd *)buf;

                            memset(systemcmd,0,sizeof(systemcmd));
                            for(int i=0;i<execevt->cnt && (i < sizeof(systemcmd) -  1);i++)
                            {
                                systemcmd[i] = execevt->buf[i];
                            }

                            ret = system(systemcmd);
                            if(ret)
                                printf("system : %s failed (%d)\n",systemcmd, ret);
                        break;
                    }

                    free(buf);

                }
            }
            else
            {
                printf("[%s] Recv Error. Connection Lost... \r\n",curdatestr());
            }

        }while(err_loop_cnt<32 && !recverror && ( (time(NULL) - start_time) < 3600));
    }
    else
    {

    }

    printf("[%s] Exit Server thread...\r\n",curdatestr());

/*
thread_exit:

    if(hSocket >= 0)
    {
        if(close(hSocket) == SOCKET_ERROR)
        {
            printf("[%s] Could not close socket\r\n",curdatestr());
        }
    }
*/
    pthread_exit(NULL);

    return NULL;
}

int main (int argc, char ** argv)
{
    int ret;

    uint8_t * buf;
    int xsize;
    int ysize;

    pthread_t threads;
    int i, rc;
    unsigned char * outbuffer;
    unsigned long outsize;

    int sockfd = 0;
    char recvBuff[1024];
    struct sockaddr_in serv_addr;
    thread_params tp;
    int quiet;
    keymng key;
    char tmp_str[512];
    int port;
    char address[512];

    mouseevent mevt;
    int mouse_x, mouse_y;
    int old_mouse_x, old_mouse_y;

    old_mouse_x = -1;
    old_mouse_y = -1;

    quiet = 0;
    if(isOption(argc, argv,"quiet",NULL, NULL) )
    {
        quiet = 1;
    }

    if(!quiet)
    {
        printf("Simple Remote Desktop v0.1 (Target)\n");
        printf("(c) 2025 Jean-François DEL NERO\n");
    }

    if(isOption(argc, argv,"password",(char*)&tmp_str, NULL) )
    {
        generatekey(&key, (char*)&tmp_str);
        memset((void*)tmp_str,0,sizeof(tmp_str));
    }
    else
    {
        printf("-password:[pass] option missing !\n");
        exit(-1);
    }

    port = 670;
    if(isOption(argc, argv,"port",(char*)&tmp_str, NULL) )
    {
        port = atoi(tmp_str);
    }

    sprintf(address,"127.0.0.1");
    if(isOption(argc, argv,"address",(char*)&address, NULL) )
    {
    }

    if(!quiet)
        printf("[%s] Connecting to %s (port %d) ...\n",curdatestr(),address, port);

    outbuffer = NULL;
    outsize = 0;

    buf = NULL;
    xsize = 0;
    ysize = 0;

    ret = 0;

    memset(recvBuff, '0',sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n[%s] Error ! Could not create socket \n",curdatestr());
        return 1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if( inet_pton(AF_INET, address, &serv_addr.sin_addr) <= 0 )
    {
        printf("\n[%s] Error ! inet_pton error occured !\n",curdatestr());
        return 1;
    }

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 )
    {
       printf("\n[%s] Error ! Connection Failed !\n",curdatestr());
       return 1;
    }

    memset(&tp,0,sizeof(tp));

    tp.hSocket = sockfd;
    tp.tid = 0;
    tp.key = &key;

    if(!quiet)
        printf("[%s] Starting RX thread...\r\n",curdatestr());

    rc = pthread_create(&threads, NULL, NetRx_Thread, (void *)&tp);
    if(rc)
    {
        printf("[%s] Error ! Can't Create the thread ! (Error %d)\r\n",curdatestr(),rc);
    }

    i = 0;
    for(;;)
    {
        ret = takescreenshot(buf, &xsize, &ysize);
        if(ret == -2)
        {
            free(buf);
            buf = malloc(sizeof(uint8_t) * 3 * xsize * ysize);
        }
        else
        {
            jpeg_compress(&outbuffer, &outsize, buf, xsize, ysize, 30);

            send_data(sockfd, &key, outbuffer, outsize,MSGTYPE_JPEGFULLSCREEN);

            getmouseposition(&mouse_x,&mouse_y);

            if(
                (mouse_x != old_mouse_x) ||
                (mouse_y != old_mouse_y)
            )
            {
                mevt.xpos = mouse_x;
                mevt.ypos = mouse_y;
                mevt.click = 0;

                send_data(sockfd, &key,(unsigned char*)&mevt, sizeof(mouseevent),MSGTYPE_MOUSE_EVENT);

                old_mouse_x = mouse_x;
                old_mouse_y = mouse_y;
            }

            usleep(1000 * 100);

            free(outbuffer);
        }
        i++;
    }

    exit(ret);
}
