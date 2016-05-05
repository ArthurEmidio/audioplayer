#include "audiochunk.h"

#include <pthread.h>
#include <libavutil/opt.h>

AudioChunkQueue* createAudioChunkQueue(int maxCapacity)
{
    AudioChunkQueue *queue = av_malloc(sizeof(AudioChunkQueue));
    queue->first = NULL;
    queue->last = NULL;
    queue->quantity = 0;
    queue->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    
    sem_unlink("/audioQueueFull");
    sem_unlink("/audioQueueEmpty");
    
    queue->full = sem_open("/audioQueueFull", O_CREAT, 0644, 0);
    queue->empty = sem_open("/audioQueueEmpty", O_CREAT, 0644, maxCapacity);
    
    return queue;
}

int insertAudioChunk(AudioChunk *chunk, AudioChunkQueue *queue)
{
    if (queue == NULL) {
        return 0;
    }
    
    AudioChunkList *newChunkList = av_malloc(sizeof(AudioChunkList));
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
    } else {
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
    sem_post(queue->empty);
    
    return 1;
}
