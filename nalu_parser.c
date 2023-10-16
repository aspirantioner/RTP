#include "nalu_parser.h"

void H264ParserInit(H264Parser *parser_p, int buff_size, char *file_name)
{
    /*open h264 file*/
    parser_p->file_fd = open(file_name, O_RDWR);
    if (parser_p->file_fd == -1)
    {
        perror("file open error");
        exit(0);
    }
    parser_p->h264_buff = malloc(sizeof(uint8_t) * buff_size);
    if (!parser_p->h264_buff)
    {
        perror("malloc error");
        exit(0);
    }

    /*init params*/
    parser_p->buff_size = buff_size;
    parser_p->nalu_info.start_index = 0;
    parser_p->nalu_info.nalu_count = 0;
    parser_p->nalu_info.start_code_size = 4; // 第一NALU帧开始码为4
    parser_p->nalu_info.end_index = 0;
    parser_p->nalu_info.nalu_tail = false;

    parser_p->read_len = read(parser_p->file_fd, parser_p->h264_buff, parser_p->buff_size);
}

void H264ParseNextNalu(H264Parser *parser_p)
{
    parser_p->nalu_info.start_index = parser_p->nalu_info.end_index;                   // 从当前已知帧的末尾开始找下一帧
    int start = parser_p->nalu_info.start_index + parser_p->nalu_info.start_code_size; // 跳过开始码
search:
    while (start + 3 <= parser_p->read_len)
    {
        if (*(parser_p->h264_buff + start) == 0x00 && *(parser_p->h264_buff + start + 1) == 0x00 && *(parser_p->h264_buff + start + 2) == 0x01)
        {
            parser_p->nalu_info.end_index = start;
            parser_p->nalu_info.start_code_size = 3; // 设置找到帧的开始码大小
            break;
        }
        if (start + 4 <= parser_p->read_len)
        {
            if (*(parser_p->h264_buff + start) == 0x00 && *(parser_p->h264_buff + start + 1) == 0x00 && *(parser_p->h264_buff + start + 2) == 0x00 && *(parser_p->h264_buff + start + 3) == 0x01)
            {

                parser_p->nalu_info.end_index = start;
                parser_p->nalu_info.start_code_size = 4;
                break;
            }
        }
        start++;
    }

    if (start + 3 > parser_p->read_len) // 存储数组中没有找到下一个开始码
    {
        int rest = parser_p->read_len - parser_p->nalu_info.start_index;                       // 计算下一帧所在当前存储数组中的大小
        if (parser_p->read_len == parser_p->buff_size && parser_p->nalu_info.start_index == 0) // 若整个数组装不下帧则进行扩容，否则将帧头左移至数组头
        {
            uint8_t *p = malloc(parser_p->buff_size * 2); // increase restore buff
            if (!p)
            {
                perror("malloc error");
                exit(0);
            }
            memcpy(p, parser_p->h264_buff + parser_p->nalu_info.start_index, rest);
            free(parser_p->h264_buff);
            parser_p->h264_buff = p;
            parser_p->buff_size *= 2;
        }
        else
        {
            /*有内存重叠,forbid memcpy*/
            for (int i = 0; i < rest; i++)
            {
                parser_p->h264_buff[i] = parser_p->h264_buff[parser_p->nalu_info.start_index + i];
            }
        }
        start = rest - 2;
        parser_p->nalu_info.start_index = 0;                                                       // 帧头已移至数组头                                                      // 帧头已移至数组头
        int ret = read(parser_p->file_fd, parser_p->h264_buff + rest, parser_p->buff_size - rest); // 左移后读取以填满空闲数据区

        if (ret == 0) // 文件已读完
        {
            parser_p->nalu_info.end_index = rest;
            parser_p->nalu_info.nalu_tail = true; // 最后一帧无需再找
        }
        else
        {
            parser_p->read_len = rest + ret;
            goto search; // 重新寻找
        }
    }
    parser_p->nalu_info.nalu_count++; // 标记当前帧所在文件序号
}

void H264ParserDestroy(H264Parser *parser_p)
{
    free(parser_p->h264_buff);
}

// int main(void)
// {
//     FILE *fp = fopen("Sample.h264", "rb");
//     struct stat file_info = {0};
//     stat("Sample.h264", &file_info);
//     int size = file_info.st_size;
//     uint8_t *buff = malloc(size);
//     fread(buff, size, 1, fp);
//     int start = 0;
//     int head = start;
//     int start_code_size = 4;
//     int end = start;
//     int count = 1;
//     while (start + 3 <= size)
//     {
//         head = start;
//         start += start_code_size;
//         while (start + 3 <= size)
//         {
//             if (*(buff + start) == 0x00 && *(buff + start + 1) == 0x00 && *(buff + start + 2) == 0x01)
//             {
//                 end = start;
//                 start_code_size = 3;
//                 printf("the %d nalu size is %d\n", count++, end - head);
//                 break;
//             }
//             if (start + 4 < size)
//             {
//                 if (*(buff + start) == 0x00 && *(buff + start + 1) == 0x00 && *(buff + start + 2) == 0x00 && *(buff + start + 3) == 0x01)
//                 {

//                     end = start;
//                     start_code_size = 4;
//                     printf("the %d nalu size is %d\n", count++, end - head);
//                     break;
//                 }
//             }
//             start++;
//         }
//     }
//     printf("the %d nalu size is %d\n", count++, size - head);
//     fclose(fp);
//     return 0;
// }