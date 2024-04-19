//stdlib
#include <stdlib.h> 

//string (pulls memset too)
#include <string.h>

//stdio functions
#include <stdio.h>
#include <iostream>

//signal handling
#include <signal.h>

//concurrency control
#include <sys/wait.h> //provides waitpid()
#include <semaphore.h> //provides sem_...()

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

//default values
const char* DEF_PORT = "10401";
const char* DEF_THREADS = "1";
const char* DEF_BUFFS = "1";

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
    if (listen(sockfd, backlog) == -1) { 
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

void handle_request(int new_fd) {
    size_t max_chars = 1024; //should be plenty for our requests
    char buffer[max_chars]; //buffer is not necessarily null terminated
    size_t bytes_recvd = recv(new_fd, &buffer, max_chars, 0); //0 -> MSG_WAITALL?
    if(bytes_recvd <= 0){
        fprintf(stderr, "server: recieve within outer fork\n"); 
        exit(1); 
    }

    printf("%.*s\n", 0, *(bytes_recvd, (char*) buffer)); //"%.*s" sets min and max string length

    //strings don't have endianness, so no need to ntoh()
    //second token should be filename
    char* method = strtok(buffer, " ");
    printf("request method = %s\n", method);
    char* path = strtok(NULL, " ");
    printf("request path = %s\n", path);
    char* protocol = strtok(NULL, " ");
    printf("request protocol = %s\n", protocol);

    if (path[0] == '/') { //if path starts with "/" take it out so it doesn't cause issues during lookup
        memmove(path, path+1, strlen(path));
    }
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
    write(new_fd, header, strlen(header));
    write(new_fd, mapped, filestat.st_size);

    //cleanup
    munmap(mapped, filestat.st_size);

    close(new_fd);
    /* from pa1
    char* connection = argv[1];
    char* token = std::strtok(connection, ":");
    printf("host = %s\n", token);
    recip->host = token;
    token = std::strtok(NULL, ":");
    if (token == NULL) { // no ":"
        fprintf(stderr,"usage: client hostname and port format should be hostname:server_port\n");
        exit(1);
    }
    printf("port = %s\n", token);
    recip->port = token; // keep port a char* bc that's the form it needs for subsequent functions
    */

    /*CHATGPT
    
    // Check if it's a GET request for test.html
        if (strstr(buffer, "GET /" FILENAME) == NULL) {
            char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            write(newsockfd, response, strlen(response));
            close(newsockfd);
            exit(0);
        }
    */

    //parse URL portion (2nd token)
    //call parse 
    //everything before "?" (if it exists) is filename
    //everything after "?" (if it exists) is put into QUERY_STRING env variable
        //call handle request -> static, dynamic

    /* from pa1
    int inner_chld_status = fork();
    if(inner_chld_status < 0) {
        fprintf(stderr, "server: inner child failed to fork\n"); 
        exit(1); 
    }

    if(inner_chld_status == 0) {
        execute_and_send(new_fd, &req);
    } else {
        close(new_fd);
    }
    */
}

void main_accept_loop(int sockfd) {
    int new_fd; // listen on sock_fd, new connection on new_fd 
    struct sockaddr_storage their_addr; // connector's address information 
    socklen_t sin_size;
    char s[INET_ADDRSTRLEN]; // IPv4

    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        std::cout << "got request" << std::endl;

        if (new_fd == -1) {
            perror("accept");
            continue; 
        }

        inet_ntop(their_addr.ss_family, 
            &(((struct sockaddr_in*)(struct sockaddr *)&their_addr)->sin_addr), 
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        int outer_chld_status = fork();
        if(outer_chld_status < 0) {
            fprintf(stderr, "server: outer child failed to fork\n"); 
            exit(1); 
        }

        //accepted a request, now get the file name and memory map/send the file contents back
        if (outer_chld_status == 0) { 
            // this is the child process now, if fork returned 0
            close(sockfd); // child doesn't need copy of the listener 
            handle_request(new_fd);
            exit(0); 
        } else {
            close(new_fd);
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

    struct addrinfo* servinfo; //return value for get_addresses
    get_addresses(&servinfo, port); // mutates servinfo, no return needed
    if (servinfo == NULL) { // if servinfo empty after retrieval, return error
        perror("two: server: no resources"); 
        return -1;
    }

    int sockfd;
    if ((sockfd = make_bound_socket(servinfo)) == -1) {
        fprintf(stderr, "server: failed to bind\n"); 
        exit(1);
    };

    // being here means socket has binded, ready to listen
    struct sigaction sa; // structure that specifies how to handle a signal
    prepare_for_connection(sockfd, &sa, atoi(buffer_str));

    freeaddrinfo(servinfo);

    main_accept_loop(sockfd);
}

//nc 127.0.0.1 10488
//curl http://localhost:10488
