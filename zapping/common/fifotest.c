/*
 *  Fifo test
 *
 *  gcc -O0 -g fifotest.c -ofifotest -lpthread
 *
 */

/* $Id: fifotest.c,v 1.1 2001-05-29 08:10:51 mschimek Exp $ */

#define _GNU_SOURCE 1
#undef HAVE_MEMALIGN

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "fifo.c"
#include "alloc.c"

#define SEC 1000000

fifo test_fifo;
pthread_t id[10];
coninfo *ci[10];

void
unfifo(void)
{
//	uninit_fifo(&test_fifo); /* XXX hangs */

	memset(&test_fifo, 0, sizeof test_fifo);	
}

void
cancel_join(pthread_t id)
{
	pthread_cancel(id);
	pthread_join(id, NULL);
}

int timestamp = 0;

/*
 *  Producing thread
 */
void *
prod1(void *p)
{
	int frame = 0, delay = (int) p;
	struct timeval tv;
	double start_time, time;
	buffer *b;
	int i;

	printf("prod1 started, sending 10 messages, creation delay %d ms\n", delay / 1000);

	for (i = 0; i < 10; i++) {
		gettimeofday(&tv, NULL);
		start_time = tv.tv_sec * (double) SEC + tv.tv_usec
			- (++frame) * delay;

		b = wait_empty_buffer(&test_fifo);

		gettimeofday(&tv, NULL);
		time = tv.tv_sec * (double) SEC + tv.tv_usec;
		frame = (time - start_time) / delay + 0.5;

		if (timestamp)
			b->used = snprintf(b->data, 255, "Message %d, created at %02d:%02d:%02d.%03d UTC",
				i + 1, (tv.tv_sec / 3600) % 24, (tv.tv_sec / 60) % 60,
				tv.tv_sec % 60,	tv.tv_usec / (SEC / 1000));
		else
			b->used = snprintf(b->data, 255, "Message %d, frame %d",
				i + 1, frame);

		usleep(delay);

		printf("prod1 sends: <%s>\n", b->data);

		send_full_buffer(&test_fifo, b);
	}

	for (;;) {
		/*
		 *  Canceled & joined here in pthread_cond_wait.
		 *  main() can't/won't cancel before the consumer received EOF,
		 *  so we don't have to explicitely disable/enable cancelation
		 *  in the producing thread.
		 */
		b = wait_empty_buffer(&test_fifo);

		b->used = 0; /* EOF */

		printf("prod1 sends EOF\n");

		send_full_buffer(&test_fifo, b);
	}

	return NULL;
}

/*
 *  Consuming thread / subroutine 1
 */
void *
cons1(void *p)
{
	int delay = (int) p;
	buffer *b;

	printf("cons1 started, processing delay %d ms\n", delay / 1000);

	for (;;) {
		b = wait_full_buffer(&test_fifo);

		if (!b->used)
			break;

		printf("cons1 received: <%s>\n", b->data);

		usleep(delay);

		send_empty_buffer(&test_fifo, b);
	}

	printf("cons1 received EOF, returns\n");

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
	int delay = (int) p;
	buffer *b1, *b2, *b3, *b4;

	printf("cons1 started\n");

	for (;;) {
		b1 = wait_full_buffer(&test_fifo);

		if (!b1->used)
			break;

		printf("cons1 received #1: <%s>, delays %d ms\n",
			b1->data, delay * delay_pattern[0] / 2000);

		usleep(delay * delay_pattern[0] / 2);

		b2 = wait_full_buffer(&test_fifo);

		if (!b2->used) {
			printf("cons1 returns #1 because EOF follows\n", b1->data);
			send_empty_buffer(&test_fifo, b1);
			break;
		}

		printf("cons1 received #2: <%s>, delays %d ms\n",
			b2->data, delay * delay_pattern[1] / 2000);

		usleep(delay * delay_pattern[1] / 2);

		printf("cons1 returns #1 out of order\n");

		send_empty_buffer(&test_fifo, b1);

		b3 = wait_full_buffer(&test_fifo);

		if (!b3->used) {
			printf("cons1 returns #2 because EOF follows\n", b1->data);
			send_empty_buffer(&test_fifo, b2);
			break;
		}

		printf("cons1 received #3: <%s>, delays %d ms\n",
			b3->data, delay * delay_pattern[2] / 2000);

		usleep(delay * delay_pattern[2] / 2);

		printf("cons1 ungets #3, #2, delays %d ms\n",
			delay * delay_pattern[3] / 2000);

		/* must be in reverse order (restriction for mc-fifos) */

		unget_full_buffer(&test_fifo, b3);
		unget_full_buffer(&test_fifo, b2);

		usleep(delay * delay_pattern[3] / 2);

		b4 = wait_full_buffer(&test_fifo);

		if (!b4->used) {
			printf("cons1 can't re-obtain #2, got EOF (maybe ok)\n");
			break;
		}

		printf("cons1 received #4: <%s>, delays %d ms\n",
			b4->data, delay * delay_pattern[4] / 2000);

		usleep(delay * delay_pattern[4] / 2);

		send_empty_buffer(&test_fifo, b4);

		/* b5 == b1 */
	}

	printf("cons1 received EOF, returns\n");

	return 0;
}

