///////////////////////////////////////////////////////////////////////////////////
// File : jpeg.h
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

int jpeg_compress(unsigned char ** outbuffer, unsigned long * outsize, uint8_t * buf, int xsize, int ysize, int quality);
int jpeg_decompress(unsigned char * inbuffer, unsigned long insize, uint8_t ** buf, int * xsize, int * ysize);

