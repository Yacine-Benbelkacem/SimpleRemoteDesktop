#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>


#include "messages.h"
#include "remote_controls_handler.h"
#include "utils.h"

void *controls_handler(void *params)
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
    execcmd * execevt;

    thread_params * tp;

    tp = (thread_params*)params;

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

                            mouseevent *mevt = (mouseevent *)buf;
                        
                            // Open X display
                            Display *display = XOpenDisplay(NULL);
                            if (!display) {
                                fprintf(stderr, "Error: Cannot open X display\n");
                                break;
                            }
                        
                            // Move mouse to (xpos, ypos)
                            XWarpPointer(display, None, DefaultRootWindow(display), 0, 0, 0, 0, mevt->xpos, mevt->ypos);
                            XFlush(display);
                        
                            // Handle mouse button clicks
                            if (mevt->click) {
                                if (mevt->click & 0x01) { // Left button down
                                    XTestFakeButtonEvent(display, 1, True, CurrentTime);
                                    XFlush(display);
                                }
                                if (mevt->click & 0x02) { // Left button up
                                    XTestFakeButtonEvent(display, 1, False, CurrentTime);
                                    XFlush(display);
                                }
                                if (mevt->click & 0x04) { // Right button down
                                    XTestFakeButtonEvent(display, 3, True, CurrentTime);
                                    XFlush(display);
                                }
                                if (mevt->click & 0x08) { // Right button up
                                    XTestFakeButtonEvent(display, 3, False, CurrentTime);
                                    XFlush(display);
                                }
                                if (mevt->click & 0x10) { // Double-click handling
                                    XTestFakeButtonEvent(display, 1, True, CurrentTime);
                                    XTestFakeButtonEvent(display, 1, False, CurrentTime);
                                    usleep(50000); // Small delay between clicks
                                    XTestFakeButtonEvent(display, 1, True, CurrentTime);
                                    XTestFakeButtonEvent(display, 1, False, CurrentTime);
                                    XFlush(display);
                                }
                            }
                            // Close X display
                            XCloseDisplay(display);

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


    return NULL;
}