#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void doit(int clientfd);
void read_requesthdrs(rio_t *rp, void *buf, int serverfd, char *hostname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, char *port, char *path);

/* You won't lose style points for including this long line in your code */
static const int is_local_test = 1; // 테스트 환경에 따른 도메인&포트 지정을 위한 상수 (0 할당 시 도메인&포트가 고정되어 외부에서 접속 가능)
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";


/* main */
int main(int argc, char **argv) {

  int listenfd, *clientfd;
  socklen_t clientlen;
  char client_hostname[MAXLINE], client_port[MAXLINE];

  struct sockaddr_storage clientaddr;
  if (argc != 2){
    fprintf(stderr, "usage :%s <port> \n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 포트번호를 가지고 듣기소켓 오픈
  while(1) {
    clientlen = sizeof(clientaddr);
    clientfd = Malloc(sizeof(int));
    *clientfd = Accept(listenfd, (SA*)&clientaddr, &clientlen); // 클라이언트 연결 요청 수신

    Getnameinfo((SA*)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s).\n", client_hostname, client_port);
    
    doit(clientfd);
    Close(clientfd);
    free(clientfd);
  }
  
  return 0;
}








/* 1. 클라이언트의 요청을 수신 */

void doit(int clientfd){
    int serverfd, content_length;
    rio_t request_rio, response_rio;
    char request_buf[MAXLINE], response_buf[MAXLINE];
    char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
    char * response_ptr, filename[MAXLINE], cgiargs[MAXLINE];
    /* 1-1 Request Line 읽기 [Client -> Proxy] */
    Rio_readinitb(&request_rio, clientfd); // request_rio 구조체를 초기화하고 파일 디스크립터를 이 구조체에 연결
    Rio_readlineb(&request_rio, request_buf, MAXLINE);
    printf("Request headers:\n %s\n", request_buf);

    // 요청 라인 parsing을 통해 'method, uri, hostname, port, path' 찾기
    sscanf(request_buf, "%s %s", method, uri);
    parse_uri(uri, hostname, port, path);

    // Server에 전송하기 위해 요청 라인의 형식 변경: 'method uri version' -> 'method path HTTP/1.0'
    sprintf(request_buf, "%s %s %s\r\n", method, path, "HTTP/1.0");

    // 지원하지 않는 method인 경우 예외 처리
    if (strcasemcp(method, "GET") && strcasecmp(method, "HEAD")) {
        clienterror(clientfd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }

    /* 1-2 Request Line 전송 [Proxy -> Server]*/
    // Server 소켓 생성
    serverfd = Open_clientfd(hostname, port);
    if (serverfd < 0) {
        clienterror(serverfd, method, "502", "Bad Gateway", "Failed to establish connection with the end server");
        return;
    }
    Rio_writen(serverfd, request_buf, strlen(request_buf)); // serverfd에 request_buf의 데이터를 쓰는 행위

    /* 2 Request Header 읽기&전송 [Client -> Proxy -> Server]*/
    read_requesthdrs(&request_rio, request_buf, serverfd, hostname, port);



    /* 3 Response Header 읽기 & 전송 [Server -> Proxy -> Client] */
    Rio_readinitb(&response_rio, serverfd);
    while (strcmp(response_buf, "\r\n")){
        Rio_readlineb(&response_rio, response_buf, MAXLINE);
        if (strstr(response_buf, "Content_lenght")) // Response Body 수신에 사용하기 위해 Content-length 저장
            content_length = atoi(scrchr(response_buf, ":") + 1);
        Rio_writen(clientfd, response_buf, strlen(response_buf));
    }
    
    /* 4 Response Body 읽기&전송 [Server -> Proxy -> Client] */
    response_ptr = malloc(content_length);
    Rio_readnb(&response_rio, response_ptr, content_length);
    Rio_writen(clientfd, response_ptr, content_length); //Client에 Response Body 전송
    free(response_ptr);
    Close(serverfd);
}

/*
    1. uri를 'hostname', 'port', 'path'로 파싱하는 함수
    2. uri 형태: 'http://hostname:port/path' 혹은 'http://hostname/path' (port는 선택사항)
*/
void parse_uri(char *uri, char *hostname, char *port, char *path){
    // host_name의 시작 위치 포인터: '//'가 있으면 //뒤(ptr+2)부터, 없으면 uri 처음부터
    char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;
    char *port_ptr = strchr(hostname_ptr, ':'); // port 시작 위치 (없으면 NULL)
    char *path_ptr = strchr(hostname_ptr, '/'); // path 시작 위치 (없으면 NULL)
    strcpy(path, path_ptr); // path 를 복사하기 위함

    if (port_ptr){// port가 있는 경우
        if (is_local_test)
            strcpy(port, "80"); // port의 기본 값인 80으로 설정
        else
            strcpy(port, "8000");
        strncpy(hostname, hostname_ptr, path_ptr-hostname_ptr); // hostname에 path_ptr-hostname_ptr개의 문자열만큼 카운트해서 복사해라. 경로빼고 복사해라
    }
}

/* 
    1. Request Header를 읽고 Server에 전송하는 함수
    2. 필수 헤더가 없는 경우에는 필수 헤더를 추가로 전송
*/
void read_requesthdrs(rio_t *request_rio, void *request_buf, int serverfd, char *hostname, char * port) {
    int is_host_exist;
    int is_connection_exist;
    int is_proxy_connection_exist;
    int is_user_agent_exist;

    Rio_readlineb(request_rio, request_buf, MAXLINE); // request_rio 를 읽어서 request_buf에 저장해라
    while (strcmp(request_buf, "\r\n")) {// 끝행이 나올때까지 반복해라
        if (strstr(request_buf, "Proxy-Connection") != NULL){
            sprintf(request_buf, "Proxy-Connection: close\r\n");
            is_proxy_connection_exist = 1;
        }
        else if (strstr(request_buf, "Connection") != NULL){
            sprintf(request_buf, "Connection: close\r\n");
            is_connection_exist = 1;
        }
        else if (strstr(request_buf, "User-Agent") != NULL){
            sprintf(request_buf, user_agent_hdr);
            is_user_agent_exist = 1;
        }
        else if (strstr(request_buf, "Host") != NULL){
            is_host_exist = 1;
        }

        Rio_writen(serverfd, request_buf, strlen(request_buf)); // Server에 전송
        Rio_readlineb(request_rio, request_buf, MAXLINE);       // 다음 줄 읽기 즉, 한줄씩읽으면 exist를 바꾸면서 서버에 전송
    }

    // 필수 헤더 미포함 시 추가로 전송
    if (!is_proxy_connection_exist)
      {
        sprintf(request_buf, "Proxy-Connection: close\r\n");
        Rio_writen(serverfd, request_buf, strlen(request_buf));
      }
    if (!is_connection_exist)
      {
        sprintf(request_buf, "Connection: close\r\n");
        Rio_writen(serverfd, request_buf, strlen(request_buf));
      }
    if (!is_host_exist)
      {
        sprintf(request_buf, "Host: %s:%s\r\n", hostname, port);
        Rio_writen(serverfd, request_buf, strlen(request_buf));
      }
    if (!is_user_agent_exist)
      {
        sprintf(request_buf, user_agent_hdr);
        Rio_writen(serverfd, request_buf, strlen(request_buf));
      }
    
    sprintf(request_buf, "\r\n"); // 종료문
    Rio_writen(serverfd, request_buf, strlen(request_buf));
    return;
}