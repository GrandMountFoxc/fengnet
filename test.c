#include <stdio.h>

int main()
{
    char str[4];
    while(scanf("%s", str)!=0){
        char a=str[0], b=str[1], c=str[2];
        char d;
        // 排序
        if(a>b)
        {
            d=a;
            a=b;
            b=d;
        }
        if(a>c)
        {
        d=a;
        a=c;
        c=d;
        }
        if(b>c)
        {
            d=b;
            b=c;
            c=d;
        }
        printf("%c %c %c\n", a, b, c);
    }
   
   return 0;
}



int length(char* str){
    int cnt = 0;
    while(str[cnt]!='\0')
        cnt++;
    return cnt;
}

void mySort(char* str){
    int n = length(str);
    for(int i=0;i<n;++i){
        for(int j=i+1;j<n;++j){
            if(str[i]>str[j]){
                char temp = str[i];
                str[i] = str[j];
                str[j] = temp;
            }
        }
    }
}

void myPrint(char* str){
    int cnt = 0;
    while(str[cnt]!='\0'){
        printf("%c ", str[cnt]);
        cnt++;
    }
    printf("\n");
}