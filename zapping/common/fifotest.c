/*
 *  Fifo test
 *
 *  gcc -g -Wall fifotest.c -ofifotest -lpthread
 *
 */

/* $Id: fifotest.c,v 1.5 2001-08-10 16:31:48 mschimek Exp $ */

#define _GNU_SOURCE 1
#define _REENTRANT 1
#define HAVE_MEMALIGN 1

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "list.h"
#include "fifo.c"

#define SEC 1000000

static fifo test_fifo;
static pthread_t id[10];

static int timestamp = 0;
static int eof_forever = 0;
static int eof_ack = 0;

static void
kill_fifo(void)
{
	destroy_fifo(&test_fifo);
	memset(&test_fifo, 0, sizeof test_fifo);	
}

static void
cancel_join(pthread_t id)
{
	pthread_cancel(id);
	pthread_join(id, NULL);
}

/*
 *  Producing thread
 */
static void *
prod1(void *p)
{
	producer prod;
	int frame = 0, delay = (int) p;
	struct timeval tv;
	double start_time, time;
	buffer *b;
	int i;

	printf("prod1 started, sending 20 messages, creation delay %d ms\n", delay / 1000);

	if (!add_producer(&test_fifo, &prod)) {
		printf("add_producer failed\n");
		return NULL;
	}

	for (i = 0; i < 20; i++) {
		gettimeofday(&tv, NULL);
		start_time = tv.tv_sec * (double) SEC + tv.tv_usec
			- (++frame) * delay;

		b = wait_empty_buffer(&prod);

		gettimeofday(&tv, NULL);
		time = tv.tv_sec * (double) SEC + tv.tv_usec;
		frame = (time - start_time) / delay + 0.5;

		if (timestamp)
			b->used = snprintf(b->data, 255, "Message %d, created at %02ld:%02ld:%02ld.%03ld UTC",
				i + 1, (tv.tv_sec / 3600) % 24, (tv.tv_sec / 60) % 60,
				tv.tv_sec % 60,	tv.tv_usec / (SEC / 1000));
		else
			b->used = snprintf(b->data, 255, "Message %d, frame %d",
				i + 1, frame);

		usleep(delay);

		printf("prod1 sends: <%s>\n", b->data);

		send_full_buffer(&prod, b);
	}

	for (;;) {
		/*
		 *  Canceled & joined here in pthread_cond_wait.
		 *  main() can't/won't cancel before the consumer received EOF,
		 *  so we don't have to explicitely disable/enable cancelation
		 *  in the producing thread.
		 *
		 *  Attention the producer will spinloop without consumers,
		 *  and can spinloop with two producers, therefore
		 *  an endless eof loop is no longer recommended.
		 */
		b = wait_empty_buffer(&prod);

		b->used = 0; /* EOF */

		printf("prod1 sends EOF\n");

		if (!eof_forever) {
			send_full_buffer(&prod, b);

			/*
			 *  This wasn't really planned and may not actually
			 *  be useful, it just happens to work (but not
			 *  with > 1 producer due to spinlooping, be warned).
			 *
			 *  NB fresh buffers (add_buffer, and looped back
			 *  ones) have b->used == -1, EINVAL and consumers
			 *  aren't allowed to change b->used.
			 */
			while (eof_ack) {
				int used;

				printf("prod1 waits for EOF ack...\n");

				b = wait_empty_buffer(&prod);
				used = b->used;
				b->used = 0; /* EOF */
	    			send_full_buffer(&prod, b);

				if (used == 0) {
					printf("prod1: got it\n");
					break;
				} else
					printf("prod1: none\n");
			}

			break;
		}

		send_full_buffer(&prod, b);
	}

	/* We may never reach this, but it doesn't matter */

	printf("prod1 returns\n");

	rem_producer(&prod);

	return NULL;
}

/*
 *  Producing thread 2, delayed
 */
