/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);


/* Tiny main 루틴 */
// argc : 인자의 개수(argument count), argv : 인자의 배열(argument vector)
// 따라서 첫번째 인자는 프로그램 이름 자체
int main(int argc, char **argv) {
  int listenfd, connfd; 
  char hostname[MAXLINE], port[MAXLINE]; // 문자열을 저장하기 위한 문자배열 
  socklen_t clientlen;                   // 클라이언트 주소 구조체 크기를 나타내는 변수
  struct sockaddr_storage clientaddr;    // struct sockaddr_storage : 가장 일반적이 소켓 주소 구조체(어떤 종류의 주소를 기대할지 알 수 없을때 유용)

  /* 포트 번호 입력받지 못했을 때 */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* 입력이 제대로 이루어지면 */
  listenfd = Open_listenfd(argv[1]);      //  인자로 받은 PORT(argv[1])에 연결 요청을 받을 준비가 된 듣기 식별자를 리턴
  while (1) {                             //  while(1) : C에서 무한루프를 생성하는 일반적인 방법 
    clientlen = sizeof(clientaddr);
    // 서버는 accept함수를 호출해서 클라이언트로부터의 연결 요청을 기다림
    // client 소켓은 server 소켓의 주소를 알고 있으니까
    // client에서 server로 넘어올 때 add정보를 가지고 올 것이라고 가정
    // accept 함수는 연결되면 식별자 connfd를 리턴
    connfd = Accept(listenfd, (SA *)&clientaddr,  // 듣기식별자, 소켓 주소 구조체의 주소, 주소(소켓 구조체)
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, // 여기서 hostname과 port는 버퍼임
                0); // 반복적으로 연결 요청 접수
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit , 트랜잭션 수행
    Close(connfd);  // line:netp:tiny:close, 자신쪽의 연결 닫음
  }
}

