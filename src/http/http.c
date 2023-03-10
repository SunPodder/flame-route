#include <http/response.h>
#include <utils/route-map.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <bits/in_addr.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <http/http.h>
#include <http/route.h>

struct arg_struct {
    FlameServer *server;
    int *socket;
};


FlameServer create_flame_server(){
    FlameServer server;
    server.address = "127.0.0.1";
    server.port = 8080;
    server.routes = route_map_create(100);
    return server;
}

Route *find_route(FlameServer *server, char *path, HTTPMethod method){
    Route *route = route_map_get(server->routes, path);
    if(route == NULL){
        return NULL;
    }
    for(int i = 0; i < 9; i++) {
        if(route->methods[i] == method){
            return route;
        }
    }
    return NULL;
}

/*
 * This function is called by server main loop
 * in a new thread to handle each connection
 *
 * @param soc: the socket file descriptor of the accepted connection
 */
static void *handle_connection(struct arg_struct *args){
    // TODO:
    // 1. Read the request from the socket
    // 2. Parse the request
    // 3. Find the route
    // 4. check it the route http method matches the request method
    // 5. call the route callback
    // 6. send the response to the client
    // 8. if route is not found send 404
    // 9. if method is not allowed send 405
    // 10. if the route is static send the file
    // 11. close the socket

    int *new_socket = args->socket;
    int kMaxBufferSize = 3000;
    char *buffer = (char*)malloc(kMaxBufferSize);
    int pos = 0, size = kMaxBufferSize;
    while(1){
        if (pos + 1 >= size) {
            // Buffer is full, resize it
            size *= 2;
            char* new_buffer = realloc(buffer, size);
            if (!new_buffer) {
                fprintf(stderr, "Error: failed to resize buffer\n");
                exit(1);
            }
            buffer = new_buffer;
        }

        // Read next chunk of data
        int n = read(*new_socket, buffer + pos, size - pos - 1);
        if (n <= 0) {
            // End of stream or error
            break;
        }
        // Update buffer position
        pos += n;
        buffer[pos] = '\0';

        // Check if entire request has been read
        if (strstr(buffer, "\r\n\r\n") != NULL) {
            break;
        }
    }

    printf("%s\n\n", buffer);
    HTTPRequest *request = parse_request(buffer);

    Route *route = find_route(args->server, request->path, request->method);

    if(route == NULL){
        HTTP404(new_socket);
        HTTPSendResponse(*new_socket, "text/html", "404 Not Found");
        close(*new_socket);
        free(buffer);
        return NULL;
    }

    HTTP200(new_socket);
    HTTPResponse *response = (HTTPResponse*)malloc(sizeof(HTTPResponse));
    route->callback(*request, *response);

    HTTPSendResponse(*new_socket, "text/html", response->body);

    close(*new_socket);
    free(buffer);
    return NULL;
}

/*
 * Sets up the socket and starts listening for connections
 * @param server: the server struct
 */
int ignite_server(FlameServer *server){
    
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(server->address);
    address.sin_port = htons( server->port );

    // binding socket to the port
    if (bind(server_fd, (struct sockaddr *)&address,
                                 sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    // listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started at %s:%d\n", server->address, server->port);

    while(1){
        // accept connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                       (socklen_t*)&addrlen))<0){
            perror("accept");
            continue;
        }

        struct arg_struct args;
        args.server = server;
        args.socket = &new_socket;

        // handle the connection in a new thread
        pthread_t thread_id;
        if( pthread_create( &thread_id, NULL,  handle_connection,
                    (void*) &args) < 0){
            perror("could not create thread");
            continue;
        }
    }

    return 0;
}

void register_route(FlameServer *server, char *path,
        HTTPMethod method[], HTTPResponse (*callback)(
            HTTPRequest *request, HTTPResponse *response)){
    // TODO:
    // 1. Create a route struct
    // 2. Add the route to the server routes hashmap <path, route>
}

void register_static_route(FlameServer *server, char *path, char *file_path){
    // TODO:
    // add the route to the server's static routes
}
