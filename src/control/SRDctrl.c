///////////////////////////////////////////////////////////////////////////////////
// File : SRDctrl.c
// Contains: SimpleRemoteDesktop viewer.
//
// This file is part of SimpleRemoteDesktop.
//
// Written by: Jean-François DEL NERO
//
// Copyright (C) 2025 Jean-François DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
#define SOCKET_ERROR        -1
#endif

#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include "messages.h"
#include "jpeg.h"
#include "utils.h"
#include "cmd_param.h"

#define QUEUE_SIZE          5
#define MAXCONNECTION 128

#define DATA_PACKET_SIZE ((2*1024) + 246)

#pragma pack(1)

typedef struct ip_packet_
{
    unsigned int type;
    unsigned int cnt;
    unsigned char data[DATA_PACKET_SIZE];
    unsigned short crc;
}ip_packet;

#pragma pack()

typedef struct thread_params_
{
    int hSocket;
    int tid;
    char clientname[2048];
    char clientip[32];
    SDL_Surface *window_surface;
    SDL_Surface *canvas;
    unsigned char * videobuf;
    int xsize;
    int ysize;
    keymng * key;
}thread_params;

pthread_t     * threads;
thread_params * threadparams[MAXCONNECTION];

typedef struct sdlkeymap_
{
    int sdl_code;
    int key_code;

}sdlkeymap;

sdlkeymap kmap[]=
{
    { SDLK_BACKSPACE, 0xff08 },
    { SDLK_RETURN,    0xff8d },
    { SDLK_ESCAPE,    0xff1b },
    { SDLK_UP,        0xFF52 },
    { SDLK_DOWN,      0xFF54 },
    { SDLK_LEFT,      0xFF51 },
    { SDLK_RIGHT,     0xFF53 },

    { -1 , 0x0000 }
};

Uint32 timer_cb(Uint32 interval, void *param)
{
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);

    return(interval);
}

