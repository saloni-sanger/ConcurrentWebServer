//my port is 10488

//stdlib
#include <stdlib.h> 

//string (pulls memset too)
#include <string.h>

//stdio functions
#include <stdio.h>
#include <iostream>

//stl queue
#include <queue>

//signal handling
#include <signal.h>

//concurrency control
#include <sys/wait.h> //provides waitpid()
#include <semaphore.h> //provides sem_...()
#include <pthread.h>

//unix socket
#include <unistd.h> 
#include <sys/types.h> 

//network
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <netdb.h> 

//file I/O and memory mapping
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

//my headers
//#include "shared.h"

//2 mutexes
//one for the queue protect enqueue/dequeueing bc producer and consumer share this
//one for writing to the socket bc they all share this

//default values
const char* DEF_PORT = "10401";
const char* DEF_THREADS = "1";
const char* DEF_BUFFS = "1";

//concurrency control
sem_t full, empty;
pthread_mutex_t queue_mutex, socket_mutex;

//shared arguments between threads should be global to avoid memory corruption
std::queue<int> q;
int sockfd;

void sigchld_handler(int s) { //waits until child is cleaned up
    // waitpid() might overwrite errno, so we save and restore it:
    // errno is a weird global variable, it needs to not be changed by waitpid()
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0); // (-1, _, _) means wait for any child process, WNOHANG means to return immediately if no child exited
    errno = saved_errno; 
}

void get_addresses(struct addrinfo** servinfo, char* port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv; // return value
    printf("attempting to start on port %s", port);
    std::cout << std::endl;

    if ((rv = getaddrinfo(NULL, port, &hints, servinfo)) != 0) { // getaddrinfo() creates structs and places them in a linked list starting at servinfo
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        exit(1);
    }

    return;
}

int make_bound_socket(struct addrinfo* servinfo) { 
    int sockfd; 
    struct addrinfo* p;
    int yes = 1;

    if (servinfo == NULL) { // if servinfo empty return error
        perror("server: no resources");
        return -1;
    }

    // loop through results and bind to first we can
    for(p = servinfo; p != NULL; p = p->ai_next) { 
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) { // try to create socket with struct from servinfo list
            perror("server: socket");
            continue;
        }
        // as long as servinfo has at least 1 struct, sockfd can be returned because it will be -1 here
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, 
                sizeof(yes)) == -1) { // allow multiple connections on one port
            perror("setsockopt");
            exit(1); 
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd);
            perror("server: bind");
            continue; 
        }

        break; // successfully did the 3 functions for a socket, move on
    }

    if(p == NULL) {
        fprintf(stderr, "server: failed to bind\n"); 
        exit(1); 
    }

    return sockfd;
}

void prepare_for_connection(int sockfd, struct sigaction* sa, int backlog) {
    //backlog is length of buffer, the number of request connections that can be 
    //accepted at one time
    //backlog = buffer = quue of pending connections at sockfd, which server will only have one of
    if (listen(sockfd, backlog) == -1) { //listen is a system call, backlog is a kernal level queue
        perror("listen"); 
        exit(1);
    }

    /*
    We can define how a child process is handled after it exits, 
    this prevents the main process from being forced to exit, 
    so we can keep listening and accept new requests :)
    */
    sa->sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa->sa_mask); // says to not block any signals, sa_mask is set of signals to be blocked
    sa->sa_flags = SA_RESTART; // makes sure that program won't fail once child process terminates, accept/recieve/fork loop will continue
    if (sigaction(SIGCHLD, sa, NULL) == -1) { // now we've built the struct sa to send into sigaction(), which actually sets our parameters
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");
}