/*
 *  Consuming thread 1/2
 */
void *
cons1of2(void *p)
{
	struct timeval tv;
	int delay = (int) p;
	buffer *b;

	printf("cons1/2 started, processing delay %d ms\n", delay / 1000);

	ci[0] = create_consumer(&test_fifo);

	for (;;) {
		b = wait_full_buffer(&test_fifo);

		gettimeofday(&tv, NULL);

		if (!b->used)
			break;

		if (timestamp)
			printf("cons1/2 received: <%s> at %02d:%02d:%02d.%03d UTC\n",
				b->data, (tv.tv_sec / 3600) % 24, (tv.tv_sec / 60) % 60,
				tv.tv_sec % 60,	tv.tv_usec / (SEC / 1000));
		else
			printf("cons1/2 received: <%s>\n", b->data);

		usleep(delay);

		send_empty_buffer(&test_fifo, b);
	}

	printf("cons1/2 received EOF, returns\n");

	return 0;
}

/*
 *  Consuming thread 2/2
 */
void *
cons2of2(void *p)
{
	struct timeval tv;
	int delay = (int) p;
	buffer *b;

	printf("cons2/2 started, processing delay %d ms\n", delay / 1000);

	ci[1] = create_consumer(&test_fifo);

	for (;;) {
		b = wait_full_buffer(&test_fifo);

		gettimeofday(&tv, NULL);

		if (!b->used)
			break;

		if (timestamp)
			printf("cons2/2 received: <%s> at %02d:%02d:%02d.%03d UTC\n",
				b->data, (tv.tv_sec / 3600) % 24, (tv.tv_sec / 60) % 60,
				tv.tv_sec % 60,	tv.tv_usec / (SEC / 1000));
		else
			printf("cons2/2 received: <%s>\n", b->data);

		usleep(delay);

		send_empty_buffer(&test_fifo, b);
	}

	printf("cons2/2 received EOF, returns\n");

	return 0;
}

/*
 *  Consuming thread 3/2 (oops...)
 */
void *
cons3of2(void *p)
{
	struct timeval tv;
	int delay = (int) p;
	buffer *b;

	printf("cons3/2 started, processing delay %d ms\n", delay / 1000);

	ci[0] = create_consumer(&test_fifo);

	for (;;) {
		b = wait_full_buffer(&test_fifo);

		gettimeofday(&tv, NULL);

		if (!b->used)
			break;

		if (timestamp)
			printf("cons3/2 received: <%s> at %02d:%02d:%02d.%03d UTC\n",
				b->data, (tv.tv_sec / 3600) % 24, (tv.tv_sec / 60) % 60,
				tv.tv_sec % 60,	tv.tv_usec / (SEC / 1000));
		else
			printf("cons3/2 received: <%s>\n", b->data);

		usleep(delay);

		send_empty_buffer(&test_fifo, b);
	}

	printf("cons3/2 received EOF, returns\n");

	return 0;
}

/*
 *  Consuming thread 2/2 with startup delay 
 */
