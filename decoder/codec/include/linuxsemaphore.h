#pragma once


#if defined LINUX

#include <semaphore.h>
//#include <mach/task.h>
//#include <mach/mach.h>

typedef struct LIN_sem_t {

	 	/*LIN_sem_t(void * i) //For initialization with NULL
	 	{
	 		msem = new sem_t;
	 	}*/
	 	LIN_sem_t()
	 	{
	 		msem = new sem_t;
	 	}

	 	~LIN_sem_t()
	 	{
	 		//if(msem) delete msem;
	 	}

	sem_t * msem;

	//int value;

}LINUX_sem_t,  *PLINUX_sem_t;


int LINUX_sem_init(LINUX_sem_t *sem, int pshared, int initialValue);

int LINUX_sem_destroy(LINUX_sem_t *sem);

int LINUX_sem_wait(LINUX_sem_t *sem);

int LINUX_sem_trywait(LINUX_sem_t *sem);

int LINUX_sem_post(LINUX_sem_t *sem);

int LINUX_sem_getvalue (LINUX_sem_t *sem, int *sval);

#endif
