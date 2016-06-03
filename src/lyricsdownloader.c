#include "lyricsdownloader.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

struct PageData {
    char *data;
    int size;
};
typedef struct PageData PageData;

static size_t _getPageData(void *input, size_t size, size_t inputSize, void *output);
static char* _formatString(const char *str);
static char* _createAZLyricsUrl(const char *artist, const char *title);
static char* _getLyricsFromPageData(PageData pageData);

char* getLyrics(const char *artist, const char *title)
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        return NULL;
    }
    
    PageData pageData;
    pageData.data = malloc(sizeof(char));
    pageData.size = 0;
    
    char *url = _createAZLyricsUrl(artist, title);
    
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _getPageData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &pageData);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    CURLcode code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    free(url);
    
    if (code != CURLE_OK) {
        return NULL;
    }
    
    char *lyrics = _getLyricsFromPageData(pageData);
    
    free(pageData.data);
    
    return lyrics;
}

static size_t _getPageData(void *input, size_t size, size_t inputSize, void *output)
{
    PageData *pageData = (PageData*) output;
    
    pageData->data = realloc(pageData->data, pageData->size + inputSize + 1);
    memcpy(pageData->data + pageData->size, input, inputSize);
    pageData->size += inputSize;
    pageData->data[pageData->size] = 0;
    
    return inputSize;
}

char* _formatString(const char *str)
{
    size_t strSize = strlen(str);
    
    char *result = malloc(strSize + 1);
    int i = 0;
    for (int j = 0; j < strSize; j++) {
        if (isalnum(str[j])) {
            result[i] = tolower(str[j]);
            i++;
        }
    }
    result[i] = 0;
    
    return result;
}

static char* _createAZLyricsUrl(const char *artist, const char *title)
{
    char *artistFormatted = _formatString(artist);
    char *titleFormatted = _formatString(title);
    
    size_t urlSize = strlen("http://www.azlyrics.com/lyrics//.html") + strlen(artistFormatted) + strlen(titleFormatted) + 1;
    char *url = malloc(urlSize);
    sprintf(url, "http://www.azlyrics.com/lyrics/%s/%s.html", artistFormatted, titleFormatted);
        
    free(artistFormatted);
    free(titleFormatted);
    
    return url;
}

static char* _getLyricsFromPageData(PageData pageData)
{
    const char *startLyricsMarker = "Sorry about that. -->";
    const char *endLyricsMarker = "</div>";
    
    int i = 0;
    while (i != pageData.size && strncmp(pageData.data + i, startLyricsMarker, strlen(startLyricsMarker)) != 0) {
        i++;
    }
    
    if (i == pageData.size) {
        return NULL;
    }
    
    while (pageData.data[i] != '>') i++;
    i++;

    int lyricsMaxSize = 500;
    char *lyrics = malloc(sizeof(char) * lyricsMaxSize);
    
    int lyricsCurrIndex = 0;
    while (strncmp(pageData.data + i, endLyricsMarker, strlen(endLyricsMarker)) != 0) {
        if (pageData.data[i] == '<') {
            i++;
            while (pageData.data[i] != '>') i++;
        } else {
            if (lyricsCurrIndex == lyricsMaxSize - 1) {
                lyricsMaxSize *= 2;
                lyrics = realloc(lyrics, lyricsMaxSize);
            }
            lyrics[lyricsCurrIndex++] = pageData.data[i];
        }
        i++;
    }
    lyrics[lyricsCurrIndex] = 0;
    
    if (strlen(lyrics) > 1) {
        if (lyricsCurrIndex == lyricsMaxSize - 1) {
            lyricsMaxSize *= 2;
            lyrics = realloc(lyrics, lyricsMaxSize);
        }
        strcpy(&lyrics[lyricsCurrIndex], "\n\nLyrics obtained from azlyrics.com");
    }
    
    return lyrics;
}
