/* Reference code that searches files for a user-specified string. 
 * 
 * Author: Naga Kandasamy kandasamy
 * Date: 7/19/2015
 *
 * Compile the code as follows: gcc -o mini_grep min_grep.c queue_utils.c -std=c99 -lpthread - Wall
 *
*/

#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include "queue.h"
#include <pthread.h>

// Function Prototypes
int serialSearch(char **);
int parallelSearchStatic(char **);
int parallelSearchDynamic(char **);
void *thread_worker(void *args);

// globals
char inputSearchstring[MAX_LENGTH];

// pthreads struct
struct threadArgsStruct{
	queue_element_t* element;
	queue_t* queue;
	struct dirent* entry;
	char* inputString;	
};

int main(int argc, char** argv){
	if(argc < 5){
		 printf("\n %s <search string> <path> <num threads> static (or) %s <search string> <path> <num threads> dynamic \n", argv[0], argv[0]);
		exit(EXIT_SUCCESS);
	}

	int num_occurrences;

	struct timeval start, stop;											
	
	/* Start the timer. */
	gettimeofday(&start, NULL);

	num_occurrences = serialSearch(argv);		
	
	/* Perform a serial search of the file system. */
	printf("\n The string %s was found %d times within the file system.", argv[1], num_occurrences);

	gettimeofday(&stop, NULL);
	printf("\n Overall execution time = %fs.", (float)(stop.tv_sec - start.tv_sec + (stop.tv_usec - start.tv_usec)/(float)1000000));


	/* Perform a multi-threaded search of the file system. */
	if(strcmp(argv[4], "static") == 0){
		printf("\n Performing multi-threaded search using static load balancing.");
		num_occurrences = parallelSearchStatic(argv);
		printf("\n The string %s was found %d times within the file system.", argv[1], num_occurrences);
	
	} else if(strcmp(argv[4], "dynamic") == 0){
		printf("\n Performing multi-threaded search using dynamic load balancing.");
		num_occurrences = parallelSearchDynamic(argv);
		printf("\n The string %s was found %d times within the file system.", argv[1], num_occurrences);
	
	} else{
		printf("\n Unknown load balancing option provided. Defaulting to static load balancing.");
		num_occurrences = parallelSearchStatic(argv);
		printf("\n The string %s was found %d times within the \file system.", argv[1], num_occurrences);
	
	}
 

	printf("\n");
	exit(EXIT_SUCCESS);
}

/* Serial search of the file system from the specified path name */
int serialSearch(char **argv){
	int num_occurrences = 0;
	
	queue_element_t *element, *new_element;
	struct stat file_stats;
	int status; 
	DIR *directory = NULL;
	struct dirent *result = NULL;
	struct dirent *entry = (struct dirent *)malloc(sizeof(struct dirent) + MAX_LENGTH);

	// set input string within struct
	//threadArgsStruct.inputString = argv[1];

	queue_t *queue = createQueue();
	
	/* Create and initialize the queue data structure. */
	element = (queue_element_t *)malloc(sizeof(queue_element_t));
	
	if(element == NULL){
		perror("malloc");
		exit(EXIT_FAILURE);
	}
		  
	strcpy(element->path_name, argv[2]);
	element->next = NULL;
	/* Insert the inital path name into the queue */
	insertElement(queue, element);

	/* While there is work in the queue, process it */
	while(queue->head != NULL){
		queue_element_t *element = removeElement(queue);
		
		/* Obtain information aobut the file */
		status = lstat(element->path_name, &file_stats);
					 
		if(status == -1){
			printf("Error obtaining stats for %s \n", element->path_name);
			free((void *)element);
			continue;
		}

		/* Ignore symbolic links */
		if(S_ISLNK(file_stats.st_mode)){
		} else if(S_ISDIR(file_stats.st_mode)){ 
			/* If directory, descend in and post work to queue. */
			printf("%s is a directory. \n", element->path_name);
			directory = opendir(element->path_name);
			
			if(directory == NULL){
				printf("Unable to open directory %s \n", element->path_name);
				continue;
			}
								
			while(1){
				status = readdir_r(directory, entry, &result); /* Read directory entry. */
				if(status != 0){
					printf("Unable to read directory %s \n", element->path_name);
					break;
				}
	
				if(result == NULL)				
					/* End of directory. */
					break; 										  
										  										  
				if(strcmp(entry->d_name, ".") == 0)	
					/* Ignore the "." and ".." entries. */
					continue;
				
				if(strcmp(entry->d_name, "..") == 0)
					continue;

				/* Insert this directory entry in the queue. */
				new_element = (queue_element_t *)malloc(sizeof(queue_element_t));
				if(new_element == NULL){
					printf("Error allocating memory. Exiting. \n");
					exit(-1);
				}
				
				/* Construct the full path name for the directory item stored in entry. */
				strcpy(new_element->path_name, element->path_name);
				strcat(new_element->path_name, "/");
				strcat(new_element->path_name, entry->d_name);
				insertElement(queue, new_element);
			}
								
			closedir(directory);
		
		} else if(S_ISREG(file_stats.st_mode)){ 		
			/* Directory entry is a regular file. */
			printf("%s is a regular file. \n", element->path_name);
			FILE *file_to_search;
			char buffer[MAX_LENGTH];
			char *bufptr, *searchptr, *tokenptr;
								
			/* Search the file for the search string provided as the command-line argument. */ 
			file_to_search = fopen(element->path_name, "r");
			if(file_to_search == NULL){
				printf("Unable to open file %s \n", element->path_name);
				continue;
			} else{
				while(1){
					/* Read in a line from the file */
					bufptr = fgets(buffer, sizeof(buffer), file_to_search);
				
					if(bufptr == NULL){
						if(feof(file_to_search)) break;
						if(ferror(file_to_search)){
							printf("Error reading file %s \n", element->path_name);
							break;
						}
					}
													 
					 /* Break up line into tokens and search each token. */ 
					tokenptr = strtok(buffer, " ,.-");
					while(tokenptr != NULL){
						searchptr = strstr(tokenptr, argv[1]);
						if(searchptr != NULL){
							printf("Found string %s within file %s. \n", argv[1], element->path_name);
							num_occurrences++;
						}
								
						/* Get next token from the line */								
						tokenptr = strtok(NULL, " ,.-");
					}
										  }
			}
								
			fclose(file_to_search);
		
		}else{
			printf("%s is of type other. \n", element->path_name);
		}
		
		free((void *)element);
	}

	return num_occurrences;
}


