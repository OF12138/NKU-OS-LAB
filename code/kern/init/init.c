/*
C语言编写的内核入口点。主要包含kern_init()函数，从kern/init/entry.S跳转过来完成其他初始化工作。
*/

#include <stdio.h>
#include <string.h>
#include <sbi.h>
int kern_init(void) __attribute__((noreturn)); // to tell the compiler that this func wont return . 

int kern_init(void) 
{
    extern char edata[], end[]; //这里声明的两个符号由链接器定义，分别指向.data段结束和.bss段结束
    memset(edata, 0, end - edata); // 清除.bss段：由于内核没有标准库，memset需要我们自己实现

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);
   while (1)
        ;
}