static void *
prod2(void *p)
{
	producer prod;
	int frame = 0, delay = (int) p;
	struct timeval tv;
	double start_time, time;
	buffer *b;
	int i;

	usleep(delay * 3);

	printf("prod2 started, sending 25 messages, creation delay %d ms\n", delay / 1000);

	if (!add_producer(&test_fifo, &prod)) {
		printf("add_producer failed\n");
		return NULL;
	}

	for (i = 0; i < 25; i++) {
		gettimeofday(&tv, NULL);
		start_time = tv.tv_sec * (double) SEC + tv.tv_usec
			- (++frame) * delay;

		b = wait_empty_buffer(&prod);

		gettimeofday(&tv, NULL);
		time = tv.tv_sec * (double) SEC + tv.tv_usec;
		frame = (time - start_time) / delay + 0.5;

		if (timestamp)
			b->used = snprintf(b->data, 255, "Message %db, created at %02ld:%02ld:%02ld.%03ld UTC",
				i + 1, (tv.tv_sec / 3600) % 24, (tv.tv_sec / 60) % 60,
				tv.tv_sec % 60,	tv.tv_usec / (SEC / 1000));
		else
			b->used = snprintf(b->data, 255, "Message %db, frame %d",
				i + 1, frame);

		usleep(delay);

		printf("prod2 sends: <%s>\n", b->data);

		send_full_buffer(&prod, b);
	}

	b = wait_empty_buffer(&prod);

	b->used = 0; /* EOF */

	printf("prod2 sends EOF\n");

	send_full_buffer(&prod, b);

	printf("prod2 returns\n");

	rem_producer(&prod);

	return NULL;
}

/*
 *  Consuming thread / subroutine 1
 */
static void *
cons1(void *p)
{
	consumer cons;
	int delay = (int) p;
	buffer *b;

	printf("cons1 started, processing delay %d ms\n", delay / 1000);

	if (!add_consumer(&test_fifo, &cons)) {
		printf("add_consumer failed\n");
		return NULL;
	}

	for (;;) {
		b = wait_full_buffer(&cons);

		if (!b->used) {
			/* Tidy and acknowledge the EOF. */
			send_empty_buffer(&cons, b);
			break;
		}

		printf("cons1 received: <%s>\n", b->data);

		usleep(delay);

		send_empty_buffer(&cons, b);
	}

	printf("cons1 received EOF, returns\n");

	rem_consumer(&cons);

	return 0;
}

char *delay_pattern;

/*
 *  Consuming thread 1, overlapping wait_full, unget
 *
 *  NB fifos may not support dequeuing and ungetting
 *  more than one buffer at a time, depends on the
 *  fifo depth.
 */
void *
cons1unget(void *p)
{
	consumer cons;
	int delay = (int) p;
	buffer *b1, *b2, *b3, *b4;

	printf("cons1 started\n");

	if (!add_consumer(&test_fifo, &cons)) {
		printf("add_consumer failed\n");
		return NULL;
	}

	for (;;) {
		b1 = wait_full_buffer(&cons);

		if (!b1->used)
			break;

		printf("cons1 received #1: <%s>, delays %d ms\n",
			b1->data, delay * delay_pattern[0] / 2000);

		usleep(delay * delay_pattern[0] / 2);

		b2 = wait_full_buffer(&cons);

		if (!b2->used) {
			printf("cons1 returns #1 because EOF follows\n");
			send_empty_buffer(&cons, b1);
			send_empty_buffer(&cons, b2);
			break;
		}

		printf("cons1 received #2: <%s>, delays %d ms\n",
			b2->data, delay * delay_pattern[1] / 2000);

		usleep(delay * delay_pattern[1] / 2);

		printf("cons1 returns #1 out of order\n");

		send_empty_buffer(&cons, b1);

		b3 = wait_full_buffer(&cons);

		if (!b3->used) {
			printf("cons1 returns #2 because EOF follows\n");
			send_empty_buffer(&cons, b2);
			send_empty_buffer(&cons, b3);
			break;
		}

		printf("cons1 received #3: <%s>, delays %d ms\n",
			b3->data, delay * delay_pattern[2] / 2000);

		usleep(delay * delay_pattern[2] / 2);

		printf("cons1 ungets #3, #2, delays %d ms\n",
			delay * delay_pattern[3] / 2000);

		/* must be in reverse order (restriction for mc-fifos) */

		unget_full_buffer(&cons, b3);
		unget_full_buffer(&cons, b2);

		usleep(delay * delay_pattern[3] / 2);

		b4 = wait_full_buffer(&cons);

		if (!b4->used) {
			printf("cons1 can't re-obtain #2, got EOF (maybe ok)\n");
			send_empty_buffer(&cons, b4);
			break;
		}

		printf("cons1 received #4: <%s>, delays %d ms\n",
			b4->data, delay * delay_pattern[4] / 2000);

		usleep(delay * delay_pattern[4] / 2);

		send_empty_buffer(&cons, b4);

		/* b5 == b1 */
	}

	/*
	 *  This isn't strictly necessary, but we behave and
	 *  it also checks wait/send paired properly.
	 */
	rem_consumer(&cons);

	printf("cons1 received EOF, returns\n");

	return 0;
}

