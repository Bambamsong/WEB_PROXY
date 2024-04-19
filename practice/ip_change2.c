#include <csapp.h>

int main(int argc, char **argv){
    struct in_addr inaddr;  /* 네트워크 바이트 오더 */
    int rc;

    if (argc != 2){
        fprintf(stderr, "usage: %s <dotted-decimal>\n", argv[0]);
        exit(0);
    }

    rc = inet_pton(AF_INET, argv[1], &inaddr);
    if (rc == 0)
        app_error("유효하지 않은 주소입니다");
    else if (rc < 0)
        unix_error("inet_pton error");

    printf("0x%x\n", ntohl(inaddr.s_addr));
    exit(0);
}