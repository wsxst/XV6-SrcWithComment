// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
acquire(struct spinlock *lk)
{
  pushcli(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

  // The xchg is atomic.
  // It also serializes, so that reads after acquire are not
  // reordered before it. 

  // xchg是 x86的一条特殊指令，具有原子性
  // xchg 交换了内存中的一个字和一个寄存器的值：如果锁已被持有，lk->locked 已经为1，xchg 会返回1然后继续循环
  //                                         如果锁未被持有，lk->locked 交换为1，xchg 返回0，成功获得了锁，这时循环可以停止
  // 为什么要使用原子操作 xchg？
  // 许多处理器会通过指令乱序来提高性能：如果一个指令需要多个周期完成，处理器会希望这条指令尽早开始执行，这样就能与其他指令交叠，避免延误太久
  // 为了避免乱序可能造成的不确定性，xv6 使用稳妥的 xchg，这样就能保证不出现乱序
  
  while(xchg(&lk->locked, 1) != 0)
    ;

  // Record info about lock acquisition for debugging.
  lk->cpu = cpu;
  getcallerpcs(&lk, lk->pcs);
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->pcs[0] = 0;
  lk->cpu = 0;

  // The xchg serializes, so that reads before release are 
  // not reordered after it.  The 1996 PentiumPro manual (Volume 3,
  // 7.2) says reads can be carried out speculatively and in
  // any order, which implies we need to serialize here.
  // But the 2007 Intel 64 Architecture Memory Ordering White
  // Paper says that Intel 64 and IA-32 will not move a load
  // after a store. So lock->locked = 0 would work here.
  // The xchg being asm volatile ensures gcc emits it after
  // the above assignments (and after the critical section).
  xchg(&lk->locked, 0);

  popcli();
}

// Record the current call stack in pcs[] by following the %ebp chain.
void
getcallerpcs(void *v, uint pcs[])
{
  uint *ebp;
  int i;
  
  ebp = (uint*)v - 2;
  for(i = 0; i < 10; i++){
    if(ebp == 0 || ebp < (uint*)KERNBASE || ebp == (uint*)0xffffffff)
      break;
    pcs[i] = ebp[1];     // saved %eip
    ebp = (uint*)ebp[0]; // saved %ebp
  }
  for(; i < 10; i++)
    pcs[i] = 0;
}

// Check whether this cpu is holding the lock.
int
holding(struct spinlock *lock)
{
  return lock->locked && lock->cpu == cpu;
}


// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.

// 使用 pushcli和 popcli来屏蔽/释放中断
// pushcli和 popcli不仅包装了 cli 和 sti，还做了计数工作，这样就需要调用两次 popcli 来抵消两次 pushcli
// 如果代码中获得了两个锁，那么只有当两个锁都被释放后中断才会被允许，因为xv6规定：允许中断时不能持有任何锁
// 为什么允许中断时不能持有任何锁？中断造成并发，并发时持有锁可能导致死锁

void
pushcli(void)
{
  int eflags;
  
  eflags = readeflags();
  cli();                               // x86 屏蔽中断的指令
  if(cpu->ncli++ == 0)                 // 首次pushcli时进入if分支
    cpu->intena = eflags & FL_IF;      // 记录首次pushcli前，eflag寄存器中的中断位情况
                                       // 为什么要记录?
}

void
popcli(void)
{
  if(readeflags()&FL_IF)
    panic("popcli - interruptible");
  if(--cpu->ncli < 0)                  // 抵消pushcli所加的次数
    panic("popcli");
  if(cpu->ncli == 0 && cpu->intena)    // 当没有任何锁时，并且首次pushcli前中断为开的情况，才允许中断
    sti();
}

