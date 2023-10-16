#ifndef _RTPSERVER_H
#define _RTPSERVER_H

#include <netinet/udp.h>
#include <net/ethernet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "nalu_parser.h"

#pragma pakc(1)
#define MTU_LEN 1400

typedef struct
{
    const char *server_ip;
    in_port_t server_port;
    int server_fd;
    struct sockaddr_in server_addr;
} RtpServer;

typedef struct
{
    uint8_t csrc_count : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t payload : 7;
    uint8_t mark : 1;
    uint16_t seq_number;
    uint32_t time_stamp;
    uint32_t csrc_indentifiers;
} RtpPkt;

typedef struct
{
    uint8_t fu_type : 5;
    uint8_t fu_nri : 2;
    uint8_t fu_f : 1;
} FuIndicator;

typedef struct
{
    uint8_t fu_type : 5;
    uint8_t fu_r : 1;
    uint8_t fu_e : 1;
    uint8_t fu_s : 1;
} FuHeader;

typedef struct
{
    uint8_t stap_type : 5;
    uint8_t stap_nri : 2;
    uint8_t stap_f : 1;
} StapHeader;

void RtpServerInit(RtpServer *server_p, const char *server_ip, in_port_t server_port);

void RtpServerSend(RtpServer *server_p, H264Parser *parser_p);

void RtpServerDestroy(RtpServer *server_p);

#endif