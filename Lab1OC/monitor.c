#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int ready = 0;

void* provider(void* arg) {
    while (1) {
        sleep(1); 
        pthread_mutex_lock(&lock);
        if (ready == 1) {
            pthread_mutex_unlock(&lock);
            continue;
        }
        ready = 1;
        printf("Event provided\n");
        pthread_cond_signal(&cond1);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void* consumer(void* arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        while (ready == 0) {
            pthread_cond_wait(&cond1, &lock);
        }
        ready = 0;
        printf("Event consumed\n");
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main() {
    pthread_t provider_thread, consumer_thread;

    pthread_create(&provider_thread, NULL, provider, NULL);
    pthread_create(&consumer_thread, NULL, consumer, NULL);

    pthread_join(provider_thread, NULL);
    pthread_join(consumer_thread, NULL);

    return 0;
}
