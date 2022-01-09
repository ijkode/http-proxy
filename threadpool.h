#include <pthread.h>

#define MAXT_IN_POOL 200

typedef struct work_st{
      int (*routine) (void*);  //the threads process function
      void * arg;  //argument to the function
      struct work_st* next;  
} work_t;

typedef struct _threadpool_st {
 	int num_threads;	//number of active threads
	int qsize;	        //number in the queue
	pthread_t *threads;	//pointer to threads
	work_t* qhead;		//queue head pointer
	work_t* qtail;		//queue tail pointer
	pthread_mutex_t qlock;		//lock on the queue list
	pthread_cond_t q_not_empty;	//non empty and empty condidtion vairiables
	pthread_cond_t q_empty;
    int shutdown;            //1 if the pool is in distruction process     
    int dont_accept;       //1 if destroy function has begun
} threadpool;

typedef int (*dispatch_fn)(void *);

threadpool* create_threadpool(int num_threads_in_pool);

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg);

void* do_work(void* p);

void destroy_threadpool(threadpool* destroyme);
