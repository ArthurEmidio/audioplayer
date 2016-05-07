#include "audiochunk.h"

#include <libavutil/opt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

AudioChunkQueue* createAudioChunkQueue(int maxCapacity)
{
    AudioChunkQueue *queue = malloc(sizeof(AudioChunkQueue));
    queue->first = NULL;
    queue->last = NULL;
    queue->quantity = 0;
    queue->capacity = maxCapacity;
    queue->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    
    sem_unlink("/full");
    sem_unlink("/empty");
    
    queue->full = sem_open("/full", O_CREAT, 0644, 0);
    queue->empty = sem_open("/empty", O_CREAT, 0644, maxCapacity);
    
    return queue;
}

void freeAudioChunk(AudioChunk **chunk) {
    free((*chunk)->data);
    free(*chunk);
}

void freeAudioChunkList(AudioChunkList **list)
{
    if (list == NULL) return;
    
    AudioChunkList *next = (*list)->next;
    freeAudioChunk(&(*list)->chunk);
    free(*list);
    freeAudioChunkList(&next);
}

void flushAudioChunkQueue(AudioChunkQueue *queue, int shouldUnlockMutex)
{
    pthread_mutex_lock(&queue->mutex);
    
    AudioChunkList *chunkList = queue->first;
    while (chunkList != NULL) {
        AudioChunkList *next = chunkList->next;

        freeAudioChunk(&chunkList->chunk);
        free(chunkList);

        chunkList = next;
        
        sem_wait(queue->full);
        sem_post(queue->empty);
    }
    
    queue->first = NULL;
    queue->last = NULL;
    queue->quantity = 0;
    
    if (shouldUnlockMutex) {
        pthread_mutex_unlock(&queue->mutex);
    }
}

void freeAudioChunkQueue(AudioChunkQueue **queue)
{
    flushAudioChunkQueue(*queue, 0);
    
    sem_unlink("/full");
    sem_unlink("/empty");
    
    sem_close((*queue)->full);
    sem_close((*queue)->empty);
    
    pthread_mutex_destroy(&(*queue)->mutex);
    
    free(*queue);
    *queue = NULL;
}

int insertAudioChunk(AudioChunk *chunk, AudioChunkQueue *queue)
{
    if (queue == NULL) {
        return 0;
    }
    
    AudioChunkList *newChunkList = malloc(sizeof(AudioChunkList));
    newChunkList->chunk = chunk;
    newChunkList->next = NULL;
    
    sem_wait(queue->empty);
    
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->quantity == 0) {
        queue->first = newChunkList;
    } else {
        queue->last->next = newChunkList;
    }
    queue->last = newChunkList;
    
    queue->quantity++;
    
    pthread_mutex_unlock(&queue->mutex);
    
    sem_post(queue->full);
    
    return 1;
}

int getNextAudioChunk(AudioChunk **chunk, AudioChunkQueue *queue, int howManyBytes)
{
    if (queue == NULL) {
        return 0;
    }
    
    sem_wait(queue->full);
    pthread_mutex_lock(&queue->mutex);
    
//    printf("%d\n", queue->quantity);
    
    AudioChunkList *first = queue->first;
    if (first->chunk->size <= howManyBytes) {
        *chunk = first->chunk;
        queue->first = queue->first->next;
        
        queue->quantity--;
        if (queue->quantity == 0) {
            queue->first = NULL;
            queue->first = NULL;
        }
        
        free(first);
        sem_post(queue->empty);
    } else {
        sem_post(queue->full);
        
        *chunk = malloc(sizeof(AudioChunk));
        (*chunk)->size = howManyBytes;
        (*chunk)->data = malloc(howManyBytes);
        memcpy((*chunk)->data, first->chunk->data, howManyBytes);
        
        first->chunk->size -= howManyBytes;
        uint8_t *remainingData = malloc(first->chunk->size);
        memcpy(remainingData, first->chunk->data + howManyBytes, first->chunk->size);
        free(first->chunk->data);
        first->chunk->data = remainingData;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    
    return 1;
}