void *Display_Thread(void *threadid)
{
    SDL_Window *window;
    thread_params * tp;
    int done;
    int xsize,ysize;
    uint32_t delay;
    int ctrldown;

    ctrldown = 0;
    done = 0;
    xsize = 1920;
    ysize = 1080;

    tp = (thread_params*)threadid;

    window = SDL_CreateWindow("Simple Remote Desktop",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              xsize, ysize,
                              SDL_WINDOW_SHOWN);

    if(!window)
    {
        printf("%s\n",SDL_GetError());
        return NULL;
    }

    SDL_Delay( 100 );

    SDL_SetWindowBordered( window, SDL_TRUE );

    delay = 500;
    SDL_AddTimer(delay, timer_cb, threadid);

    tp->window_surface = SDL_GetWindowSurface(window);

    tp->canvas = SDL_CreateRGBSurfaceWithFormat( 0, xsize, ysize, 24, SDL_PIXELFORMAT_RGB24);

    tp->videobuf = malloc(xsize*ysize*sizeof(uint32_t));

    if(tp->videobuf)
        memset(tp->videobuf,0,xsize*ysize*sizeof(uint32_t));

    SDL_StartTextInput();

    while (!done)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                done = 1;
            }

            if (event.type == SDL_USEREVENT)
            {
                char tmpstr[64];
                sprintf((char*)tmpstr, (char*)"Ping !\n");
                send_data(tp->hSocket, tp->key, (unsigned char*)tmpstr, strlen((char*)tmpstr),0x00000000);
            }

            if (event.type == SDL_MOUSEMOTION)
            {
                mouseevent mevt;

                mevt.xpos = event.motion.x;
                mevt.ypos = event.motion.y;
                mevt.click = 0;

                send_data(tp->hSocket, tp->key, (void*)&mevt, sizeof(mouseevent),0x00000003);
            }

            if( event.type == SDL_KEYDOWN )
            {
                if(SDL_GetModState() & KMOD_CTRL)
                {
                    unsigned char tmp[512];
                    scancodekeybevent * kevt;

                    kevt = (scancodekeybevent *)&tmp;

                    kevt->state = 0x0001;
                    kevt->cnt = 1;
                    kevt->buf[0] = 0xffe3;
                    send_data(tp->hSocket, tp->key, (void*)kevt, sizeof(scancodekeybevent) + (kevt->cnt*sizeof(uint16_t)),MSGTYPE_SCANCODE_KEYBOARD_EVENT);
                    ctrldown = 1;
                }

                //Handle copy
                if( event.key.keysym.sym == SDLK_c && SDL_GetModState() & KMOD_CTRL )
                {
                    //SDL_SetClipboardText( inputText.c_str() );

                    unsigned char tmp[512];
                    scancodekeybevent * kevt;

                    kevt = (scancodekeybevent *)&tmp;

                    kevt->state = 0x0000;
                    kevt->cnt = 1;
                    kevt->buf[0] = 'c';
                    send_data(tp->hSocket, tp->key, (void*)kevt, sizeof(scancodekeybevent) + (kevt->cnt*sizeof(uint16_t)),MSGTYPE_SCANCODE_KEYBOARD_EVENT);
                }
                //Handle paste
                else if( event.key.keysym.sym == SDLK_v && SDL_GetModState() & KMOD_CTRL )
                {
                    //Copy text from temporary buffer
                    char* tempText = SDL_GetClipboardText();
                    printf("-- %s\n",tempText);
                    //inputText = tempText;
                    SDL_free( tempText );

                    unsigned char tmp[512];
                    scancodekeybevent * kevt;

                    kevt = (scancodekeybevent *)&tmp;

                    kevt->state = 0x0000;
                    kevt->cnt = 1;
                    kevt->buf[0] = 'v';
                    send_data(tp->hSocket, tp->key, (void*)kevt, sizeof(scancodekeybevent) + (kevt->cnt*sizeof(uint16_t)),MSGTYPE_SCANCODE_KEYBOARD_EVENT);

                }
                else if( event.key.keysym.sym == SDLK_x && SDL_GetModState() & KMOD_CTRL )
                {
                    unsigned char tmp[512];
                    scancodekeybevent * kevt;

                    kevt = (scancodekeybevent *)&tmp;

                    kevt->state = 0x0000;
                    kevt->cnt = 1;
                    kevt->buf[0] = 'x';
                    send_data(tp->hSocket, tp->key, (void*)kevt, sizeof(scancodekeybevent) + (kevt->cnt*sizeof(uint16_t)),MSGTYPE_SCANCODE_KEYBOARD_EVENT);
                }
                else
                {
                    int i;
                    i = 0;
                    while(kmap[i].sdl_code >= 0)
                    {

                        if(event.key.keysym.sym == kmap[i].sdl_code)
                        {
                            unsigned char tmp[512];
                            scancodekeybevent * kevt;

                            kevt = (scancodekeybevent *)&tmp;

                            kevt->state = 0x0000;
                            kevt->cnt = 1;
                            kevt->buf[0] = kmap[i].key_code;
                            send_data(tp->hSocket, tp->key, (void*)kevt, sizeof(scancodekeybevent) + (kevt->cnt*sizeof(uint16_t)),MSGTYPE_SCANCODE_KEYBOARD_EVENT);
                        }
                        i++;
                    }
                }
            }

            if( event.type == SDL_KEYUP )
            {
                if(ctrldown && !(SDL_GetModState() & KMOD_CTRL))
                {
                    unsigned char tmp[512];
                    scancodekeybevent * kevt;

                    kevt = (scancodekeybevent *)&tmp;

                    kevt->state = 0x0002;
                    kevt->cnt = 1;
                    kevt->buf[0] = 0xffe3;
                    send_data(tp->hSocket, tp->key, (void*)kevt, sizeof(scancodekeybevent) + (kevt->cnt*sizeof(uint16_t)),MSGTYPE_SCANCODE_KEYBOARD_EVENT);

                    ctrldown = 0;
                }
            }

            if( event.type == SDL_TEXTINPUT )
            {
                unsigned char tmp[512];
                scancodekeybevent * kevt;

                kevt = (scancodekeybevent *)&tmp;
                kevt->state = 0x0000;
                //Not copy or pasting
                if( !( (SDL_GetModState() & KMOD_CTRL) && ( event.text.text[ 0 ] == 'c' || event.text.text[ 0 ] == 'C' || event.text.text[ 0 ] == 'v' || event.text.text[ 0 ] == 'V' ) ) )
                {
                    //Append character
                    kevt->cnt = strlen(event.text.text);
                    for(int i=0;i<kevt->cnt;i++)
                    {
                        kevt->buf[i] = event.text.text[i];
                    }

                    send_data(tp->hSocket, tp->key, (void*)kevt, sizeof(scancodekeybevent) + kevt->cnt,MSGTYPE_SCANCODE_KEYBOARD_EVENT);
                }
            }

            if ( (event.type == SDL_MOUSEBUTTONDOWN) || (event.type == SDL_MOUSEBUTTONUP) )
            {
                mouseevent mevt;

                mevt.xpos = event.button.x;
                mevt.ypos = event.button.y;
                mevt.click = 0;

                // 3 2 1 0
                // RURDLULD
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    if (event.type == SDL_MOUSEBUTTONDOWN)
                    {
                        mevt.click = 0x1;
                        if ( event.button.clicks == 2 )
                        {
                            mevt.click |= 0x10;
                        }
                    }
                    else
                    {
                        mevt.click = 0x2;
                    }
                }
                else
                {
                    if (event.button.button == SDL_BUTTON_RIGHT)
                    {
                        if (event.type == SDL_MOUSEBUTTONDOWN)
                        {
                            mevt.click = 0x4;
                            if ( event.button.clicks == 2 )
                            {
                                mevt.click |= 0x10;
                            }
                        }
                        else
                        {
                            mevt.click = 0x8;
                        }
                    }
                }

                send_data(tp->hSocket, tp->key, (void*)&mevt, sizeof(mouseevent),MSGTYPE_MOUSE_EVENT);
            }
        }

        unsigned char  *buffer = (unsigned char*) tp->canvas->pixels;
        unsigned char  *row;

        SDL_LockSurface(tp->canvas);
        int pitch = tp->canvas->pitch;

        for (int i=0; i<ysize; i++)
        {
            row = (unsigned char*)((char*)buffer + (pitch * i) );
            memcpy(row,&tp->videobuf[i*tp->xsize*3],3*tp->xsize);
        }

        SDL_UnlockSurface(tp->canvas);

        SDL_BlitSurface(tp->canvas, 0, tp->window_surface, 0);

        SDL_UpdateWindowSurface(window);

        SDL_Delay( 100 );
    }

    SDL_DestroyWindow(window);

    return NULL;
}

