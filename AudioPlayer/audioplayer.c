#include "audioplayer.h"

AVFormatContext *formatCtx = NULL;
AVCodecContext *audioCodecCtx = NULL;
int audioStreamIndex = -1;
AudioChunkQueue *audioChunkQueue = NULL;
struct SwrContext *swrCtx = NULL;
SDL_AudioSpec audioSpec;

int startVideoPlayer(const char *videoName)
{
    av_register_all();
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("A SDL nao pode ser inicializada: %s\n", SDL_GetError());
        exit(1);
    }
    
    openFile(videoName);
    
    getStreamInformation();
    
    audioCodecCtx = getCodecContext(audioStreamIndex);
    
    audioChunkQueue = createAudioChunkQueue(250);
    
    configureAudio();
    
    pthread_t producerThread;
    pthread_create(&producerThread, NULL, &producer, NULL);
    pthread_join(producerThread, NULL);
    
    avcodec_close(audioCodecCtx);
    
    avformat_close_input(&formatCtx);
    
    return 0;
}

void audioConsumer(void *userdata, uint8_t *stream, int streamLength)
{
    AudioChunk *chunk = NULL;
    int bytesFree = streamLength;
    
    while (bytesFree > 0) {
        if (getNextAudioChunk(&chunk, audioChunkQueue, bytesFree)) {
            memcpy(stream, chunk->data, chunk->size);
            bytesFree -= chunk->size;
            stream += chunk->size;
            free(chunk->data);
            free(chunk);
        } else {
            memset(stream, 0, bytesFree);
        }
    }
}

void* producer(void *args)
{
    SDL_Event event;
    SDL_PauseAudio(0);
    
    AVPacket packet;
    AVFrame *audioFrame = av_frame_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *yuvFrame = av_frame_alloc();
    int finishedFrame;
    
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    
    while (av_read_frame(formatCtx, &packet) >= 0) {
        if (packet.stream_index == audioStreamIndex) {
            avcodec_decode_audio4(audioCodecCtx, audioFrame, &finishedFrame, &packet);
            av_packet_unref(&packet);
            
            if (finishedFrame) {
                AudioChunk *audioChunk = malloc(sizeof(AudioChunk));
                
                audioChunk->size = av_samples_get_buffer_size(NULL, audioCodecCtx->channels, audioFrame->nb_samples, AV_SAMPLE_FMT_S16, 1);
                audioChunk->data = malloc(audioChunk->size);
                swr_convert(swrCtx, &audioChunk->data, audioChunk->size, (const uint8_t **) audioFrame->data, audioFrame->nb_samples);
                
                insertAudioChunk(audioChunk, audioChunkQueue);
            }
        } else {
            av_packet_unref(&packet);
        }
        
        SDL_PollEvent(&event);
        if (event.type == SDL_QUIT) {
            SDL_Quit();
            exit(0);
        }
    }
    
    av_frame_free(&yuvFrame);
    av_frame_free(&frame);
    
    return NULL;
}

void openFile(const char *fileName)
{
    if (avformat_open_input(&formatCtx, fileName, NULL, NULL) < 0) {
        printf("Erro: arquivo invalido.\n");
        exit(1);
    }
}

void getStreamInformation()
{
    if (avformat_find_stream_info(formatCtx, NULL) < 0) {
        printf("Erro: nao foi possivel encontrar informacoes sobre o conteudo do video\n");
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

AVCodecContext* getCodecContext(int streamIndex)
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

void configureAudio()
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
    tempAudioSpec.samples = 1024; // TODO: maybe change
    tempAudioSpec.callback = audioConsumer;
    
    if (SDL_OpenAudio(&tempAudioSpec, &audioSpec) < 0) {
        printf("Erro: erro em abrir o audio: %s\n", SDL_GetError());
        exit(-1);
    }
}
