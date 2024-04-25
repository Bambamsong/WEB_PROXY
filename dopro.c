#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define LRU_MAGIC_NUMBER 9999
#define CACHE_OBJS_COUNT 10

void *thread(void *vargp);
void doit(int connfd);
void parse_uri(char *uri,char *hostname,char *path,int *port);
void build_the_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio);
int connect_endServer(char *hostname,int port,char *http_header);

// 캐시에서 추가된 함수
void cache_init();
int cache_find(char *url);
void cache_uri(char *uri, char *buf);

void readerPre(int i);
void readerAfter(int i);

static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

typedef struct 
{
  char cache_obj[MAX_OBJECT_SIZE];
  char cache_url[MAXLINE];
  int LRU; // 가장 오래된 것 부터 삭제 
  int isEmpty; // 이 블럭에 캐시 정보가 들었는지 empty인지 아닌지 체크

  int readCnt;  // count of readers
  sem_t wmutex;  // protects accesses to cache 세마포어 타입. 1: 사용가능, 0: 사용 불가능
  sem_t rdcntmutex;  // protects accesses to readcnt
}cache_block; // 캐쉬블럭 구조체로 선언


typedef struct
{
  cache_block cacheobjs[CACHE_OBJS_COUNT];  // ten cache blocks
  int cache_num; // 캐시(10개) 넘버 부여
}Cache;

Cache cache;

int main(int argc, char **argv) {

  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  struct sockaddr_storage clientaddr;
  socklen_t clientlen;
  pthread_t tid;

  if (argc != 2){
    fprintf(stderr, "usage :%s <port> \n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 포트번호를 가지고 듣기소켓 오픈
  while(1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);

    Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s).\n", hostname, port);
    
    /* Thread 추가 */
    // 첫 번째 인자 : 새로 생성된 스레드의 ID를 저장할 포인터
    // 두 번째 인자 : 스레드 속성 지정(보통은 NULL)
    // 세 번째 인자 : 스레드 함수 (스레드가 진행해야할 일을 함수로 만들어 넣음)
    // 네 번째 인자 : 스레드 함수의 매개변수 (위 함수에 넣는 인자)
    Pthread_create(&tid, NULL, thread, (void *)connfd); 
  }
  return 0;
}

void *thread(void *vargs) { 
  int connfd = (int)vargs; // argument로 받은 것을 connfd에 넣는다
  Pthread_detach(pthread_self());
  doit(connfd);
  Close(connfd); 
  // connfd를 여러개로 만드는 이유? main 함수 while 돌 때마다 accept 쓰레드생성함수 호출되고, 
  // 그 생성 함수에서 쓰레드함수 호출하는데 호출할 때마다 connfd가 연결됨. (클라 여러개!)
  // 요청받으면 쓰레드 만들고, 이 쓰레드마다 connfd를 만든다. 그러면 프로세스는 하나인데 쓰레드 여러개 - 거기서 connfd쭈루룩.
  // 그래서 main함수가 아닌 thread함수에 doit 이 있다!
}

void doit(int connfd){
  
  int end_serverfd;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char endserver_http_header[MAXLINE];
  char hostname[MAXLINE], filepath[MAXLINE];
  int port;

  rio_t rio, server_rio;
  /*
    rio : client's rio
    server_rio : endserver's rio
  */

  Rio_readinitb(&rio, connfd);
  Rio_readlineb(&rio, buf, MAXLINE);
  sscanf(buf, "%s %s %s", method, uri, version); // 공백으로 구분된 3개의 문자열을 구분하여 각각에 저장

  // buf = GET http://hostname:port/path HTTP/1.0 의 형태

  if (strcasecmp(method, "GET")) {
    printf("Proxy does not implement the method");
    return;
  }

  /* 
    uri 를 hostname, filepath(filename), port로 파싱하기 
    uri = http://hostname:port/path 의 형태
  */
  parse_uri(uri, hostname, filepath, &port);

  /* 최종 서버에 전송될 http header 만들기 */
  build_the_header(endserver_http_header, hostname, filepath, port, &rio);

  /* 최종 서버에 연결하기 */
  end_serverfd = connect_endServer(hostname, port, endserver_http_header);
  if (end_serverfd<0) {
    printf("connection failed\n");
    return;
  }

  Rio_readinitb(&server_rio, end_serverfd);
  /* 최종 서버에 http header 'write' */
  Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

  /* 최종서버로부터 응답메세지 받고 그것을 클라이언트에 전송 */
  size_t n;
  while((n=Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) { //Rio_readlineb는 읽은 바이트 수를 반환
    printf("Proxy received %d bytes, then send\n", n);
    Rio_writen(connfd, buf, n); // 성공하면 쓰여진 바이트 수 반환
  }
  Close(end_serverfd);
}

/* 최종 서버에 전송될 헤더 만들기 */
void build_the_header(char * http_header, char * hostname, char * filepath, int port, rio_t *client_rio) {
  char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
  /* 요청 라인 */
  sprintf(request_hdr, "GET %s HTTP/1.0\r\n", filepath);
  /* get other request header for client rio and change it */
  while(Rio_readlineb(client_rio,buf,MAXLINE)>0)
    {
        if(strcmp(buf,"\r\n")==0) break;/*EOF*/

        if(!strncasecmp(buf,"HOST",strlen("HOST")))/*Host:*/
        {
            strcpy(host_hdr,buf);
            continue;
        }

        if(strncasecmp(buf,"Connection",strlen("Connection"))
                &&strncasecmp(buf,"Proxy-connection",strlen("Proxy-connection"))
                &&strncasecmp(buf,"User_agent",strlen("User_agent")))
        {
            strcat(other_hdr,buf);
        }
    }
    if(strlen(host_hdr)==0)
    {
        sprintf(host_hdr,"Host: %s\r\n",hostname);
    }
    sprintf(http_header,"%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            "Connection: close\r\n",
            "Proxy-Connection: close\r\n",
            user_agent_hdr,
            other_hdr,
            "\r\n");
    return ;
}
/*Connect toa the end server*/
inline int connect_endServer(char *hostname,int port,char *http_header){
    char portStr[100];
    sprintf(portStr,"%d",port);
    return Open_clientfd(hostname,portStr);
}


void parse_uri(char *uri,char *hostname,char *path,int *port)
{
  // http://hostname:port/path
    *port = 80; // 초기값 기본 80 으로 지정
    char* pos = strstr(uri,"//");

    pos = pos!=NULL? pos+2:uri;

    char*pos2 = strstr(pos,":");
    if(pos2!=NULL)  // : 있을때
    {   
        *pos2 = '\0';
        // pos = hostname\0port/path 의 형태
        sscanf(pos,"%s",hostname);
        sscanf(pos2+1,"%d%s",port,path);
    }
    else// : 없을때
    {   // pos = hostname/path
        pos2 = strstr(pos,"/"); // *pos2 = / 
        if(pos2!=NULL)
        {
            *pos2 = '\0';
            sscanf(pos,"%s",hostname);
            *pos2 = '/';
            sscanf(pos2,"%s",path);
        }
        else // path 가 아예 없을때
        {
            sscanf(pos,"%s",hostname);
        }
    }
    return;
}