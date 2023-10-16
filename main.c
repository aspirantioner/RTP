#include "RtpServer.h"

int main(void)
{
    RtpServer rtp_srv;
    RtpServerInit(&rtp_srv, "127.0.0.1", 1234);
    H264Parser parser;
    H264ParserInit(&parser, INIT_BUFF_SIZE, "./Sample.h264");
    RtpServerSend(&rtp_srv, &parser);
    H264ParserDestroy(&parser);
    RtpServerDestroy(&rtp_srv);
    return 0;
}