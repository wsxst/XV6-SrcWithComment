# Initial process execs /init.
# 初始化进程执行

#include "syscall.h"
#include "traps.h"


# exec(init, argv)
.globl start
start:
  pushl $argv
  pushl $init
  pushl $0  // where caller pc would be
  movl $SYS_exec, %eax
  int $T_SYSCALL

# for(;;) exit();
exit: # 这种就属于标签
  movl $SYS_exit, %eax
  int $T_SYSCALL
  jmp exit # 跳转到exit标签

# char init[] = "/init\0";
init:
  .string "/init\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:
  .long init
  .long 0