/*
 *  Consuming thread 1/2
 */
void *
cons1of2(void *p)
{
	consumer cons;
	struct timeval tv;
	int delay = (int) p;
	buffer *b;

	printf("cons1/2 started, processing delay %d ms\n", delay / 1000);

	if (!add_consumer(&test_fifo, &cons)) {
		printf("add_consumer failed\n");
		return NULL;
	}

	for (;;) {
		b = wait_full_buffer(&cons);

		gettimeofday(&tv, NULL);

		if (!b->used) {
			send_empty_buffer(&cons, b);
			break;
		}

		if (timestamp)
			printf("cons1/2 received: <%s> at %02ld:%02ld:%02ld.%03ld UTC\n",
				b->data, (tv.tv_sec / 3600) % 24, (tv.tv_sec / 60) % 60,
				tv.tv_sec % 60,	tv.tv_usec / (SEC / 1000));
		else
			printf("cons1/2 received: <%s>\n", b->data);

		usleep(delay);

		send_empty_buffer(&cons, b);
	}

	printf("cons1/2 received EOF, returns\n");

	rem_consumer(&cons);

	return 0;
}

/*
 *  Consuming thread 2/2
 */
void *
cons2of2(void *p)
{
	consumer cons;
	struct timeval tv;
	int delay = (int) p;
	buffer *b;

	printf("cons2/2 started, processing delay %d ms\n", delay / 1000);

	if (!add_consumer(&test_fifo, &cons)) {
		printf("add_consumer failed\n");
		return NULL;
	}

	for (;;) {
		b = wait_full_buffer(&cons);

		gettimeofday(&tv, NULL);

		if (!b->used) {
			send_empty_buffer(&cons, b);
			break;
		}

		if (timestamp)
			printf("cons2/2 received: <%s> at %02ld:%02ld:%02ld.%03ld UTC\n",
				b->data, (tv.tv_sec / 3600) % 24, (tv.tv_sec / 60) % 60,
				tv.tv_sec % 60,	tv.tv_usec / (SEC / 1000));
		else
			printf("cons2/2 received: <%s>\n", b->data);

		usleep(delay);

		send_empty_buffer(&cons, b);
	}

	printf("cons2/2 received EOF, returns\n");

	rem_consumer(&cons);

	return 0;
}

/*
 *  Consuming thread 3/2 (oops...)
 */
void *
cons3of2(void *p)
{
	consumer cons;
	struct timeval tv;
	int delay = (int) p;
	buffer *b;

	printf("cons3/2 started, processing delay %d ms\n", delay / 1000);

	if (!add_consumer(&test_fifo, &cons)) {
		printf("add_consumer failed\n");
		return NULL;
	}

	for (;;) {
		b = wait_full_buffer(&cons);

		gettimeofday(&tv, NULL);

		if (!b->used) {
			send_empty_buffer(&cons, b);
			break;
		}

		if (timestamp)
			printf("cons3/2 received: <%s> at %02ld:%02ld:%02ld.%03ld UTC\n",
				b->data, (tv.tv_sec / 3600) % 24, (tv.tv_sec / 60) % 60,
				tv.tv_sec % 60,	tv.tv_usec / (SEC / 1000));
		else
			printf("cons3/2 received: <%s>\n", b->data);

		usleep(delay);

		send_empty_buffer(&cons, b);
	}

	printf("cons3/2 received EOF, returns\n");

	rem_consumer(&cons);

	return 0;
}

/*
 *  Consuming thread 2/2 with startup delay 
 */
void *
cons2later(void *p)
{
	consumer cons;
	int delay = (int) p;
	buffer *b;

	printf("cons2later started, startup delay %d ms, "
		"processing delay 100 ms\n", delay / 1000);

	usleep(delay);

	printf("cons2/2 creating consumer\n");

	if (!add_consumer(&test_fifo, &cons)) {
		printf("add_consumer failed\n");
		return NULL;
	}

	for (;;) {
		b = wait_full_buffer(&cons);

		if (!b->used) {
			send_empty_buffer(&cons, b);
			break;
		}

		printf("cons2/2 received: <%s>\n", b->data);

		usleep(100 * 1000);

		send_empty_buffer(&cons, b);
	}

	printf("cons2/2 received EOF, returns\n");

	rem_consumer(&cons);

	return 0;
}

