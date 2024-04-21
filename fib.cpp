//cgi program is a c/c++ executable
//arguments are passed into the cgi script via the QUERY_STRING environment variable

//parse QUERY_STRING, process "user=__char*_&n=__int_" as needed

//calculate nth fibonacci number % 1,000,000,007

//return HTTP resposne back to the client
//<username>, welcome to the CGI Program!
//The <n>th Fibonacci number is _.

#include <iostream>
#include <cstdlib>

//strtok
#include <stdio.h>
#include <string.h>

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
            n = atoi(strtok(NULL, "&"));
            var = strtok(NULL, "=");
            if (strcmp(var, "user") == 0) {
                uname = strtok(NULL, "");
            }
        }
        int result = fib(n) % 1000000007;

        std::cout << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
        std::cout << uname << ", welcome to the CGI Program!\nThe " << n << "th Fibonnaci number is " << result << ".\n";
        /*
        std::cout << "Content-Type: text/html\r\n\r\n";
        std::cout << "<!DOCTYPE html>\n<html>\n<head>\n<title>Fibonacci Result</title>\n</head>\n<body>\n";
        std::cout << "<h1>Fibonacci(" << n << ") = " << result << "</h1>\n";
        std::cout << "</body>\n</html>\n";
        */
    }
    return 0;
}