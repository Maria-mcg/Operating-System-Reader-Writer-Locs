#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include "readerwriter.h"

int main(int argc, char* argv[]) {

    char file[]= "scenarios.txt";
    FILE *fp = fopen(file, "r");
    if (fp == NULL)
    {
        fprintf(stderr,"Failed to open scenario-file \"scenarios.txt\". [[File %s at line %d]]\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    size_t line_length= 255;
    char line[line_length];
    long long line_counter=0;
    while (fgets(line, line_length, fp) != NULL) {

        ++line_counter;
        printf("Reading line %lld: %s...\n", line_counter, line);

        rwlock_t rw;
        rwlock_init_rwstr(&rw, line, line_length);

        if(rw.initial_writers == 0 || rw.initial_readers == 0) {
            printf("Line %lld: %s must have a positive number of readers and writers!\n", line_counter, line);
            continue;
        }

        srand(time(NULL));

        thread_info readers[rw.initial_readers];
        thread_info writers[rw.initial_writers];
        long long shared_resource_value= 0;
        long long expected_result= rw.initial_writers;


        /*for(size_t i=0; i < rw.initial_readers; ++i) {
            readers[i].rw= &rw;
            readers[i].shared_resource= &shared_resource_value;
            pthread_create(&readers[i].thread_id, 0, rwlock_run_reader, &readers[i]);
        }

        for(size_t i=0; i < rw.initial_writers; ++i) {
            writers[i].rw= &rw;
            writers[i].shared_resource= &shared_resource_value;
            pthread_create(&writers[i].thread_id, 0, rwlock_run_writer , &writers[i]);
        }*/

        size_t reader_count=0;
        size_t writer_count=0;
        for(size_t i=0; i < line_length; ++i) {
            if(line[i] == '\0')
                break;

            if(line[i] == 'r' || line[i] == 'R') {
                readers[reader_count].rw= &rw;
                readers[reader_count].shared_resource= &shared_resource_value;
                pthread_create(&readers[reader_count].thread_id, 0, rwlock_run_reader, &readers[reader_count]);
                printf("[reader] thread: %u\tSpawning...\n", readers[reader_count].thread_id);
                ++reader_count;
            }

            if(line[i] == 'w' || line[i] == 'W') {
                writers[writer_count].rw= &rw;
                writers[writer_count].shared_resource= &shared_resource_value;
                pthread_create(&writers[writer_count].thread_id, 0, rwlock_run_writer , &writers[writer_count]);
                printf("[writer] thread: %u\tSpawning...\n", writers[writer_count].thread_id);
                ++writer_count;
            }
        }

        reader_count=0;
        writer_count=0;
        for(size_t i=0; i < line_length; ++i) {
            if(line[i] == '\0')
                break;

            if(line[i] == 'r' || line[i] == 'R') {
                pthread_join(readers[reader_count].thread_id, NULL);
                ++reader_count;
            }

            if(line[i] == 'w' || line[i] == 'W') {
                pthread_join(writers[writer_count].thread_id, NULL);
                ++writer_count;
            }
        }

        rwlock_destroy(&rw);

        printf("\n==================================RESULT=====================================\n");
        printf("\tshared-resource final value: %lld, expected: %lld\n", shared_resource_value, expected_result);
    }
    fclose(fp);
    return EXIT_SUCCESS;
}