void static_request(int new_fd, char* path) {
    //open and memory map requested file
    //mem mapping allows server to read contents of file directly from disk into memory without having to perform explicit read operations
    //OS will map a region of virtual memory to the file on the disk, leveraging OS's mem management and caching mecahnisms
    //it's also simpler to use, donh't have to treat files on disk differently than standard memory access
    int fd = open(path, O_RDONLY);
    struct stat filestat;
    fstat(fd, &filestat);
    void *mapped = mmap(NULL, filestat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    //send HTTP response with file contents
    char header[1024];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", filestat.st_size);

    pthread_mutex_lock(&socket_mutex);
    write(new_fd, header, strlen(header));
    write(new_fd, mapped, filestat.st_size);
    pthread_mutex_unlock(&socket_mutex);

    //cleanup
    munmap(mapped, filestat.st_size);

    close(new_fd);
}

void dynamic_request(int new_fd, char* path) {
    char* params = path;
    params += strlen("fib.cgi?"); //get everything after ? in path
    printf("path: %s\n", path);

    printf("params: %s\n", params);

    //setenv("QUERY_STRING", params, 1); //1 overwrites what wasw previously in QUERY_STRING

    //redirect standard output to the socket before executing fib.cpp

    //make envp char* array, fib.cgi can access what you put in this via environ global variable
    //new idea: let's set the environ global var for ourselves (to "QUERY_STRING=__"), bc execve() will give fib.cgi the parent's environment
    char executable[] = "fib.cgi"; //computer can't run .cpp source files, only binary executables (which (the correct one) will be created via Makefile)
    char* args[] = {executable, NULL};
    char query_string[strlen("QUERY_STRING=")+strlen(params)]; //allocates buffer big enough for both strings
    strcpy(query_string, "QUERY_STRING="); //copy for string literal
    strcat(query_string, params); //add to the end, already null terminated

    printf("qstring = %s\n", query_string); //isnt printing even with flush
    fflush(stdout);
    char* env_args[] = {query_string, NULL};

    if (dup2(new_fd, STDOUT_FILENO) == -1) {
        perror("dup2 stdout");
        close(new_fd);
        exit(EXIT_FAILURE);
    }
    if (dup2(new_fd, STDERR_FILENO) == -1) {
        perror("dup2 stderr");
        close(new_fd);
        exit(EXIT_FAILURE);
    }

    close(new_fd); //close new_fd before terminating this process and calling exec, which will just print
    //in the event that execve fails, new_fd will still be closed because it is called here
    
    if (execve(args[0], args, env_args) == -1) {
        perror("execve");
        exit(EXIT_FAILURE);
    }

    // If execve fails, print error message
    perror("execve");
    exit(EXIT_FAILURE);
}

void* consume(void* arg) { //needs (queue q)
    //convert void* arguments back
    std::queue<int>* q = (std::queue<int>*) arg;

    while(1) {
        sem_wait(&full); //when there is something to consume in the queue

        ssize_t max_chars = 1024; //should be plenty for our requests
        char buffer[max_chars]; //buffer is not necessarily null terminated
        ssize_t total_bytes = 0; //number of bytes recieved so far
        pthread_mutex_lock(&queue_mutex);
        //error, saying this pop() is returning void so I can't assign it to something
        int new_fd = q->front(); //get the new_fd to consume and process
        q->pop();
        pthread_mutex_unlock(&queue_mutex);

        pthread_mutex_lock(&socket_mutex); //I think I need this right?
        ssize_t bytes_read = read(new_fd, &buffer, max_chars); //ssize_t is a signed size_t
        if(bytes_read == 0) {
            fprintf(stderr, "server: read() within outer fork\n"); 
            exit(1); 
        }
        printf("bytes read: %d\n", bytes_read);
        //this fixed the missing n=5 before, why is it breaking now?
        while (bytes_read != 0) {
            if(bytes_read < 0) {
                fprintf(stderr, "server: read() within outer fork\n"); 
                exit(1); 
            }

            total_bytes += bytes_read;
            //if end of request (\r\n\r\n) is in the buffer, do not attempt to read again, will get stuck
            if (strstr(buffer, "\r\n\r\n") != NULL) {
                break;
            }
    
            //read() and write() are universally used, recv() and send() are for more specialized cases
            //so for this one use read() and write()
            
            printf("before read\n"); //it's waiting for the curl to close it's connection before continuing, shange recv() to read()
            //nothing is left to read possible, how to move past this if there's nothing left in the request
            bytes_read = read(new_fd, (&buffer + total_bytes), max_chars); //add into buffer offset by however many bytes already recieved (until request is done recv'ing)
            printf("after read\n");
        }
        pthread_mutex_unlock(&socket_mutex);

        //strings don't have endianness, so no need to ntoh()
        //second token should be filename
        printf("buffer: %.*s\n", total_bytes, buffer);
        char* method = strtok(buffer, " ");
        printf("request method = %s\n", method);
        char* path = strtok(NULL, " "); //takes out &n=5????
        printf("request path = %s\n", path);
        char* protocol = strtok(NULL, " ");
        printf("request protocol = %s\n", protocol);

        if (path[0] == '/') { //if path starts with "/" take it out so it doesn't cause issues during lookup
            memmove(path, path+1, strlen(path));
        }

        if (strstr(path, "fib.cgi") == NULL) { //if path does not request fib.cgi, treat it as a static request
            static_request(new_fd, path); //critical region is locked
        } else { //dynamic requests need to wait until thread is fully done before continuing
            pid_t pid = fork();
            if(pid == -1) {
                fprintf(stderr, "server: child failed to fork\n"); 
                exit(1); 
            }

            pthread_mutex_lock(&socket_mutex);//lock output before dup2() and execve() inside child process
            if(pid == 0) { //is there a way to correctly lock the socket mutex inside here
                close(sockfd); // child doesn't need copy of the listener 
                dynamic_request(new_fd, path); //close(new_fd) is called within dynamic request before execve()
            } else {
                //parent: wait for the child process to complete
                int status;
                waitpid(pid, &status, 0);
                close(new_fd);
                pthread_mutex_unlock(&socket_mutex); //would possibly locking the mutex within the child process cause issues?
            }
        }

        sem_post(&empty); //signal that a slot in the buffer is emptied
    }
}

void* produce(void* arg) { //arg should have (int sockfd, queue q), returns nothing
    //convert void* arguments back
    std::pair<std::queue<int>*, int*> * args = (std::pair<std::queue<int>*, int*> *) arg;
    std::queue<int>* q = args->first;
    int sockfd = *(args->second);

    while(1) {
        int new_fd; // listen on sock_fd, new connection on new_fd 
        struct sockaddr_storage their_addr; // connector's address information 
        socklen_t sin_size;
        char s[INET_ADDRSTRLEN]; // IPv4

        while(1) {
            sem_wait(&empty); //I believe this will be true while there is an empty slot in the buffer (while semaphore > 0?)

            sin_size = sizeof their_addr;
            new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size); //do I need to lock the socket to accept?, I think no
            std::cout << "got request" << std::endl;

            if (new_fd == -1) {
                perror("accept");
                continue; 
            } else {
                pthread_mutex_lock(&queue_mutex);
                q->push(new_fd); //pass accepted sockfd into queue for consumers to consume
                pthread_mutex_unlock(&queue_mutex);
                printf("Produced item %d for Queue: \n", new_fd);
            }

            sem_post(&full); //signal that a slot in the buffer has filled

            /* to test connected IP
            inet_ntop(their_addr.ss_family, 
                &(((struct sockaddr_in*)(struct sockaddr *)&their_addr)->sin_addr), 
                s, sizeof s);
            printf("server: got connection from %s\n", s);
            */
        }
    }
}

