// 互斥锁，区别于自旋锁

// cr20191106
// 互斥锁和自旋锁都能实现互斥的功能
// 互斥锁属于sleep-waiting，若调用者检测到锁已被其他线程占用，则会阻塞
// 自旋锁属于busy-waiting，调用者会循环检查锁是否被占用直到自己获得锁，不会阻塞（体现在acquire函数）
// 自旋锁的缺点：1.死锁：一个函数在持有锁时调用另一个需要获得该锁的函数，会造成死锁
//              2.降低cpu使用效率
// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?
                     // 是否开启了锁的使用?  --eg: kmem的锁在kinit2函数开启使用

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.
};

