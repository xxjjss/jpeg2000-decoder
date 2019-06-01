#pragma once


#if defined MAC_OS

#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/mach.h>

typedef struct _sem_t {

// 	_sem_t(void * i) {}
// 	_sem_t() {}

	semaphore_t msem;

	int value;

}  Mac_sem_t;


int Mac_sem_init(Mac_sem_t *sem, int pshared, int initialValue);

int Mac_sem_destroy(Mac_sem_t *sem);

int Mac_sem_wait(Mac_sem_t *sem);

int Mac_sem_trywait(Mac_sem_t *sem);

int Mac_sem_post(Mac_sem_t *sem);

int Mac_sem_getvalue (Mac_sem_t *sem, int *sval);

#endif
