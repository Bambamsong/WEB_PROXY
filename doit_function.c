#include <stdio.h>
#include <string.h>

/* sscanf */
// 문자열에서 데이터를 읽어 정수 변수에 저장 -> sscanf사용
int main(void) {
    
    char *str = "ABC 123";    // 문자열 "ABC 123"을 포인터 "str"에 할당
    int num = 0;              // 정수형 변수 'num'을 선언하고 0으로 초기화
    
    sscanf(str, "%*s %d", &num);    // 문자열 'str' 에서 데이터를 읽는다
    // "%*s %d" 서식 지정자는 문자열 str에서 문자열(%*s)을 무시하고
    // 공백 이후에 나오는 정수(%d)를 읽어 변수 num에 저장
    printf("%d\n", num);
}

/* strcasecmp */
// strcmp vs strcasecmp : strcmp는 대소문자 구분, strcasecmp는 대소문자 구분 x
// 두개의 문자열을 비교하는 함수 - 동일한 문자열일경우 0 반환
int main() {
    const char *str1 = "Hello";
    const char *str2 = "HELLO";

    int result = strcasecmp(str1, str2);

    if (result == 0)
        printf("두 문자열은 동일합니다.\n");
    else if (result < 0)
        printf("str1이 str2보다 사전적으로 앞에 나옵니다.\n");
    else
        printf("str1이 str2보다 사전적으로 뒤에 나옵니다.\n");

    return 0;
}

/* stat */
// 파일에 대한 정보를 가져오는데 사용
// 파일의 메타데이터를 검색하여 파일의 크기, 소유자, 권한 및 변경된 시간과 같은 정보를 확인 할 수 있음
#include <sys/stat.h>

int stat(const char *path, struct stat *buf);
// 성공할 경우 0 반환, 실패할 경우 -1 반환

/* sprintf */
// 서식화된 문자열을 생성하여 문자열로 저장하는 함수
    #include <stdio.h>

    int sprintf(char *str, const char *format, ...);
        // str : 문자열이 저장될 버퍼를 나타내는 문자열 포인터
        // format : 서식 지정자를 가진 서식화된 문자열
        // ... : 서식 지정자에서 사용되는 각각의 인수 값
    // 예시
    #include <stdio.h>

    int main() {
        char buffer[50];
        int number = 25;
        float value = 3.14;

        // 서식화된 문자열을 생성하여 buffer에 저장
        sprintf(buffer, "Number: %d, Value: %.2f", number, value);

        // 결과 문자열 출력
        printf("Resulting string: %s\n", buffer);

        return 0;
    }
    // 출력결과 : Resulting string: Number: 25, Value: 3.14

/* strstr */
// 문자열 내에서 하위 문자열을 찾는데 사용
// 첫번째 일치하는 지점의 포인터 반환, 없으면 NULL
#include <string.h>

char *strstr(const char *haystack, const char *needle);
    // haystack : 검색할 대상 문자열
    // needle : 대상 문자열 내에서 찾을 문자열


/* strcpy */
// 문자열을 복사하는 데 사용
// 버퍼가 반환!
#include <string.h>

char *strcpy(char *dest, const char *src);
    // dest : 복사된 문자열이 저장될 버퍼
    // src : 복사할 문자열
    // return dest


/* strcat */
// 두 번째 문자열을 첫 번째 문자열 뒤에 추가하는 데 사용
#include <string.h>

char *strcat(char *dest, const char *src);
    // dest : 문자열이 추가될 버퍼
    // src : 추가할 문자열

/* Mmap */
// 파일을 메모리 매핑하여 파일의 내용을 읽기 전용으로 메모리에 매핑할 때 사용
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, __off_t );
    // addr : 메모리 매핑을 위해 요청하는 주소. 보통 0이며 커널이 메모리 영역 자동 할당
    // length : 메모리 매핑할 파일의 크기
    // prot : 메모리 보호 방법
    // flags : 메모리 매핑 옵션 
    // fd : 메모리 매핑할 파일의 파일 디스크립터
    // offset : 파일 내의 오프셋


