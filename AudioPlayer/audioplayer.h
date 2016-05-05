#ifndef audioplayer_h
#define audioplayer_h

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>

#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

#include <SDL/SDL.h>

#include "audiochunk.h"

int startVideoPlayer(const char *videoName);

void openFile(const char *fileName);

void getStreamInformation();

AVCodecContext* getCodecContext(int streamIndex);

void configureAudio();

void* producer(void *args);

void audioConsumer(void *userdata, uint8_t *stream, int streamLength);

#endif /* audioplayer_h */
