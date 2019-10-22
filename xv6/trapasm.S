# 建立trap frame，调用trap（tf）函数，并在调用完trap函数后恢复现场
#include "mmu.h"

# vectors.S sends all traps here.
.globl alltraps
alltraps:
  # Build trap frame.这个结构包含了陷入程序完成之后处理器恢复用户进程寄存器值的所有信息
  pushl %ds
  pushl %es
  pushl %fs
  pushl %gs
  pushal # 这里应该是把所有的32位通用寄存器压栈
  
  # Set up data and per-cpu segments.
  # 让寄存器ds,es中含有指向数据段的指针值
  movw $(SEG_KDATA<<3), %ax
  movw %ax, %ds
  movw %ax, %es
  # 让寄存器fs,gs中含有指向per-cpu数据段的指针值
  movw $(SEG_KCPU<<3), %ax
  movw %ax, %fs
  movw %ax, %gs

  # Call trap(tf), where tf=%esp
  pushl %esp # 将esp中的值作为参数传给trap
  call trap # 调用trap函数
  addl $4, %esp # esp的值+4,4是个立即数。由于内存布局中栈是由高地址向低地址扩展的，因此将esp的值加4相当于将栈顶元素弹出，此时esp的值指向标签trapret

  # Return falls through to trapret...
.globl trapret
trapret:
  popal # 所有通用寄存器出栈
  # 以下4行，依次出栈
  popl %gs
  popl %fs
  popl %es
  popl %ds
  addl $0x8, %esp  # trapno and errcode 陷入号和错误代码出栈，原理同上
  iret # 回用户空间
