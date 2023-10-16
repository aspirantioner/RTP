#include "RtpServer.h"

void RtpServerInit(RtpServer *server_p, const char *server_ip, in_port_t server_port)
{
    server_p->server_ip = server_ip;
    server_p->server_port = server_port;
    server_p->server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_p->server_fd == -1)
    {
        perror("create socket failed!");
        exit(0);
    }

    // 设置服务器地址和端口
    memset(&server_p->server_addr, 0, sizeof(server_p->server_addr));
    server_p->server_addr.sin_family = AF_INET;
    server_p->server_addr.sin_addr.s_addr = inet_addr(server_p->server_ip);
    server_p->server_addr.sin_port = htons(server_p->server_port);

    // 绑定套接字到端口
    // if (bind(server_p->server_fd, (struct sockaddr *)&(server_p->server_addr), sizeof(server_p->server_addr)) < 0)
    // {
    //     perror("bind failed");
    //     exit(0);
    // }
}

void RtpServerSend(RtpServer *server_p, H264Parser *parser_p)
{
    NaluPkt nalu; // get nalu header;

    uint8_t send_buff[MTU_LEN + sizeof(RtpPkt)]; // RTP Header and RTP Payload
    RtpPkt *rtp_pkt = (RtpPkt *)send_buff;       // RTP Header
    uint8_t *data_buff;                          // RTP PAYLOAD

    StapHeader *stap_header; // STAP Hedaer
    uint16_t *nalu_size;     // STAP NALU SZIE

    FuIndicator *fu_indicator; // SLICE FU INDICATE
    FuHeader *fu_header;       // SLICE FU HEADER

    rtp_pkt->version = 2;
    rtp_pkt->padding = 0;
    rtp_pkt->extension = 0;
    rtp_pkt->csrc_count = 0;
    rtp_pkt->payload = 96; // H264 TYPE

    rtp_pkt->seq_number = 0;
    rtp_pkt->mark = 0;
    rtp_pkt->time_stamp = 0;
    rtp_pkt->csrc_indentifiers = htonl(0x12345678); // random number

    int rest_size = MTU_LEN;
    int ret = 0;
    int old_start_offset;
    int nalu_frame_size;

    while (1)
    {
        old_start_offset = parser_p->nalu_info.start_code_size;                                      // restore last nalu start code size
        H264ParseNextNalu(parser_p);                                                                 // get nalu  end
        *(uint8_t *)&nalu = parser_p->h264_buff[parser_p->nalu_info.start_index + old_start_offset]; // get nalu header

        /*nalu data error*/
        if (nalu.nalu_f)
        {
            continue;
        }
        nalu_frame_size = parser_p->nalu_info.end_index - parser_p->nalu_info.start_index - old_start_offset;
        printf("rtpSendNAL  len = %d M=%d\n", nalu_frame_size, rtp_pkt->mark);
        /*slice*/
        if (nalu_frame_size + 2 >= MTU_LEN)
        {
            /*send last rest aggregate RTP data*/
            if (rest_size != MTU_LEN)
            {

                /*byte seq convert*/
                rtp_pkt->seq_number = htons(rtp_pkt->seq_number);
                rtp_pkt->time_stamp = htonl(rtp_pkt->time_stamp);

                ret = sendto(server_p->server_fd, send_buff, MTU_LEN - rest_size + sizeof(RtpPkt), 0, (struct sockaddr *)&server_p->server_addr, sizeof(struct sockaddr));
                printf("rtpSendData cache [%d]: ", ret);
                for (int i = 0; i < 20; i++)
                {
                    printf("%02X ", send_buff[i]);
                }
                printf("\n");

                /*recovery after send*/
                rtp_pkt->seq_number = ntohs(rtp_pkt->seq_number);
                rtp_pkt->time_stamp = ntohl(rtp_pkt->time_stamp);
                rtp_pkt->seq_number++;
                usleep(1000000 / 200);
            }
            data_buff = send_buff + sizeof(RtpPkt);
            int count = 0;
            int copy_index = parser_p->nalu_info.start_index + old_start_offset + 1; // give up nalu header
            fu_indicator = (FuIndicator *)data_buff;
            fu_indicator->fu_type = 28; // use FU-A
            fu_indicator->fu_f = 0;
            fu_indicator->fu_nri = nalu.nalu_nri;
            fu_header = (FuHeader *)(data_buff + 1);
            fu_header->fu_type = nalu.nalu_type;
            fu_header->fu_r = 0;

            data_buff += 2; // start restore nalu payload
            while (copy_index + MTU_LEN - 2 < parser_p->nalu_info.end_index)
            {

                if (count == 0)
                {
                    fu_header->fu_s = 1;
                    fu_header->fu_e = 0;
                }
                else
                {
                    fu_header->fu_s = 0;
                    fu_header->fu_e = 0;
                }

                memcpy(data_buff, parser_p->h264_buff + copy_index, MTU_LEN - 2);
                rtp_pkt->seq_number = htons(rtp_pkt->seq_number);
                rtp_pkt->time_stamp = htonl(rtp_pkt->time_stamp);

                ret = sendto(server_p->server_fd, send_buff, MTU_LEN + sizeof(RtpPkt), 0, (struct sockaddr *)&server_p->server_addr, sizeof(struct sockaddr));
                printf("rtpSendData cache [%d]: ", ret);
                for (int i = 0; i < 20; i++)
                {
                    printf("%02X ", send_buff[i]);
                }
                printf("\n");
                rtp_pkt->seq_number = ntohs(rtp_pkt->seq_number);
                rtp_pkt->time_stamp = ntohl(rtp_pkt->time_stamp);
                rtp_pkt->seq_number++;

                copy_index += (MTU_LEN - 2);
                usleep(1000000 / 200);
                count++;
            }
            /*slice end bit set*/
            fu_header->fu_e = 1;
            fu_header->fu_s = 0;
            memcpy(data_buff, parser_p->h264_buff + copy_index, parser_p->nalu_info.end_index - copy_index);
            rtp_pkt->seq_number = htons(rtp_pkt->seq_number);
            rtp_pkt->time_stamp = htonl(rtp_pkt->time_stamp);

            ret = sendto(server_p->server_fd, send_buff, 2 + parser_p->nalu_info.end_index - copy_index + sizeof(RtpPkt), 0, (struct sockaddr *)&server_p->server_addr, sizeof(struct sockaddr));
            printf("rtpSendData cache [%d]: ", ret);
            for (int i = 0; i < 20; i++)
            {
                printf("%02X ", send_buff[i]);
            }
            printf("\n");
            rtp_pkt->seq_number = ntohs(rtp_pkt->seq_number);
            rtp_pkt->time_stamp = ntohl(rtp_pkt->time_stamp);
            rtp_pkt->seq_number++;
            rtp_pkt->time_stamp += 90000 / 25;
            rest_size = MTU_LEN;
            usleep(1000000 / 200);
            continue;
        }
        else
        {
        check:
            if (rest_size == MTU_LEN)
            {
                data_buff = send_buff + sizeof(RtpPkt);
                stap_header = (StapHeader *)data_buff;
                nalu_size = (uint16_t *)(data_buff + 1);
                stap_header->stap_f = 0;
                stap_header->stap_type = 24;
                stap_header->stap_nri = nalu.nalu_nri;
                rest_size -= 3;
                data_buff += 3;
                memcpy(data_buff, parser_p->h264_buff + parser_p->nalu_info.start_index + old_start_offset, nalu_frame_size);
                *nalu_size = htons(nalu_frame_size);
                data_buff += nalu_frame_size;
                rest_size -= nalu_frame_size;
                rtp_pkt->time_stamp += 90000 / 25;
            }
            else if (rest_size >= 2 + nalu_frame_size)
            {
                nalu_size = (uint16_t *)data_buff;
                data_buff += 2;
                stap_header->stap_nri = stap_header->stap_nri > nalu.nalu_nri ? stap_header->stap_nri : nalu.nalu_nri;
                memcpy(data_buff, parser_p->h264_buff + parser_p->nalu_info.start_index + old_start_offset, nalu_frame_size);
                *nalu_size = htons(nalu_frame_size);
                data_buff += nalu_frame_size;
                rest_size -= (nalu_frame_size + 2);
                rtp_pkt->time_stamp += 90000 / 25;
            }

            else
            {
                rtp_pkt->seq_number = htons(rtp_pkt->seq_number);
                rtp_pkt->time_stamp = htonl(rtp_pkt->time_stamp);

                ret = sendto(server_p->server_fd, send_buff, MTU_LEN - rest_size + sizeof(RtpPkt), 0, (struct sockaddr *)&server_p->server_addr, sizeof(struct sockaddr));
                printf("rtpSendData cache [%d]:", ret);
                for (int i = 0; i < 20; i++)
                {
                    printf("%02X ", send_buff[i]);
                }
                printf("\n");
                rtp_pkt->seq_number = ntohs(rtp_pkt->seq_number);
                rtp_pkt->time_stamp = ntohl(rtp_pkt->time_stamp);
                rtp_pkt->seq_number++;
                rest_size = MTU_LEN;

                usleep(1000000 / 200);
                goto check;
            }
        }

        /*last rtp  pkt*/
        if (parser_p->nalu_info.nalu_tail)
        {
            if (rest_size != MTU_LEN)
            {
                rtp_pkt->mark = 1;
                rtp_pkt->time_stamp -= 90000 / 25;
                rtp_pkt->seq_number = htons(rtp_pkt->seq_number);
                rtp_pkt->time_stamp = htonl(rtp_pkt->time_stamp);

                ret = sendto(server_p->server_fd, send_buff, MTU_LEN - rest_size + sizeof(RtpPkt), 0, (struct sockaddr *)&server_p->server_addr, sizeof(struct sockaddr));
                printf("rtpSendData cache [%d]:", ret);
                for (int i = 0; i < 20; i++)
                {
                    printf("%02X ", send_buff[i]);
                }
                printf("\n");
            }
            break;
        }
    }
}

void RtpServerDestroy(RtpServer *server_p)
{
    close(server_p->server_fd);
}
