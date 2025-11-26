//
// Example from: http://www.amparo.net/ce155/sem-ex.c
//
// Adapted using some code from Downey's book on semaphores
//
// Compilation:
//
//      g++ main.cpp -lpthread -o main -lm
// or 
//      make
//

#include <unistd.h>     /* Symbolic Constants */
#include <sys/types.h>  /* Primitive System Data Types */
#include <errno.h>      /* Errors */
#include <stdio.h>      /* Input/Output */
#include <stdlib.h>     /* General Utilities */
#include <pthread.h>    /* POSIX Threads */
#include <string.h>     /* String handling */
#include <semaphore.h>  /* Semaphore */
#include <iostream>
using namespace std;

class Semaphore {
public:
    Semaphore(int initVal) {
        sem_init(&sem, 0, initVal);
    }

    ~Semaphore() {
        sem_destroy(&sem);
    }

    void wait() {
        sem_wait(&sem);
    }

    void signal() {
        sem_post(&sem);
    }

private:
    sem_t sem;
};

// Function declarations for each problem
void run_readers_writers_nostarve();
void run_rw_writer_priority();
void run_dining_phil_asym();
void run_dining_phil_host();

// Entry point for selecting a problem to run
int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <problem number>\n", argv[0]);
        printf("  1 = No-starve RW\n");
        printf("  2 = Writer-priority RW\n");
        printf("  3 = Philosophers (asymmetric)\n");
        printf("  4 = Philosophers (footman)\n");
        return 1;
    }

    int choice = atoi(argv[1]);

    // Call the selected problem's runner
    switch (choice) {
        case 1: run_readers_writers_nostarve(); break;
        case 2: run_rw_writer_priority(); break;
        case 3: run_dining_phil_asym(); break;
        case 4: run_dining_phil_host(); break;
        default:
            puts("Invalid problem #. Must be 1-4.");
            return 1;
    }

    pthread_exit(NULL);
}

// --------------------- Problem 1 ---------------------

int readers1 = 0;
Semaphore r_mutex1(1);   // protects readers1 count
Semaphore resource1(1);  // resource lock

void* reader1(void* arg)
{
    int reader_id = (int)(long)arg;

    for (int round = 0; round < 4; ++round) {
        r_mutex1.wait();
        readers1++;
        if (readers1 == 1) {
            resource1.wait();  // first reader locks resource
        }
        r_mutex1.signal();

        printf("Reader %d reading\n", reader_id);
        sleep(1);  // simulate reading

        r_mutex1.wait();
        readers1--;
        if (readers1 == 0) {
            resource1.signal();  // last reader unlocks
        }
        r_mutex1.signal();

        sleep(1);  // simulate thinking
    }

    pthread_exit(NULL);
}

void* writer1(void* arg)
{
    int writer_id = (int)(long)arg;

    for (int round = 0; round < 4; ++round) {
        resource1.wait();  // exclusive access

        printf("Writer %d writing\n", writer_id);
        sleep(2);  // simulate writing

        resource1.signal();
        sleep(1);  // rest
    }

    pthread_exit(NULL);
}

void run_readers_writers_nostarve()
{
    const int NUM_R = 5;
    const int NUM_W = 5;

    pthread_t r_threads[NUM_R], w_threads[NUM_W];

    // Start reader threads
    for (long i = 0; i < NUM_R; ++i) {
        pthread_create(&r_threads[i], NULL, reader1, (void*)(i + 1));
    }
    // Start writer threads
    for (long i = 0; i < NUM_W; ++i) {
        pthread_create(&w_threads[i], NULL, writer1, (void*)(i + 1));
    }

    // Wait for all threads to finish
    for (int i = 0; i < NUM_R; ++i) pthread_join(r_threads[i], NULL);
    for (int i = 0; i < NUM_W; ++i) pthread_join(w_threads[i], NULL);
}

// --------------------- Problem 2 ---------------------

int readers2 = 0, writers2 = 0;
Semaphore r_mutex2(1), w_mutex2(1);
Semaphore try_read(1);      // blocks readers if writers waiting
Semaphore resource2(1);     // controls resource access

