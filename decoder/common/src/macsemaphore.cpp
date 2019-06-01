
#include "macsemaphore.h"

#if defined MAC_OS

int Mac_sem_init(Mac_sem_t *sem, int pshared, int initialValue)

{

    int result = semaphore_create(mach_task_self(), &(sem->msem), SYNC_POLICY_FIFO, initialValue);

    sem->value = initialValue;

    return result;
}

 

int Mac_sem_destroy(Mac_sem_t *sem)

{
    int result = semaphore_destroy(mach_task_self(), sem->msem);

    sem->value = -1;

    return result;
}

 

int Mac_sem_wait(Mac_sem_t *sem)

{

    int result = semaphore_wait(sem->msem);

	if(result == 0)
	{
		--(sem->value);
	}

    return result;
}

 

int Mac_sem_trywait(Mac_sem_t *sem)

{

    mach_timespec_t wait_time = {0, 0};
    int result = semaphore_timedwait(sem->msem, wait_time);

    if(result == 0)
	{
		--(sem->value);    
	}
    return result;

}

 

int Mac_sem_post(Mac_sem_t *sem)

{

  int result = semaphore_signal(sem->msem);

    if(result == 0)
	{
		++(sem->value);
	}

	return result;
}

 

int Mac_sem_getvalue (Mac_sem_t *sem, int *sval)

{

     if(sem == NULL)
	return -1;
     return *sval = sem->value;
}


#endif


