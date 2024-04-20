#include <stdio.h>


/* argc는 프로그램이 실행될 때 전달되는 명령행 인수(argument count)의 개수를 나타내는 변수 
   이 변수는 'main' 함수의 첫 번째 매개변수로 선언되며, 정수형으로 선언
   명령행 인수(argument)란 프로그램이 실행될 때 프로그램 이름을 제외한 추가적인 인수 
   ./program argument1 argument2 
   
   argc = 3
   argv[0] = ./program
   argv[1] = "argument1"
   argv[2] = "argument2"

   strtoul : 문자열을 부호 없는 long으로 변환하는 역할
*/

/* 16진수 인자를 16비트 네트워크 바이트 오더로 변환하고 그 결과를 출력하는 프로그램 */
int main(int argc, char * argv[]){
    /* 입력되는 값이 한개 이하일 경우 '아무것도 입력되지 않았습니다!' */
    if (argc < 2){
        printf("nothing inputed");
        return -1;
    }
    /* 입력되는 두번째 값부터 변환 */
    unsigned long change = strtoul(argv[1], NULL, 16);
    printf("adress : %p\n", change);
    printf("adress : %x\n", change);
    printf("adress : %d\n", change);
    return 0;
}