/* 한 개의 HTTP 트랜잭션을 처리 */
void doit(int fd) // 여기서 fd는 connfd !
{
    int is_static;
    struct stat sbuf; // 파일정보를 저장하는 구조체
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE] , cgiargs[MAXLINE];
    rio_t rio;

    /* 요청 라인을 읽기 */
    Rio_readinitb(&rio, fd); // rio_t 구조체를 초기화하여 rio변수가 fd 에서 안전하게 읽을수 있도록 함
    Rio_readlineb(&rio, buf, MAXLINE); // 1 rio_t구조체에서 데이터를 읽는다. 2 한 줄씩 읽는다. 3 읽을줄을 버퍼에 저장한다 4. 버퍼에 저장된 바이트 수를 반환한다.
    printf("Reqeust headers: \n");
    printf("%s", buf); // 요청된 라인을 printf로 보여줌 (최초 요청라인: GET / HTTP/1.1)
    sscanf(buf, "%s %s %s", method, uri, version); // 문자배열 buf에서 데이터를 읽는다. 읽은것을 각각의 배열에 저장
    if (strcasecmp(method, "GET")){ // method 문자열이 "GET" 과 일치하는지 확인
      clienterror(fd, method, "501", "Not implemented", "Tiny does not implement thes method");
      return;
    }
    read_requesthdrs(&rio); // 'rio'는 RIO패키지를 사용하여 안전하게 한 줄씩 읽어들이는 구조체 , 이 구조체를 사용하여 요청 헤더를 읽어옴

    /* GET요청으로부터 URI를 분석하고 추출 */
    is_static = parse_uri(uri, filename, cgiargs); // 1 이면 static
      // uri : 파싱할 URI를 나타내는 문자열
      // filename : 추출된 파일이름이 저장될 문자열
      // cgiargs : 추출된 cgi인자가 저장될 문자열
    if (stat(filename, &sbuf) < 0) { // stat : 파일의 상태를 검사하는 함수(성공하면 0 실패하면 -1), filename : 파일의 경로를 나타내는 문자열 , sbuf : 파일의 상태 정보가 저장될 구조체에 대한 포인터
      clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
      return;
    }

    /* 정적 콘텐츠를 서빙할지 동적 컨텐츠를 서빙할지 여부를 확인하는 조건문 */
    if (is_static) { /* 정적 콘텐츠 서빙 */
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // S_ISREG : 파일의 모드를 확인(일반모드인지), S_IXUSR : 실행가능한 파일 여부 확인, 읽기 권한(S_IRUSR)을 가지고 있는지 판별
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
        return;
      }
      serve_static(fd, filename, sbuf.st_size); // 정적 파일을 클라이언트에게 제공하는 함수
    }
    else { /* 동적 콘텐츠 서빙 */
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
        return;
      }
      serve_dynamic(fd, filename, cgiargs); // 동적 파일을 클라이언트에게 제공하는 함수
    }

}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];
  // buf : HTTP 요청이나 응답을 저장하는 데 사용
  // body : 응답의 본문을 저장하는 데 사용

  /* sprintf 함수를 사용하여 HTML형식의 오류 메세지 생성 to body */
  sprintf(body, "<html><title>Tiny Error</title>");         // html 문서의 시작부분
  sprintf(body, "%s<body bgcolor =""ffffff"">\r\n", body);  
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Web server</em>\r\n", body);
  // sprintf에서 body를 계속 사용하는 이유는 호출될 때마다 이전 호출에서 저장된 것을 잃어버리기 때문

  /* print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); // 데이터(buf)를 소켓(fd)에 정확히(strlen) 바이트 쓴다 , 데이터를 클라이언트에게 보내기 위해 사용
  sprintf(buf, "Content-tpye: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp) {
  
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE); // rp로 부터 데이터를 읽어서 buf에 저장
  // 한줄을 읽어들인다('\n'만나면 break되는 식)
  // strcmp(str1,str2) 같은 경우 0을 반환 -> 이 경우만 탈출
  // buf가 '\r\n'이 되는 경우는 모든 줄을 읽고 나서 마지막 줄에 도착한 경우이다.
  // 헤더의 마지막 줄은 비어있다
  while(strcmp(buf, "\r\n")) { // 요청헤더가 끝날때까지 계속 헤더를 읽는다
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}


// Tiny는 정적 컨텐츠를 위한 홈 디렉토리가 자신의 현재 디렉토리이고,
// 실행파일의 홈 디렉토리는 /cgi-bin이라고 가정한다.

/* parse_uri 함수 */
int parse_uri(char *uri, char *filename, char *cgiargs) {
  
  char *ptr;
  /* 정적 콘텐츠 */
  if (!strstr(uri, "cgi-bin")){ // uri 에서 cgi-bin이 없으면 실행해라
    strcpy(cgiargs, "");        // cgiargs에 공백복사 = cgiargs 버퍼 초기화
    strcpy(filename, ".");      // filename에 . 복사 = 현재 디렉토리를 나타냄
    strcat(filename, uri);      // filename에 uri 이어서 붙혀준다.
    if (uri[strlen(uri)-1] == '/') // 문자열의 마지막 문자 가져옴, 그값이 / 이면 = uri가 /로 끝나는 경우
        strcat(filename, "home.html"); // home.html을 파일이름에 추가 -> 11.10과제 adder.html로 변경
    return 1;
  }
  /* 동적 콘텐츠 */
  // uri 예시 : dynamic: /cgi-bin/adder?first=1213&second=1232
  else {
    ptr = index(uri, '?'); // ? 의 위치를 가리킨다
    if (ptr) {
      strcpy(cgiargs, ptr+1); // ? 다음위치부터 가리켜야하니까 ptr+1
      *ptr = '\0';            // ? 위치에 \0 을 넣는다
    }
    else {
      strcpy(cgiargs, "");
    }
    //윗 과정 끝나면
    strcpy(filename, ".");
    strcat(filename, uri); //이어붙이는 함수, 파일네임에 uri 이어 붙이기(이때 ? => \0로 변환되어있으
    return 0;
  }
}
/* 정적 컨텐츠를 클라이언트에 서비스 */
void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetpye[MAXLINE], buf[MAXBUF];

  /* 클라이언트에 응답 헤더 보내기 */
  get_filetype(filename, filetpye);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetpye);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers: \n");
  printf("%s", buf);

  /* 클라이언트에 응답 바디 보내기 */
  srcfd = Open(filename, O_RDONLY, 0); // filename 이라는 파일을 읽기 전용으로 연다
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // srcfd라는 파일디스크립터를 메모리 매핑할 예정이다
  srcp = (char *)Malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd); // 해당파일디스크립터를 닫는 행위. 성공적으로 닫으면 0 반환
  Rio_writen(fd, srcp, filesize);
  // Munmap(srcp, filesize); // 위에서 메모리 매핑 해준 파일디스크립터를 매핑해제 해주는행위
  free(srcp);
}

/* 파일 이름에서 파일 유형(확장자)찾기 */
// ex) 텍스트파일, 이미지파일, html파일 등
// 파일네임을 가지고 확인 -> 파일타입에 저장
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html")) strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif")) strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png")) strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg")) strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4")) strcpy(filetype, "video/mp4");
  else strcpy(filetype, "text/plain");
}

/* 동적컨텐츠를 클라이언트에 제공 */
void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL }; // 포인터배열(포인터를 인자로 갖는 배열)의 원소는 한개이고 그 값은 NULL이다

  // 클라이언트에 성공을 알려주는 응답 라인을 보내는 것으로 시작
  // Rio_writen 을 하게 되면 buf가 소실됌! 그래서 다시 데이터를 넣어줌
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child */
    /* Real server would set all CGI vars here */
    // 이때 부모 프로세스는 자식의 PID(Process ID)를, 자식 프로세스는 0을 반환받는다.

    // Real server would set all CGI vars here(실제 서버는 여기서 다른 CGI 환경변수도 설정)

    // QUERY_STRING 환경변수를 요청 URI의 CGI 인자들을 넣겠소. 
    // 세 번째 인자는 기존 환경 변수의 유무에 상관없이 값을 변경하겠다면 1, 아니라면 0
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}
