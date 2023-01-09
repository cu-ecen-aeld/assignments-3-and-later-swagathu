#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int ret = 0, ret1 = 0;
    struct thread_data * params = (struct thread_data *) thread_param;
    
    usleep(params->wait_to_obtain_ms *1000);
    ret += pthread_mutex_lock(params->mutex);
    usleep(params->wait_to_release_ms *1000);
    ret += pthread_mutex_unlock(params->mutex);
    if (ret == 0 && ret1 == 0)
    {
        params->thread_complete_success = true;
    }
    else
    {
        params->thread_complete_success = false;
    }
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    int ret = 0;
    struct thread_data * thread_param;
    thread_param = malloc(sizeof(struct thread_data));

    thread_param->mutex = mutex;
    thread_param->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_param->wait_to_release_ms = wait_to_release_ms;
    
    ret = pthread_create(thread, NULL, &threadfunc, (void *)thread_param);
    if (ret != 0)
    {
        printf("Error thread creation: %s\n",strerror(errno));
        return false;
    }

    return true;
}