void *
cons2later(void *p)
{
	int delay = (int) p;
	buffer *b;

	printf("cons2later started, startup delay %d ms, "
		"processing delay 100 ms\n", delay / 1000);

	usleep(delay);

	printf("cons2/2 creating consumer\n");

	ci[1] = create_consumer(&test_fifo);

	for (;;) {
		b = wait_full_buffer(&test_fifo);

		if (!b->used)
			break;

		printf("cons2/2 received: <%s>\n", b->data);

		usleep(100 * 1000);

		send_empty_buffer(&test_fifo, b);
	}

	printf("cons2/2 received EOF, returns\n");

	return 0;
}

/*
 *  Consuming thread 2/2 terminates early
 */
void *
cons2early(void *p)
{
	int delay = (int) p;
	buffer *b;
	int i;

	printf("cons2/2 started, processing delay %d ms\n", delay / 1000);

	ci[1] = create_consumer(&test_fifo);

	for (i = 0; i < 5; i++) {
		b = wait_full_buffer(&test_fifo);

		if (!b->used)
			break;

		printf("cons2/2 received: <%s>\n", b->data);

		usleep(delay);

		send_empty_buffer(&test_fifo, b);
	}

	if (i >= 5)
		printf("cons2/2 terminates early\n");
	else
		printf("cons2/2 received EOF, returns\n");

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
	int delay = (int) p;
	buffer *b1, *b2, *b3, *b4;

	printf("cons2/2 started\n");

	ci[1] = create_consumer(&test_fifo);

	for (;;) {
		b1 = wait_full_buffer(&test_fifo);

		if (!b1->used)
			break;

		printf("cons2/2 received #1: <%s>, delays %d ms\n",
			b1->data, delay * delay_pattern[0] / 2000);

		usleep(delay * delay_pattern[0] / 2);

		b2 = wait_full_buffer(&test_fifo);

		if (!b2->used) {
			printf("cons2/2 returns #1 because EOF follows\n", b1->data);
			send_empty_buffer(&test_fifo, b1);
			break;
		}

		printf("cons2/2 received #2: <%s>, delays %d ms\n",
			b2->data, delay * delay_pattern[1] / 2000);

		usleep(delay * delay_pattern[1] / 2);

		printf("cons2/2 returns #1 out of order\n");

		send_empty_buffer(&test_fifo, b1);

		b3 = wait_full_buffer(&test_fifo);

		if (!b3->used) {
			printf("cons2/2 returns #2 because EOF follows\n", b1->data);
			send_empty_buffer(&test_fifo, b2);
			break;
		}

		printf("cons2/2 received #3: <%s>, delays %d ms\n",
			b3->data, delay * delay_pattern[2] / 2000);

		usleep(delay * delay_pattern[2] / 2);

		printf("cons2/2 ungets #3, #2, delays %d ms\n",
			delay * delay_pattern[3] / 2000);

		/* must be in reverse order (restriction for mc-fifos) */

		unget_full_buffer(&test_fifo, b3);
		unget_full_buffer(&test_fifo, b2);

		usleep(delay * delay_pattern[3] / 2);

		b4 = wait_full_buffer(&test_fifo);

		if (!b4->used) {
			printf("cons2/2 can't re-obtain #2, got EOF (maybe ok)\n");
			break;
		}

		printf("cons2/2 received #4: <%s>, delays %d ms\n",
			b4->data, delay * delay_pattern[4] / 2000);

		usleep(delay * delay_pattern[4] / 2);

		send_empty_buffer(&test_fifo, b4);

		/* b5 == b1 */
	}

	printf("cons2/2 received EOF, returns\n");

	return 0;
}