/*
 *  Consuming thread 2/2 terminates early
 */
void *
cons2early(void *p)
{
	consumer cons;
	int delay = (int) p;
	buffer *b;
	int i;

	printf("cons2/2 started, processing delay %d ms\n", delay / 1000);

	if (!add_consumer(&test_fifo, &cons)) {
		printf("add_consumer failed\n");
		return NULL;
	}

	for (i = 0; i < 5; i++) {
		b = wait_full_buffer(&cons);

		if (!b->used) {
			send_empty_buffer(&cons, b);
			break;
		}

		printf("cons2/2 received: <%s>\n", b->data);

		usleep(delay);

		send_empty_buffer(&cons, b);
	}

	if (i >= 5)
		printf("cons2/2 terminates early\n");
	else
		printf("cons2/2 received EOF, returns\n");

	rem_consumer(&cons);

	return 0;
}

/*
 *  Consuming thread 2/2, overlapping wait_full, unget, same as above
 *
 *  Tour de force, Luke.
 */
void *
cons2unget(void *p)
{
	consumer cons;
	int delay = (int) p;
	buffer *b1, *b2, *b3, *b4;

	printf("cons2/2 started\n");

	if (!add_consumer(&test_fifo, &cons)) {
		printf("add_consumer failed\n");
		return NULL;
	}

	for (;;) {
		b1 = wait_full_buffer(&cons);

		if (!b1->used) {
			send_empty_buffer(&cons, b1);
			break;
		}

		printf("cons2/2 received #1: <%s>, delays %d ms\n",
			b1->data, delay * delay_pattern[0] / 2000);

		usleep(delay * delay_pattern[0] / 2);

		b2 = wait_full_buffer(&cons);

		if (!b2->used) {
			printf("cons2/2 returns #1 because EOF follows\n");
			send_empty_buffer(&cons, b1);
			send_empty_buffer(&cons, b2);
			break;
		}

		printf("cons2/2 received #2: <%s>, delays %d ms\n",
			b2->data, delay * delay_pattern[1] / 2000);

		usleep(delay * delay_pattern[1] / 2);

		printf("cons2/2 returns #1 out of order\n");

		send_empty_buffer(&cons, b1);

		b3 = wait_full_buffer(&cons);

		if (!b3->used) {
			printf("cons2/2 returns #2 because EOF follows\n");
			send_empty_buffer(&cons, b2);
			send_empty_buffer(&cons, b3);
			break;
		}

		printf("cons2/2 received #3: <%s>, delays %d ms\n",
			b3->data, delay * delay_pattern[2] / 2000);

		usleep(delay * delay_pattern[2] / 2);

		printf("cons2/2 ungets #3, #2, delays %d ms\n",
			delay * delay_pattern[3] / 2000);

		/* must be in reverse order (restriction for mc-fifos) */

		unget_full_buffer(&cons, b3);
		unget_full_buffer(&cons, b2);

		usleep(delay * delay_pattern[3] / 2);

		b4 = wait_full_buffer(&cons);

		if (!b4->used) {
			printf("cons2/2 can't re-obtain #2, got EOF (maybe ok)\n");
			send_empty_buffer(&cons, b4);
			break;
		}

		printf("cons2/2 received #4: <%s>, delays %d ms\n",
			b4->data, delay * delay_pattern[4] / 2000);

		usleep(delay * delay_pattern[4] / 2);

		send_empty_buffer(&cons, b4);

		/* b5 == b1 */
	}

	printf("cons2/2 received EOF, returns\n");

	rem_consumer(&cons);

	return 0;
}