void plot_pix(uint8_t *buf, int xsize,int ysize, int x, int y, uint32_t col)
{
    if( (x<0 || y<0) || (x>=xsize || y>=ysize) )
    {
        return;
    }

    buf[(((y * xsize) + x) * 3) + 0] = (col) & 0xFF;
    buf[(((y * xsize) + x) * 3) + 1] = (col>>8) & 0xFF;
    buf[(((y * xsize) + x) * 3) + 2] = (col>>16) & 0xFF;
}

void draw_mouse_pointer(uint8_t *buf, int xsize,int ysize, int x, int y)
{
    int i;

    // w      b      w
    //  w     b     w
    //   w    b    w
    //    w   b   w
    //     w  b  w
    //      w b w
    //       wbw
    // bbbbbbbBbbbbbbb
    //       wbw
    //      w b w
    //     w  b  w
    //    w   b   w
    //   w    b    w
    //  w     b     w
    // w      b      w

    for( i= 0 ; i < 8; i++)
    {
        plot_pix(buf, xsize, ysize, x + i, y + i, 0xFFFFFF);
        plot_pix(buf, xsize, ysize, x - i, y - i, 0xFFFFFF);
        plot_pix(buf, xsize, ysize, x + i, y - i, 0xFFFFFF);
        plot_pix(buf, xsize, ysize, x - i, y + i, 0xFFFFFF);
    }

    for( i= 0 ; i < 8; i++)
    {
        plot_pix(buf, xsize, ysize, x + i, y, 0x000000);
        plot_pix(buf, xsize, ysize, x - i, y, 0x000000);
        plot_pix(buf, xsize, ysize, x, y + i, 0x000000);
        plot_pix(buf, xsize, ysize, x, y - i, 0x000000);
    }
}

