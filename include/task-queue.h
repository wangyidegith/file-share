#include <pthread.h>

#define QUEUE_SIZE 1024

template<typename T>
class TaskQueue {
    private:
        T items[QUEUE_SIZE];
        int front;
        int rear;
        int count;
        pthread_mutex_t mutex;
        pthread_cond_t not_full;
        pthread_cond_t not_empty;
    public:
        TaskQueue();
        void enqueue(T item);
        T dequeue();
        ~TaskQueue();
};

template<typename T>
TaskQueue<T>::TaskQueue() {
    front = 0;
    rear = 0;
    count = 0;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&not_full, NULL);
    pthread_cond_init(&not_empty, NULL);

}

template<typename T>
void TaskQueue<T>::enqueue(T item) {
    pthread_mutex_lock(&mutex);
    while (count == QUEUE_SIZE) {
        pthread_cond_wait(&not_full, &mutex);
    }
    items[rear] = item;
    rear = (rear + 1) % QUEUE_SIZE;
    count++;
    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&mutex);
}

template<typename T>
T TaskQueue<T>::dequeue() {
    pthread_mutex_lock(&mutex);
    while (count == 0) {
        pthread_cond_wait(&not_empty, &mutex);
    }
    T item = items[front];
    front = (front + 1) % QUEUE_SIZE;
    count--;
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&mutex);
    return item;
}

template<typename T>
TaskQueue<T>::~TaskQueue() {
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&not_full);
    pthread_cond_destroy(&not_empty);
}