int
main(int ac, char **av)
{
#if 0
#endif
	printf("test: two threads (mp1e style)\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	cons1((void *)(SEC / 10));
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	kill_fifo();

	printf("test: supervising a producer and consumer thread,\n"
	       "endless eof, will cancel the producer\n");

	eof_forever = 1;
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	kill_fifo();
	eof_forever = 0;

	printf("test: supervising a producer and consumer thread\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	/*
	 *  Here the order is irrelevant because we don't cancel,
	 *  the threads exit when done.
	 */
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();

	printf("test: producer waits for EOF ack\n");

	eof_ack = 1;
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();
	eof_ack = 0;

	printf("test: consumer overlaps wait/send"
		" and ungets two buffers in a row, delay 2-2-0-0-2\n");

	delay_pattern = "\2\2\0\0\2";
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1unget, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();

	printf("test: consumer overlaps wait/send"
		" and ungets two buffers in a row, delay 1-1-1-4-1\n");

	delay_pattern = "\1\1\1\4\1";
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1unget, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();

	printf("test: slow producer\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 2));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();

	printf("test: slow consumer (prod1 shall drop frames, "
		"reducing the cpu load)\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1, (void *)(SEC / 2));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();

	printf("test: two consumers\n");

	timestamp = 1;
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons2of2, (void *)(SEC / 10));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons2/2\n");
	pthread_join(id[2], NULL); /* when finished */
	printf("cons2/2 joined, waiting for cons1/2\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1/2 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();
	timestamp = 0;

	printf("test: two unbalanced consumers "
	    "(cons2/2 shall drop frames, not prod1)\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons2of2, (void *)(SEC / 2));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons2/2\n");
	pthread_join(id[2], NULL); /* when finished */
	printf("cons2/2 joined, waiting for cons1/2\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1/2 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();

	printf("test: two consumers, cons2/2 joins in later "
	    "(cons2/2 shall not receive old messages, "
	    "prod1 and cons1/2 shall not be interrupted)\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons2later, (void *)(5 * SEC / 10));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons2/2\n");
	pthread_join(id[2], NULL); /* when finished */
	printf("cons2/2 joined, waiting for cons1/2\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1/2 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();

	printf("test: two consumers, cons2/2 leaves early "
	   "(prod1 and cons1/2 shall not be interrupted)\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons2early, (void *)(SEC / 10));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons2/2\n");
	pthread_join(id[2], NULL); /* when finished */
	printf("cons2/2 joined, waiting for cons1/2\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1/2 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();

	printf("test: two consumers, cons2/2 ungets, delay 2-0-2-1-1 "
	   "(prod1 and cons1/2 should be interrupted as little as possible)\n");

	delay_pattern = "\2\0\2\1\1";
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons2unget, (void *)(SEC / 10));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons2/2\n");
	pthread_join(id[2], NULL); /* when finished */
	printf("cons2/2 joined, waiting for cons1/2\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1/2 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();

	printf("test: three consumers\n");

	timestamp = 1;
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[3], NULL, cons3of2, (void *)(SEC / 10));
	pthread_create(&id[2], NULL, cons2of2, (void *)(SEC / 10));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons3/2\n");
	pthread_join(id[3], NULL); /* when finished */
	printf("cons3/2 joined, waiting for cons2/2\n");
	pthread_join(id[2], NULL); /* when finished */
	printf("cons2/2 joined, waiting for cons1/2\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1/2 joined, waiting for prod1\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod1 joined, test finished\n\n");
	kill_fifo();
	timestamp = 0;

	printf("test: two producers, fair share of empty buffers and "
	       "supressed eof\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons1, (void *)(SEC / 10));
	pthread_create(&id[1], NULL, prod1, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod2, (void *)(SEC / 10));
	usleep(15 * SEC / 10);
	printf("main thread adds cons2\n");
	pthread_create(&id[3], NULL, cons2of2, (void *)(SEC / 10));
	printf("main thread waiting for cons2\n");
	pthread_join(id[3], NULL); /* when finished */
	printf("cons2 joined, waiting for prod1\n");
	pthread_join(id[2], NULL); /* when finished */
	printf("cons1 joined, waiting for prod1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("prod1 joined, waiting for prod2\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod2 joined, attempt to add consumer (should fail after eof)\n");
	pthread_create(&id[2], NULL, cons1of2, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[2], NULL); /* when finished */
	printf("cons1 joined, test finished\n\n");
	kill_fifo();

	printf("test: two unbalanced producers, fair share of empty "
	       "buffers and supressed eof\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons1, (void *)(SEC / 100));
	pthread_create(&id[1], NULL, prod1, (void *)(SEC / 20));
	pthread_create(&id[0], NULL, prod2, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[2], NULL); /* when finished */
	printf("cons1 joined, waiting for prod1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("prod1 joined, waiting for prod2\n");
	pthread_join(id[0], NULL); /* when finished */
	printf("prod2 joined, test finished\n\n");
	kill_fifo();

	/* more */

	exit(EXIT_SUCCESS);
}
