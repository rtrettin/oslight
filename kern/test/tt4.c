#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <synch.h>
#include <thread.h>
#include <current.h>
#include <clock.h>
#include <test.h>
#include <safelist.h>

void init(void);
int appendThread(void *data, unsigned long num);
int appendAndPollThread(void *data, unsigned long num);
int pollThread(void *data, unsigned long num);
int frontThread(void *data, unsigned long num);
void testlist(void);

static struct safelist *global_list;

int tt4(int nargs, char **args) {
	init();
	(void)nargs;
	(void)args;
	testlist();
	safelist_destroy(global_list);
	return 0;
}

void init() {
	global_list = safelist_create();
	if(global_list == NULL) {
		panic("creating new list failed\n");
	}
}

int appendThread(void *data, unsigned long num) {
	(void) data;
	unsigned long val = num;
	safelist_push_back(global_list, &val);
	return 0;
}

int appendAndPollThread(void *data, unsigned long num) {
	(void) data;
	unsigned long val = num;
	safelist_push_back(global_list, &val);
	void* res = safelist_next(global_list);
	unsigned int* pop = res;
	KASSERT(*pop == val);
	return *pop;
}

int pollThread(void *data, unsigned long num) {
	(void) data;
	(void) num;
	void* res = safelist_next(global_list);
	safelist_pop_front(global_list);
	unsigned int* pop = res;
	return *pop;
)

int frontThread(void *data, unsigned long num) {
	(void) data;
	(void) num;
	if(safelist_front(global_list) == NULL) {
		return 0;
	}
	return -1;
}

void testlist() {
	kprintf("testing threads.\n");
	struct thread *front;
	thread_fork("front_thread", NULL, frontThread, NULL, 1, &front);
	KASSERT(thread_join(front) == 0);
	kprintf("Testing return immediately passed.\n");

	struct thread *first;
	struct thread *next;
	struct thread *third;
	thread_fork("append_thread", NULL, appendAndPollThread, NULL, 1, &first);
	thread_fork("append_thread", NULL, appendAndPollThread, NULL, 1, &next);
	thread_fork("append_thread", NULL, appendAndPollThread, NULL, 1, &third);

	KASSERT(thread_join(first) == 1);
	KASSERT(thread_join(next) == 1);
	KASSERT(thread_join(third) == 1);

	KASSERT(safelist_isempty(global_list) == false);
	KASSERT(safelist_getsize(global_list) == 3);
	safelist_assertvalid(global_list);
	kprintf("After push_back, thread had 3 elements.\n");

	safelist_pop_front(global_list);
	safelist_pop_front(global_list);
	safelist_pop_front(global_list);

	thread_fork("append_thread", NULL, appendAndPollThread, NULL, 2, &first);
	KASSERT(thread_join(first) == 2);

	safelist_pop_front(global_list);

	KASSERT(safelist_getsize(global_list) == 0);
	KASSERT(safelist_isempty(global_list) == true);

	kprintf("Testing push_back returns correct number.\n");

	thread_fork("poll_thread", NULL, pollThread, NULL, 1, &first);
	clocksleep(1);
	thread_fork("append_thread", NULL, appendThread, NULL, 3);
	KASSERT(thread_join(first) == 3);

	kprintf("Testing next waited for element. \n");

	safelist_assertvalid(global_list);

	kprintf("All tests passed.\n");
}
