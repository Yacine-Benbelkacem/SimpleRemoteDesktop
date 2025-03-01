#ifndef _WRITE_JPEG_H
#define _WRITE_JPEG_H

#include <pixman.h>
#include <stdio.h>

int write_to_jpeg_stream(unsigned char ** outbuffer, unsigned long * len, void *buf, int quality);
#endif
