///////////////////////////////////////////////////////////////////////////////////
// File : message.h
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

#ifndef MESSAGES_H
#define MESSAGES_H

#define SOCKET_ERROR        -1
#define NET_PACKET_SIZE 1024

#pragma pack(1)
typedef struct message_header_
{
    unsigned char salt[64];
    uint32_t sign;          // "DATA" 0x41544144
    uint32_t tx_total_size;
    uint32_t user_size;
    uint32_t type;
    uint32_t id;
    uint8_t  data[];
}message_header;
#pragma pack()

#define MSGTYPE_PING                    0x00000000
#define MSGTYPE_JPEGFULLSCREEN          0x00000001
#define MSGTYPE_SCANCODE_KEYBOARD_EVENT 0x00000002
#define MSGTYPE_MOUSE_EVENT             0x00000003
#define MSGTYPE_EXEC                    0x00000004

#pragma pack(1)
typedef struct mouseevent_
{
    int32_t xpos;
    int32_t ypos;
    int32_t click;
}mouseevent;

typedef struct keybevent_
{
    int32_t cnt;
    char buf[];
}keybevent;

typedef struct scancodekeybevent_
{
    int32_t cnt;
    uint16_t state;
    uint16_t buf[];
}scancodekeybevent;

typedef struct execcmd_
{
    int32_t cnt;
    char buf[];
}execcmd;

typedef struct keymng_
{
    unsigned char key[128/8];
    unsigned char iv[128/8];
    unsigned char lastrx[128/8];
    unsigned char lasttx[128/8];
    uint32_t cnt;
}keymng;

#pragma pack()

int send_data(int sockfd, keymng * key, unsigned char * data, int size, uint32_t type);

int waitandreceivepacket(int hSocket, keymng * key, void * buffer,int buffersize);

int generatekey(keymng * key, char * password);

int genrandiv(keymng * key);

#endif // MESSAGES_H