///////////////////////////////////////////////////////////////////////////////////
// File : message.c
// Contains: Network / protocol layer.
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "sha256.h"

#include "aes.h"
#include "utils.h"

#include "messages.h"

uint32_t getTick()
{
    struct timespec ts;
    unsigned theTick = 0U;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    theTick  = ts.tv_nsec / 1000000;
    theTick += ts.tv_sec * 1000;

    return theTick;
}

static inline uint32_t xorshift_rand( uint32_t seed )
{
        /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;

        return seed;
}

int send_data(int sockfd, keymng * key, unsigned char * data, int size, uint32_t type)
{
    int err_loop_cnt;
    int rv;
    int i, j, offs;

    message_header header;
    unsigned char netbuf[NET_PACKET_SIZE];
    unsigned char * ptr;
    aes_context aesctx;
    uint32_t * tmp_ptr;
    uint32_t rand;

    tmp_ptr = (uint32_t *)netbuf;
    rand = xorshift_rand( (key->cnt ^ 0xAC628E10) + key->iv[3] + getTick() );
    for(i=0;i<NET_PACKET_SIZE/sizeof(uint32_t);i++)
    {
        rand = xorshift_rand( rand );
        tmp_ptr[i] = rand;
    }

    memset(&header,0,sizeof(header));

    genrandiv(key);
    memcpy((void*)&header.salt[0],  (void*)key->iv, 128/8);
    genrandiv(key);
    memcpy((void*)&header.salt[16], (void*)key->iv, 128/8);
    genrandiv(key);
    memcpy((void*)&header.salt[32], (void*)key->iv, 128/8);
    genrandiv(key);
    memcpy((void*)&header.salt[48], (void*)key->iv, 128/8);

    header.sign = 0x41544144;
    header.user_size = size;
    header.tx_total_size = sizeof(header) + size;
    header.type = type;

    if( header.tx_total_size & (NET_PACKET_SIZE - 1 ) )
        header.tx_total_size = ( header.tx_total_size & ~(NET_PACKET_SIZE - 1) ) + NET_PACKET_SIZE;

    ptr = (unsigned char *)&header;

    aes_set_key( &aesctx, key->key, 128 );

    err_loop_cnt = 0;
    offs = 0;

    memset((char*)key->lasttx,0,128/8);

    while ( ( offs < header.tx_total_size ) && err_loop_cnt<32)
    {
        memset(netbuf,0,sizeof(netbuf));

        i = 0;
        while( i < NET_PACKET_SIZE )
        {
            if( offs < sizeof(header) + size )
            {
                if( offs < sizeof(header))
                {
                    netbuf[i] = *ptr++;
                }
                else
                {
                    netbuf[i] = *data++;
                }
            }

            offs++;
            i++;
        }

        for( i= 0; i < NET_PACKET_SIZE; i += 16 )
        {
            for( j=0;j<128/8;j++)
            {
                netbuf[i + j] = netbuf[i + j] ^ key->lasttx[j];
            }

            aes_encrypt( &aesctx, &netbuf[i], &netbuf[i] );
            memcpy((char*)&key->lasttx,&netbuf[i],128/8);
        }

        do
        {
            //clearerr(hSocket);
            rv = send(sockfd, (char*)netbuf, NET_PACKET_SIZE, 0);

            if(rv == -1 && errno == EINTR)
            {
                err_loop_cnt++;
            }
            else
            {
                err_loop_cnt = 0;
            }
        }while (rv == -1 && errno == EINTR && err_loop_cnt<32);
    }

    if(err_loop_cnt >= 32)
        return -1;
    else
        return 0;
}

int waitandreceivepacket(int hSocket, keymng * key, void * buffer,int buffersize)
{
    struct timeval tv;
    fd_set input_set;
    int rv,recverror;
    int bufsize,bufindex;
    unsigned char * tmp_ptr;
    aes_context aesctx;
    unsigned char tmpblock[128/8];
    int i,j;

    recverror = 0;
    tv.tv_sec = 20;
    tv.tv_usec = 0;

    FD_ZERO(&input_set);
    FD_SET(hSocket,&input_set);

    aes_set_key( &aesctx, key->key, 128 );

    rv = select(hSocket+1, &input_set, NULL, NULL, &tv);
    if(rv>0)
    {
        bufsize = buffersize;
        bufindex = 0;
        tmp_ptr = (unsigned char*)buffer;

        while(bufsize)
        {
            rv = recv(hSocket,(char*)&tmp_ptr[bufindex],bufsize,0);

            if( rv<0 || !rv )
            {
#ifdef __WIN32__
                printf("recv failed: %d\n", WSAGetLastError());
#endif
                // Error...
                return 1;
            }

            if( rv < buffersize )
            {
                bufsize -= rv;
                bufindex += rv;
            }
            else
            {
                bufsize = 0;
            }
        }

        for( i= 0; i < NET_PACKET_SIZE; i += 16 )
        {
            memcpy(tmpblock, &tmp_ptr[i], 128/8);

            aes_decrypt( &aesctx, &tmp_ptr[i], &tmp_ptr[i] );

            for(j=0;j<(128/8);j++)
            {
                tmp_ptr[i+j] = tmp_ptr[i+j] ^ key->lastrx[j];
            }

            memcpy(&key->lastrx, tmpblock, 128/8);
        }

    }
    else
    {
        recverror = 1;
    }

    return recverror;
}

int generatekey(keymng * key, char * password)
{
    char tmp_str[128];
    unsigned char sha256sum[32];
    sha256_context sha_ctx;

    memset( key, 0, sizeof(keymng) );

    sha256_starts( &sha_ctx );

    sprintf(tmp_str,"$PASSWORD$_");

    sha256_update( &sha_ctx, (uint8 *)tmp_str, strlen(tmp_str) );

    memset( tmp_str, 0, sizeof(tmp_str) );

    sha256_update( &sha_ctx, (uint8 *)password, strlen(password) );

    sha256_finish( &sha_ctx, sha256sum );

    memcpy( key->key, sha256sum, 16 );

    return 1;
}

int genrandiv(keymng * key)
{
    char tmp_str[128];
    char * str;
    unsigned char sha256sum[32];
    sha256_context sha_ctx;

    sha256_starts( &sha_ctx );

    str = curdatestr();

    sprintf(tmp_str,"0x%.8X - 0x%.8X ---",key->cnt,getTick());

    sha256_update( &sha_ctx, (uint8 *)tmp_str, strlen(tmp_str) );

    memset( tmp_str, 0, sizeof(tmp_str) );

    sha256_update( &sha_ctx, (uint8 *)str, strlen(str) );

    sha256_finish( &sha_ctx, sha256sum );

    memcpy( key->iv, sha256sum, 16 );
    for(int i=0;i<16;i++)
    {
        key->iv[i] = key->iv[i] ^ sha256sum[i + 16];
    }

    key->cnt++;

    return 1;
}
