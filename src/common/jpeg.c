///////////////////////////////////////////////////////////////////////////////////
// File : jpeg.c
// Contains: libjpeg interface layer.
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

int jpeg_compress(unsigned char ** outbuffer, unsigned long * outsize, uint8_t * buf, int xsize, int ysize, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];

    if(!buf)
        return -1;

    *outbuffer = NULL;
    *outsize = 0;

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);

    cinfo.image_width = xsize;
    cinfo.image_height = ysize;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality (&cinfo, quality, TRUE);

    jpeg_mem_dest(&cinfo, outbuffer, outsize );

    jpeg_start_compress(&cinfo, 1);

    // Encode
    while (cinfo.next_scanline < cinfo.image_height)
    {
        row_pointer[0] = &buf[cinfo.next_scanline * xsize * 3];
        jpeg_write_scanlines(&cinfo, row_pointer, TRUE);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return 1;
};

int jpeg_decompress(unsigned char * inbuffer, unsigned long insize, uint8_t ** buf, int * xsize, int * ysize)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    int rc,pixel_size,size;

    if(!buf)
        return -1;

    *buf = NULL;
    *xsize = 0;
    *ysize = 0;

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, inbuffer, insize);

    rc = jpeg_read_header(&cinfo, TRUE);

    if (rc != 1) {
        printf("File does not seem to be a normal JPEG");
        return -1;
    }

    jpeg_start_decompress(&cinfo);

    *xsize = cinfo.output_width;
    *ysize = cinfo.output_height;

    pixel_size = cinfo.output_components;

    size = (*xsize) * (*ysize) * pixel_size;

    *buf = (unsigned char*) malloc(size);
    if(*buf)
    {
        while (cinfo.output_scanline < cinfo.output_height)
        {
            unsigned char *buffer_array[1];
            buffer_array[0] = *buf + (cinfo.output_scanline) * ((*xsize) * pixel_size);

            jpeg_read_scanlines(&cinfo, buffer_array, TRUE);
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return 1;
};

