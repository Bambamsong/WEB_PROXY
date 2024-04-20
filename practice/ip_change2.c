#include "csapp.h"

/*  gcc -o ip_change2 ip_change2.c csapp.o -lpthread 의미 
    1. -o ip_change2: 컴파일된 실행 파일의 이름을 ip_change2로 설정합니다. 즉, 컴파일된 프로그램의 실행 파일 이름이 ip_change2가 됩니다. 
    2. ip_change2.c: 컴파일할 C 소스 파일의 이름입니다. 이 프로그램의 메인 소스 코드가 포함되어 있습니다.
    3. csapp.o: 컴파일러에 대해 알려주는 외부 객체 파일입니다. 이는 csapp.o라는 이름의 라이브러리나 객체 파일이 프로젝트 폴더에 있다고 가정합니다. 이 파일에는 csapp.h에 선언된 함수들의 정의가 포함되어 있을 것입니다.
    4. -lpthread: 컴파일러가 링커에게 pthread 라이브러리를 링크하도록 지시합니다. -lpthread는 POSIX 스레드 라이브러리를 사용할 때 필요한 링크 옵션입니다.
    
    따라서 이 명령은 ip_change2.c라는 소스 파일을 컴파일하여 ip_change2라는 실행 파일을 생성하고, 이 때 csapp.o 파일을 링크하며 pthread 라이브러리를 사용
*/

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