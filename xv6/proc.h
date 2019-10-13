// Segments in proc->gdt.
// 全局描述符里面总共有7段
#define NSEGS     7

// Per-CPU state
/*
 * 这个结构体设计目的是存储CPU状态
 */
struct cpu {
  uchar id;                    // Local APIC ID; index into cpus[] below
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  
  // Cpu-local storage variables; see below
  struct cpu *cpu;
  struct proc *proc;           // The currently-running process.
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// Per-CPU variables, holding pointers to the
// current cpu and to the current process.
// The asm suffix tells gcc to use "%gs:0" to refer to cpu
// and "%gs:4" to refer to proc.  seginit sets up the
// %gs segment register so that %gs refers to the memory
// holding those two variables in the local cpu's struct cpu.
// This is similar to how thread-local variables are implemented
// in thread libraries such as Linux pthreads.
/*
 * 这一块没看懂啥意思……？
 */
extern struct cpu *cpu asm("%gs:0");       // &cpus[cpunum()]
extern struct proc *proc asm("%gs:4");     // cpus[cpunum()].proc

//PAGEBREAK: 17（这个啥意思？）
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// 【Contexts are stored at the bottom of the stack they
// describe】（这句话啥意思？）; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates（操控） it.
/*
 * 这里其实定义了内核进行上下文切换需要用到的几个寄存器
 */
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

/*
 * 枚举类型，枚举了进程的6种状态：UNUSED未使用态、EMBRYO初始态、SLEEPING等待态、RUNNABLE就绪态、RUNNING运行态、ZOMBIE僵尸态。
 * 状态的含义如下：
 * UNUSED：进程未被创建（即PCB空闲）时的状态，我感觉这就是最开始的状态，而且这里给我的感觉是，PCB是一开始就存在的，只不过在系统请求创建进程时才会将空闲的PCB分配给这个进程；
 * EMBRYO：需要分配一个进程控制块且找到一个处于UNUSED状态的进程控制块时，把此进程控制块状态设置为要使用的状态；
 * SLEEPING：进程由于等待某资源等原因无法执行，进入睡眠状态，即等待态，我印象里这个时候就会把程序写入磁盘（不知道是不是和中级调度弄混了，中级调度的确是将暂时不运行的进程从内存写入外存）；
 * RUNNABLE：进程获得了除CPU之外的所有资源，处于可运行状态，即就绪态，这个时候会有一个就绪队列，处于就绪态的进程挨个获得CPU（无抢占的前提下）；
 * RUNNING：进程获得CPU，正在运行的状态，即执行态；
 * ZOMBIE：进程结束的状态。
 * 状态转换图见https://github.com/wsxst/XV6-SrcWithComment/blob/master/XV6%E8%BF%9B%E7%A8%8B%E7%8A%B6%E6%80%81%E8%BD%AC%E6%8D%A2%E5%9B%BE.vsdx，需要用visio打开。
 *
 * 这和Linux中进程状态有点区别。
 * Linux内核中定义了以下几种状态：
 * #define TASK_RUNNING 0
 * #define TASK_INTERRUPTIBLE 1
 * #define TASK_UNINTERRUPTIBLE 2
 * #define TASK_ZOMBIE 4
 * #define TASK_STOPPED 8
 * 其中：
 * TASK_RUNNING是就绪态，进程当前只等待CPU资源，其他资源已经全部到位。
 * TASK_INTERRUPTIBLE和TASK_UNINTERRUPTIBLE都是阻塞态，进程当前正在等待除CPU外的其他系统资源，两者的区别在于前者可以被信号唤醒，后者不可以。
 * TASK_ZOMBIE是僵尸态，进程已经结束运行，但是进程控制块尚未注销，个人感觉属于一个中间的一个临时状态。
 * TASK_STOPPED是挂起状态，主要用于调试目的，我理解的所谓挂起就是把当前程序存入磁盘，进程接收到SIGSTOP信号后会进入该状态，在接收到SIGCONT后又会恢复运行。
 */
enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
/* 
 * 使用这个结构体来维护一个进程的状态。
 * 其中最为重要的状态是进程的页表，内核栈，当前运行状态。
 */
struct proc {
  uint sz;                     //以B为单位，记录进程所占有的内存空间的大小
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process 进程在内核态的栈
  enum procstate state;        // Process state
  /*
   * volatile的本意是“易变的” 因为访问寄存器要比访问内存单元快的多,所以编译器一般都会作减少存取内存的优化，但有可能会读脏数据。当要求使用volatile声明变量值的时候，系统总是重新从它所在的内存读取数据，即使它前面的指令刚刚从该处读取过数据。
   * 精确地说就是，遇到这个关键字声明的变量，编译器对访问该变量的代码就不再进行优化，从而可以提供对特殊地址的稳定访问；如果不使用valatile，则编译器将对所声明的语句进行优化。
   */
  volatile int pid;            // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall 中断进程后，需要恢复进程继续执行所保存的寄存器内容
  struct context *context;     // swtch() here to run process 切换进程时，需要维护的硬件寄存器内容
  void *chan;                  // If non-zero, sleeping on chan 如果该变量值不为NULL，则表示进程睡眠态时所处于的睡眠队列（的头指针）
  int killed;                  // If non-zero, have been killed 如果该变量值不为0，则表示该进程已经被杀死了
  struct file *ofile[NOFILE];  // Open files 进程打开的文件数组
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging) 这个值也就调试时有用，我觉得作用是让开发者方便看到具体是哪个进程的行为
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
