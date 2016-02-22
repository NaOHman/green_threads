ifeq ($(shell uname), Darwin)
	APPLE_CCFLAGS = -m64
	APPLE_ASFLAGS = -arch x86_64
endif

CFLAGS = $(APPLE_CCFLAGS) -g -Wall -lrt

green_threads: green_threads.o gt_switch.o
	$(CC) $(APPLE_CCFLAGS) -o $@ $^

gt_test: green_threads.o gt_switch.o
	$(CC) $(APPLE_CCFLAGS) -o $@ $^

.S.o:
	as $(APPLE_ASFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f *.o gt_test green_threads
