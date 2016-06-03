#ifndef lyricsdownloader_h
#define lyricsdownloader_h

/*!
 * \brief Browses the website azlyrics.com for lyrics.
 * \param artist The song's artist.
 * \param title The song's title.
 * \return The lyrics, if available. Otherwise, \c NULL is returned.
 */
char* getLyrics(const char *artist, const char *title);

#endif /* lyricsdownloader_h */
