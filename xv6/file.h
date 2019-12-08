//最高层的，也是进程所看到的，一个打开文件用file来描述
//即文件描述符。
//注意，file与inode各有一个ref。
struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;//文件类型
  int ref; // reference count，多少个进程引用了这个文件
  char readable;      //可读
  char writable;      //可写
  struct pipe *pipe;  //管道文件
  struct inode *ip; //i节点
  uint off; //读写文件的偏移指针
};

//存储在磁盘中inode的结构体dinode，在内存中的存在形式
// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count 统计有多少个指针指向它。如果 ref 变为0，内核就会丢掉这个 i 节点
  int flags;          // I_BUSY, I_VALID

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};
#define I_BUSY 0x1
#define I_VALID 0x2

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
