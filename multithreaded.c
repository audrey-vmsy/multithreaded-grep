#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>

int NTHREADS; // number of threads to be created
int check = 1; // number of available tasks
int waiting = 0; // number of threads waiting for a task
char search_string[150]; // string to be searched with grep
pthread_mutex_t lock; // initialization of lock
pthread_cond_t cond; // initialization of condition variable

// Serves as a node in the pointer array that queues the tasks
struct task {
	char curpath[250]; // Takes note of the path to be traversed once the task is chosen
	struct task *next;
};
// Facilitates the queueing of the pointer array
struct queue {
	struct task *head; // Takes note of the task in front of the queue
	struct task *tail; // Takes note of the task at the back of the queue
}*q;
// Enqueues new paths as tasks
void enqueue(char p[250]) {
	struct task *t = malloc(sizeof(struct task));
	strcpy(t->curpath, p);
	t->next = NULL;
	if (q->head == NULL && q->tail == NULL) {
		q->head = t;
		q->tail = t;
	} else {
		q->tail->next = t;
		q->tail = t;
	}
}
// Dequeues path to be traversed
char *dequeue() {
	if (q->head != NULL) {
		struct task *t = malloc(sizeof(struct task));
		t = q->head;
		if (q->head == q->tail) q->tail = NULL;
		q->head = q->head->next;
		char *p = malloc(sizeof (char)*250);
		strcpy(p, t->curpath);
		free(t);
		return p;
	} else {
		return NULL;
	}
}

// Function entered by threads, continually checks for queued paths to traverse
// and files to perform 'grep' on
void find_grep(void *id) {
	// Set thread's TID based on their order of initialization
	int TID = *((int *) id);
	free(id);
	
	while (1){
		// acquire lock to dequeue a task
		pthread_mutex_lock(&lock);
		
		// Check for available tasks, if none, enter while loop
		while (check < 1) {
			waiting++; // increment to note that a thread is waiting
			
			// If all the threads are waiting, there will be nothing left to queue
			// so we exit the loop
			if (waiting >= NTHREADS){
				pthread_mutex_unlock(&lock); // give up lock before leaving 
				goto end;
			}
			
			// Otherwise, make thread wait and give up lock
			pthread_cond_wait(&cond, &lock);
		}
		
		// Dequeue task and place new path in p
		char p[250];
		strcpy(p, dequeue());
		check--; // Decrement check to note that there is one less task
		printf("[%d] DIR %s\n", TID, p);
		
		// Give up lock to let other threads check for tasks
		pthread_mutex_unlock(&lock);
		
		// Directory traversal
		struct dirent *dp;
		DIR *dir = opendir(p);
		
		// Set up absolute path of dequeued path, add '/' at the end in
		// preparation of concatenating the newly found files/directories
		char first[250], slash[2];
		realpath(p, first);
		strcpy(slash, "/");
		strcat(first, slash);
		
		while ((dp = readdir(dir)) != NULL){
			char buffer[250];
			// If directory is found...
			if (dp->d_type == DT_DIR && strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0){
				strcpy(buffer, first);
				strcat(buffer, dp->d_name); // Add the newly found directory's name to the path
				
				pthread_mutex_lock(&lock); 
				
				enqueue(buffer); // Enqueue the new absolute path
				check++; // Increment to note that one new task is available
				printf("[%d] ENQUEUE %s\n", TID, buffer);
				
				// If there is a thread waiting, decrement the number of
				// waiting threads and wake one up 
				if (waiting > 0) {
					waiting--;
					pthread_cond_signal(&cond);
				}
				pthread_mutex_unlock(&lock);
			// If a file is found...
			} else if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0){
				strcpy(buffer, first);
				strcat(buffer, dp->d_name); // Add the newly found file's name to the path
				
				// Set up command to send to system by 
				// preparing and concatenating the needed strings
				char space[2], command1[20], command2[20], tempbuffer[250];
				strcpy(space, " ");
				strcpy(command1, "grep -c ");
				strcpy(command2, " > /dev/null");
				strcpy(tempbuffer, buffer);
				
				strcat(command1, search_string);
				strcat(command1, space);
				strcat(tempbuffer, command2);
				strcat(command1, tempbuffer);
				
				// Execute command and check if it succeeded
				int ret = system(command1);
				pthread_mutex_lock(&lock);
				
				// Print "PRESENT" if grep succeeded, "ABSENT" if not
				if (ret == 0) printf("[%d] PRESENT %s\n", TID, buffer);
				else printf("[%d] ABSENT %s\n", TID, buffer);
				
				pthread_mutex_unlock(&lock);
			}
		}
		closedir(dir);
	}
	
	// Goes here when all threads are waiting, wake up others to exit
	end:
	pthread_cond_broadcast(&cond);
	
}

int main(int argc, char *argv[]) {
	// Initialize values from input arguments
	NTHREADS = strtol(argv[1], NULL, 10);
	char rootpath[250];
	strcpy(rootpath, argv[2]);
	strcpy(search_string, argv[3]);
	
	// Initialize queue pointer array
	q = malloc(sizeof(struct queue));
	q->head = NULL;
	q->tail = NULL;
	
	// Set first task in queue as rootpath and enqueue it
	char first[250];
	realpath(rootpath, first);
	enqueue(first);
	
	pthread_t thread[NTHREADS];

	// Initialize threads
	for (int i = 0; i < NTHREADS; i++) {
		int *arg = malloc(sizeof(*arg)); 
		*arg = i; // to take note of i as the TID (helps keep track of the order of initialization)
		pthread_create(&thread[i], NULL, (void *) find_grep, arg);
	}

	for (int i = 0; i < NTHREADS; i++) {
		pthread_join(thread[i], NULL);
	}
	
	pthread_mutex_destroy(&lock);
	
	return 0;
}