//signature needs to change
void parse_argv(int argc, char* argv[], char** port, char** thread_str, char** buffer_str) {
    // default values
    *(port) = (char*) DEF_PORT;
    *(thread_str) = (char*) DEF_THREADS;
    *(buffer_str) = (char*) DEF_BUFFS;

    for (int i = 1; i < argc; i+=2) {
        if ((i+1) >= argc) {
            fprintf(stderr, "specifier does not have corresponding value.\n");
            exit(1);
        }
        if (strcmp("-p", argv[i]) == 0) { //check if port is <= 65535 (max port #)
            if (atoi(argv[i+1]) > 65535) {
                fprintf(stderr, "port does not exist.\n");
                exit(1);
            }
            *(port) = argv[i+1];
        }
        else if (strcmp("-t", argv[i]) == 0) {
            if (atoi(argv[i+1]) < 1) {
                fprintf(stderr, "number of worker threads is not a positive integer.\n");
                exit(1);
            }
            *(thread_str) = argv[i+1];
        }
        else if (strcmp("-b", argv[i]) == 0) {
            if (atoi(argv[i+1]) < 1) { //does not catch -b "12r"
                fprintf(stderr, "length of buffer is not a positive integer.\n");
                exit(1);
            }
            *(buffer_str) = argv[i+1];
        }
        else {
            fprintf(stderr, "setup improperly formatted.\n");
            exit(1);
        }
    }
}

