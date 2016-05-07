#include "audioplayer.h"
#include "audiochunk.h"
#include "lyricsdownloader.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/dict.h>
#include <libavformat/avio.h>

#include <SDL/SDL.h>

static void* _producer(void *args);
static void _consumer(void *userdata, uint8_t *stream, int streamLength);

static void _getStreamInformation();
static AVCodecContext* _getCodecContext(int streamIndex);
static void _configureAudio();

AVFormatContext *formatCtx = NULL;
AVCodecContext *audioCodecCtx = NULL;
int audioStreamIndex = -1;
AudioChunkQueue *audioChunkQueue = NULL;
struct SwrContext *swrCtx = NULL;
SDL_AudioSpec audioSpec;

int producerHasFinished;
pthread_mutex_t producerHasFinishedMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t producerHasFinishedCond = PTHREAD_COND_INITIALIZER;

enum { PLAYING, PAUSED, STOPPED } playbackState;
pthread_mutex_t playbackStateMutex = PTHREAD_MUTEX_INITIALIZER;

int displayAudioBufferInterval = 0;
pthread_mutex_t displayAudioBufferMutex = PTHREAD_MUTEX_INITIALIZER;
sem_t *displayAudioBufferSem;

pthread_mutex_t lyricsMutex = PTHREAD_MUTEX_INITIALIZER;

void* _downloadLyrics(void *args)
{
    AVDictionaryEntry *titleEntry = av_dict_get(formatCtx->metadata, "title", NULL, 0);
    AVDictionaryEntry *artistEntry = av_dict_get(formatCtx->metadata, "artist", NULL, 0);
    AVDictionaryEntry *lyricsEntry = av_dict_get(formatCtx->metadata, "lyrics", NULL, AV_DICT_IGNORE_SUFFIX);
    
    if (lyricsEntry == NULL && titleEntry != NULL && artistEntry != NULL) {
        pthread_mutex_lock(&lyricsMutex);
        char *lyrics = getLyrics(artistEntry->value, titleEntry->value);
        if (lyrics != NULL) {
            av_dict_set(&formatCtx->metadata, "lyrics", lyrics, AV_DICT_DONT_STRDUP_VAL);
        }
        pthread_mutex_unlock(&lyricsMutex);
    }
    
    return NULL;
}

void* _displayAudioBufferFunc(void *args)
{
    int interval;
    
    while (1) {
        sem_wait(displayAudioBufferSem);
        
        while (interval >= 0) {
            pthread_mutex_lock(&audioChunkQueue->mutex);
            printf("Audio buffer: %d / %d\n", audioChunkQueue->quantity, audioChunkQueue->capacity);
            pthread_mutex_unlock(&audioChunkQueue->mutex);
            
            pthread_mutex_lock(&displayAudioBufferMutex);
            interval = displayAudioBufferInterval;
            pthread_mutex_unlock(&displayAudioBufferMutex);
            
            SDL_Delay(interval);
        }
    }
    
    return NULL;
}

void showAudioBuffer(int interval)
{
    pthread_mutex_lock(&displayAudioBufferMutex);
    displayAudioBufferInterval = interval;
    pthread_mutex_unlock(&displayAudioBufferMutex);
    sem_post(displayAudioBufferSem);
}

void hideAudioBuffer()
{
    pthread_mutex_lock(&displayAudioBufferMutex);
    displayAudioBufferInterval = -1;
    pthread_mutex_unlock(&displayAudioBufferMutex);
}

void showAudioLyrics()
{
    pthread_mutex_lock(&lyricsMutex);
    AVDictionaryEntry *lyricsEntry = av_dict_get(formatCtx->metadata, "lyrics", NULL, AV_DICT_IGNORE_SUFFIX);
    pthread_mutex_unlock(&lyricsMutex);
    
    if (lyricsEntry != NULL) {
        printf("%s\n\n", lyricsEntry->value);
    } else {
        printf("There are no lyrics available for this song.\n");
    }
}

void showAudioInfo()
{
    printf("Filename: %s\n", formatCtx->filename);
    printf("Codec: %s\n", audioCodecCtx->codec_descriptor->long_name);
    if (audioCodecCtx->channels > 0 && audioCodecCtx->channels <= 2) {
        printf("Channels: %d (%s)\n", audioCodecCtx->channels, audioCodecCtx->channels == 1 ? "mono" : "stereo");
    } else {
        printf("Channels: %d\n", audioCodecCtx->channels);
    }
    printf("Sample rate: %d Hz\n", audioCodecCtx->sample_rate);
    printf("Duration: %lld s\n", (long long) formatCtx->duration);
    
    AVDictionaryEntry *titleEntry = av_dict_get(formatCtx->metadata, "title", NULL, 0);
    if (titleEntry != NULL) {
        printf("Title: %s\n", titleEntry->value);
    }
    
    AVDictionaryEntry *artistEntry = av_dict_get(formatCtx->metadata, "artist", NULL, 0);
    if (artistEntry != NULL) {
        printf("Artist: %s\n", artistEntry->value);
    }
    
    AVDictionaryEntry *albumEntry = av_dict_get(formatCtx->metadata, "album", NULL, 0);
    if (albumEntry != NULL) {
        printf("Album: %s\n", albumEntry->value);
    }
    
    printf("\n\n");
}

