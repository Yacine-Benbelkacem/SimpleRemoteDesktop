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

#ifdef WAYLAND_ENV
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>

// Function to send input events
void send_event(int fd, unsigned short type, unsigned short code, int value) {
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    write(fd, &ev, sizeof(ev));
}

// Function to move the mouse
void move_mouse(int fd, int dx, int dy) {
    send_event(fd, EV_ABS, ABS_X, dx);
    send_event(fd, EV_ABS, ABS_Y, dy);
    send_event(fd, EV_SYN, SYN_REPORT, 0);
}

// Function to simulate mouse click
void click_mouse(int fd, int button, int press) {
    send_event(fd, EV_KEY, button, press);
    send_event(fd, EV_SYN, SYN_REPORT, 0);
}
#endif

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
#ifdef WAYLAND_ENV
        int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0) {
            perror("Error opening /dev/uinput");
            return NULL;
        }
    
    
        // Enable mouse events
        ioctl(fd, UI_SET_EVBIT, EV_KEY);
        ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
        ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
    
        // Enable absolute positioning
        ioctl(fd, UI_SET_EVBIT, EV_ABS);
        ioctl(fd, UI_SET_ABSBIT, ABS_X);
        ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    
       // Configure absolute positioning range
        struct uinput_abs_setup abs_x = {
            .code = ABS_X,
            .absinfo = { .minimum = 0, .maximum = 1920, .value = 0 }
        };
        struct uinput_abs_setup abs_y = {
            .code = ABS_Y,
            .absinfo = { .minimum = 0, .maximum = 1080, .value = 0 }
        };
    
        ioctl(fd, UI_ABS_SETUP, &abs_x);
        ioctl(fd, UI_ABS_SETUP, &abs_y);
    
        // Create virtual device
        struct uinput_setup usetup;
        memset(&usetup, 0, sizeof(usetup));
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x1234;
        usetup.id.product = 0x5678;
        snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "Virtual Mouse");
    
        ioctl(fd, UI_DEV_SETUP, &usetup);
        ioctl(fd, UI_DEV_CREATE);
#endif
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
#ifdef WAYLAND_ENV                        
                    		// Move the mouse pointer
                            move_mouse(fd, mevt->xpos, mevt->ypos);

                            if (mevt->click) {
                                if (mevt->click & 0x01) { // Left button down
                                printf("left down\n");
                                        click_mouse(fd, BTN_LEFT, 1); // Press
                                }
                                if (mevt->click & 0x02) { // Left button up
                                printf("left up \n");
                                    click_mouse(fd, BTN_LEFT, 0); // Release
                                }
                                if (mevt->click & 0x04) { // Right button down
                                printf("right down\n");
                                    click_mouse(fd, BTN_RIGHT, 1);
                                }
                                if (mevt->click & 0x08) { // Right button up
                                        printf("right up\n");
                                click_mouse(fd, BTN_RIGHT, 0);
                                }
                                if (mevt->click & 0x10) { // Double-click handling
                                    click_mouse(fd, BTN_LEFT, 1); // Press
                                    click_mouse(fd, BTN_LEFT, 0); // Release
                                    usleep(50000); // Small delay between clicks
                                    click_mouse(fd, BTN_LEFT, 1); // Press
                                    click_mouse(fd, BTN_LEFT, 0); // Release
                                }
                            }
#else
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
#endif
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
    
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
    }
    else
    {

    }

    printf("[%s] Exit Server thread...\r\n",curdatestr());


    return NULL;
}