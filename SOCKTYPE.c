#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
int main() {
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];
    // Clear the hints struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM; // 0 to get address list for all socket types
    // Get address info
    if ((status = getaddrinfo("www.x.com", NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    } 
    printf("IP addresses for www.example.com:\n\n");
    // Loop through all the results and print out IP addresses
    for(p = res; p != NULL; p = p->ai_next) {
        void *addr;
        char *ipver;
        char ip_port[INET6_ADDRSTRLEN + 10]; // For IPv6 + port
        char socket_type[20]; // To hold the socket type as string
        // Get the socket type
        switch (p->ai_socktype) {
            case SOCK_STREAM:
                strcpy(socket_type, "SOCK_STREAM");
                break;
            case SOCK_DGRAM:
                strcpy(socket_type, "SOCK_DGRAM");
                break;
            case SOCK_RAW:
                strcpy(socket_type, "SOCK_RAW");
                break;
            default:
                strcpy(socket_type, "Unknown");
                break;
        }
        // Get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
            inet_ntop(AF_INET, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
            sprintf(ip_port, "%s:%d", ipstr, ntohs(ipv4->sin_port));
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
            inet_ntop(AF_INET6, &(ipv6->sin6_addr), ipstr, sizeof(ipstr));
            sprintf(ip_port, "[%s]:%d", ipstr, ntohs(ipv6->sin6_port));
        }
        // Print the IP address, port, and socket type
        printf(" %s: %s (%s)\n", ipver, ip_port, socket_type);
    }
    freeaddrinfo(res); // Free the linked list
    return 0;
}