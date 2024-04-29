/*
Name: Saloni Sanger
Course: Computer Networks
Date: April 29th, 2024
File: http_messaging.h
Description: header to hold functions for response creation
    and sending. 
Goals: I would like fib.cpp to be able to create its responses 
    with this file, and I would like to figure out how to add
    successful responses for both static and dynamic requests, as
    well as adding the Date to the response header.
    Because I didn't have the time to figure this out, I have 
    awkward code replication throughout this project.
*/

//stdlib
#include <stdlib.h> 

//std io functions
#include <stdio.h>
#include <iostream>

//socket write()
#include <unistd.h>

//strlen()
#include <string.h>

//struct stat
#include <sys/stat.h>

//adapted from Dr. Zhu's code

#define MAXBUF 8192

void write_or_die(int fd, char* buf, size_t length) {
    int rv;
    if ((rv = write(fd, buf, length)) == -1) {
        perror("server: write");
        exit(1);
    }
}

void write_error_response(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg) {
    char buf[MAXBUF], body[MAXBUF];
    //create body first, its length is needed for header
    sprintf(body, "" //second \r\n substitute before data
    "<!doctype html>\r\n"
    "<head>\r\n"
    "  <title>OSTEP WebServer Error</title>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "  <h2>%s: %s</h2>\r\n"
    "  <p>%s: %s</p>\r\n"
    "</body>\r\n"
    "</html>\r\n", errnum, shortmsg, longmsg, cause);

    //header
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    write_or_die(fd, buf, strlen(buf));

    sprintf(buf, "Connection: close\r\n");
    write_or_die(fd, buf, strlen(buf));

    /*
    get_date_time_string(&buf);
    write_or_die(fd, buf, strlen(buf));
    */

    sprintf(buf, "Content-Length: %lu\r\n", strlen(body)); //%lu = long unsigned integer
    write_or_die(fd, buf, strlen(buf));

    sprintf(buf, "Content-Type: text/html\r\n");
    write_or_die(fd, buf, strlen(buf));

    sprintf(buf, "Server: cpsc4510 web server 1.0\r\n");
    write_or_die(fd, buf, strlen(buf));

    write_or_die(fd, body, strlen(body));
}
/*
void write_successful_static_response(int fd, struct stat filestat, void* mapped) {
    char buf[MAXBUF];

    //header
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    write_or_die(fd, buf, strlen(buf));

    sprintf(buf, "Connection: close\r\n");
    write_or_die(fd, buf, strlen(buf));

    
    //get_date_time_string(&buf);
    //printf("buffer outside datetime: %s", buf);
    //write_or_die(fd, buf, strlen(buf));
    

    sprintf(buf, "Content-Length: %ld\r\n", filestat.st_size);
    write_or_die(fd, buf, strlen(buf));

    sprintf(buf, "Content-Type: text/html\r\n");
    write_or_die(fd, buf, strlen(buf));

    sprintf(buf, "Server: cpsc4510 web server 1.0\r\n\r\n");
    write_or_die(fd, buf, strlen(buf));

    int rv;
    if ((rv = write(fd, mapped, filestat.st_size)) == -1) {
        perror("server: write");
        exit(1);
    }
}

void print_successful_dyn_response(char body[]) {
    char buf[MAXBUF];

    //header
    std::cout << "HTTP/1.1 200 OK\r\n";

    std::cout << "Connection: close\r\n";

    char* date_time;
    get_date_time_string(&date_time);
    std::cout << "Date: " << date_time << "\r\n";

    std::cout << "Content-Length: " << strlen(body) << "\r\n";

    std::cout << "Content-Type: text/html\r\n";

    std::cout << "Server: cpsc4510 web server 1.0\r\n\r\n";

    std::cout << body;
}
*/