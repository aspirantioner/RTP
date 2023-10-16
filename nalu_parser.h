#ifndef _NALU_PARSER_H
#define _NALU_PARSER_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#pragma pack(1)

#define INIT_BUFF_SIZE 20480 // define init nalu size

typedef struct
{
    int start_index;     // 本帧的头索引
    int end_index;       // 本帧的尾索引
    int start_code_size; // 下一帧开始头大小
    int nalu_count;      // nalu帧所在文件里的序号
    bool nalu_tail;      // 是否为最后一帧
} NaluInfo;

typedef struct
{
    int file_fd;
    uint8_t *h264_buff;
    int buff_size;
    NaluInfo nalu_info;
    int read_len;
} H264Parser;

typedef struct
{
    uint8_t nalu_type : 5;
    uint8_t nalu_nri : 2;
    uint8_t nalu_f : 1;
} NaluPkt;

void H264ParserInit(H264Parser *parser_p, int buff_size, char *file_name);

void H264ParseNextNalu(H264Parser *parser_p);

void H264ParserDestroy(H264Parser *parser_p);

#endif