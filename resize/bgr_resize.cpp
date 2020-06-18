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

#include <chrono>
struct Timer
{
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    void tic() { start = std::chrono::high_resolution_clock::now(); }
    double toc()
    {
        std::chrono::duration<double> diff = std::chrono::high_resolution_clock::now() - start;
        return diff.count() * 1000; //ms
    }
};

int main(int argc, char **argv)
{

    FILE *fp = nullptr;
    uint32_t  src_width;
    uint32_t  src_height;
    uint32_t  dst_width;
    uint32_t  dst_height;
    char yuv_name[64] = "";
    char out_name[64] = "";
    int s32Ret;

    if (argc < 7)
    {
        printf("usag:./bgr_resize yuv_file width height  output_file width height\n");
        return -1;
    }
    else
    {
        sprintf(yuv_name, argv[1]);
        src_width = atoi(argv[2]);
        src_height = atoi(argv[3]);
        sprintf(out_name, argv[4]);
        dst_width = atoi(argv[5]);
        dst_height = atoi(argv[6]);
    }

    int  inputsize = src_width * src_height * 3/2;
    uint8_t  *inputdata = nullptr;
    std::vector<uint8_t> dst;

    inputdata = (uint8_t*)malloc(inputsize);
    if(inputdata == nullptr)
    {
        printf("malloc data failed.\n");
        return 0;
    }
    fp = fopen(yuv_name, "rb");
    if(fp == nullptr)
    {
        printf("open file failed.\n");
        free(inputdata);
        return -1;
    }

    s32Ret = fread(inputdata, 1, inputsize, fp);
    fclose(fp);

    /*swcale*/
    int linesize[4];
    uint8_t *data[4];
    struct SwsContext *sc;
    uint8_t *dst_data[4];
    int dst_linesize[4];
    int ret  = 0;

    linesize[0] = src_width;
    linesize[1] = src_width;
    linesize[2] = src_width;
    data[0] = inputdata;
    data[1] = data[0]+src_width*src_height;
    data[2] = data[1]+1;

    Timer total;

again:
    total.tic();
    sc = sws_getContext(src_width, src_height, AV_PIX_FMT_NV21,
                        dst_width, dst_height, AV_PIX_FMT_NV21, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (sc == nullptr)
    {
        printf("malloc sws_getContext failed.\n");
        //free(inputdata);
        return -1;
    }

    ret = av_image_alloc(dst_data, dst_linesize, dst_width, dst_height, AV_PIX_FMT_NV21, 1);
    if (ret < 0)
    {
        printf("Could not allocate dst images, ret:%d\n", ret);
        //free(inputdata);
        sws_freeContext(sc);
        return -2;
    }

    ret = sws_scale(sc, data, linesize, 0, src_height, dst_data, dst_linesize);
    if (ret != (int)dst_height)
    {
        printf("sws scale error.ret:%d\n", ret);
        //free(inputdata);
        sws_freeContext(sc);
        av_freep(&dst_data[0]);
        return -4;
    }

    dst.resize(dst_width * dst_height * 3/2);

    memcpy(&dst[0], dst_data[0], dst_width * dst_height * 3/2);
    //free(inputdata);
    sws_freeContext(sc);
    av_freep(&dst_data[0]);
    printf("HHHHHHHHHHHHHHHHH %s total time: %lf\n", __func__, total.toc());
    goto again;

    FILE *dfp = nullptr;
    dfp = fopen(out_name, "wb");
    if (dfp == NULL)
    {
        printf("open %s file failed.\n", out_name);
        return -1;
    }

    s32Ret = fwrite(&dst[0], 1, dst_width*dst_height*3/2, dfp);
    printf("Write %d bytes to yuv\n", s32Ret);
    fclose(dfp);

    return 0;
}
