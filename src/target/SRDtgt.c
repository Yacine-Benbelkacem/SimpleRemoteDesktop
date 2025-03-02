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

#ifdef WAYLAND_ENV
#include <pixman.h>
#include "wayland_screenshot.h"
#endif
#include "remote_controls_handler.h"

#if defined(__WIN32__)
    const char* inet_ntop(int af, const void* src, char* dst, size_t size);
    int inet_pton(int af, const char* src, void* dst);
#endif


int main (int argc, char ** argv)
{
    int ret;
    
    #ifdef WAYLAND_ENV
        pixman_image_t * buf;
    #else
        uint8_t * buf;
    #endif

    int xsize;
    int ysize;

    pthread_t threads;
    pthread_attr_t thread_attr;  

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
        printf("[%s] Starting RX controls thread...\r\n",curdatestr());

    if(pthread_attr_init(&thread_attr) != 0){
        printf("\n[%s] Error ! pthread_attr_init failed !\n",curdatestr());
        return 1;
    }

    pthread_attr_setdetachstate(&thread_attr,PTHREAD_CREATE_DETACHED);

    rc = pthread_create(&threads, &thread_attr, controls_handler, (void *)&tp);
    if(rc)
    {
        printf("[%s] Error ! Can't Create the thread ! (Error %d)\r\n",curdatestr(),rc);
    }

    i = 0;
    /*
    * For fast local area networks, we have to wait
    * server SDL initialization. 
    */
    sleep(1);
    for(;;)
    {
        #ifdef WAYLAND_ENV
            ret = wayland_screenshot(&buf);
            if(ret > 0)
            {
                //jpeg_compress(&outbuffer, &outsize, buf, xsize, ysize, 30);
                write_to_jpeg_stream(&outbuffer, &outsize, buf,100);
    
                send_data(sockfd, &key, outbuffer, outsize-1, MSGTYPE_JPEGFULLSCREEN);            
                
                usleep(1000);
            }
            else 
            {
        #else
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

        #endif
        
                free(outbuffer);
            }
        i++;
    }

    exit(ret);
}
