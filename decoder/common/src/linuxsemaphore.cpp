
#include "linuxsemaphore.h"
#define NULL (void *) 0

#if defined LINUX

int LINUX_sem_init(LINUX_sem_t *sem, int pshared, int initialValue)

{
	int result = sem_init((sem->msem), pshared, initialValue);
	//sem->value = initialValue;

	return result;
}



int LINUX_sem_destroy(LINUX_sem_t *sem)

{
	int result = sem_destroy((sem->msem));
	//sem->value = -1;

	return result;
}



int LINUX_sem_wait(LINUX_sem_t *sem)

{

	int result = sem_wait((sem->msem));

	/*if(result == 0)
	{
		--(sem->value);
	}*/

	return result;
}



int LINUX_sem_trywait(LINUX_sem_t *sem)

{	
	int result = sem_trywait((sem->msem));

	/*if(result == 0)
	{
		--(sem->value);    
	}*/
	return result;

}



int LINUX_sem_post(LINUX_sem_t *sem)

{

	int result = sem_post((sem->msem));

	/*if(result == 0)
	{
		++(sem->value);
	}*/

	return result;
}



int LINUX_sem_getvalue (LINUX_sem_t *sem, int *sval)

{

	if(sem == NULL)
		return -1;

	return sem_getvalue(sem->msem, sval);

	//return *sval = sem->value;
}


#endif


