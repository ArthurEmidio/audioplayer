#ifndef audiochunk_h
#define audiochunk_h

#include <semaphore.h>
#include <fcntl.h>
#include <stdint.h>

/*!
 * \brief Represents an audio chunk.
 */
struct AudioChunk {
    uint8_t *data;
    int size;
};
typedef struct AudioChunk AudioChunk;

/*!
 * \brief Represents a linked list of \c AudioChunk.
 */
struct AudioChunkList {
    struct AudioChunkList *next;
    AudioChunk *chunk;
};
typedef struct AudioChunkList AudioChunkList;

/*!
 * \brief Represents a queue of \c AudioChunk.
 */
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

/*!
 * \brief Creates an \c AudioChunk queue with the given capacity.
 * \param maxCapacity The queue's maximum capacity.
 * \return The queue.
 */
AudioChunkQueue* createAudioChunkQueue(int maxCapacity);

/*!
 * \brief Inserts an \c AudioChunk into the queue.
 *
 * This function is thread-safe. In case this function is called when the queue is full,
 * it waits until a new slot is available.
 * \param chunk The \c AudioChunk to be enqueued.
 * \param queue The queue.
 * \return 1 if the chunk was succesfully inserted, or 0 otherwise (\c queue is \c NULL).
 *
 * \sa getNextAudioChunk()
 */
int insertAudioChunk(AudioChunk *chunk, AudioChunkQueue *queue);

/*!
 * \brief Dequeues an \c AudioChunk.
 *
 * This function is thread-safe. In case this function is called when the queue is empty, it
 * waits until a new \c AudioChunk is inserted into the queue.
 * \param chunk[out] The dequeued \c AudioChunk.
 * \param queue The queue.
 * \param howManyBytes How many bytes the \c AudioChunk should contain, at maximum.
 * If the next \c AudioChunk to be dequeued has a larger size than \c howManyBytes, the remaining
 * will be available in the next dequeued \c AudioChunk.
 * function merges multiple \c AudioChunks to fulfill this requirement.
 * \return 1 if an \c AudioChunk was dequeued, or 0 otherwise (\c queue is \c NULL).
 *
 * \sa insertAudioChunk()
 */
int getNextAudioChunk(AudioChunk **chunk, AudioChunkQueue *queue, int howManyBytes);

/*!
 * \brief Flushes the given queue.
 * \param queue The queue to be flushed.
 * \param shouldUnlockMutex Whether the function should unlock the queue's mutex after flushing it.
 */
void flushAudioChunkQueue(AudioChunkQueue *queue, int shouldUnlockMutex);

/*!
 * \brief Frees the given queue.
 * \param queue The queue to be freed.
 */
void freeAudioChunkQueue(AudioChunkQueue **queue);

#endif /* audiochunk_h */
