/*
Name: Saloni Sanger
Course: Computer Networks
Date: April 29th, 2024
File: fib.cpp
Description: fib.cpp is compiled into a cgi program.
    It parses the QUERY_STRING environment variable for parameters,
    calcultes the nth fibonacci number. Then, it constructs and prints
    an HTTP response.
    In some cases, it prints an HTTP error response and exits.
*/

#include <iostream>
#include <cstdlib>
#include <string>
#include <sstream>

//strtok
#include <stdio.h>
#include <string.h>

//my headers
#include "http_messaging.h"

extern char** environ;

int fib(int n) {
    if (n <= 1)
        return n;
    return fib(n - 1) + fib(n - 2);
}

//add to readme weaknesses: fib.cgi cannot skip unusable parameters in request URL
//assumes only 2 parameters sent in, either user=_&n=_ or n=_&user=_
int main() {
    char *params = getenv("QUERY_STRING"); //this was set by creating envp[] in server
    if (params != nullptr) {
        char* uname;
        int n = 0;
        char* var = strtok(params, "=");
        if (strcmp(var, "user") == 0) {
            uname = strtok(NULL, "&");
            var = strtok(NULL, "=");
            if (strcmp(var, "n") == 0) {
                n = atoi(strtok(NULL, ""));
            }
        } else if (strcmp(var, "n") == 0) {
            n = atoi(strtok(NULL, "&")); //atoi will silently fail if n is not an int
            var = strtok(NULL, "=");
            if (strcmp(var, "user") == 0) {
                uname = strtok(NULL, "");
            }
        }

        if (n < 0 || n > 10000) {
            char error[] = "The parameter 'n' for fib.cgi is not an integer, a negative integer, or a positive integer larger than 10,000.";
            char errnum[] = "500";
            char reason[] = "Internal Server Error";
            char msg[] = "Server could not complete this request.";
            std::stringstream body;
            body << "\r\n<!doctype html>\r\n<head>\r\n<title>OSTEP WebServer Error</title>\r\n</head>\r\n<body>\r\n<h2>" << errnum;
            body << ": " << reason << "</h2>\r\n<p>" << error << ": " << msg << "</p>\r\n</body>\r\n</html>\r\n";
            std::string b = body.str();
            std::stringstream header;
            header << "HTTP/1.1 " << errnum << " " << reason << "\r\nConnection: close\r\nContent-Length: " << b.length() << "\r\nContent-Type: text/html\r\n";
            header << "Server: cpsc4510 web server 1.0\r\n";
            std::string h  = header.str();
            std::cout << h << b << std::endl;
            exit(1);
        }

        int result = fib(n) % 1000000007;

        std::cout << "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/html\r\nServer: cpsc4510 web server 1.0\r\n";
        std::stringstream ss;
        ss << uname << ", welcome to the CGI Program!\nThe " << n << "th Fibonnaci number is " << result << ".\n";
        std::string body = ss.str();
        std::cout << "Content-Length: " << body.length() << "\r\n\r\n";
        std::cout << body << std::endl;
    }
    return 0;
}