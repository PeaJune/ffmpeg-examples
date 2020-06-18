#include "decode.hpp"

VideoCapture::VideoCapture(){
    m_nWidth = 0;
    m_nHeight = 0;
    m_enType = -1;
    m_nFrames = 0;
    m_nVideoStreamIndex = -1;
    m_pFormatCtx = NULL;
    m_pOptions = NULL;
    m_pBsfCtx = NULL;
    m_pBsfFilter = NULL;
    m_pVideoCodec = nullptr;
    m_pVideoCodecCtx = nullptr;
    m_frame = av_frame_alloc();
}

int VideoCapture::open(char *filename)
{
    printf("Input filename: %s\n", filename);

    int ret;

    avformat_network_init();

    av_init_packet(&m_pkt);
    m_pkt.data = nullptr;
    m_pkt.size = 0;

    av_dict_set(&m_pOptions, "rtsp_transport", "tcp", 0);
    av_dict_set(&m_pOptions, "rtsp_flags", "prefer_tcp", 0);
    av_dict_set(&m_pOptions, "allowed_media_types", "video", 0);
    av_dict_set(&m_pOptions, "stimeout", "5000", 0);
    av_dict_set(&m_pOptions, "probesize", "409600", 0);
    av_dict_set(&m_pOptions, "buffer_size", "409600", 0);
    av_dict_set(&m_pOptions, "max_delay", "50000", 0);


    ret = avformat_open_input(&m_pFormatCtx, filename, 0, &m_pOptions);
    if(ret < 0)
    {
        printf("Could not open input file.\n");
        goto open_input_failed;
    }
  
    ret = avformat_find_stream_info(m_pFormatCtx, 0);
    if(ret < 0)
    {
        printf("Failed to retrieve input stream information.");
        goto find_stream_info_failed;
    }

    if(m_pFormatCtx->streams[0]->codecpar->codec_id == AV_CODEC_ID_H264)
    {
        m_enType = 0;
        printf("Input format is H264.\n");
    }else if(m_pFormatCtx->streams[0]->codecpar->codec_id == AV_CODEC_ID_H265){
        m_enType = 1;
        printf("Input format is H265.\n");
    }else{
        m_enType = -1;
        printf("Not support pFormatCtx->video_codec_id: %d\n",
            m_pFormatCtx->streams[0]->codecpar->codec_id);
        goto stream_video_type_unknown;
    }
    
    m_nVideoStreamIndex = av_find_best_stream(m_pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if(m_nVideoStreamIndex < 0)
    {
        printf("Not a video file.\n");
        goto not_video_file;
    }

    if(m_enType == 0)
    {
        m_pBsfFilter = (AVBitStreamFilter*)av_bsf_get_by_name("h264_mp4toannexb");
    }else if(m_enType == 1){
        m_pBsfFilter = (AVBitStreamFilter*)av_bsf_get_by_name("hevc_mp4toannexb");
    }else{
        printf("unknonw video format.\n");
        goto stream_video_type_unknown;
    }
    if(m_pBsfFilter == NULL)
    {
        printf("not found bitstream filter.\n");
        goto stream_video_type_unknown;
    }

    ret = av_bsf_alloc(m_pBsfFilter, &m_pBsfCtx);
    if(ret < 0)
    {
        printf("alloc bsf error.\n");
        goto bsf_alloc_failed;
    }

    /*copy codec paramter to bsfCtx input paramter, get specify codec id.*/
    ret = avcodec_parameters_copy(m_pBsfCtx->par_in, m_pFormatCtx->streams[m_nVideoStreamIndex]->codecpar);
    if(ret != 0)
    {
        printf("copy to bsf paramter failed, error:%d\n", ret);
        goto copy_bsf_paramter_failed;
    }

    ret = av_bsf_init(m_pBsfCtx);
    if(ret != 0)
    {
        printf("init bsf failed. error: %d\n", ret);
        goto bsf_init_failed;
    }

    av_dump_format(m_pFormatCtx, 0, filename, 0);    

    m_nWidth = m_pFormatCtx->streams[m_nVideoStreamIndex]->codecpar->width;
    m_nHeight = m_pFormatCtx->streams[m_nVideoStreamIndex]->codecpar->height;
    printf("width: %d\n", m_pFormatCtx->streams[m_nVideoStreamIndex]->codecpar->width);
    printf("height: %d\n", m_pFormatCtx->streams[m_nVideoStreamIndex]->codecpar->height);

    m_pVideoCodec = avcodec_find_decoder(m_pFormatCtx->streams[m_nVideoStreamIndex]->codecpar->codec_id);
    if(m_pVideoCodec == nullptr)
    {
        printf("avcodec find decoder failed.\n");
        goto avcodec_find_decoder_failed;
    }

    m_pVideoCodecCtx = avcodec_alloc_context3(m_pVideoCodec);
    if(m_pVideoCodecCtx == nullptr)
    {
        printf("avcodec alloc context3 failed!\n");
        goto bsf_init_failed;
    }

    m_pVideoCodecCtx->thread_type = FF_THREAD_FRAME;
    m_pVideoCodecCtx->thread_count = 1;

    if(avcodec_open2(m_pVideoCodecCtx, m_pVideoCodec, NULL) != 0)
    {
        printf("avcdec_open2 failed....\n");
        goto avcodec_open2_failed;
    }

    avcodec_flush_buffers(m_pVideoCodecCtx);

    return 0;


avcodec_open2_failed:
avcodec_find_decoder_failed:
    avcodec_free_context(&m_pVideoCodecCtx);
bsf_alloc_failed:
copy_bsf_paramter_failed:
bsf_init_failed:
    av_bsf_free(&m_pBsfCtx);
    m_pBsfCtx = nullptr;

find_stream_info_failed:
stream_video_type_unknown:
not_video_file:
    avformat_close_input(&m_pFormatCtx);
    m_pFormatCtx = nullptr;

open_input_failed:
    avformat_network_deinit();

    return ret;
}

int VideoCapture::grab()
{
    int ret = 0;
read_again:
    if(av_read_frame(m_pFormatCtx, &m_pkt) >= 0)
    {
        if(m_pkt.stream_index == m_nVideoStreamIndex)
        {
            if(m_pkt.flags & AV_PKT_FLAG_KEY){
                m_gotIDR = true;
            }
            if(!m_gotIDR)
            {
                printf("not IDR ....\n");
                av_packet_unref(&m_pkt);
                goto read_again;
            }

            if(av_bsf_send_packet(m_pBsfCtx, &m_pkt) < 0){
                printf("bsf send packet error.\n");
                av_packet_unref(&m_pkt);
                return -1;
            }

            if(av_bsf_receive_packet(m_pBsfCtx, &m_pkt) != 0)
            {
                printf("bsf recv packet error.\n");
                av_packet_unref(&m_pkt);
                return -1;
            }
            printf("read a frame...\n");
            //av_packet_unref(&m_pkt);
            /*
            * send packet to hardware....
            * av_packet_unref(&m_pkt);
            */
//send_retry:
           if(avcodec_send_packet(m_pVideoCodecCtx, &m_pkt)!= 0)
           {
               printf("avcodec_send_packet failed.!");
               av_packet_unref(&m_pkt);
               m_gotIDR = false;
               goto read_again;
               //goto send_retry;
           }
           printf("send.......\n");
           av_packet_unref(&m_pkt);
           printf("free send pakcet memory..\n");
           m_nFrames++;
        }else{
            printf("not video...\n");
            goto read_again;
        }
    }else{
        printf("read error....\n");
        ret = -1;
    }

    return ret;
}

int VideoCapture::close()
{
    printf("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
    if(m_pVideoCodec != nullptr)
    {
        printf("release pool buffer...\n");
        avcodec_close(m_pVideoCodecCtx);
        avcodec_free_context(&m_pVideoCodecCtx);
        av_frame_free(&m_frame);
    }
    if(m_pFormatCtx != nullptr)
    {
        avformat_flush(m_pFormatCtx);
        avformat_close_input(&m_pFormatCtx);
        m_pFormatCtx = nullptr;
    }

    if(m_pBsfCtx != nullptr)
    {
        av_bsf_free(&m_pBsfCtx);
        m_pBsfCtx = nullptr;
    }

    if(m_pOptions != nullptr)
    {
        av_dict_free(&m_pOptions);
        m_pOptions = nullptr;
    }

    avformat_network_deinit();

    return 0;
}

void pgm_save(unsigned char **buf, int *wrap, int xsize, int ysize,
                     char *filename)
{
    FILE *f;
    int i,j;
    static int count = 0;
    unsigned char *data;
    count++;
    char fn[64] = "";

    sprintf(fn, "test_yuv_%d.yuv", count);


    f = fopen(fn, "w");
    //fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    // for (i = 0; i < ysize; i++)
    //     fwrite(buf + i * wrap, 1, xsize, f);
    data = buf[0];
    for(i=0; i<3; i++)
    {
        printf("i:%d\n", i);
        printf("wrap:%d\n", wrap[i]);
        data=buf[i];
        printf("data:%p\n", data);
        //if(i>0) ysize=ysize/2;
        
        for(j=0;j<ysize; j++)
        {
            fwrite(data, 1, wrap[i], f);
            data+=wrap[i];
        }
        if(i==0) ysize=ysize/2;
    }
    fclose(f);
}

#pragma pack(1)

typedef struct
{
    short type;
    int size;
    short reserved1;
    short reserved2;
    int offset;
} BMPHeader;

typedef struct
{
    int size;
    int width;
    int height;
    short planes;
    short bitsPerPixel;
    unsigned compression;
    unsigned imageSize;
    int xPelsPerMeter;
    int yPelsPerMeter;
    int clrUsed;
    int clrImportant;

} BMPInfoHeader;

#pragma pack()

int saveBMPFile(uint8_t *pu8BgrBuf, int width, int height, const char *name)
{
    BMPHeader hdr;
    BMPInfoHeader infoHdr;
    int i;
    FILE *fp = NULL;

    fp = fopen(name, "wb");
    if (NULL == fp)
    {
        printf("saveBMPFile: Err: Open!\n");
        return (-1);
    }

    infoHdr.size = sizeof(BMPInfoHeader);
    infoHdr.width = width;
    infoHdr.height = height;
    infoHdr.planes = 1;
    infoHdr.bitsPerPixel = 24;
    infoHdr.compression = 0;
    infoHdr.imageSize = width * height;
    infoHdr.xPelsPerMeter = 0;
    infoHdr.yPelsPerMeter = 0;
    infoHdr.clrUsed = 0;
    infoHdr.clrImportant = 0;

    hdr.type = 0x4D42;
    hdr.size = 54 + infoHdr.imageSize * 3;
    hdr.reserved1 = 0;
    hdr.reserved2 = 0;
    hdr.offset = 54;

    fwrite(&hdr, sizeof(hdr), 1, fp);
    fwrite(&infoHdr, sizeof(infoHdr), 1, fp);
#if 1
    for (i = 0; i < height; i++)
    {
        fwrite(pu8BgrBuf + (height - 1 - i) * width * 3, sizeof(unsigned char), width * 3, fp);
    }
#endif
#if 0
    for (i = 0; i < infoHdr.imageSize; i++)
    {
        fwrite(pu8BgrBuf + i, sizeof(unsigned char), 1, fp);
        fwrite(pu8BgrBuf + infoHdr.imageSize + i, sizeof(unsigned char), 1, fp);
        fwrite(pu8BgrBuf + infoHdr.imageSize * 2 + i, sizeof(unsigned char), 1, fp);
    }
#endif
    fclose(fp);
    fp = NULL;

    return 0;
}

void yuv2bgr(AVFrame *frame)
{
    struct SwsContext *sc;
    int h = 0;
    int ret = 0;
    uint8_t *dst_data[4];
    int dst_linesize[4];
    uint8_t *src_data[4];
    int src_linesize[4];
    AVPixelFormat  srcPixelFormat;

    int size = frame->width*frame->height;

    src_data[0] = (uint8_t*)malloc(size*3/2);
    src_data[1] = src_data[0]+size;
    src_data[2] = src_data[1]+size/4;
    src_linesize[0] = frame->width;
    src_linesize[1] = frame->width/2;
    src_linesize[2] = frame->width/2;

    memcpy(src_data[0], frame->data[0], size);
    memcpy(src_data[1], frame->data[1], size/4);
    memcpy(src_data[2], frame->data[2], size/4);

    switch(frame->format)
    {
        case AV_PIX_FMT_YUVJ420P:
            srcPixelFormat = AV_PIX_FMT_YUV420P;
            break;
        case AV_PIX_FMT_YUVJ422P:
            srcPixelFormat = AV_PIX_FMT_YUV422P;
            break;
        case AV_PIX_FMT_YUVJ444P:
            srcPixelFormat = AV_PIX_FMT_YUV444P;
            break;
        default:
            srcPixelFormat = (AVPixelFormat)frame->format;
    }

    sc = sws_getContext(frame->width, frame->height, srcPixelFormat, 
                            frame->width, frame->height, AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if(sc == nullptr)
    {
        printf("malloc sws_getContext failed.\n");
        return;
    }
    // printf("yuv2bgr  width:%d\n", frame->width);
    // printf("yuv2bgr  height:%d\n", frame->height);

    ret = av_image_alloc(dst_data, dst_linesize, frame->width, frame->height, AV_PIX_FMT_BGR24, 1);
    if(ret < 0)
    {
        printf("Could not allocate dst images, ret:%d\n", ret);
        sws_freeContext(sc);
        return;
    }
    h = sws_scale(sc, src_data, src_linesize, 0, frame->height, dst_data, dst_linesize);
    if(h != frame->height)
    {
        printf("sws scale error.ret:%d\n", h);
        sws_freeContext(sc);
        av_freep(&dst_data[0]);
        return;
    }
    /*save bgr  picture.*/
    static int count = 0;
    count++;
    char fn[64] = "";

    sprintf(fn, "test_bgr_%d.bmp", count);
    saveBMPFile(dst_data[0], frame->width, frame->height, fn);
    sws_freeContext(sc);
    av_freep(&dst_data[0]);
}

int VideoCapture::retrieve()
{
    int ret = 0;
    printf("receive frame....\n");
    ret = avcodec_receive_frame(m_pVideoCodecCtx, m_frame);
    if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        printf("user must try to send new input, or no more output frames\n");
        av_frame_unref(m_frame);
        return ret;
    }else if(ret < 0)
    {
        printf("decode failed.\n");
        av_frame_unref(m_frame);
        return ret;
    }

    printf("width:%d\n", m_frame->width);
    printf("height:%d\n", m_frame->height);
    printf("format:%d\n",m_frame->format);
    printf("y:%d\n", m_frame->linesize[0]);
    printf("u:%d\n", m_frame->linesize[1]);
    printf("v:%d\n", m_frame->linesize[2]);
    printf("3v:%d\n", m_frame->linesize[3]);

    //m_frame->siz
    printf("data 0:%p\n", m_frame->data[0]);
    //pgm_save(m_frame->data, m_frame->linesize, m_frame->width, m_frame->height, nullptr);
    yuv2bgr(m_frame);
    //printf();
    av_frame_unref(m_frame);
    return 0;
}