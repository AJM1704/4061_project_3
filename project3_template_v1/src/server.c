#include "../include/server.h"
#include <linux/limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// /********************* [ Helpful Global Variables ] **********************/
int num_dispatcher =
    0; // Global integer to indicate the number of dispatcher threads
int num_worker = 0; // Global integer to indicate the number of worker threads
FILE *logfile;      // Global file pointer to the log file
int queue_len = 0;  // Global integer to indicate the length of the queue
database_entry_t database[DATABASE_SIZE];
pthread_t dispatcher_thread[MAX_THREADS];
pthread_t worker_thread[MAX_THREADS];
request_t request_queue[MAX_QUEUE_LEN];
int queued_item_count;
int queue_front;
int queue_back;
pthread_mutex_t request_queue_mutex;
pthread_mutex_t request_queue_itemcount_mutex;
pthread_cond_t queue_not_full_cond;
pthread_cond_t queue_not_empty_cond;
pthread_mutex_t logfile_mutex;
int database_image_count = 0;


/* TODO: Intermediate Submission
  TODO: Add any global variables that you may need to track the requests and
  threads [multiple funct]  --> How will you track the p_thread's that you
  create for workers? [multiple funct]  --> How will you track the p_thread's
  that you create for dispatchers? [multiple funct]  --> Might be helpful to
  track the ID's of your threads in a global array What kind of locks will you
  need to make everything thread safe? [Hint you need multiple] What kind of CVs
  will you need  (i.e. queue full, queue empty) [Hint you need multiple] How
  will you track the number of images in the database? How will you track the
  requests globally between threads? How will you ensure this is thread safe?
  Example: request_t req_entries[MAX_QUEUE_LEN]; [multiple funct]  --> How will
  you update and utilize the current number of requests in the request queue?
  [worker()]        --> How will you track which index in the request queue to
  remove next? [dispatcher()]    --> How will you know where to insert the next
  request received into the request queue? [multiple funct]  --> How will you
  track the p_thread's that you create for workers? TODO How will you store the
  database of images? What data structure will you use? Example:
  database_entry_t database[100];
*/

// TODO: Implement this function
/**********************************************
 * image_match
   - parameters:
      - input_image is the image data to compare
      - size is the size of the image data
   - returns:
       - database_entry_t that is the closest match to the input_image
************************************************/
// just uncomment out when you are ready to implement this function
int image_match(char *input_image, int size, database_entry_t* entry) {
  const char *closest_file = NULL;
  int closest_distance = 10;
  int closest_index = 0;
  for (int i = 0; i < database_image_count/* replace with your database size*/; i++) {
    const char
        *current_file = database[i].buffer; /* TODO: assign to the buffer from the database struct*/
    int result = memcmp(input_image, current_file, size);
    if (result == 0) {
      *entry = database[closest_index];
      return 0;
    }

    else if (result < closest_distance) {
      closest_distance = result;
      closest_file = current_file;
      closest_index = i;
    }
  }

  if (closest_file != NULL) {
    *entry = database[closest_index];
    return 0;
  } else {
    printf("No closest file found.\n");
    return -1;
  }
}

// TODO: Implement this function
/**********************************************
 * LogPrettyPrint
   - parameters:
      - to_write is expected to be an open file pointer, or it
        can be NULL which means that the output is printed to the terminal
      - All other inputs are self explanatory or specified in the writeup
   - returns:
       - no return value
************************************************/
pthread_mutex_t log_mutex;

void LogPrettyPrint(FILE *to_write, int threadId, int requestNumber,
                    char *file_name, int file_size) {
  pthread_mutex_lock(&log_mutex);
  if (!to_write) {
    if (file_size ==  -1) {
      printf("[%d][%d][][No match!]\n", threadId, requestNumber);
    }
    else {
      printf("[%d][%d][%s][%d]\n", threadId, requestNumber, file_name, file_size);
    }
  }
  else {
    pthread_mutex_lock(&logfile_mutex);
    if (file_size ==  -1) {
      fprintf(to_write, "[%d][%d][][No match!]\n", threadId, requestNumber);
    }
    else {
      fprintf(to_write, "[%d][%d][%s][%d]\n", threadId, requestNumber, file_name, file_size);
    }
    // Flush file write because we might not close the file in case of cntrl c!
    fflush(logfile);
    pthread_mutex_unlock(&logfile_mutex);
  }
  pthread_mutex_unlock(&log_mutex);
}

