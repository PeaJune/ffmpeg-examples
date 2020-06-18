#include "decode.hpp"
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
    if (argc < 2)
    {
        std::cout << "./media  filename" << std::endl;
        return -1;
    }

    VideoCapture vcp;
    int ret = 0;
    int count = 10;

    ret = vcp.open(argv[1]);
    if(ret != 0)
    {
        printf("open file failed.error:%d\n", ret);
        return -1;
    }
again:
    Timer total;
    total.tic();
    ret = vcp.grab();
    if(ret != 0)
    {
        printf("grab frame failed,error:%d\n", ret);
        vcp.close();
        return -2;
    }

    ret = vcp.retrieve();
    printf("HHHHHHHHHHHHHHHHH %s total time: %lf\n", __func__, total.toc());
    
    count--;
    if(count>=0)
        goto again;
    printf("close...\n");
    vcp.close();
    printf("close...\n");

    return 0;
}