void* reader2(void* arg)
{
    int rid = (int)(long)arg;

    for (int round = 0; round < 4; ++round) {
        try_read.wait();
        r_mutex2.wait();

        readers2++;
        if (readers2 == 1) resource2.wait();  // first reader

        r_mutex2.signal();
        try_read.signal();

        printf("Reader %d reading\n", rid);
        sleep(1);

        r_mutex2.wait();
        readers2--;
        if (readers2 == 0) resource2.signal();
        r_mutex2.signal();

        sleep(1);
    }

    pthread_exit(NULL);
}

void* writer2(void* arg)
{
    int wid = (int)(long)arg;

    for (int turn = 0; turn < 4; ++turn) {
        w_mutex2.wait();
        writers2++;
        if (writers2 == 1) try_read.wait();  // first writer blocks readers
        w_mutex2.signal();

        resource2.wait();

        printf("Writer %d writing\n", wid);
        sleep(2);

        resource2.signal();

        w_mutex2.wait();
        writers2--;
        if (writers2 == 0) try_read.signal();  // last writer unblocks readers
        w_mutex2.signal();

        sleep(1);
    }

    pthread_exit(NULL);
}

void run_rw_writer_priority()
{
    const int NREAD = 5, NWRITE = 5;
    pthread_t readers[NREAD], writers[NWRITE];

    for (long i = 0; i < NREAD; ++i)
        pthread_create(&readers[i], NULL, reader2, (void*)(i + 1));
    for (long i = 0; i < NWRITE; ++i)
        pthread_create(&writers[i], NULL, writer2, (void*)(i + 1));

    for (int i = 0; i < NREAD; ++i) pthread_join(readers[i], NULL);
    for (int i = 0; i < NWRITE; ++i) pthread_join(writers[i], NULL);
}

// --------------------- Problem 3 ---------------------

const int NUM_PHIL = 5;
Semaphore sticks1[NUM_PHIL] = {
    Semaphore(1), Semaphore(1), Semaphore(1), Semaphore(1), Semaphore(1)
};

void* philosopher1(void* arg)
{
    int id = (int)(long)arg;
    int left = id;
    int right = (id + 1) % NUM_PHIL;

    for (int i = 0; i < 4; ++i) {
        printf("Philosopher %d is thinking\n", id);
        sleep(1);

        // Asymmetric solution: 0 picks right first
        if (id == 0) {
            sticks1[right].wait();
            sticks1[left].wait();
        } else {
            sticks1[left].wait();
            sticks1[right].wait();
        }

        printf("Philosopher %d is eating\n", id);
        sleep(2);

        sticks1[left].signal();
        sticks1[right].signal();
        printf("Philosopher %d released forks\n", id);
    }

    pthread_exit(NULL);
}

void run_dining_phil_asym()
{
    pthread_t threads[NUM_PHIL];
    for (long i = 0; i < NUM_PHIL; ++i)
        pthread_create(&threads[i], NULL, philosopher1, (void*)i);

    for (int i = 0; i < NUM_PHIL; ++i)
        pthread_join(threads[i], NULL);
}

// --------------------- Problem 4 ---------------------

Semaphore sticks2[NUM_PHIL] = {
    Semaphore(1), Semaphore(1), Semaphore(1), Semaphore(1), Semaphore(1)
};
Semaphore host(4); // footman: allows only 4 philosophers to eat

void* philosopher2(void* arg)
{
    int id = (int)(long)arg;
    int left = id;
    int right = (id + 1) % NUM_PHIL;

    for (int i = 0; i < 4; ++i) {
        printf("Philosopher %d is thinking\n", id);
        sleep(1);

        host.wait();  // ask footman for permission
        sticks2[left].wait();
        sticks2[right].wait();

        printf("Philosopher %d is eating\n", id);
        sleep(2);

        sticks2[left].signal();
        sticks2[right].signal();
        host.signal();  // leave table

        printf("Philosopher %d put down forks\n", id);
    }

    pthread_exit(NULL);
}

void run_dining_phil_host()
{
    pthread_t philosophers[NUM_PHIL];
    for (long i = 0; i < NUM_PHIL; ++i)
        pthread_create(&philosophers[i], NULL, philosopher2, (void*)i);

    for (int i = 0; i < NUM_PHIL; ++i)
        pthread_join(philosophers[i], NULL);
}
