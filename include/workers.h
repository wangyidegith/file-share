#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include <vector>
#include <unistd.h>

#define THREAD_POOL_SIZE 4

typedef struct {
    void* (*worker)(void*);
    void* argument;
} Task;

class Workers {
    private:
        std::vector<pthread_t> threads;
        std::queue<Task> taskQueue;
        pthread_mutex_t mutex;
        pthread_cond_t condition;
        bool stop;

        static void* threadFunction(void* pool) {
            Workers* threadPool = (Workers*)pool;
            while (true) {
                Task task;
                pthread_mutex_lock(&threadPool->mutex);
                while (threadPool->taskQueue.empty() && !threadPool->stop) {
                    pthread_cond_wait(&threadPool->condition, &threadPool->mutex);
                }
                if (threadPool->stop) {
                    pthread_mutex_unlock(&threadPool->mutex);
                    break;
                }
                task = threadPool->taskQueue.front();
                threadPool->taskQueue.pop();
                pthread_mutex_unlock(&threadPool->mutex);
                task.worker(task.argument);
            }
            return nullptr;
        }

    public:
        Workers() : stop(false) {
            pthread_mutex_init(&mutex, nullptr);
            pthread_cond_init(&condition, nullptr);
            for (int i = 0; i < THREAD_POOL_SIZE; i++) {
                pthread_t thread;
                pthread_create(&thread, nullptr, threadFunction, this);
                threads.push_back(thread);
            }
        }

        ~Workers() {
            pthread_mutex_lock(&mutex);
            stop = true;
            pthread_cond_broadcast(&condition);
            pthread_mutex_unlock(&mutex);
            for (pthread_t thread : threads) {
                pthread_join(thread, nullptr);
            }
            pthread_mutex_destroy(&mutex);
            pthread_cond_destroy(&condition);
        }

        void enqueue(void* (*worker)(void*), void* argument) {
            pthread_mutex_lock(&mutex);
            taskQueue.push({worker, argument});
            pthread_cond_signal(&condition);
            pthread_mutex_unlock(&mutex);
        }
};

