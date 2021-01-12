// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifdef MUMBLE
	#include "mumble_pch.hpp"
#else
	#include "murmur_pch.h"
#endif

#include "SSLLocks.h"

static QMutex **locks = NULL;

void locking_callback(int mode, int type, const char *, int) {
	if (mode & CRYPTO_LOCK) {
		locks[type]->lock();
	} else {
		locks[type]->unlock();
	}
}

unsigned long id_callback() {
	// OpenSSL's thread ID callback must return an unsigned long.
	// It's therefore important that we make sure the value from
	// Qt's QThread::currentThreadId actually makes sense to return
	// in this case.
	//
	// On Windows, the return type is a DWORD. This is excellent,
	// because on an LLP64 like 64-bit Windows, unsigned long is
	// only 32-bits wide, just like DWORD.
	//
	// On macOS and embedded Linux, it's void *. And when Qt targets
	// X11, it's unsigned long.
	//
	// So, in all cases, it's safe to use unsigned long.
	//
	// The trouble is, since the return type of QThread::currentThreadId()
	// differs between platforms, we have to be clever about how we convert
	// that type to an unsigned long. The way we do it here is to first convert
	// it to an uintptr_t, since both pointer types and integer types will
	// convert to that without the compiler complaining. Then, we convert
	// *that* to an unsigned long. (Note that for LLP64 platforms, such as
	// 64-bit Windows, this conversion is from a 64-bit type to a 32-bit type)
	uintptr_t val = reinterpret_cast<uintptr_t>(QThread::currentThreadId());
	return static_cast<unsigned long>(val);
}

void SSLLocks::initialize() {
	int nlocks = CRYPTO_num_locks();

	locks = reinterpret_cast<QMutex **>(calloc(nlocks, sizeof(QMutex *)));
	if (locks == NULL) {
		qFatal("SSLLocks: unable to allocate locks array");

		// This initializer is called early during program
		// execution, and the message handler that causes
		// qFatal to terminate execution might not be registered
		// yet. So, do exit(1) just in case.
		exit(1);
	}

	for (int i = 0; i < nlocks; i++) {
		locks[i] = new QMutex;
	}

	CRYPTO_set_locking_callback(locking_callback);
	CRYPTO_set_id_callback(id_callback);
}

void SSLLocks::destroy() {
	// If SSLLocks was never initialized, or has been destroyed already,
	// don't try to do it again.
	if (locks == NULL) {
		return;
	}

	CRYPTO_set_locking_callback(NULL);
	CRYPTO_set_id_callback(NULL);

	int nlocks = CRYPTO_num_locks();
	for (int i = 0; i < nlocks; i++) {
		delete locks[i];
	}

	free(locks);
	locks = NULL;
}