/*
  TODO: Implement this function for Intermediate Submission
  * loadDatabase
    - parameters:
        - path is the path to the directory containing the images
    - returns:
        - no return value
    - Description:
        - Traverse the directory and load all the images into the database
          - Load the images from the directory into the database
          - You will need to read the images into memory
          - You will need to store the file name in the database_entry_t struct
          - You will need to store the file size in the database_entry_t struct
          - You will need to store the image data in the database_entry_t struct
          - You will need to increment the number of images in the database
*/
/***********/
void loadDatabase(char *path){
  struct dirent *entry;
  DIR *dir = opendir(path);
  char dir_path[BUFF_SIZE]; 
  strncpy(dir_path, path, BUFF_SIZE);

  if (dir == NULL) {
    perror("Could not open directory");
    return;
  }
  
  while ((entry = readdir(dir)) != NULL && database_image_count < DATABASE_SIZE)
  {
    if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
    {
      continue;
    }

    char file_path[BUFF_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
    printf("Adding %s/%s to the database.\n", path, entry->d_name);

    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
      perror("Could not open file");
      continue;
    }

    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    if (file_size < 0) {
      perror("Error reading file!");
      continue;
    }
    rewind(file);

    char *buffer = (char *) malloc(file_size);
    if (buffer == NULL) {
      perror("Memory allocation failed");
      fclose(file);
      continue;
    }

    fread(buffer, 1, file_size, file);
    fclose(file);


    strncpy(database[database_image_count].file_name, entry->d_name, 1028 - 1);
    database[database_image_count].file_name[1028 - 1] = '\0';
    database[database_image_count].file_size = file_size;
    database[database_image_count].buffer = buffer;
    printf("Added %s[%d] as [%d] database element\n", database[database_image_count].file_name, database[database_image_count].file_size, database_image_count);
    database_image_count += 1;

  }
  closedir(dir);
  printf("Loaded %d images into database!\n", database_image_count);
}

void *dispatch(void *thread_id) {
  while (1) {
    //TODO: [VIVEK] change request_details to normal request_t use fd and really create it, test locks make CVs.
    size_t file_size = 0;
    request_t request;

    /* TODO: Intermediate Submission
     *    Description:      Accept client connection
     *    Utility Function: int accept_connection(void)
     */

    int conn = accept_connection();
    printf("Dispatcher [%d] accepted connection!\n", *(int*) thread_id);

    /* TODO: Intermediate Submission
     *    Description:      Get request from client
     *    Utility Function: char * get_request_server(int fd, size_t
     * *filelength)
     */

    char* image_bytes = get_request_server(conn, &file_size);

    /* TODO
     *    Description:      Add the request into the queue
         //(1) Copy the filename from get_request_server into allocated memory
     to put on request queue


         //(2) Request thread safe access to the request queue

         //(3) Check for a full queue... wait for an empty one which is signaled
     from req_queue_notfull

         //(4) Insert the request into the queue

         //(5) Update the queue index in a circular fashion (i.e. update on
     circular fashion which means when the queue is full we start from the
     beginning again)

         //(6) Release the lock on the request queue and signal that the queue
     is not empty anymore
    */
    request.buffer = image_bytes;
    request.file_descriptor = conn;
    request.file_size = file_size;

    pthread_mutex_lock(&request_queue_mutex);
    while (queued_item_count ==  queue_len) {
      pthread_cond_wait(&queue_not_full_cond, &request_queue_mutex);
    }

    request_queue[queue_back] = request;
    queue_back += 1;
    if (queue_back == queue_len) {
      queue_back = 0;
    }
    queued_item_count += 1;
    printf("Dispatcher [%d] added request to queue [%d]!\n", *(int*) thread_id, queued_item_count);
    pthread_cond_signal(&queue_not_empty_cond);
    pthread_mutex_unlock(&request_queue_mutex);
  }
  return NULL;
}

