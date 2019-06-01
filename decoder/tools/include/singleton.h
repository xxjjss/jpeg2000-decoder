
#pragma once

#include <assert.h>

template<class T>
class Singleton
{
    public:
        static T * GetInstance()
        {
            return mInstance;
        }

		static bool IsCreated()
		{
			return (mInstance != NULL);
		}


		static void Destory()
		{
			if (mInstance != NULL)
			{
				delete mInstance;
				mInstance = NULL;
			}
		}

		static T * Create()
		{
			if (mInstance == NULL)
			{
				mInstance = new T();
			}
			assert(mInstance);
			return mInstance;
		}

	protected:
        Singleton(){}

		virtual	~Singleton() {}

	private:
		// disable constructor; copy-constructor and operator =
        Singleton(const T&);
        Singleton& operator=(const T&);

		static T * mInstance;
};

template <class T>
T *Singleton<T>::mInstance = NULL;