/* Parallel search with static load balancing across threads */
int parallelSearchStatic(char **argv){
	printf("Executing parallel word search using static load balancing...\n");
	int num_occurrences = 0;
	
	// set thread count from user arguments
	threadCount = atoi(argv[3]);
	strcpy(inputSearchString, argv[1]);

	// allocate threads
	pthread_t threads[threadCount];

	// queue initializations
	queue = create_queue();
	
	// verify memory allocation works for queue
	queue_element_t *element;
	element = (queue_element_t *)malloc(sizeof(queue_element_t));
	if(element == NULL){
		printf("ERROR: memory allocation for queue failed.\n");
		printf("Exiting.\n");
		exit(EXIT_FAILURE);
	}

	// user arg as path to the element
	strcpy(element->path_name, argv[2]);
	
	// adding first element to the queue
	firstElement->next = NULL;
	insertElement(queue, element);	
	
	// timing starts
	struct timeval start;
	gettimeofday(&start, null);

	// pthread creation
	int t;
	for(t = 0; t < threadCount; t++){
		pthread_create(&threads[t], NULL, thread_worker,(void *) t);
	}

	// pthread joining
	for(t = 0; t < threadCount; t++){
		pthread_join(threads[t], NULL);
	}

	// timing stops
	struct timeval stop;
	gettimeofday(&stop, NULL);

	// execution time
	prinft("static load balancing execution time = %fs/", (float)(stop.tv_sec - start.tv_sec + (stop.tv_usec - start.tv_usec)/(float)1000000));	

	return num_occurrences;
}

/* Parallel search with dynamic load balancing */
int parallelSearchDynamic(char **argv){
	int num_occurrences = 0;

	return num_occurrences;
}

// verfiying whether the directory as an element is valid for the queue
// this code is adapted from serialSearch() which was created by Michael Lui
// to do: verify needed args and export
int verify_directory(){
	int status;
	struct dirent *result = NULL;
	queue_element_t* new_element;

	printf("%s is a directory %s \n", element->path_name);
	DIR *directory = opendir(element->path_name);

	if(directory == NULL){
		printf("Unable to open directory %s \n", element->path_name);
		return -1;
	}

	// checking if meets critiera from serialSearch
	while(1){
		/* Read directory entry */
		status = readdir_r(directory, entry, &result);
		
		if(status != 0){
			printf("Unable to read directory %s \n", element->path_name);
			break;
		}

		if(result == NULL){
			/* End of directory */
			break;
		}

		/* Ignore the "." and ".." entries */
		if((strcmp(entry->d_name, ".") == 0) || ((strcmp(entry->d_name, "..") == 0))){
			continue;
		}

		/* check for memory allocation before constructing full path name*/
		new_element = (queue_element_t *)malloc(sizeof(queue_element_t));

		if(new_element == NULL){
			printf("malloc failure \n");
			exit(-1);
		}

		/* Construct the full path name for the directory item stored in entry */
		strcpy(new_element->path_name, element->path_name);
		strcpy(new_element->path_name, "/");
		strcat(new_element->path_name, entry->d_name);
	
		// we're good, now add it to the queue
		insertElement(queue, new_element);
	}

	closedir(directory);
	return 0;
}

// verifying whether the file as an element is valid for the queue
// this code is adapted from serialSearch() which was created by Michael Lui
// to do: verify needed args and export
verify_file(){
	int num_occurences = 0;
	/* Directory entry is a regular file */
	printf("%s is a regular file. \n", element->path_name);

	FILE *file_to_search;
	char buffer[MAX_LENGTH];
	char *bufptr, *searchptr, *tokenptr;

	/* Search the file for the search string provided as the command-line argument */
	file_to_search = fopen(element->path_name, "r");
	if(file_to_search == NULL){
		printf("Unable to open file %s \n", element->path_name);
		continue;
	} else {
		while(1){
			/* Read in a line from the file */
			bufptr = fgets(buffer, sizeof(buffer), file_to_search);

			if(bufptr == NULL){
				if(feof(file_to_search))
					break;
				if(ferror(file_to_search)){
					prtinf("Error reading file %s \n", element->path_name);
					break;
				}
			}

			/* Break up line into tokens and search each token. */
			tokenptr = strtok(buffer, " ,.-");
			while(tokenptr != NULL){
				searchptr = strstr(tokenptr, argv[1]);
				if(searchptr != NULL){
					printf("Found string %s within file %s \n", argv[1], element->path_name);
					num_occurrences++;
				}

				/* Get next token from the line */ 
				tokenptr = strtok(NULL, " ,.-");
			}
		}
	}

	fclose(file_to_search);
	return num_occurences;
}

void *thread_worker(void *args){
	int count = 0;
	int thread_id = (int)args;

	queue_element_t *element, *new_element;
	struct stat file_stats;
	int status;
	DIR *directory = NULL;
	struct dirent *result = NULL;
	struct dirent *entry = (struct dirent *)malloc(sizeof(struct dirent) + MAX_LENGTH;

	while(1){
		
	}	
}