void startAudioPlayer(const char *fileName, int bufferSize)
{
    av_register_all();
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("A SDL nao pode ser inicializada: %s\n", SDL_GetError());
        exit(1);
    }
    
    if (avformat_open_input(&formatCtx, fileName, NULL, NULL) < 0) {
        printf("Erro: arquivo invalido.\n");
        exit(1);
    }
    
    _getStreamInformation();
    
    audioCodecCtx = _getCodecContext(audioStreamIndex);
    audioChunkQueue = createAudioChunkQueue(bufferSize);
    _configureAudio();
    
    sem_unlink("/displayAudioBuffer");
    displayAudioBufferSem = sem_open("/displayAudioBuffer", O_CREAT, 0644, 0);
    pthread_t displayAudioBufferThread;
    pthread_create(&displayAudioBufferThread, NULL, &_displayAudioBufferFunc, NULL);
    
    pthread_t lyricsDownloaderThread;
    pthread_create(&lyricsDownloaderThread, NULL, &_downloadLyrics, NULL);
    
    pthread_t producerThread;
    pthread_create(&producerThread, NULL, &_producer, NULL);
}

void stopAudioPlayer()
{
    pthread_mutex_lock(&playbackStateMutex);
    if (playbackState != STOPPED) {
        playbackState = STOPPED;
        flushAudioChunkQueue(audioChunkQueue, 1);
        pthread_mutex_unlock(&playbackStateMutex);
        
        pthread_mutex_lock(&producerHasFinishedMutex);
        if (producerHasFinished) {
            pthread_cond_broadcast(&producerHasFinishedCond);
        }
        pthread_mutex_unlock(&producerHasFinishedMutex);
    }
    pthread_mutex_unlock(&playbackStateMutex);
}

void pauseAudioPlayer()
{
    pthread_mutex_lock(&playbackStateMutex);
    playbackState = PAUSED;
    pthread_mutex_unlock(&playbackStateMutex);
}

void playAudioPlayer()
{
    pthread_mutex_lock(&playbackStateMutex);
    playbackState = PLAYING;
    pthread_mutex_unlock(&playbackStateMutex);
}

void finishAudioPlayer()
{
    freeAudioChunkQueue(&audioChunkQueue);
    swr_free(&swrCtx);
    avcodec_close(audioCodecCtx);
    avformat_close_input(&formatCtx);
}

void _consumer(void *data, uint8_t *stream, int streamLength)
{
    static int shouldBroadcast = 0;
    
    // Once all the audio has been played, let the producer know the consumption has finished.
    if (shouldBroadcast) {
        stopAudioPlayer();
        shouldBroadcast = 0;
        pthread_cond_broadcast(&producerHasFinishedCond);
        return;
    }
    
    pthread_mutex_lock(&playbackStateMutex);
    if (playbackState == STOPPED || playbackState == PAUSED) {
        pthread_mutex_unlock(&playbackStateMutex);
        return;
    }
    pthread_mutex_unlock(&playbackStateMutex);
    
    AudioChunk *chunk = NULL;
    int bytesFree = streamLength;
    
    while (bytesFree > 0) {
        pthread_mutex_lock(&producerHasFinishedMutex);
        if (sem_trywait(audioChunkQueue->full) != 0) {
            if (producerHasFinished && !shouldBroadcast) {
                shouldBroadcast = 1;
                memset(stream, 0, bytesFree); // filling the remaining space of stream with 0s
                pthread_mutex_unlock(&producerHasFinishedMutex);
                break;
            }
        } else {
            sem_post(audioChunkQueue->full);
        }
        pthread_mutex_unlock(&producerHasFinishedMutex);
        
        getNextAudioChunk(&chunk, audioChunkQueue, bytesFree);
        memcpy(stream, chunk->data, chunk->size);
        bytesFree -= chunk->size;
        stream += chunk->size;
        free(chunk->data);
        free(chunk);
    }
}

