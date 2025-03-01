///////////////////////////////////////////////////////////////////////////////////
// File : screenshot.c
// Contains: screenshot functions.
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
#include <stdio.h>
#include <stdint.h>

#ifdef __unix__

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>

int takescreenshot(uint8_t * buf, int * xsize, int * ysize)
{
    Display * display;
    XWindowAttributes gwa;
    Window root;
    XImage * image;
    uint32_t pixel;
    int x,y;
    int width = 0;
    int height = 0;

    display = XOpenDisplay(NULL);
    if(!display)
        return -1;

    root = DefaultRootWindow(display);

    XGetWindowAttributes(display, root, &gwa);

    width = gwa.width;
    height = gwa.height;

    if( (height != *ysize) || (width != *xsize) || !buf )
    {
        *xsize = width;
        *ysize = height;

        XCloseDisplay(display);

        return -2;
    }

    image = XGetImage(
        display,
        root,
        0,
        0,
        width,
        height,
        AllPlanes,
        ZPixmap
    );

    if( image )
    {
        for ( y = 0; y < height; y++ )
        {
            for ( x = 0; x < width; x++ )
            {
                // pixel = (blue & 0xFF) | ((green>>8) & 0xFF) | ((red>>16) & 0xFF)
                pixel = XGetPixel(image, x, y);

                *buf++ = ((pixel>>16) & 0xFF);
                *buf++ = ((pixel>>8) & 0xFF);
                *buf++ = ( pixel & 0xFF);
            }
        }

        XDestroyImage(image);
    }

    XCloseDisplay(display);

    return 1;
}

int getmouseposition(int * x, int * y)
{
    Display * display;
    Window root;
    int win_x, win_y, root_x, root_y = 0;
    unsigned int mask = 0;
    Window child_win, root_win;

    display = XOpenDisplay(NULL);
    if(!display)
        return -1;

    root = DefaultRootWindow(display);

    XQueryPointer(display, root, &child_win, &root_win, &root_x, &root_y, &win_x, &win_y, &mask);

    XCloseDisplay(display);

    if(x)
        *x = win_x;

    if(y)
        *y = win_y;

    return 1;
}

#else

int takescreenshot(uint8_t * buf, int * xsize, int * ysize)
{
    return 0;
}

int getmouseposition(int * x, int * y)
{
    return 0;
}

#endif
