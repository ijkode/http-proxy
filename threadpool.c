#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>

threadpool* create_threadpool(int num_threads_in_pool) {
    if(num_threads_in_pool<1 || num_threads_in_pool>MAXT_IN_POOL) {
        perror("error: usage\n");
        return NULL;
    }
    threadpool *t_pool = (threadpool*)malloc(sizeof (threadpool));
    if (t_pool == NULL)
    {
        perror("error: malloc\n");
        return NULL;
    }
    t_pool->num_threads = num_threads_in_pool;
    t_pool->qsize = 0;
    pthread_mutex_init(&t_pool->qlock, NULL);
    pthread_cond_init(&t_pool->q_not_empty, NULL);
    pthread_cond_init(&t_pool->q_empty, NULL);
    t_pool->qhead = NULL;
    t_pool->qtail = NULL;
    t_pool->shutdown = 0;
    t_pool->dont_accept = 0;
    t_pool->threads = (pthread_t*)calloc(num_threads_in_pool, sizeof(pthread_t));
    if (t_pool->threads == NULL) {
        perror("error: calloc\n");
        return NULL;
    }

    for(int i = 0; i<num_threads_in_pool ; i++) {
        pthread_create(&t_pool->threads[i],NULL,do_work,t_pool);
    }
    return t_pool;
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg) {

    if(from_me == NULL || dispatch_to_here == NULL) { // check for valid input
        return;
    }
    pthread_mutex_lock(&(from_me->qlock));
    if(from_me->dont_accept == 1){
        pthread_mutex_unlock(&(from_me->qlock));
        return;
    }
    pthread_mutex_unlock(&(from_me->qlock));


    work_t *work = (work_t*)malloc(sizeof(work_t));
    if (work == NULL){
        perror("error: malloc\n");
        return;
    }

    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;
    pthread_mutex_lock(&(from_me->qlock));
    if(from_me->dont_accept == 1) {
        free(work);
        return;
    }

    if (from_me->qsize == 0) {
        from_me->qhead = work;
        from_me->qtail = from_me->qhead;
    }
    else {
        from_me->qtail->next = work;
        from_me->qtail = from_me->qtail->next;
    }

    from_me->qsize++;
    pthread_cond_signal(&(from_me->q_not_empty));
    pthread_mutex_unlock(&(from_me->qlock));
}

void* do_work(void* p) {
    if(p == NULL)
        return NULL;

    threadpool *t_pool = (threadpool*)p;
    work_t *tmp;
    while (1) {
        pthread_mutex_lock(&(t_pool->qlock));
        if(t_pool->shutdown == 1) {
            pthread_mutex_unlock(&(t_pool->qlock));
            return NULL;
        }
        if(t_pool->qsize == 0) {
            pthread_cond_wait(&(t_pool->q_not_empty), &(t_pool->qlock));
        }
        if (t_pool->shutdown == 1) {
            pthread_mutex_unlock(&(t_pool->qlock));
            return NULL;
        }
        t_pool->qsize--;
        tmp = t_pool->qhead;
        if (t_pool->qsize == 0) {
            t_pool->qhead = NULL;
            t_pool->qtail = NULL;

            if (t_pool->dont_accept == 1) {
                pthread_cond_signal(&(t_pool->q_empty));
            }
        }
        else {
            t_pool->qhead = t_pool->qhead->next;
        }
        pthread_mutex_unlock(&(t_pool->qlock));
        tmp->routine(tmp->arg);
        free(tmp);
    }
}

void destroy_threadpool(threadpool* destroyme) {
    if (destroyme == NULL) {
        return;
    }
    pthread_mutex_lock(&(destroyme->qlock));

    destroyme->dont_accept = 1;

    if (destroyme->qsize != 0) {
        pthread_cond_wait(&(destroyme->q_empty), &(destroyme->qlock));
    }
    destroyme->shutdown = 1;

    pthread_cond_broadcast(&(destroyme->q_not_empty));
    pthread_mutex_unlock(&(destroyme->qlock));

    for (int i = 0; i < destroyme->num_threads; i++) {
        pthread_join(destroyme->threads[i], NULL);
    }

    if (destroyme->threads != NULL) {
        free(destroyme->threads);
    }

    free(destroyme);
}