void* _producer(void *args)
{
    playbackState = PLAYING;
    SDL_PauseAudio(0);
    
    SDL_Event event;
    producerHasFinished = 0;
    
    AVPacket packet;
    AVFrame *audioFrame = av_frame_alloc();
    int finishedFrame;
    
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    
    while (1) {
        pthread_mutex_lock(&playbackStateMutex);
        if (playbackState == STOPPED) {
            av_seek_frame(formatCtx, audioStreamIndex, 0, 0);
            flushAudioChunkQueue(audioChunkQueue, 1); // WARNING: causa busy waiting
        }
        pthread_mutex_unlock(&playbackStateMutex);
        
        if (av_read_frame(formatCtx, &packet) >= 0) {
            if (packet.stream_index == audioStreamIndex) {
                avcodec_decode_audio4(audioCodecCtx, audioFrame, &finishedFrame, &packet);
                av_packet_unref(&packet);
                
                if (finishedFrame) {
                    AudioChunk *audioChunk = malloc(sizeof(AudioChunk));
                    audioChunk->size = av_samples_get_buffer_size(NULL, audioCodecCtx->channels, audioFrame->nb_samples, AV_SAMPLE_FMT_S16, 1);
                    audioChunk->data = malloc(audioChunk->size);
                    swr_convert(swrCtx, &audioChunk->data, audioChunk->size, (const uint8_t **) audioFrame->data, audioFrame->nb_samples);
                    insertAudioChunk(audioChunk, audioChunkQueue);
                    av_frame_unref(audioFrame);
                }
            } else {
                av_packet_unref(&packet);
            }
        } else {
            pthread_mutex_lock(&producerHasFinishedMutex);
            producerHasFinished = 1;
            pthread_cond_wait(&producerHasFinishedCond, &producerHasFinishedMutex);
            producerHasFinished = 0;
            pthread_mutex_unlock(&producerHasFinishedMutex);
        }
        
        SDL_PollEvent(&event);
    }
    
    return NULL;
}

void _getStreamInformation()
{
    if (avformat_find_stream_info(formatCtx, NULL) < 0) {
        printf("Erro: nao foi possivel encontrar informacoes sobre o conteudo do audio\n");
        exit(1);
    }
    
    for (int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
            break;
        }
    }
    
    if (audioStreamIndex == -1) {
        printf("Erro: nao existe stream de audio no arquivo especificado\n");
        exit(1);
    }
}

AVCodecContext* _getCodecContext(int streamIndex)
{
    AVCodecContext *codecCtxOrig = formatCtx->streams[streamIndex]->codec;
    AVCodec *codec = avcodec_find_decoder(codecCtxOrig->codec_id);
    if (codec == NULL) {
        printf("Erro: codec de video invalido\n");
        exit(1);
    }
    
    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if (avcodec_copy_context(codecCtx, codecCtxOrig) < 0) {
        printf("Erro: nao pode copiar o contexto do codec de video\n");
        exit(1);
    }
    avcodec_close(codecCtxOrig);
    
    if (avcodec_open2(codecCtx, codec, NULL) < 0) {
        printf("Erro: nao pode abrir o codec de video\n");
        exit(1);
    }
    
    return codecCtx;
}

void _configureAudio()
{
    swrCtx = swr_alloc();
    av_opt_set_int(swrCtx, "in_channel_count", audioCodecCtx->channels, 0);
    av_opt_set_int(swrCtx, "out_channel_count", audioCodecCtx->channels, 0);
    av_opt_set_int(swrCtx, "in_channel_layout", audioCodecCtx->channel_layout, 0);
    av_opt_set_int(swrCtx, "out_channel_layout", audioCodecCtx->channel_layout, 0);
    av_opt_set_int(swrCtx, "in_sample_rate", audioCodecCtx->sample_rate, 0);
    av_opt_set_int(swrCtx, "out_sample_rate", audioCodecCtx->sample_rate, 0);
    av_opt_set_sample_fmt(swrCtx, "in_sample_fmt", audioCodecCtx->sample_fmt, 0);
    av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
    swr_init(swrCtx);
    
    SDL_AudioSpec tempAudioSpec;
    tempAudioSpec.freq = audioCodecCtx->sample_rate;
    tempAudioSpec.format = AUDIO_S16SYS;
    tempAudioSpec.channels = 2;
    tempAudioSpec.silence = 0;
    tempAudioSpec.samples = 32768; // TODO: maybe change
    tempAudioSpec.callback = _consumer;
    
    if (SDL_OpenAudio(&tempAudioSpec, &audioSpec) < 0) {
        printf("Erro: erro em abrir o audio: %s\n", SDL_GetError());
        exit(-1);
    }
}
