#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <inttypes.h>

#define MAX_THREADS 4
#define TIME 500000
#define TIMERSIG SIGVTALRM

static const int s_stack_size = 0x400000;
static char *s_stack_to_free = NULL;

struct green_thread {
	struct gt_context {
		uint64_t rsp;
		uint64_t r15;
		uint64_t r14;
		uint64_t r13;
		uint64_t r12;
		uint64_t rbx;
		uint64_t rbp;
	} context;
	enum {
		Unused,
		Running,
		Ready,
	} state;
	char *stack;
};

struct green_thread gt_table[MAX_THREADS];
struct green_thread *current_gt;

void gt_init();
void gt_return(int ret);
void gt_switch(struct gt_context *old, struct gt_context *new);
bool gt_schedule();
static void gt_stop();
int gt_create(void (*function)());

static void handler(int signo, siginfo_t *si, void *ctx) {
    gt_schedule();
}

void gt_init()
{
	current_gt = &gt_table[0];
	current_gt->state = Running;
}

void __attribute__((noreturn))
gt_return(int exitValue)
{
    /*timer_settime(timer, 0, &t_val, NULL);*/
	if (current_gt != &gt_table[0]) {
		current_gt->state = Unused;
		gt_schedule();
		assert(!"reachable");
	}

	while (gt_schedule()) {
		;
	}

	exit(exitValue);
}

bool gt_schedule()
{
	struct green_thread *next_gt = current_gt;
	while (next_gt->state != Ready) {
		next_gt++;
		if (next_gt == &gt_table[MAX_THREADS]) {
			next_gt = &gt_table[0];
		}
		if (next_gt == current_gt) {
			return false;
		}
	}

	if (current_gt->state != Unused) {
		current_gt->state = Ready;
	}
	next_gt->state = Running;

	if (current_gt->state == Unused) {
		s_stack_to_free = current_gt->stack;
	}

	struct gt_context *old = &current_gt->context;
	current_gt = next_gt;
	gt_switch(old, &next_gt->context);

	if (s_stack_to_free) {
		printf("Freeing stack!\n");
		free(s_stack_to_free);
		s_stack_to_free = NULL;
	}

	return true;
}

static void gt_stop() {
	gt_return(0);
}

int gt_create(void (*function)())
{
	struct green_thread *p;
	for (p = &gt_table[0];; p++) {
		if (p == &gt_table[MAX_THREADS]) {
			return -1;
		} else if (p->state == Unused) {
			break;
		}
	}

	char *stack = malloc(s_stack_size);
	if (!stack) {
		return -1;
	}
	p->stack = stack;

	*(uint64_t *)&stack[s_stack_size -  8] = (uint64_t)gt_stop;
	*(uint64_t *)&stack[s_stack_size - 16] = (uint64_t)function;
	p->context.rsp = (uint64_t)&stack[s_stack_size - 16];
	p->state = Ready;

	return 0;
}

void do_work()
{
	static int x;
	int id = ++x;
	for (uint64_t i = 0; i < 10000000; i++) {
		printf("%d %" PRIu64 "\n", id, i);
	}
}

int main()
{
    //setup timer interrupt handler
    struct sigaction act = {
        .sa_sigaction = handler,
        .sa_flags = SA_NODEFER,
    };
    sigemptyset( &act.sa_mask );
    sigaction(TIMERSIG, &act, NULL );

    //Set the virtual timer to interrupt evert TIME nanoseconds
    static const struct itimerval t_val = {{0,TIME}, {0, TIME},};
    setitimer(ITIMER_VIRTUAL, &t_val, NULL);

	gt_init();
	gt_create(do_work);
	gt_create(do_work);
    gt_return(1);
}
