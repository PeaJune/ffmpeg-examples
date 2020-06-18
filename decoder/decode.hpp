#ifndef __DECODE_HPP__
#define __DECODE_HPP__

#include <iostream>
#include <vector>

extern "C"
{
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

class VideoCapture
{
public:
    VideoCapture();
    int open(char *filename);
    int decode_init(){};  /*decode init*/
    int close();
    int grab();
    int retrieve();
    int get_width(){ return m_nWidth; }
    int get_height(){ return m_nHeight; }
    int get_enType(){ return m_enType; }

private:
    int m_nWidth;
    int m_nHeight;
    int m_enType;
    int m_nFrames;

    int m_gotIDR;
    AVFormatContext *m_pFormatCtx;
    int m_nVideoStreamIndex;
    AVDictionary *m_pOptions;
    AVPacket m_pkt;
    AVBSFContext *m_pBsfCtx;
    AVBitStreamFilter *m_pBsfFilter;
    AVCodecContext *m_pVideoCodecCtx;
    AVCodec        *m_pVideoCodec;
    AVFrame         *m_frame;

};

#endif