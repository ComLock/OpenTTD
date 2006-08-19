/* $Id$ */

#include "stdafx.h"
#include "thread.h"
#include <stdlib.h>

#if defined(__AMIGA__) || defined(__MORPHOS__)
OTTDThread* OTTDCreateThread(OTTDThreadFunc function, void* arg) { return NULL; }
void* OTTDJoinThread(OTTDThread* t) { return NULL; }


#elif defined(__OS2__)

#define INCL_DOS
#include <os2.h>
#include <process.h>

struct OTTDThread {
	TID thread;
	OTTDThreadFunc func;
	void* arg;
	void* ret;
};

static void Proxy(void* arg)
{
	OTTDThread* t = arg;
	t->ret = t->func(t->arg);
}

OTTDThread* OTTDCreateThread(OTTDThreadFunc function, void* arg)
{
	OTTDThread* t = malloc(sizeof(*t));

	if (t == NULL) return NULL;

	t->func = function;
	t->arg  = arg;
	t->thread = _beginthread(Proxy, NULL, 32768, t);
	if (t->thread != -1) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void* OTTDJoinThread(OTTDThread* t)
{
	void* ret;

	if (t == NULL) return NULL;

	DosWaitThread(&t->thread, DCWW_WAIT);
	ret = t->ret;
	free(t);
	return ret;
}


#elif defined(UNIX)

#include <pthread.h>

struct OTTDThread {
	pthread_t thread;
};

OTTDThread* OTTDCreateThread(OTTDThreadFunc function, void* arg)
{
	OTTDThread* t = malloc(sizeof(*t));

	if (t == NULL) return NULL;

	if (pthread_create(&t->thread, NULL, function, arg) == 0) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void* OTTDJoinThread(OTTDThread* t)
{
	void* ret;

	if (t == NULL) return NULL;

	pthread_join(t->thread, &ret);
	free(t);
	return ret;
}


#elif defined(WIN32)

#include <windows.h>

struct OTTDThread {
	HANDLE thread;
	OTTDThreadFunc func;
	void* arg;
	void* ret;
};

static DWORD WINAPI Proxy(LPVOID arg)
{
	OTTDThread* t = arg;
	t->ret = t->func(t->arg);
	return 0;
}

OTTDThread* OTTDCreateThread(OTTDThreadFunc function, void* arg)
{
	OTTDThread* t = malloc(sizeof(*t));
	DWORD dwThreadId;

	if (t == NULL) return NULL;

	t->func = function;
	t->arg  = arg;
	t->thread = CreateThread(NULL, 0, Proxy, t, 0, &dwThreadId);

	if (t->thread != NULL) {
		return t;
	} else {
		free(t);
		return NULL;
	}
}

void* OTTDJoinThread(OTTDThread* t)
{
	void* ret;

	if (t == NULL) return NULL;

	WaitForSingleObject(t->thread, INFINITE);
	CloseHandle(t->thread);
	ret = t->ret;
	free(t);
	return ret;
}
#endif