void *NetRx_Thread(void *threadid)
{
    int tid,i;
    unsigned char rx_buffer[NET_PACKET_SIZE];
    int hSocket;
    struct timeval curtime;
    time_t start_time;
    int recverror;
    int mess_cnt;
    int ofs;
    int mouse_x,mouse_y;
    mouseevent * mevt;

    unsigned char * pic_ptr;
    int xsize,ysize;

    int err_loop_cnt;
    message_header header;

    thread_params * tp;

    tp = (thread_params*)threadid;

    pthread_detach(pthread_self());

    hSocket=tp->hSocket;
    tid=tp->tid;
    recverror = 0;

    mouse_x = 0;
    mouse_y = 0;

    mess_cnt = 0;

    start_time = time(NULL);

    // wait until either socket has data ready to be recv()d (timeout 10.5 secs)

    printf("[%s] Incoming connection : %s\r\n",curdatestr(), tp->clientname);

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
                    case MSGTYPE_JPEGFULLSCREEN: // JPEG full screen
                        xsize = 0;
                        ysize = 0;
                        pic_ptr = NULL;
                        jpeg_decompress(buf, header.user_size, &pic_ptr, &xsize, &ysize);

                        if(tp->videobuf)
                        {
                            memcpy(tp->videobuf,pic_ptr,xsize*ysize*3);
                            tp->xsize = xsize;
                            tp->ysize = ysize;

                            draw_mouse_pointer(tp->videobuf, xsize, ysize, mouse_x, mouse_y);
                        }
                        mess_cnt++;

                        free(pic_ptr);
                    break;
                    case MSGTYPE_SCANCODE_KEYBOARD_EVENT: // Keyboard scancode(s)
                    break;
                    case MSGTYPE_MOUSE_EVENT: // Mouse position / buttons state
                        mevt = (mouseevent *)buf;

                        mouse_x = mevt->xpos;
                        mouse_y = mevt->ypos;
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

    printf("[%s] Exit Server thread...\r\n",curdatestr());

//thread_exit:

    if(hSocket >= 0)
    {
        if(close(hSocket) == SOCKET_ERROR)
        {
            printf("[%s] Could not close socket\r\n",curdatestr());
        }
    }

    threadparams[tid]=0x0;

    pthread_exit(NULL);

    return NULL;
}

