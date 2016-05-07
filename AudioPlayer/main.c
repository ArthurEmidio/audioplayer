#include "audioplayer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#undef main // prevenindo que o SDL tenha conflitos com a definição de main

int main(int argc, char *argv[])
{
    startAudioPlayer(argc > 1 ? argv[1] : "summer.mp3", 250);
    
    char *opt = (char*) malloc(sizeof(char) * 100);
    while (1) {
        printf("Comando: ");
        scanf(" %[^\n]s", opt);
        
        if (strcmp(opt, "exit") == 0 || strcmp(opt, "close") == 0) {
            break;
        } else if (strcmp(opt, "pause") == 0) {
            pauseAudioPlayer();
        } else if (strcmp(opt, "play") == 0) {
            playAudioPlayer();
        } else if (strcmp(opt, "stop") == 0) {
            stopAudioPlayer();
        } else if (strcmp(opt, "info") == 0) {
            showAudioInfo();
        } else if (strcmp(opt, "lyrics") == 0) {
            showAudioLyrics();
        } else if (strncmp(opt, "show_buffer", sizeof("show_buffer") - 1) == 0) {
            char *rem = opt + sizeof("show_buffer");
            while (!isdigit(*rem)) rem++;
            int interval = (int) strtol(rem, NULL, 10);
            showAudioBuffer(interval);
        } else if (strcmp(opt, "hide_buffer") == 0) {
            hideAudioBuffer();
        } else {
            printf("Comando Invalido! ");
        }
    }
    
    finishAudioPlayer();
    
    return 0;
}