#include "audioplayer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#undef main // prevenindo que o SDL tenha conflitos com a definição de main

int main(int argc, char *argv[])
{
    startAudioPlayer(argc > 1 ? argv[1] : "girl.mp3", 250);
    
    char *opt = (char*) malloc(sizeof(char) * 100);
    while (1) {
        printf("Command (enter \"help\" to view all options): ");
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
        } else if (strncmp(opt, "show_buffer", strlen("show_buffer")) == 0) {
            char *rem = opt + strlen("show_buffer") + 1;
            while (*rem && !isdigit(*rem)) rem++;
            int interval = *rem != 0 ? ((int) strtol(rem, NULL, 10)) : 200;
            showAudioBuffer(interval);
        } else if (strcmp(opt, "hide_buffer") == 0) {
            hideAudioBuffer();
        } else if (strcmp(opt, "help") == 0) {
            printf("Available commands:\n");
            printf("\tplay: plays the song.\n");
            printf("\tpause: pauses the song.\n");
            printf("\tstop: stops the song.\n");
            printf("\tinfo: displays information about the file.\n");
            printf("\tlyrics: displays the song's lyrics (if available on file or at azlyrics.com).\n");
            printf("\tshow_buffer N: displays the buffer occupation every N milliseconds. N is 200, by default.\n");
            printf("\thide_buffer: disables the command given by the command \"show_buffer\".\n");
            printf("\texit: exits the player.\n");
            printf("\n\n");
        } else {
            printf("Invalid command!\n");
        }
    }
    
    finishAudioPlayer();
    
    return 0;
}