int
main(int ac, char **av)
{
	int i;

	printf("test: two threads (mp1e style)\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	cons1((void *)(SEC / 10));
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();

	printf("test: supervising a producer and consumer thread\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();

	printf("test: consumer overlaps wait/send"
		" and ungets two buffers in a row, delay 2-2-0-0-2\n", i);

	delay_pattern = "\2\2\0\0\2";
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1unget, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();

	printf("test: consumer overlaps wait/send"
		" and ungets two buffers in a row, delay 1-1-1-4-1\n", i);

	delay_pattern = "\1\1\1\4\1";
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1unget, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();

	printf("test: slow producer\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 2));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();

	printf("test: slow consumer (prod1 shall drop frames, "
		"reducing the cpu load)\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[1], NULL, cons1, (void *)(SEC / 2));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1\n");
	pthread_join(id[1], NULL); /* when finished */
	printf("cons1 joined\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();

	printf("test: two consumers\n");

	timestamp = 1;
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons2of2, (void *)(SEC / 10));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1/2, cons2/2\n");
/* XXX hangs */
//	pthread_join(id[2], NULL); /* when finished */
//	printf("cons2/2 joined\n");
//	pthread_join(id[1], NULL); /* when finished */
//	printf("cons1/2 joined\n");
usleep(SEC * 2);
printf("main thread aborts waiting\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();
	timestamp = 0;

	printf("test: two unbalanced consumers "
	    "(cons2/2 shall drop frames, not prod1)\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons2of2, (void *)(SEC * 1));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1/2, cons2/2\n");
/* XXX hangs */
//	pthread_join(id[2], NULL); /* when finished */
//	printf("cons2/2 joined\n");
//	pthread_join(id[1], NULL); /* when finished */
//	printf("cons1/2 joined\n");
usleep(SEC * 12);
printf("main thread aborts waiting\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();

	printf("test: two consumers, cons2/2 joins in later "
	    "(cons2/2 shall not receive old messages, "
	    "prod1 and cons1/2 shall not be interrupted)\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons2later, (void *)(5 * SEC / 10));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1/2, cons2/2\n");
/* XXX hangs */
//	pthread_join(id[2], NULL); /* when finished */
//	printf("cons2/2 joined\n");
//	pthread_join(id[1], NULL); /* when finished */
//	printf("cons1/2 joined\n");
usleep(SEC * 2);
printf("main thread aborts waiting\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();

	printf("test: two consumers, cons2/2 leaves early "
	   "(prod1 and cons1/2 shall not be interrupted)\n");

	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons2early, (void *)(SEC / 10));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1/2, cons2/2\n");
/* XXX hangs */
//	pthread_join(id[2], NULL); /* when finished */
//	printf("cons2/2 joined\n");
//	pthread_join(id[1], NULL); /* when finished */
//	printf("cons1/2 joined\n");
usleep(SEC * 2);
printf("main thread aborts waiting\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();

	printf("test: two consumers, cons2/2 ungets, delay 2-0-2-1-1 "
	   "(prod1 and cons1/2 should be interrupted as little as possible)\n");

	delay_pattern = "\2\0\2\1\1";
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons2unget, (void *)(SEC / 10));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1/2, cons2/2\n");
/* XXX hangs */
//	pthread_join(id[2], NULL); /* when finished */
//	printf("cons2/2 joined\n");
//	pthread_join(id[1], NULL); /* when finished */
//	printf("cons1/2 joined\n");
usleep(SEC * 2);
printf("main thread aborts waiting\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();

	printf("test: three consumers\n");

	timestamp = 1;
	init_buffered_fifo(&test_fifo, "test", 5, 256);
	pthread_create(&id[2], NULL, cons3of2, (void *)(SEC / 10));
	pthread_create(&id[2], NULL, cons2of2, (void *)(SEC / 10));
	pthread_create(&id[1], NULL, cons1of2, (void *)(SEC / 10));
	pthread_create(&id[0], NULL, prod1, (void *)(SEC / 10));
	printf("main thread waiting for cons1/2, cons2/2\n");
/* XXX hangs */
//	pthread_join(id[3], NULL); /* when finished */
//	printf("cons3/2 joined\n");
//	pthread_join(id[2], NULL); /* when finished */
//	printf("cons2/2 joined\n");
//	pthread_join(id[1], NULL); /* when finished */
//	printf("cons1/2 joined\n");
usleep(SEC * 2);
printf("main thread aborts waiting\n");
	cancel_join(id[0]);
	printf("prod1 joined, test finished\n\n");
	unfifo();
	timestamp = 0;

	exit(EXIT_SUCCESS);
}