int main(int argc, char* argv[])
{
    int hSocket,hServerSocket;  /* handle to socket */
    struct sockaddr_in Address; /* Internet socket address stuct */
    struct sockaddr_in Address_con; /* Internet socket address stuct */
    //struct sigaction sa;
    int nAddressSize=sizeof(struct sockaddr_in);
    int nAddressSize_con=sizeof(struct sockaddr_in);
    int nHostPort;
    int rc,i;
    thread_params * tp;
    char hostname[512];
    char servname[512];
    char password[512];
    char tmp_str[512];
    int connection;
    int quiet;
    int port;
    keymng key;

#ifdef __WIN32__
    WSADATA wsaData;
    DWORD tv;
#else
    struct timeval tv;
#endif

    quiet = 0;
    if(isOption(argc, argv,"quiet",NULL, NULL) )
    {
        quiet = 1;
    }

    if(!quiet)
    {
        printf("Simple Remote Desktop v0.1 (Controller)\n");
        printf("(c) 2025 Jean-François DEL NERO\n");
    }

    if(isOption(argc, argv,"password",(char*)&password, NULL) )
    {
        generatekey(&key, (char*)&password);
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

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
    {
        return -1;
    }

#ifdef __WIN32__
    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != NO_ERROR)
    {
        return -1;
    }
#endif

    connection=0;

    threads = malloc(sizeof(pthread_t) * MAXCONNECTION);
    if(!threads)
    {
        printf("[%s] Allocation error !\r\n",curdatestr());
        return 0;
    }

    memset(threads,0,sizeof(pthread_t) * MAXCONNECTION);

    memset(&threadparams,0,sizeof(thread_params*) * MAXCONNECTION);

    nHostPort = port;

    printf("[%s] Starting server\r\n",curdatestr());

    printf("[%s] Making socket\r\n",curdatestr());
    /* make a socket */
    hServerSocket=socket(AF_INET,SOCK_STREAM,0);

    if(hServerSocket == SOCKET_ERROR)
    {
        printf("[%s] Could not make a socket\r\n",curdatestr());
        return 0;
    }

#ifdef __WIN32__
    tv = 20*1000;
    setsockopt(hServerSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(DWORD));
#else
    tv.tv_sec = 20;
    tv.tv_usec = 0;
    setsockopt(hServerSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
#endif
    /* fill address struct */
    Address.sin_addr.s_addr=INADDR_ANY;
    Address.sin_port=htons(nHostPort);
    Address.sin_family=AF_INET;

    printf("[%s] Binding to port %d\r\n",curdatestr(),nHostPort);

    /* bind to a port */
    if(bind(hServerSocket,(struct sockaddr*)&Address,sizeof(Address))
                        == SOCKET_ERROR)
    {
        printf("[%s] Could not connect to host\r\n",curdatestr());
        return 0;
    }

    /*  get port number */
    getsockname( hServerSocket, (struct sockaddr *) &Address,(socklen_t *)&nAddressSize);
    printf("[%s] opened socket as fd (%d) on port (%d) for stream i/o\r\n",curdatestr(),hServerSocket, ntohs(Address.sin_port) );
    printf("[%s] Server\r\n\
              sin_family        = %d\r\n\
              sin_addr.s_addr   = %d\r\n\
              sin_port          = %d\r\n"
              , curdatestr()
              , Address.sin_family
              , (int)(Address.sin_addr.s_addr)
              , (int)(ntohs(Address.sin_port))
            );

    printf("[%s] Making a listen queue of %d elements\r\n",curdatestr(),QUEUE_SIZE);
    /* establish listen queue */
    if(listen(hServerSocket,QUEUE_SIZE) == SOCKET_ERROR)
    {
        printf("[%s] Could not listen\r\n",curdatestr());
        return 0;
    }

    tp = malloc( sizeof(thread_params) *  MAXCONNECTION );
    if(!tp)
    {
        printf("[%s] Allocation error !\r\n",curdatestr());
        return 0;
    }

    memset(tp,0, sizeof(thread_params) *  MAXCONNECTION );

    for(;;)
    {
        printf("[%s] Waiting for a connection\r\n",curdatestr());
        /* get the connected socket */
        hSocket=accept(hServerSocket,(struct sockaddr*)&Address,(socklen_t *)&nAddressSize);

        //int nBytes = 512;
        //setsockopt(hSocket, SOL_SOCKET, SO_RCVLOWAT,(const char *) &nBytes, sizeof(int));
        //setsockopt(hSocket, SOL_SOCKET, SO_SNDLOWAT,(const char *) &nBytes, sizeof(int));
        getsockname( hSocket, (struct sockaddr *) &Address_con,(socklen_t *)&nAddressSize_con);

        printf("[%s] Got a connection\r\n",curdatestr());
        printf("[%s] Client\r\n\
              sin_family        = %d\r\n\
              sin_addr.s_addr   = %d.%d.%d.%d\r\n\
              sin_port          = %d\r\n"
              , curdatestr()
              , Address.sin_family
              , (int)((Address.sin_addr.s_addr>>0)&0xFF),(int)((Address.sin_addr.s_addr>>8)&0xFF),(int)((Address.sin_addr.s_addr>>16)&0xFF),(int)((Address.sin_addr.s_addr>>24)&0xFF)
              , ntohs(Address.sin_port)
            );

        connection = 0;
        for(i=0;i<MAXCONNECTION;i++)
        {
            if(!threadparams[i])
            {
                connection++;
            }
        }
        printf("[%s] %d Slot(s)\r\n",curdatestr(),connection);

        connection=0;
        while((connection<MAXCONNECTION) && threadparams[connection])
        {
            connection++;
        }

        if(connection<MAXCONNECTION)
        {
            memset(&tp[connection],0,sizeof(thread_params));

            threadparams[connection] = &tp[connection];
            threadparams[connection]->hSocket = hSocket;
            threadparams[connection]->tid = connection;
            threadparams[connection]->key = &key;

            memset(hostname,0,sizeof(hostname));
            memset(servname,0,sizeof(servname));
            getnameinfo((struct sockaddr*)&Address, sizeof(Address), hostname, sizeof(hostname), servname, sizeof(servname), NI_NAMEREQD);

            snprintf(threadparams[connection]->clientip,32,"%d.%d.%d.%d:%d"
              , (int)((Address.sin_addr.s_addr>>0)&0xFF),(int)((Address.sin_addr.s_addr>>8)&0xFF),(int)((Address.sin_addr.s_addr>>16)&0xFF),(int)((Address.sin_addr.s_addr>>24)&0xFF)
              , ntohs(Address.sin_port)
            );

            snprintf(threadparams[connection]->clientname,2048-1,"%d.%d.%d.%d:%d - %s - %s -"
              , (int)( (Address.sin_addr.s_addr>>0)&0xFF ), (int)( (Address.sin_addr.s_addr>>8)&0xFF ), (int)( (Address.sin_addr.s_addr>>16)&0xFF ), (int)( (Address.sin_addr.s_addr>>24)&0xFF )
              , ntohs(Address.sin_port)
              , hostname
              , servname
            );

            printf("[%s] Starting thread... (Index %d)\r\n",curdatestr(),connection);
            rc = pthread_create(&threads[connection], NULL, NetRx_Thread, (void *)&tp[connection]);
            if(rc)
            {
                printf("[%s] Error ! Can't Create the thread ! (Error %d)\r\n",curdatestr(),rc);
            }

            rc = pthread_create(&threads[connection], NULL, Display_Thread, (void *)&tp[connection]);
            if(rc)
            {
                printf("[%s] Error ! Can't Create the thread ! (Error %d)\r\n",curdatestr(),rc);
            }
        }
        else
        {
            printf("[%s] Error ! Too many connections!\r\n",curdatestr());
            close(hSocket);
        }
    }
}

