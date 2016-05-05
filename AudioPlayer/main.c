#include "audioplayer.h"

#undef main // prevenindo que o SDL tenha conflitos com a definição de main

int main(int argc, char *argv[])
{
    startVideoPlayer(argc > 1 ? argv[1] : "summer.mp3");
    
    return 0;
}