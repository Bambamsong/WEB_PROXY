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
// 캐시에서 추가된 함수
void cache_init();
int cache_find(char *url);
void cache_uri(char *uri, char *buf);

void readerPre(int i);
void readerAfter(int i);

typedef struct 
{
  char cache_obj[MAX_OBJECT_SIZE];
  char cache_url[MAXLINE];
  int LRU;            // least recently used 가장 최근에 사용한 것의 우선순위를 뒤로 미움 (캐시에서 삭제할 때)
  int isEmpty;        // 이 블럭에 캐시 정보가 들었는지 empty인지 아닌지 체크

  int readCnt;        // count of readers
  sem_t wmutex;       // 접근 막기 by mutex 이진 세마포어 타입. 1: 사용가능, 0: 사용 불가능
  sem_t rdcntmutex;  
}cache_block;         // 캐쉬블럭 구조체

typedef struct
{
  cache_block cacheobjs[10];  // ten cache blocks
  int cache_num;              // 캐시(10개) 넘버 부여
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
  Signal(SIGPIPE, SIG_IGN); // 소켓 통신 중 연결이 종료되었을 때 프로그램이 비정상적으로 종료되는 것을 방지
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

  char url_store[100];
  strcpy(url_store, uri); // doit으로 받아온 connfd가 들고있는 uri를 넣어준다

  int cache_index;
  // cache_index정수 선언, url_store에 있는 인덱스를 확인.
  if ((cache_index=cache_find(url_store)) != -1) { // url_store에 들어있는 캐쉬인덱스에 접근을 했다는 것 
    readerPre(cache_index); // 캐시 뮤텍스를 풀어줌 (열어줌 0->1)
    Rio_writen(connfd, cache.cacheobjs[cache_index].cache_obj, strlen(cache.cacheobjs[cache_index].cache_obj));
    // 캐시에서 찾은 값을 connfd에 쓰고, 캐시에서 그 값을 바로 보내게 됨
    readerAfter(cache_index); // 닫아줌 1->0
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
  char cachebuf[10];
  int sizebuf = 0;
  size_t n; // 캐시에 없을 때 찾아주는 과정
  while ((n=Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {

    sizebuf += n;
    /* proxy거쳐서 서버에서 response오는데, 그 응답을 저장하고 클라이언트에 보냄 */
    if (sizebuf < 10) // 작으면 response 내용을 저장
      strcat(cachebuf, buf); // cachebuf에 but(response값) 다 이어붙혀놓음(캐시내용)
    Rio_writen(connfd, buf, n);
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


// 이진 세마포어 방식 사용해보자!
// LRU(Least Recently Used) 가장 오랫동안 캐시힛? 되지 않은 것 삭제
// 캐시구조체 구성요소 : 캐쉬사이즈, 캐쉬에 저장될 url, 이진 세마포어(숫자)
// cache_init : 캐시를 초기화하는 함수 캐시 객체를 초기화하고 세마포어를 설정
void cache_init() {
  cache.cache_num = 0; // 맨 처음이니까
  int i;
  for (i=0; i<10; i++) {
    cache.cacheobjs[i].LRU = 0; // LRU : 우선 순위를 미는 것, 처음이니까 0
    cache.cacheobjs[i].isEmpty = 1; // 1이 비어있다는 뜻

    // Sem_init : 세마포어 함수 
    // 첫 번째 인자 : 초기화할 세마포어의 포인터
    // 두 번째 인자 : 0 - 쓰레드들끼리 세마포어 공유, 그 외 - 프로세스 간 공유
    // 세 번째 인자 : 초기 값
    // 뮤텍스 만들 포인터의 값을 0으로 설정 : 0을 써서 쓰레드에 쓰는거라고 표시(초기값은 1이니까 0으로 해야함)
    Sem_init(&cache.cacheobjs[i].wmutex, 0, 1); // wmutex : 한번에 한 쓰레드만이 캐시에 쓰기 작업 수행 가능
    Sem_init(&cache.cacheobjs[i].rdcntmutex, 0, 1); // read count mutex : 읽기작업이 캐시에 동시에 접근할 때 이 뮤텍스를 사용하여 캐시 보호
    cache.cacheobjs[i].readCnt = 0; // read count를 0으로 놓고 init을 끝냄
  }
}

/* cache_find : 해당 uri가 캐쉬에 존재하는지 확인하는 작업 ---------------------------*/
int cache_find(char *url) {
  int i;
  for (i=0; i<10; i++) {
    // 읽기 작업 시작
    readerPre(i);
    // 캐시가 비어 있지 않고, URL이 일치하는 경우
    if ((cache.cacheobjs[i].isEmpty == 0) && (strcmp(url, cache.cacheobjs[i].cache_url) == 0))
      break;
    // 읽기 작업 완료  
    readerAfter(i);
  }
  // 캐시를 찾지 못한 경우
  if (i >= 10)
    return -1;
  // 캐시를 찾은 경우 해당 인덱스 반환
  return i;
}
/*
  P : 세마포어를 잠그는 기능
  V : 세마포어를 해제하는 기능
*/
void readerPre(int i) { // i = 해당인덱스
  // 읽기 카운트 뮤텍스 잠금
  P(&cache.cacheobjs[i].rdcntmutex);
  // 읽기 카운트 증가
  cache.cacheobjs[i].readCnt++; 
  // 읽기 카운트가 1이면 쓰기 뮤텍스 잠금
  if (cache.cacheobjs[i].readCnt == 1)
    P(&cache.cacheobjs[i].wmutex);
  // 읽기 카운트 뮤텍스 해제 
  V(&cache.cacheobjs[i].rdcntmutex); 
}

void readerAfter(int i) {
  // 읽기 카운트 뮤텍스 잠금
  P(&cache.cacheobjs[i].rdcntmutex);
  // 읽기 카운트 감소
  cache.cacheobjs[i].readCnt--;
  // 읽기 카운트가 0이 되면 쓰기 뮤텍스 해제
  if (cache.cacheobjs[i].readCnt == 0)
    V(&cache.cacheobjs[i].wmutex);
  // 읽기 카운트 뮤텍스 해제
  V(&cache.cacheobjs[i].rdcntmutex);
}


/* 해당 uri를 캐쉬에 저장하는 작업 ------------------------------------- */

int cache_eviction() { // 캐시 내보내기
  int min = 7777;         // LRU값을 비교하기 위해 설정
  int minindex = 0;       // LRU 값이 가장 낮은 인덱스를 저장하는 변수 설정
  int i;
  for (i=0; i<10; i++) {  // 캐시 객체 수만큼 반복
    readerPre(i);         // 현재 캐시에 대한 읽기 작업 시작
    if (cache.cacheobjs[i].isEmpty == 1) {// 비어있는지 확인
      minindex = i;       // 비어있다면 최소 LRU 값을 갖는 캐시 객체의 인덱스를 i로 설정
      readerAfter(i);     // 읽기 작업 완료
      break;
    }
    if (cache.cacheobjs[i].LRU < min) { //현재 캐시 객체의 LRU 값이 현재까지의 최소 LRU 값보다 작은지 확인
      minindex = i;
      min = cache.cacheobjs[i]. LRU;
      readerAfter(i);
      continue;
    }
    readerAfter(i);
  }
  return minindex; // LRU 값이 가장 작은 캐시 객체의 인덱스를 반환
}

// 쓰기 작업 시작
void writePre(int i) { // 해당 캐시 객체의 뮤텍스 잠금 -> 쓰기 작업 진행되는 동안 다른 스레드 수정 못하게 보호
  P(&cache.cacheobjs[i].wmutex);
}
// 쓰기 작업 완료
void writeAfter(int i) {
  V(&cache.cacheobjs[i].wmutex);
}

void cache_LRU(int index) { //index 보다 작은 인덱스에 대한 LRU 업데이트
  int i;
  for (i=0; i<index; i++) {
    writePre(i);
    if (cache.cacheobjs[i].isEmpty == 0 && i != index)
      cache.cacheobjs[i].LRU--;
    writeAfter(i);
  }
  i++;
  for (i; i<10; i++) {
    writePre(i);
    if (cache.cacheobjs[i].isEmpty == 0 && i != index) {
      cache.cacheobjs[i].LRU--; // 이미 찾은 애는 7777로 보냈으니 그 앞에있는 애들 인덱스 -1씩 내려준다
    }
    writeAfter(i);
  }
}
void cache_uri(char *uri, char *buf) {
  // 캐시에서 비어있는 블록을 찾아 반환하는 함수
  int i = cache_eviction();
  // 쓰기 작업 시작
  writePre(i);

  // 캐시 객체에 정보 복사
  strcpy(cache.cacheobjs[i].cache_obj, buf);
  strcpy(cache.cacheobjs[i].cache_url, uri);
  cache.cacheobjs[i].isEmpty = 0;
  cache.cacheobjs[i].LRU = 7777; // 가장 최근에 했으니 우선순위 7777
  cache_LRU(i); // LRU정책에 따라 캐시 블록 업데이트하는 함수

  // 쓰기 작업 완료
  writeAfter(i);
}