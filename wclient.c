/*
Name: Saloni Sanger
Course: Computer Networks
Date: April 29th, 2024
File: wclient.c
Description: wclient sends an HTTP request to wserver,
    recieves the web server's response, and prints it.
*/

// std io functions
#include <stdio.h> 

// std lib
#include <stdlib.h> 

// unix socket
#include <unistd.h>
#include <sys/types.h> 

// string
#include <string.h> 

// network
#include <netdb.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
// good idea to include netinet if using arpa
#include <netinet/in.h> 

void parse_argv(int argc, char* argv[], char** server, char** port) {
    for (int i = 1; i < argc; i+=2) {
        if ((i+1) >= argc) {
            fprintf(stderr, "specifier does not have corresponding value.\n");
            exit(1);
        }
        if (strcmp("-s", argv[i]) == 0) { 
            *(server) = argv[i+1];
        }
        else if (strcmp("-p", argv[i]) == 0) { // check if port is <= 65535 (max port #)
            if (atoi(argv[i+1]) > 65535) {
                fprintf(stderr, "port does not exist.\n");
                exit(1);
            }
            *(port) = argv[i+1];
        }
        else {
            fprintf(stderr, "setup improperly formatted.\n");
            exit(1);
        }
    }
}

void get_addresses(struct addrinfo** servinfo, char* server, char* port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rv; // return value

    if ((rv = getaddrinfo(server, port, &hints, servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        exit(1);
    }
    /*
    servinfo holds sockets available at the host:port combination requested from the command line,
    now we want to connect to one and send our request.
    Then, server will fork, execute, and return from its new_fd, which is connected to the same host:port combination.
    */
    return; 
}

int connect_socket(struct addrinfo* servinfo) {
    int sockfd; 
    struct addrinfo* p;

    if (servinfo == NULL) { // if servinfo empty return error
        perror("client: no resources");
        return -1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) { 
            perror("client: socket"); 
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd);
            perror("client: connect");
            continue; 
        }

        break; // reaches here if successfully connected to a socket
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n"); 
        return 2;
    }

    /*test
    char s[INET_ADDRSTRLEN];
    inet_ntop(p->ai_family, &(((struct sockaddr_in*)(struct sockaddr *)&p)->sin_addr), 
            s, sizeof s);
    printf("client: connecting to %s\n", s);
    */

    freeaddrinfo(servinfo);

    return sockfd;
}

void send_request(int sockfd, char* url, char* server) { // write() the request into the socket
    char* protocol = strtok(url, "//");
    char* connection = strtok(NULL, "/");
    char* path = strtok(NULL, "");
    char req_header[1048];
    sprintf(req_header, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, server);
    //url is a string, which don't have endianness
    write(sockfd, req_header, strlen(req_header)); // writes/sends request into socket
}

void read_and_print(int sockfd) {
    int numbytes;
    char buf[1048]; 
    memset(&buf, 0, sizeof buf);
    while ((numbytes = read(sockfd, buf, sizeof(buf))) > 0) { 
        printf("%s", buf);
    } 
    if (numbytes == -1) {
        perror("client: read");
        exit(1);
    }

    close(sockfd);
    return;
}

int main(int argc, char* argv[]) {
    char* server;
    char* port;
    parse_argv(argc, argv, &server, &port);
    printf("argv parsed\nserver: %s\nport: %s\n", server, port); //test, good
    printf("Enter the URL you would like to request,\nplease provide no extra spaces:\n");
    char url[256];
    scanf("%255s", url); // get user input, set url variable, avoid buffer overflow
    printf("URL recieved\nurl: %s\n", url); //test
    struct addrinfo* servinfo;
    get_addresses(&servinfo, server, port);

    int sockfd;
    if ((sockfd = connect_socket(servinfo)) == -1) {
        fprintf(stderr, "client: failed to connect\n"); 
        exit(1);
    };

    send_request(sockfd, url, server);

    read_and_print(sockfd);

    return 0;
}