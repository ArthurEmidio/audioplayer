#ifndef audioplayer_h
#define audioplayer_h

void startAudioPlayer(const char *fileName, int bufferSize);

void finishAudioPlayer();

void pauseAudioPlayer();

void playAudioPlayer();

void stopAudioPlayer();

void showAudioInfo();

void showAudioLyrics();

void showAudioBuffer(int interval);

void hideAudioBuffer();

#endif /* audioplayer_h */
