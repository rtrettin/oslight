// Unit test for thread_join
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <clock.h>
#include <test.h>
#include <safelist.h>
#include <lib.h>
#include <synch.h>
#include <types.h>

void start();

int addThread(void *data, unsigned long num);
int apThread(void *data, unsigned long num);
int pollThread(void *data, unsigned long num);
int topThread(void *data, unsigned long num);

void test_all();

static struct safelist *glist;

int tt4(int nargs, char **args) {
	start();
	(void)nargs;
	(void)args;
	test_all();
	safelist_destroy(glist);
	return 0;
}

void start() {
	glist = safelist_create();
	if(glist == NULL) {
		panic("creating new list failed\n");
	}
}

int addThread(void *data, unsigned long num) {
	(void)data;
	unsigned long val = num;
	safelist_push_back(glist, &val);
	return 0;
}

int apThread(void *data, unsigned long num) {
	(void)data;
	unsigned long val = num;
	safelist_push_back(glist, &val);
	void *res = safelist_next(glist);
	unsigned int *pop = res;
	KASSERT(*pop == val);
	return *pop;
}

int pollThread(void *data, unsigned long num) {
	(void)data;
	(void)num;
	void *res = safelist_next(glist);
	safelist_pop_front(glist);
	unsigned int *pop = res;
	return *pop;
)

int topThread(void *data, unsigned long num) {
	(void)data;
	(void)num;
	if(safelist_front(glist) == NULL) {
		return 0;
	}
	return -1;
}

void test_all() {
	kprintf("testing threads.\n");
	struct thread *top;
	thread_fork("top_thread", NULL, topThread, NULL, 1, &top);
	KASSERT(thread_join(top) == 0);
	kprintf("Testing return immediately passed.\n");

	struct thread *first;
	struct thread *next;
	struct thread *third;
	thread_fork("append_thread", NULL, apThread, NULL, 1, &first);
	thread_fork("append_thread", NULL, apThread, NULL, 1, &next);
	thread_fork("append_thread", NULL, apThread, NULL, 1, &third);

	KASSERT(thread_join(first) == 1);
	KASSERT(thread_join(next) == 1);
	KASSERT(thread_join(third) == 1);

	KASSERT(safelist_isempty(glist) == false);
	KASSERT(safelist_getsize(glist) == 3);
	safelist_assertvalid(glist);
	kprintf("After push_back, thread had 3 elements.\n");

	safelist_pop_front(glist);
	safelist_pop_front(glist);
	safelist_pop_front(glist);

	thread_fork("append_thread", NULL, apThread, NULL, 2, &first);
	KASSERT(thread_join(first) == 2);

	safelist_pop_front(glist);

	KASSERT(safelist_getsize(glist) == 0);
	KASSERT(safelist_isempty(glist) == true);

	kprintf("Testing push_back returns correct number.\n");

	thread_fork("poll_thread", NULL, pollThread, NULL, 1, &first);
	clocksleep(1);
	thread_fork("append_thread", NULL, addThread, NULL, 3);
	KASSERT(thread_join(first) == 3);

	kprintf("Testing next waited for element. \n");

	safelist_assertvalid(glist);

	kprintf("All tests passed.\n");
}