int main(int argc, char* argv[]) {
    char* port;
    char* thread_str;
    char* buffer_str;
    parse_argv(argc, argv, &port, &thread_str, &buffer_str);

    //testing
    printf("argc: %i\n", argc);
    printf("port: %s\n", port);
    printf("thread_str: %s\n", thread_str);
    printf("buffer_str: %s\n", buffer_str);

    //make ur threads based on -t
    //array of ints for p_ids
    //pthread_create() for handle_request() function (adjust to void*)
    pthread_t producer;
    pthread_t consumer_threads[atoi(thread_str)]; //thread_str = # of consumer threads requested by command line
    
    if (sem_init(&full, 0, 0) == -1) { //initialized to how many slots in the Queue of connections are full
        perror("semaphore initialization 1 in main");
    }
    if (sem_init(&empty, 0, atoi(buffer_str)) == -1) { //initialized to how many slots in the Queue of connections are empty (size of buffer = size of Queue)
        perror("semaphore initialization 2 in main");
    }

    if (pthread_mutex_init(&queue_mutex, NULL) == -1) {
        perror("mutex initialization 1 in main");
    }
    if (pthread_mutex_init(&socket_mutex, NULL) == -1) {
        perror("mutex initialization 2 in main");
    }

    struct addrinfo* servinfo; //return value for get_addresses
    get_addresses(&servinfo, port); // mutates servinfo, no return needed
    if (servinfo == NULL) { // if servinfo empty after retrieval, return error
        perror("two: server: no resources"); 
        return -1;
    }

    if ((sockfd = make_bound_socket(servinfo)) == -1) {
        fprintf(stderr, "server: failed to bind\n"); 
        exit(1);
    };

    // being here means socket has binded, ready to listen
    struct sigaction sa; // structure that specifies how to handle a signal
    prepare_for_connection(sockfd, &sa, atoi(buffer_str));

    freeaddrinfo(servinfo);

    //producer function is to accept and put connection into queue
    //consumer function is to handle connection
        //pretty sure I don't need to worry about making q a fixed size because the semaphore's waits and posts will manage how many free spots there are based on initial buffer_str
    std::pair<std::queue<int>*, int*> producer_args(&q, &sockfd);
    //don't make a producer thread seperately because main will just call it, continue with its program unblocked, and exit the program without waiting for the thread to finish
    pthread_create(&producer, NULL, produce, (void*)&producer_args);
    for (int i = 0; i < atoi(thread_str); i++) {
        pthread_create(&consumer_threads[i], NULL, consume, (void*)&q);
    }

    pthread_join(producer, NULL);
    // Cancel each consumer thread
    for (int i = 0; i < atoi(thread_str); i++) {
        pthread_cancel(consumer_threads[i]);
    }

    // Destroy semaphores
    sem_destroy(&full);
    sem_destroy(&empty);
    //do I need to "cleanup" threads and semaphores if the server continuously listens for connections anyway? will my code even ever reach here?
    //sem_destroy(&full);
    //sem_destroy(&empty);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&socket_mutex);

}

//nc 127.0.0.1 10488
//curl "http://localhost:10488/blah?blah=blah&blah=blah" 
//^ USE QUOTATIONS WITH CURL
