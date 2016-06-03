#ifndef audioplayer_h
#define audioplayer_h

#include <stdint.h>

/*!
 * \brief Starts the audio player
 * \param fileName The name of the audio file.
 * \param bufferSize The buffer capacity, in number of \c AudioChunks.
 */
void startAudioPlayer(const char *fileName, int bufferSize);

/*!
 * \brief Finishes the program.
 */
void finishAudioPlayer();

/*!
 * \brief Pauses the audio player.
 */
void pauseAudioPlayer();

/*!
 * \brief Plays the audio player.
 */
void playAudioPlayer();

/*!
 * \brief Stops the audio player.
 */
void stopAudioPlayer();

/*!
 * \brief Displays information about the audio file: filename, number of channels, codec,
 * title, artist, album, sample rate, duration.
 */
void showAudioInfo();

/*!
 * \brief Displays the audio lyrics, if available.
 */
void showAudioLyrics();

/*!
 * \brief Displays how many elements are stored in the audio buffer.
 * \param interval The interval on which the information should be displayed, in milliseconds.
 * \sa hideAudioBuffer()
 */
void showAudioBuffer(int interval);

/*!
 \brief Stops displaying buffer information.
 * \sa showAudioBuffer()
 */
void hideAudioBuffer();

#endif /* audioplayer_h */
