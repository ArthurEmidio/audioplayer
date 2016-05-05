#ifndef audiochunk_h
#define audiochunk_h

#include <semaphore.h>

struct AudioChunk {
    uint8_t *data;
    int size;
};
typedef struct AudioChunk AudioChunk;

struct AudioChunkList {
    struct AudioChunkList *next;
    AudioChunk *chunk;
};
typedef struct AudioChunkList AudioChunkList;

struct AudioChunkQueue {
    AudioChunkList *first;
    AudioChunkList *last;
    pthread_mutex_t mutex;
    int quantity;
    int capacity;
    sem_t *full;
    sem_t *empty;
};
typedef struct AudioChunkQueue AudioChunkQueue;

AudioChunkQueue* createAudioChunkQueue(int maxCapacity);

int insertAudioChunk(AudioChunk *chunk, AudioChunkQueue *queue);

int getNextAudioChunk(AudioChunk **chunk, AudioChunkQueue *queue, int howManyBytes);

#endif /* audiochunk_h */