void *worker(void *thread_id) {

  // You may use them or not, depends on how you implement the function
  int num_request =
      0; // Integer for tracking each request for printing into the log file
  // int fileSize = 0; // Integer to hold the size of the file being requested
  // void *memory =
  //     NULL; // memory pointer where contents being requested are read and stored
  // int fd = INVALID; // Integer to hold the file descriptor of incoming request
  char * image_bytes;      // String to hold the contents of the file being requested
  database_entry_t match; 

  /* TODO : Intermediate Submission
   *    Description:      Get the id as an input argument from arg, set it to ID
   */
  int id = *(int*) thread_id;

  while (1) {
    /* TODO
    *    Description:      Get the request from the queue and do as follows
      //(1) Request thread safe access to the request queue by getting the
    req_queue_mutex lock
      //(2) While the request queue is empty conditionally wait for the request
    queue lock once the not empty signal is raised

      //(3) Now that you have the lock AND the queue is not empty, read from the
    request queue

      //(4) Update the request queue remove index in a circular fashion

      //(5) Fire the request queue not full signal to indicate the queue has a
    slot opened up and release the request queue lock
      */

    /* TODO
     *    Description:       Call image_match with the request buffer and file
     * size store the result into a typeof database_entry_t send the file to the
     * client using send_file_to_client(int fd, char * buffer, int size)
     */

    /* TODO
     *    Description:       Call LogPrettyPrint() to print server log
     *    update the # of request (include the current one) this thread has
     * already done, you may want to have a global array to store the number for
     * each thread parameters passed in: refer to write up
     */
    pthread_mutex_lock(&request_queue_mutex);
    while (queued_item_count == 0) {
      pthread_cond_wait(&queue_not_empty_cond, &request_queue_mutex); 
    }

    num_request += 1;
    printf("Worker [%d] recieved request from queue\n", id);
    request_t request = request_queue[queue_front];
    image_bytes = request.buffer;
    if (image_match(image_bytes, request.file_size, &match) == 0) {
      send_file_to_client(request.file_descriptor, match.buffer, match.file_size);
      LogPrettyPrint(NULL, id, num_request, match.file_name, match.file_size);
      LogPrettyPrint(logfile, id, num_request, match.file_name, match.file_size);
    }
    else  {
      // No match!
      send_file_to_client(request.file_descriptor, "\0", 1);
      LogPrettyPrint(NULL, id, num_request, "", -1);
      LogPrettyPrint(logfile, id, num_request, match.file_name, match.file_size);
    }
    queued_item_count -= 1;
    printf("Worker [%d] reduced requests in queue: [%d]\n", id, queued_item_count);
    queue_front += 1;
    if (queue_front == queue_len) {
      queue_front = 0;
    }
    
    pthread_cond_signal(&queue_not_full_cond);
    pthread_mutex_unlock(&request_queue_mutex);
  }
}

int main(int argc, char *argv[]) {
  pthread_mutex_init(&log_mutex, NULL);
  if (argc != 6) {
    printf("usage: %s port path num_dispatcher num_workers queue_length \n",
           argv[0]);
    return -1;
  }

  int port = -1;
  char path[BUFF_SIZE] = "no path set\0";
  num_dispatcher = -1; // global variable
  num_worker = -1;     // global variable
  queue_len = -1;      // global variable

  /* TODO: Intermediate Submission
   *    Description:      Get the input args --> (1) port (2) database path (3)
   * num_dispatcher (4) num_workers  (5) queue_length
   */

  port = atoi(argv[1]);
  strncpy(path, argv[2], BUFF_SIZE);
  num_dispatcher = atoi(argv[3]);
  num_worker = atoi(argv[4]);
  queue_len = atoi(argv[5]);

  /* TODO: Intermediate Submission
   *    Description:      Open log file
   *    Hint:             Use Global "File* logfile", use "server_log" as the
   * name, what open flags do you want?
   */

  logfile = fopen("server_log.txt", "w+");
  if (logfile == NULL) {
    perror("Error opening logfile !");
    exit(EXIT_FAILURE);
  }
  /* TODO: Intermediate Submission
   *    Description:      Start the server
   *    Utility Function: void init(int port); //look in utils.h
   */

  init(port);

  /* TODO : Intermediate Submission
   *    Description:      Load the database
   *    Function: void loadDatabase(char *path); // prototype in server.h
   */

  loadDatabase(path);

  /* TODO: Intermediate Submission
   *    Description:      Create dispatcher and worker threads
   *    Hints:            Use pthread_create, you will want to store pthread's
   * globally You will want to initialize some kind of global array to pass in
   * thread ID's How should you track this p_thread so you can terminate it
   * later? [global]
   */
  int dispatcher_ids[MAX_THREADS];
  int worker_ids[MAX_THREADS];

  for (int i = 0; i < num_dispatcher; i++) {
    dispatcher_ids[i] = i;
    pthread_create(&dispatcher_thread[i], NULL, dispatch, &dispatcher_ids[i]);
  }

  for (int i = 0; i < num_worker; i++) {
    worker_ids[i] = i;
    pthread_create(&worker_thread[i], NULL, worker, &worker_ids[i]);
  }
  // Wait for each of the threads to complete their work
  // Threads (if created) will not exit (see while loop), but this keeps main
  // from exiting
  int i;
  for (i = 0; i < num_dispatcher; i++) {
    fprintf(stderr, "JOINING DISPATCHER %d \n", i);
    if ((pthread_join(dispatcher_thread[i], NULL)) != 0) {
      printf("ERROR : Fail to join dispatcher thread %d.\n", i);
    }
  }
  for (i = 0; i < num_worker; i++) {
    fprintf(stderr, "JOINING WORKER %d \n",i);
    if ((pthread_join(worker_thread[i], NULL)) != 0) {
      printf("ERROR : Fail to join worker thread %d.\n", i);
    }
  }
  fprintf(stderr, "SERVER DONE \n"); // will never be reached in SOLUTION
  fclose(logfile);                   // closing the log files
  pthread_mutex_destroy(&log_mutex);
}