#ifndef MUTEX_LOCK_HPP
#define MUTEX_LOCK_HPP

#include <Mutex.hpp>

namespace godot {

class MutexLock {
	Mutex *mutex;

public:
	MutexLock(Mutex *p_mutex) {
		mutex = p_mutex;
		if (mutex) mutex->lock();
	}
	~MutexLock() {
		if (mutex) mutex->unlock();
	}
};

}

#endif