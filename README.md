# 操作系统课程设计报告

由于在proj1的基础上进行修改会导致proj2出现难以解决的错误，我选择将两个proj单独实现
- proj1 见`pintost`
- proj2 见`pintosu`

## 1 实验项目介绍

### 1.1 实验项目总体要求

1. 配置pintos环境
2. Project 1: Threads
   1. Alarm Clock
   2. Priority Scheduling
   3. Advanced Scheduler

3. Project 2: User Programs
   1. Process Termination Messages
   2. Argument Passing
   3. System Calls


### 1.2 实验环境配置步骤

#### 1.2.1 运行环境配置

##### 安装Ubuntu18.04

方便起见，我使用VWware16创建了一个Ubuntu18.04的虚拟机，在上面编写和测试pintos。同时为了传输文件的便利，我还安装了VMware Tools，在虚拟机的主机间配置了一个共享文件夹 。

##### 安装必要的工具

- git：用来进行版本控制，以及提交代码到判题系统
- vim：Linux特有的一个文本编辑器
- gcc：GNU Compiler Collection，GNU编译器套件
- make：一个强大的程序维护工具，可以根据事先定义的文件间依赖关系及在其基础上应执行的操作，结合文件修改或建立的时间顺序，自动、有选择地完成产生新版本的必须操作
- build-essential：Ubuntu在缺省情况下，是没有C/C++编译环境的，但是其提供了一个build-essential软件包，用来配置编译c/c++所需要的软件包
- qemu：一个纯软件实现的虚拟化模拟器，用来跑pintos

安装需要的shell命令如下：

```shell
sudo apt-get install git
sudo apt-get install vim
sudo apt-get install gcc
sudo apt-get install make
sudo apt-get install build-essential
sudo apt-get install qemu
```

##### 下载和配置pintos

- 在pintos的git公开仓库中找到最新版本的文件，下载到Windows上的共享文件夹里。下载地址：https://pintos-os.org/cgi-bin/gitweb.cgi?p=pintos-anon;a=tree;h=refs/heads/master;hb=refs/heads/master

- 在Ubuntu中将共享文件夹里的pintos解压到主目录

  ```shell
  tar -zxvf pintos-anon-master-f685123.tar.gz -C ~/pintos
  ```

- 对pintos进行本地化配置

  - 打开`pintos/src/utils/pintos-gdb`，将变量`GDBMACROS`中的路径改写为自己的pintos路径
  
  - 打开`pintos/src/utils/Makefile`，将变量`LOADLIBES`改名为`LDLIBS`
  
  - 在当前路径执行`make`，编译`utils`
  
  - 打开`pintos/src/threads/Make.vars`，将第7行的变量`SIMULATOR`的值由`--bochs`改为`--qemu`
  
  - 在当前路径执行`make`，编译`threads`
  
  - 打开`pintos/src/utils/pintos`，将104行的`bochs`替换为`qemu`
  
  - 打开`~/.bashrc`，在最后一行加上
  
    ```shell
    export PATH=~/pintos/src/utils:$PATH
    ```
  
    从而添加环境变量
  
  - 重新打开终端，输入`source ~/.bashrc `使设置生效

### 1.3 FAQ

1. **Q**：函数、文件太多，如何快速查看其中的关系？

   **A**：推荐使用静态代码分析工具*understand*，其集成代码编辑器、代码跟踪器、代码分析器于一体，能够可视化表示函数调用结构，还可以在整个工程内快速查找函数调用情况，使源码阅读和理解的过程事半功倍。

## 2 Project 1: Threads

### 2.1 pintos线程管理框架

pintos原生的线程结构如下：

```c
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };
```

源码中已经实现了（目前结构下的）线程的创建、初始化、调度、休眠，以及让出CPU等功能。

##### 存在的问题

- 线程的休眠实际上使用忙等待实现的，CPU利用率低
- *thread_yield*中线程直接插入就绪队列末尾，优先级形同虚设
- 只支持整数运算

### 2.2 主要函数功能及实现流程

下面介绍pintos源码中关于线程的主要函数功能及实现流程：

- *init_thread*

  ```c
  /* Does basic initialization of T as a blocked thread named
     NAME. */
  static void
  init_thread (struct thread *t, const char *name, int priority)
  {
    enum intr_level old_level;
  
    ASSERT (t != NULL);
    ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT (name != NULL);
  
    memset (t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy (t->name, name, sizeof t->name);
    t->stack = (uint8_t *) t + PGSIZE;
    t->priority = priority;
    t->magic = THREAD_MAGIC;
  
    old_level = intr_disable ();
    list_push_back (&all_list, &t->allelem);
    intr_set_level (old_level);
  }
  ```

  对线程进行初始化，并将其放入用于记录所有线程的*all_list*队列中。

- *thread_create*

  ```c
  /* Creates a new kernel thread named NAME with the given initial
     PRIORITY, which executes FUNCTION passing AUX as the argument,
     and adds it to the ready queue.  Returns the thread identifier
     for the new thread, or TID_ERROR if creation fails.
  
     If thread_start() has been called, then the new thread may be
     scheduled before thread_create() returns.  It could even exit
     before thread_create() returns.  Contrariwise, the original
     thread may run for any amount of time before the new thread is
     scheduled.  Use a semaphore or some other form of
     synchronization if you need to ensure ordering.
  
     The code provided sets the new thread's `priority' member to
     PRIORITY, but no actual priority scheduling is implemented.
     Priority scheduling is the goal of Problem 1-3. */
  tid_t
  thread_create (const char *name, int priority,
                 thread_func *function, void *aux) 
  {
    struct thread *t;
    struct kernel_thread_frame *kf;
    struct switch_entry_frame *ef;
    struct switch_threads_frame *sf;
    tid_t tid;
  
    ASSERT (function != NULL);
  
    /* Allocate thread. */
    t = palloc_get_page (PAL_ZERO);
    if (t == NULL)
      return TID_ERROR;
  
    /* Initialize thread. */
    init_thread (t, name, priority);
    tid = t->tid = allocate_tid ();
  
    /* Stack frame for kernel_thread(). */
    kf = alloc_frame (t, sizeof *kf);
    kf->eip = NULL;
    kf->function = function;
    kf->aux = aux;
  
    /* Stack frame for switch_entry(). */
    ef = alloc_frame (t, sizeof *ef);
    ef->eip = (void (*) (void)) kernel_thread;
  
    /* Stack frame for switch_threads(). */
    sf = alloc_frame (t, sizeof *sf);
    sf->eip = switch_entry;
    sf->ebp = 0;
  
    /* Add to run queue. */
    thread_unblock (t);
  
    return tid;
  }
  ```

  - 为新线程分配空间
  - 为新线程分配线程标识符并初始化
  - 为需要的栈分配空间并初始化
  - 加入就绪队列等待调度

- *thread_unblock*

  ```c
  /* Transitions a blocked thread T to the ready-to-run state.
     This is an error if T is not blocked.  (Use thread_yield() to
     make the running thread ready.)
  
     This function does not preempt the running thread.  This can
     be important: if the caller had disabled interrupts itself,
     it may expect that it can atomically unblock a thread and
     update other data. */
  void
  thread_unblock (struct thread *t) 
  {
    enum intr_level old_level;
  
    ASSERT (is_thread (t));
  
    old_level = intr_disable ();
    ASSERT (t->status == THREAD_BLOCKED);
    list_push_back (&ready_list, &t->elem);
    t->status = THREAD_READY;
    intr_set_level (old_level);
  }
  ```

  将一个阻塞态（THREAD_BLOCKED）的线程放入就绪（THREAD_READY）队列中。

- *thread_block*

  ```c
  /* Puts the current thread to sleep.  It will not be scheduled
     again until awoken by thread_unblock().
  
     This function must be called with interrupts turned off.  It
     is usually a better idea to use one of the synchronization
     primitives in synch.h. */
  void
  thread_block (void) 
  {
    ASSERT (!intr_context ());
    ASSERT (intr_get_level () == INTR_OFF);
  
    thread_current ()->status = THREAD_BLOCKED;
    schedule ();
  }
  ```

  将线程的状态置为阻塞态，并调用*schedule*进行线程的调度。

- *schedule*

  ```c
  /* Schedules a new process.  At entry, interrupts must be off and
     the running process's state must have been changed from
     running to some other state.  This function finds another
     thread to run and switches to it.
  
     It's not safe to call printf() until thread_schedule_tail()
     has completed. */
  static void
  schedule (void) 
  {
    struct thread *cur = running_thread ();
    struct thread *next = next_thread_to_run ();
    struct thread *prev = NULL;
  
    ASSERT (intr_get_level () == INTR_OFF);
    ASSERT (cur->status != THREAD_RUNNING);
    ASSERT (is_thread (next));
  
    if (cur != next)
      prev = switch_threads (cur, next);
    thread_schedule_tail (prev);
  }
  ```

  找到正在要结束运行（running_thread ()->status != THREAD_RUNNING）的线程，以及下一个要运行的线程，二者切换。

- *thread_yield*

  ```c
  /* Yields the CPU.  The current thread is not put to sleep and
     may be scheduled again immediately at the scheduler's whim. */
  void
  thread_yield (void) 
  {
    struct thread *cur = thread_current ();
    enum intr_level old_level;
    
    ASSERT (!intr_context ());
  
    old_level = intr_disable ();
    if (cur != idle_thread) 
      list_push_back (&ready_list, &cur->elem);
    cur->status = THREAD_READY;
    schedule ();
    intr_set_level (old_level);
  }
  ```

  找到正在运行的线程，将其放到就绪队列的末尾，再调用*schedule*执行下一个线程。

- *thread_start*

  ```c
  /* Starts preemptive thread scheduling by enabling interrupts.
     Also creates the idle thread. */
  void
  thread_start (void) 
  {
    /* Create the idle thread. */
    struct semaphore idle_started;
    sema_init (&idle_started, 0);
    thread_create ("idle", PRI_MIN, idle, &idle_started);
  
    /* Start preemptive thread scheduling. */
    intr_enable ();
  
    /* Wait for the idle thread to initialize idle_thread. */
    sema_down (&idle_started);
  }
  ```

  创建空闲线程*idle*，并开启中断从而启动抢占的线程调度。
  
- *next_thread_to_run*

  ```c
  /* Chooses and returns the next thread to be scheduled.  Should
     return a thread from the run queue, unless the run queue is
     empty.  (If the running thread can continue running, then it
     will be in the run queue.)  If the run queue is empty, return
     idle_thread. */
  static struct thread *
  next_thread_to_run (void) 
  {
  if (list_empty (&ready_list))
   return idle_thread;
  else
   return list_entry (list_pop_front (&ready_list), struct thread, elem);
  }
  ```

  选择下一个要调度的进程时，直接采用就绪队列的第一个；当就绪队列为空时，返回进程"idle"。

### 2.3 任务1：Alarm Clock

#### 2.3.1 任务描述

重新实现在`devices/timer.c`中定义的*timer_sleep*。 尽管提供了一个可行的实现，但是它是“busy waits”的，也就是说，它循环检查当前时间并调用*thread_yield*直到足够的时间过去。重新实现它以避免忙等待。

#### 2.3.2 实验过程

为了去除忙等待机制，我们应该把循环调用*thread_yield*的过程修改为阻塞线程，并同时在线程的结构中加入一个成员变量记录休眠时间，在每个时钟中断减少休眠时间的值，当其变为0时认为休眠结束，唤醒线程。

##### DATA STRUCTURES

>### A.2.1 `struct thread`
>
>线程的主要Pintos数据结构是“struct thread”，在“threads/thread.h”中声明。

- 在thread中增加ticks_sleep，用来记录线程被阻塞的时间。

  - \<thread.h\>

  ```C
  struct thread
    {
      /* Owned by thread.c. */
      tid_t tid;                          /* Thread identifier. */
      enum thread_status status;          /* Thread state. */
      char name[16];                      /* Name (for debugging purposes). */
      uint8_t *stack;                     /* Saved stack pointer. */
      int priority;                       /* Priority. */
      struct list_elem allelem;           /* List element for all threads list. */
  
      /* Shared between thread.c and synch.c. */
      struct list_elem elem;              /* List element. */
  
  #ifdef USERPROG
      /* Owned by userprog/process.c. */
      uint32_t *pagedir;                  /* Page directory. */
  #endif
  
      /* Owned by thread.c. */
      unsigned magic;                     /* Detects stack overflow. */
  
      /*---------------my code---------------*/
      int ticks_sleep;    // 剩余休眠时间
      /*---------------my code---------------*/
    };
  ```

##### ALGORITHMS

- 修改*timer_sleep*，将忙等待修改成阻塞

  - \<timer.c\>

  ```c
  void
  timer_sleep (int64_t ticks) 
  {
    int64_t start = timer_ticks ();
  
    ASSERT (intr_get_level () == INTR_ON);
  
    /*---------------my code---------------*/
    if (ticks <= 0) {
      return;
    }
  
    // 获取当前中断状态，并关中断
    enum intr_level old_level = intr_disable();
    
    thread_current()->ticks_sleep = ticks;
    thread_block();
  
    // 恢复中断状态
    intr_set_level(old_level);
    /*---------------my code---------------*/
  }
  ```

- 由于添加了成员变量`ticks_sleep`，我们应该为其修改相应的初始化函数

  - \<threads.c\>的`thread_create`声明中，添加

  ```c
  t->ticks_sleep = 0;  // 初始化为0
  ```

- 在每个时钟中断处加入对`ticks_sleep`的检测

  - \<threads.c\>

  ```c
  static void
  timer_interrupt (struct intr_frame *args UNUSED)
  {
    ticks++;
  
    /*---------------my code---------------*/
    thread_foreach(check_blocked_thread, NULL);
    /*---------------my code---------------*/
    
    thread_tick ();
  }
  ```

- 定义函数*check_blocked_thread*

  - \<threads.c\>

  ```c
  void
  check_blocked_thread(struct thread *t, void *aux UNUSED)
  {
    if (t->status == THREAD_BLOCKED && t->ticks_sleep > 0) {
      t->ticks_sleep -= 1;
      // 时间到了，解除休眠
      if (t->ticks_sleep <= 0) {
        thread_unblock(t);  // 放入就绪队列
      }
    }
  }
  ```

#### 2.3.3 实验结果

完成这些步骤之后，除了"alarm-priority"以外，所有和alarm相关的测试点都能通过了。要通过这个测试点，我们需要实现优先级调度，因此把它放到任务2一起完成。

#### 2.3.4 FAQ

1. **Q**：如何做到为所有的线程统计`ticks_sleep`？

   **A**：自己实现一个队列去循环统计线程休眠时间实在是太过麻烦，我们只需调用pintos提供的*thread_foreach*函数即可。

   >Function: void **thread_foreach** (thread_action_func *action, void *aux)
   >
   >- 遍历所有线程t，并在每个线程上调用`action(t，aux)`。 action必须引用与`thread_action_func()`给出的签名相匹配的函数：
   >
   >Type: **void thread_action_func (struct thread \*thread, void \*aux)**
   >给定aux，对线程执行一些操作。

### 2.4 任务2：Priority Scheduling

#### 2.4.1 任务描述

- 实现优先级调度

  > 当将一个线程添加到具有比当前正在运行的线程更高的优先级的就绪列表时，当前线程应立即将处理器移交给新线程。同样，当线程正在等待锁、信号量或条件变量时，应首先唤醒优先级最高的线程。线程可以随时提高或降低其自身的优先级，但是降低其优先级以使其不再具有最高优先级时，则必须立即让出CPU。

- 解决优先级反转问题

  > 分别考虑高、中和低优先级线程H，M和L。如果H需要等待L（例如，对于由L持有的锁），而M在就绪列表中，然后H将永远无法获得CPU，因为低优先级线程不会获得任何CPU时间。解决此问题的部分方法是，当L持有锁时，H将其优先级“捐赠”给L。L释放（这样H可以获得）锁。

- 实现优先级捐赠

  > 您将需要考虑适用于需要优先捐赠的所有不同情况。确保处理多个捐赠，其中将多个优先级捐赠给单个线程。您还必须处理嵌套捐赠：如果H正在等待M持有的锁，而M正在等待L持有的锁，则M和L都应提升为H的优先级。如有必要，您可以对嵌套优先级捐赠的深度进行合理假设，例如8个级别。您必须实现锁的优先级捐赠。您无需为其他Pintos同步结构实现优先级捐赠。但需要在所有情况下都实施优先级调度。

#### 2.4.2 实验过程

- 优先级调度

  根据**2.2**的分析我们可知，pintos在进行进程调度时，是直接把就绪队列的第一个拿过来的，因此要实现优先级调度，我们只需保证就绪队列是一个优先级队列。
  
  在pintos中，涉及到就绪队列插入操作的函数只有*thread_unblock*、*init_thread*、*thread_yield*，我们需要把其中粗暴的“push_back”改为按照优先级插入。
  
  但这样只能实现根据时间片抢占的进程调度。根据任务描述，我们需要在一个线程添加到具有比当前正在运行的线程更高的优先级的就绪列表时，当前线程应立即将处理器移交给新线程。因此，在设置一个线程优先级时，应该立即重新安排执行顺序，即调用一次*thread_yield*。
  
- 优先级反转

  任务描述中提到的优先级捐赠就是为了解决优先级反转的问题。当发现高优先级的线程因为低优先级线程占用资源而阻塞时，应该让高优先级线程给低优先级线程“捐赠”优先级，即将低优先级线程的优先级提升到等待它所占有的资源的最高优先级线程的优先级。具体思路如下：

  - 当线程拥有多个锁时，若每个锁被多个线程申请，那么可以**让锁保留这些线程的最大优先级**，然后捐赠给低优先级的占有它的线程，捐赠后线程的优先级应该是**所有锁保留的优先级的最大值**。在线程释放锁的时候，应该让它**回归到最初的优先级**。
  - 如果线程之间的资源占用情况呈现链状，如线程A占用锁x，线程B申请x又占用y，线程C申请y又占用z，优先级A<B<C，则线程C的优先级应该递归地捐赠到A。

##### DATA STRUCTURES

为了之后解决优先级反转和捐赠的问题，我们需要给thread和lock结构中加入一些成员变量：

- \<threads.h\>

  ```C
  struct thread
    {
      ...
  
      /*---------------my code---------------*/
      int ticks_sleep;    // 剩余休眠时间
      int orig_priority;  // 原优先级
      struct list locks;  // 获取的lock
      struct lock* lock_acquiring;  // 请求的锁
      /*---------------my code---------------*/
    };
  ```

- \<synch.h\>

  ```c
  struct lock 
    {
      struct thread *holder;      /* Thread holding lock (for debugging). */
      struct semaphore semaphore; /* Binary semaphore controlling access. */
      
      /*---------------my code---------------*/
      struct list_elem elem;
      int max_priority;
      /*---------------my code---------------*/
    };
  ```


##### ALGORITHMS

- 保证就绪队列是一个优先级队列

  - \<threads.c\>

  找到*thread_unblock*、*init_thread*、*thread_yield*函数，将其中的`thread_unblock () `修改为以下形式

  ```c
  list_insert_ordered(&ready_list, &t->elem, (list_less_func *) &thread_priority_cmp, NULL);
  ```

  这里使用的比较函数*thread_priority_cmp*，需要我们自己实现：

  ```c
  bool
  thread_priority_cmp(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
  {
    return list_entry(a, struct thread, elem)->priority > list_entry(b, struct thread, elem)->priority;
  }
  ```

- 实现抢占式调度

  - \<threads.c\>

  创建新线程时移交处理器。实际上这里可以直接`thread_yield()`，但是为了减少不必要的时间浪费，我增加了判断的过程，新进程优先级较低时不执行*thread_yield*。

  ```c
  tid_t
  thread_create (const char *name, int priority,
                 thread_func *function, void *aux) 
  {
    ...
    
    /*---------------my code---------------*/
    if (thread_current()->priority < priority) {
      thread_yield();  // 新进程优先级高于当前进程，则当前进程让出CPU
    }
    /*---------------my code---------------*/
    return tid;
  }
  ```

  设置优先级时移交处理器。

  ```c
  void
  thread_set_priority (int new_priority) 
  {
    /*---------------my code---------------*/
    enum intr_level old_level = intr_disable();
    
    int old_priority = thread_current()->priority;
  
    // orig_priority降低时没必要yield
    if (old_priority < new_priority)
    {
      thread_current()->priority = new_priority;
      thread_yield();  // 每次调整优先级后都要让当前进程让出CPU
    }
  
    intr_set_level(old_level);
    /*---------------my code---------------*/
  }
  ```

- 实现优先级捐赠

  原理在**2.4.2**开头讲过，这里不再赘述。

  - \<synch.c\>

  申请锁时的操作具体实现如下：

  ```C
  void
  lock_acquire (struct lock *lock)
  {
    ASSERT (lock != NULL);
    ASSERT (!intr_context ());
    ASSERT (!lock_held_by_current_thread (lock));
  
    /*---------------my code---------------*/
    if (!thread_mlfqs) {
      if (lock->holder != NULL) {
        struct thread* cur_thread = thread_current();
        struct lock* pre_lock = lock;
        cur_thread->lock_acquiring = lock;
        for (; pre_lock && pre_lock->holder && cur_thread->priority > pre_lock->max_priority; pre_lock = pre_lock->holder->lock_acquiring) {
  	// 如果请求的锁有占用者，则捐赠优先级
          pre_lock->max_priority = cur_thread->priority;
          thread_donate_the_priority(pre_lock->holder);
        }
      }
    }  
    /*---------------my code---------------*/
  
    sema_down (&lock->semaphore);
  
    /*---------------my code---------------*/
    enum intr_level old_level = intr_disable();
    struct thread* cur_thread = thread_current();
    if (!thread_mlfqs) {
      cur_thread->lock_acquiring = NULL;
      lock->max_priority = cur_thread->priority;
      thread_hold_the_lock(lock);
    }
    lock->holder = cur_thread;  // 成功获得该锁
    intr_set_level (old_level);
    /*---------------my code---------------*/
  }
  ```

  - \<threads.c\>

  *lock_acquire*中的*thread_donate_the_priority*和*thread_hold_the_lock*是我自定义的函数：

  ```C
  void
  thread_donate_the_priority(struct thread *t)
  {
    enum intr_level old_level = intr_disable();
    
    int max_priority = t->orig_priority;
    int lock_priority;
  
    if (!list_empty(&t->locks)) {
      list_sort(&t->locks, lock_priority_cmp, NULL);
      lock_priority = list_entry(list_front(&t->locks), struct lock, elem)->max_priority;
      max_priority = (max_priority > lock_priority) ? max_priority : lock_priority;
    }
    t->priority = max_priority;
  
    if(t->status == THREAD_READY) {
      list_remove(&t->elem);
      list_insert_ordered(&ready_list, &t->elem, thread_priority_cmp, NULL);
    }
    
    intr_set_level(old_level);
  }
  
  void
  thread_hold_the_lock(struct lock *lock)
  {
    enum intr_level old_level = intr_disable();
    list_insert_ordered(&thread_current()->locks, &lock->elem, lock_priority_cmp, NULL);
    if(thread_current()->priority < lock->max_priority){
      thread_current()->priority = lock->max_priority;
      thread_yield();
    }
    intr_set_level(old_level);
  }
  ```

  还需要实现一下比较函数*lock_priority_cmp*

  ```C
  bool
  lock_priority_cmp(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
  {
    return list_entry(a, struct lock, elem)->max_priority > list_entry(b, struct lock, elem)->max_priority;
  }
  ```

  - \<synch.c\>

  修改*lock_release*

  ```C
  void
  lock_release (struct lock *lock) 
  {
    ASSERT (lock != NULL);
    ASSERT (lock_held_by_current_thread (lock));
  
    /*---------------my code---------------*/
    if (!thread_mlfqs)
  	  thread_remove_the_lock(lock);
    /*---------------my code---------------*/
    
    lock->holder = NULL;
    sema_up (&lock->semaphore);
  
  }
  ```

  - \<threads.c\>

  实现*thread_remove_the_lock*

  ```C
  void
  thread_remove_the_lock(struct lock* lock)
  {
    enum intr_level old_level = intr_disable();
    list_remove(&lock->elem);
    thread_update_priority(thread_current());
    intr_set_level(old_level);
  }
  ```

  释放掉锁后，考虑到优先级捐赠，线程的优先级可能会变化，此时需要*thread_update_priority*实现这一操作

  ```C
  void
  thread_update_priority(struct thread *t)
  {
    enum intr_level old_level = intr_disable();
    int max_priority = t->orig_priority;
    int lock_priority;
  
    if (!list_empty(&t->locks))
    {
      list_sort(&t->locks, lock_priority_cmp, NULL);
      lock_priority = list_entry(list_front(&t->locks), struct lock, elem)->max_priority;
      max_priority = (max_priority > lock_priority) ? max_priority : lock_priority;
    }
  
    t->priority = max_priority;
    intr_set_level (old_level);
  }
  ```

  由于修改了thread的结构，我们对*thread_set_priority*函数设置优先级的过程也需要进行一些修改

  ```C
  void
  thread_set_priority (int new_priority) 
  {
    /*---------------my code---------------*/
    if (thread_mlfqs) {
      return;
    }
  
    enum intr_level old_level = intr_disable();
    
    int old_priority = thread_current()->priority;
    thread_current()->orig_priority = new_priority;
  
    // 有锁且orig_priority降低时没必要yield
    if (list_empty(&thread_current()->locks) || old_priority < new_priority)
    {
      thread_current()->priority = new_priority;
      thread_yield();  // 每次调整优先级后都要让当前进程让出CPU
    }
  
    intr_set_level(old_level);
    /*---------------my code---------------*/
  }
  ```

  别忘了加入初始化

  ```C
  static void
  init_thread (struct thread *t, const char *name, int priority)
  {
    enum intr_level old_level;
  
    ASSERT (t != NULL);
    ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT (name != NULL);
  
    memset (t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy (t->name, name, sizeof t->name);
    t->stack = (uint8_t *) t + PGSIZE;
    t->priority = priority;
    t->magic = THREAD_MAGIC;
  
    /*---------------my code---------------*/
    t->orig_priority = priority;
    list_init(&t->locks);
    t->lock_acquiring = NULL;
    t->ticks_sleep = 0;
    t->nice = 0;
    t->recent_cpu = CVT_INT2FP(0);
    /*---------------my code---------------*/
    
    old_level = intr_disable ();
    list_insert_ordered (&all_list, &t->allelem, (list_less_func *) &thread_priority_cmp, NULL);
    intr_set_level (old_level);
  }
  ```

- 我们还需要实现优先级唤醒

  - \<synch.c\>

  修改为优先级队列

  ```C
  void
  cond_signal (struct condition *cond, struct lock *lock UNUSED) 
  {
    ASSERT (cond != NULL);
    ASSERT (lock != NULL);
    ASSERT (!intr_context ());
    ASSERT (lock_held_by_current_thread (lock));
  
    if (!list_empty (&cond->waiters)) {
      /*---------------my code---------------*/
      list_sort (&cond->waiters, cond_sema_priority_cmp, NULL);
      /*---------------my code---------------*/
      sema_up (&list_entry (list_pop_front (&cond->waiters), struct semaphore_elem, elem)->semaphore);
    }
  }
  ```

  实现自定义的比较函数*cond_sema_priority_cmp*

  ```C
  bool
  cond_sema_priority_cmp(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
  {
    struct semaphore_elem *sema_a = list_entry(a, struct semaphore_elem, elem);
    struct semaphore_elem *sema_b = list_entry(b, struct semaphore_elem, elem);
    return list_entry(list_front(&sema_a->semaphore.waiters), struct thread, elem)->priority > list_entry(list_front(&sema_b->semaphore.waiters), struct thread, elem)->priority;
  }
  ```

  修改为优先级队列

  ```C
  void
  sema_down (struct semaphore *sema) 
  {
    enum intr_level old_level;
  
    ASSERT (sema != NULL);
    ASSERT (!intr_context ());
  
    old_level = intr_disable ();
    while (sema->value == 0) 
    {
      /*---------------my code---------------*/
      list_insert_ordered(&sema->waiters, &thread_current()->elem, thread_priority_cmp, NULL);
      /*---------------my code---------------*/
      thread_block ();
    }
    sema->value--;
    intr_set_level (old_level);
  }
  
  void
  sema_up (struct semaphore *sema) 
  {
    enum intr_level old_level;
  
    ASSERT (sema != NULL);
  
    old_level = intr_disable ();
    if (!list_empty (&sema->waiters)) {
      /*---------------my code---------------*/
      list_sort(&sema->waiters, thread_priority_cmp, NULL);
      /*---------------my code---------------*/
      thread_unblock (list_entry (list_pop_front (&sema->waiters), struct thread, elem));
    }
    sema->value++;
    /*---------------my code---------------*/
    thread_yield();
    /*---------------my code---------------*/
    intr_set_level (old_level);
  }
  ```

#### 2.4.3 实验结果

通过了所有alarm\*和priority\*测试点。

#### 2.4.4 FAQ

1. **Q**：如何做到为各列表排序和按顺序插入？

   **A**：pintos源码中已经为我们提供了相应的函数，我们无需关注与OS原理无关的算法实现，调用即可。

   >Function: void **thread_foreach** (thread_action_func *action, void *aux)
   >
   >- \<list.h\>
   >
   >```C
   >/* Operations on lists with ordered elements. */
   >void list_sort (struct list *,
   >                list_less_func *, void *aux);
   >void list_insert_ordered (struct list *, struct list_elem *,
   >                          list_less_func *, void *aux);
   >void list_unique (struct list *, struct list *duplicates,
   >                  list_less_func *, void *aux);
   >```

### 2.5 任务3：Advance Scheduler

#### 2.5.1 任务描述

实现类似于4.4BSD调度器的多级反馈队列调度程序，以减少在系统上运行作业的平均响应时间。

多级反馈的详细要求如下：

> 这种类型的调度程序维护着几个准备运行的线程队列，其中每个队列包含具有不同优先级的线程。在任何给定时间，调度程序都会从优先级最高的非空队列中选择一个线程。如果最高优先级队列包含多个线程，则它们以“轮转(round robin)”顺序运行。
>
> 调度程序有几处要求在一定数量的计时器滴答之后更新数据。在每种情况下，这些更新应在任何普通内核线程有机会运行之前进行，这样内核线程就不可能看到新增加的“timer_ticks()”值，只能看到旧的调度程序数据值。
>
> 类似于4.4BSD调度程序不包括优先级捐赠。

实现调度程序所需的计算如下：

- 每个线程直接在其控制下的*nice*值在-20和20之间。每个线程还有一个优先级，介于0（`PRI_MIN`）到63（`PRI_MAX`）之间，每四个*tick*使用以下公式重新计算一次：
  $$
  \text{priority}=\text{PRI\_MAX}-\frac{1}{4}\text{recent\_cpu}-2 \cdot\text{nice}
  $$

- *recent_cpu*测量线程“最近”接收到的CPU时间。在每个计时器滴答中，运行线程的*recent_cpu*递增1。每秒一次，每个线程的*recent_cpu*都以这种方式更新：
  $$
  \text{recent\_cpu}=\frac{2\cdot \text{load\_avg}}{2\cdot \text{load\_avg}+1}\cdot \text{recent\_cpu}+\text{nice}
  $$

- *load_avg*估计过去一分钟准备运行的平均线程数。它在引导时初始化为0，并每秒重新计算一次，如下所示：
  $$
  \text{load\_avg}=\frac{59}{60}\cdot \text{load\_avg}+\frac{1}{60}\cdot \text{ready\_threads}
  $$
  其中*ready_threads*是在更新时正在运行或准备运行的线程数（不包括空闲线程）。

您必须编写代码，以允许我们在Pintos启动时选择调度算法策略。默认情况下，优先级调度程序必须处于活动状态，但是我们必须能够通过“-mlfqs”内核选项选择4.4BSD调度程序。传递此选项会将在“threads/thread.h”中声明的“thread_mlfqs”设置为true，当由parse_options()解析选项时会发生这种情况，这发生在main()的启动部分。

启用4.4BSD调度程序后，线程不再直接控制自己的优先级。 应该忽略对thread_create()的优先级参数以及对thread_set_priority()的任何调用，而对thread_get_priority()的调用应返回调度程序设置的线程的当前优先级。

#### 2.5.2 实验过程

- 定点小数运算的实现

  | 计算                                           | 操作
  | ---------------------------------------------- | ------------------------------------------------------------ |
  | Convert `n` to fixed point:                    | `n * f`                                                      |
  | Convert `x` to integer (rounding toward zero): | `x / f`                                                      |
  | Convert `x` to integer (rounding to nearest):  | `(x + f / 2) / f` if `x >= 0`, `(x - f / 2) / f` if `x <= 0`. |
  | Add `x` and `y`:                               | `x + y`                                                      |
  | Subtract `y` from `x`:                         | `x - y`                                                      |
  | Add `x` and `n`:                               | `x + n * f`                                                  |
  | Subtract `n` from `x`:                         | `x - n * f`                                                  |
  | Multiply `x` by `y`:                           | `((int64_t) x) * y / f`                                      |
  | Multiply `x` by `n`:                           | `x * n`                                                      |
  | Divide `x` by `y`:                             | `((int64_t) x) * f / y`                                      |
  | Divide `x` by `n`:                             | `x / n`                                                      |

  这里可以新建一个头文件来对需要的运算进行宏定义。
  
- 实现新变量的赋值、初始化等工作

- 按照新定义的优先级对线程进行调度

##### DATA STRUCTURES

- 在thread中增加nice和recent_cpu，同时添加全局变量load_avg

  - \<threads.h\>

  ```C
  #define NICE_MIN -20
  #define NICE_MAX 20
  
  int load_avg;
  ...
  
  struct thread
    {
      ...
  
      /*---------------my code---------------*/
      int nice;
      int recent_cpu;
      /*---------------my code---------------*/
    };
  ```

##### ALGORITHMS

- 实现定点小数运算

  - 新建\<fixed-point.h\>

  根据**B.6 Fixed-Point Real Arithmetic**中的要求把带符号的32位整数的最低14位指定为小数位。

  ```C
  #ifndef FIXED_POINT_H
  #define FIXED_POINT_H
  
  #define Q 14
  #define F (1 << Q)
  
  #define CVT_INT2FP(n) ((int) (n << Q))
  #define CVT_FP2INT(x) ((int) (x >> Q))
  #define CVT_ROUND_FP2INT(x) ((int) ((x >= 0) ? CVT_FP2INT(x + (1 << (Q - 1))) : CVT_FP2INT(x - (1 << (Q - 1)))))
  
  #define FP_ADD_FP(x, y) (x + y)
  #define FP_SUB_FP(x, y) (x - y)
  
  #define FP_ADD_INT(x, n) (x + CVT_INT2FP(n))
  #define FP_SUB_INT(x, n) (x - CVT_INT2FP(n))
  
  #define FP_MUL_FP(x, y) CVT_FP2INT(((int64_t) x) * y)
  #define FP_DIV_FP(x, y) ((int) (CVT_INT2FP((int64_t) x) / y))
  
  #define FP_MUL_INT(x, n) ((int) (x * n))
  #define FP_DIV_INT(x, n) ((int) (x / n))
  
  #endif
  ```

- 补全新成员变量的赋值、取值函数

  - \<thread.c\>

  ```C
  /* Sets the current thread's nice value to NICE. */
  void
  thread_set_nice (int nice UNUSED) 
  {
    nice = nice < NICE_MIN ? NICE_MIN : nice;
    nice = nice > NICE_MAX ? NICE_MAX : nice;
  
    struct thread *cur_thread = thread_current();
    cur_thread->nice = nice;
    
    if (cur_thread != idle_thread){
      cur_thread->priority = CVT_FP2INT(FP_SUB_INT(FP_SUB_FP(CVT_INT2FP(PRI_MAX), FP_DIV_INT(cur_thread->recent_cpu, 4)), 2*cur_thread->nice));
      cur_thread->priority = cur_thread->priority > PRI_MAX ? PRI_MAX : cur_thread->priority;
      cur_thread->priority = cur_thread->priority < PRI_MIN ? PRI_MIN : cur_thread->priority;
    }
  
    thread_yield();
  }
  
  /* Returns the current thread's nice value. */
  int
  thread_get_nice (void) 
  {
    return thread_current()->nice;
  }
  
  /* Returns 100 times the system load average. */
  int
  thread_get_load_avg (void) 
  {
    return CVT_ROUND_FP2INT(FP_MUL_INT(load_avg, 100));
  }
  
  /* Returns 100 times the current thread's recent_cpu value. */
  int
  thread_get_recent_cpu (void) 
  {
    return CVT_ROUND_FP2INT(FP_MUL_INT(thread_current()->recent_cpu, 100));
  }
  ```

- 更新3个变量的值

  - \<timer.c\>

  ```C
  static void
  timer_interrupt (struct intr_frame *args UNUSED)
  {
    ticks++;
  
    /*---------------my code---------------*/
    thread_foreach(check_blocked_thread, NULL);
  
    if (thread_mlfqs){
      // 需要的数据结构都在thread.c，故这里只调用函数
      // 若无运行态进程，则无需计算
      increase_recent_cpu();
  
      // 每秒计算recent_cpu和load_avg
      if (ticks % TIMER_FREQ == 0){
        cal_1();
      }
      // 每4个tick更新priority
      else if (ticks % 4 == 0){
        cal_2();
      }
    }
    /*---------------my code---------------*/
    
    thread_tick ();
  }
  ```

  - \<threads.c\>

  实现自定义的*increase_recent_cpu*、*cal_1*、*cal_2*函数

  ```C
  void
  increase_recent_cpu()
  {
    struct thread *cur_thread = thread_current();
    if (cur_thread != idle_thread){
      cur_thread->recent_cpu += CVT_INT2FP(1);
    }
  }
  
  void
  cal_1()
  {
    int ready_threads = list_size(&ready_list);
    int run_thread = (thread_current() != idle_thread) ? 1 : 0;
    ready_threads += run_thread;
    load_avg = FP_ADD_FP(FP_DIV_INT(FP_MUL_INT(load_avg, 59), 60), FP_DIV_INT(CVT_INT2FP(ready_threads), 60));
  
    thread_foreach(update_recent_cpu_and_mlfqs_priority, NULL);
  }
  
  void
  cal_2()
  {
    struct thread *cur_thread = thread_current();
    if (cur_thread != idle_thread){
      cur_thread->priority = CVT_FP2INT(FP_SUB_INT(FP_SUB_FP(CVT_INT2FP(PRI_MAX), FP_DIV_INT(cur_thread->recent_cpu, 4)), 2*cur_thread->nice));
      cur_thread->priority = cur_thread->priority > PRI_MAX ? PRI_MAX : cur_thread->priority;
      cur_thread->priority = cur_thread->priority < PRI_MIN ? PRI_MIN : cur_thread->priority;
    }
  }
  ```

  *cal_1*中使用的*update_recent_cpu_and_mlfqs_priority*也要实现

  ```C
  void
  update_recent_cpu_and_mlfqs_priority(struct thread *t, void *aux UNUSED)
  {
    if (t == idle_thread){
      return;
    }
    
    t->recent_cpu = FP_ADD_INT(FP_MUL_FP(FP_DIV_FP(FP_MUL_INT(load_avg, 2), FP_ADD_INT(FP_MUL_INT(load_avg, 2), 1)), t->recent_cpu), t->nice);
    
    t->priority = CVT_FP2INT(FP_SUB_INT(FP_SUB_FP(CVT_INT2FP(PRI_MAX), FP_DIV_INT(t->recent_cpu, 4)), 2*t->nice));
    t->priority = t->priority > PRI_MAX ? PRI_MAX : t->priority;
    t->priority = t->priority < PRI_MIN ? PRI_MIN : t->priority;
  }
  ```

- 别忘了初始化

  - \<threads.c\>

  *init_thread*中加入：

  ```C
  t->nice = 0;
  t->recent_cpu = CVT_INT2FP(0);
  ```

- 由于

  > 类似于4.4BSD调度程序不包括优先级捐赠。

  故涉及到优先级捐赠的操作都要加上`if (!thread_mlfqs)`

#### 2.5.3 实验结果

27个测试点全部pass

#### 2.5.4 FAQ

1. **Q**：对make的编写不熟悉，不知道如何给自定义文件编写正确的规则怎么办？

   **A**：不需要自定义.c文件，定义.h文件即可以利用make的隐式规则参与项目的编译

   > 新建“.h”文件不需要编辑“Makefile”.

## 3 Project 2: User Programs

### 3.1 pintos用户程序框架

pintos基本代码已经支持加载和运行用户程序，但需要是普通的C程序，只要它们适合内存并且仅使用我们实现的系统调用即可。值得注意的是，无法实现`malloc()`，因为该项目所需的系统调用均不支持内存分配。Pintos也不能运行使用浮点运算的程序，因为在切换线程时内核不会保存和恢复处理器的浮点单元。

#### 存在的问题

- 没有I/O
- 无法交互
- 没有内部同步。 并发访问会互相干扰。 应该使用同步来确保一次只有一个进程正在执行文件系统代码.
- 文件大小在创建时固定。 根目录表示为一个文件，因此可以创建的文件数也受到限制
- 文件数据是按单个扩展区分配的，也就是说，单个文件中的数据必须占用磁盘上连续的扇区范围。因此，随着时间的推移使用文件系统，外部碎片会成为一个严重的问题
- 没有子目录
- 文件名限制为14个字符
- 操作过程中的系统崩溃可能会以无法自动修复的方式损坏磁盘。没有文件系统修复工具

### 3.2 内存分配流程和堆栈的使用

> Pintos中的虚拟内存分为两个区域：用户虚拟内存和内核虚拟内存。用户虚拟内存的范围从虚拟地址0到“PHYS_BASE”（在“threads/vaddr.h”中定义），默认为“ 0xc0000000”（3GB）。内核虚拟内存占用了其余的虚拟地址空间，从“PHYS_BASE”到最大4GB。
>
> 用户虚拟内存是按进程管理的。当内核从一个进程切换到另一个进程时，它还会通过更改处理器的页目录基址寄存器来切换用户虚拟地址空间（请参阅“userprog/pagedir.c”中的“pagedir_activate()”）。“struct thread”包含指向进程的页表的指针。
>
> 内核虚拟内存是全局的。无论运行什么用户进程或内核线程，它始终以相同的方式映射。在Pintos中，内核虚拟内存从“PHYS_BASE”开始一对一映射到物理内存。也就是说，虚拟地址“PHYS_BASE”访问物理地址0，虚拟地址“PHYS_BASE”+“0x1234”访问物理地址“0x1234”，依此类推，直到机器的物理内存大小为止。
>
> 用户程序只能访问其自己的用户虚拟内存。尝试访问内核虚拟内存会导致页面错误，由“userprog/exception.c”中的“page_fault()”处理，该进程将终止。内核线程可以访问内核虚拟内存，也可以访问正在运行的进程的用户虚拟内存（如果用户进程正在运行）。但是，即使在内核中，尝试以未映射的用户虚拟地址访问内存也会导致页面错误。
>
> 从概念上讲，每个进程都可以随意选择自己的用户虚拟内存。 实际上，用户虚拟内存的布局是这样的:
>
> ```
>    PHYS_BASE +----------------------------------+
>              |            user stack            |
>              |                 |                |
>              |                 |                |
>              |                 V                |
>              |          grows downward          |
>              |                                  |
>              |                                  |
>              |                                  |
>              |                                  |
>              |           grows upward           |
>              |                 ^                |
>              |                 |                |
>              |                 |                |
>              +----------------------------------+
>              | uninitialized data segment (BSS) |
>              +----------------------------------+
>              |     initialized data segment     |
>              +----------------------------------+
>              |           code segment           |
>   0x08048000 +----------------------------------+
>              |                                  |
>              |                                  |
>              |                                  |
>              |                                  |
>              |                                  |
>            0 +----------------------------------+
> ```
>
> 在本项目中，用户堆栈的大小是固定的，但在项目3中，将允许其增长。传统上，未初始化的数据段的大小可以通过系统调用来调整，但是您不必实现这一点。
>
> Pintos中的代码段始于用户虚拟地址“0x08084000”，距离地址空间底部约128MB。 此值在[SysV-i386](https://oj.etao.net/osd/pintos_13.html)中指定，没有特殊意义。

##### 堆栈使用举例

> 考虑如何处理以下示例命令的参数：“/bin/ls -l foo bar”。首先，将命令分解为单词：“/bin /ls”，“-l”，“foo”，“bar”。 将单词放在堆栈的顶部。 顺序无关紧要，因为它们将通过指针进行引用。
>
> 然后，按从右到左的顺序将每个字符串的地址以及一个空指针哨兵压入堆栈。这些是“argv”的元素。空指针sendinel可以确保C标准所要求的argv[argc]是空指针。该命令确保“argv[0]”位于最低虚拟地址。字对齐的访问比未对齐的访问要快，因此为了获得最佳性能，在第一次压入之前将堆栈指针向下舍入为4的倍数。
>
> 然后，依次按“argv”（“argv[0]”的地址）和“argc”。最后，推送一个伪造的“返回地址”：尽管入口函数将永远不会返回，但其堆栈框架必须具有与其他任何结构相同的结构。
>
> 下表显示了在用户程序开始之前堆栈的状态以及相关的寄存器，假设PHYS_BASE为“0xc0000000”：
>
> | Address    | Name           | Data               | Type        |
> | ---------- | -------------- | ------------------ | ----------- |
> | 0xbffffffc | argv[3]\[…]    | “bar\0”            | char[4]     |
> | 0xbffffff8 | argv[2]\[…]    | “foo\0”            | char[4]     |
> | 0xbffffff5 | argv[1]\[…]    | “-l\0”             | char[3]     |
> | 0xbfffffed | argv[0]\[…]    | “/bin/ls\0</samp>” | char[8]     |
> | 0xbfffffec | word-align     | 0                  | uint8_t     |
> | 0xbfffffe8 | argv[4]        | 0                  | char *      |
> | 0xbfffffe4 | argv[3]        | 0xbffffffc         | char *      |
> | 0xbfffffe0 | argv[2]        | 0xbffffff8         | char *      |
> | 0xbfffffdc | argv[1]        | 0xbffffff5         | char *      |
> | 0xbfffffd8 | argv[0]        | 0xbfffffed         | char *      |
> | 0xbfffffd4 | argv           | 0xbfffffd8         | char **     |
> | 0xbfffffd0 | argc           | 4                  | int         |
> | 0xbfffffcc | return address | 0                  | void (*) () |
>
> 在这个例子中，堆栈指针将被初始化为0xbfffffcc。
>
> 如上所示，您的代码应在用户虚拟地址空间的最顶部，即虚拟地址“PHYS_BASE”（在“threads/vaddr.h”中定义）下方的页面中开始堆栈。
>
> 您可能会发现在“”中声明的非标准`hex_dump（）`函数对于调试参数传递代码很有用。在上面的示例中将显示以下内容：
>
> ```
> bfffffc0                                      00 00 00 00 |            ....|
> bfffffd0  04 00 00 00 d8 ff ff bf-ed ff ff bf f5 ff ff bf |................|
> bfffffe0  f8 ff ff bf fc ff ff bf-00 00 00 00 00 2f 62 69 |............./bi|
> bffffff0  6e 2f 6c 73 00 2d 6c 00-66 6f 6f 00 62 61 72 00 |n/ls.-l.foo.bar.|
> ```

### 3.3 任务1：Argument Passing

#### 3.3.1 任务描述

通过扩展`process_execute()`来实现将参数传递给新进程的功能，以使它不仅将程序文件名作为参数，而且还将其按空格划分，第一个单词是程序名称，第二个单词是第一个参数，依此类推。

在命令行中，多个空格等效于一个空格。

#### 3.3.2 实验过程

- pintos文档中第一个任务并不是Argument Passing，而是Process Termination Messages，保险起见，我决定先把这个任务完成。完成这个任务只需在\<threads.c\>中加入相应的输出即可。

- 将参数按空格分割成文件名和各参数
- 文件名传给*load*函数，按文件名加载可执行文件
- 参数入栈

##### DATA STRUCTURES

- 在thread结构体中加入成员变量`int st_exit`，作为退出代码

##### ALGORITHMS

- Process Termination Messages

  - \<threads.c\>

  在*thread_exit*中加入

  ```C
  printf ("%s: exit(%d)\n", thread_name(), thread_current()->st_exit);
  ```

- Argument Passing

  - \<process.c\>

  创建一个运行用户程序的线程，该用户程序来源于一可执行文件。可执行文件的名称在参数file_name中。使用*strtok_r*分割参数，并传递到*thread_create*和*start_process*中。

  ```C
  tid_t
  process_execute (const char *file_name)
  {
    tid_t tid;
    /* Make a copy of FILE_NAME.
       Otherwise there's a race between the caller and load(). */
    char *fn_copy = malloc(strlen(file_name) + 1);
    strlcpy (fn_copy, file_name, strlen(file_name) + 1);
    
    /*---------------my code-------------*/
    char *hondo_name = malloc(strlen(file_name) + 1);
    char * tmp = NULL;
    strlcpy (hondo_name, file_name, strlen(file_name) + 1);
    // strtok_r为破坏性操作，故需要备份
    hondo_name = strtok_r (hondo_name, " ", &tmp);  // 截取命令（文件）名
    
    /* Create a new thread to execute FILE_NAME. */
    tid = thread_create (hondo_name, PRI_DEFAULT, start_process, fn_copy);
    if (tid == TID_ERROR){
      free (hondo_name);
      free (fn_copy);
      return tid;
    }
  
    sema_down(&thread_current()->sema);
    free (hondo_name);
    free (fn_copy);
    if (!thread_current()->success)
      return TID_ERROR;  // 无法创建新进程
    /*---------------my code-------------*/
  
    return tid;
  }
  ```

  在*start_process*中加载可执行文件，并让各参数入栈

  ```C
  static void
  start_process (void *file_name_)
  {
    char *file_name = file_name_;
    struct intr_frame if_;
    bool success;
  
    memset (&if_, 0, sizeof if_);
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;
  
    /*---------------my code-------------*/
    // 此时filename仍是整行字符串，故需分割
    char *fn_copy = malloc(strlen(file_name) + 1);
    strlcpy(fn_copy, file_name, strlen(file_name) + 1);
  
    char *tmp = NULL;
    file_name = strtok_r (file_name, " ", &tmp);
    // 需要输入文件名
    success = load (file_name, &if_.eip, &if_.esp);
  
    char *token = NULL;
    if (success){
      int argc = 0;
      int argv[64];  // 最多64个参数
      for (token = strtok_r (fn_copy, " ", &tmp); token != NULL; token = strtok_r (NULL, " ", &tmp)){
        if_.esp -= strlen(token) + 1;  // 用户堆栈向下增长
        memcpy (if_.esp, token, strlen(token)+1);
        argv[argc++] = (int) if_.esp;  // 存储参数地址
      }
      push_arg(&if_.esp, argv, argc);  // 入栈
  
      thread_current ()->parent->success = true;  // 告诉父进程加载成功
      sema_up (&thread_current ()->parent->sema);
    }
    else{
      thread_current ()->parent->success = false;
      sema_up (&thread_current ()->parent->sema);
      thread_exit ();
    }
  
    free(fn_copy);
    /*---------------my code-------------*/
  
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
    NOT_REACHED ();
  }
  ```

  实现自定义函数*push_arg*，完成参数入栈工作

  ```C
  // 参数入栈
  void
  push_arg (void **esp, int *argv, int argc)
  {
    // 字对齐，在第一次入栈前将堆栈指针向下舍入为4的倍数
    *esp = (int) *esp & 0xfffffffc;
    *esp -= 4;
    *(int *) *esp = 0;  // 将void **esp强行转换后去指针指向esp的值
    int i = argc - 1;
    while (i >= 0) {
      *esp -= 4;
      *(int *) *esp = argv[i];
      --i;
    }
    *esp -= 4;
    *(int *) *esp = (int) *esp + 4;  // 压入argv[0]在栈中的地址
    *esp -= 4;
    *(int *) *esp = argc;
    *esp -= 4;
    *(int *) *esp = 0;  // return address
  }
  ```

#### 3.3.3 实验结果

由于系统调用*write*未完成，该任务完成后无法打印出正确结果。

#### 3.3.4 FAQ

1. **Q**：为什么使用*strtok_r*而不是*strtok*？

   **A**：带有r的函数主要来自于UNIX下面，其与不带r的函数的区别是，它们是线程安全的。

### 3.4 任务2：Process Control Syscalls

#### 3.4.1 任务描述

在“userprog/syscall.c”中实现系统调用处理程序。它将需要检索系统调用号，然后检索系统调用参数，并执行适当的操作。

需要完成的系统调用如下：

- System Call: void **halt** (void)
  - 通过调用“shutdown_power_off()”（在“threads/init.h”中声明）终止Pintos。很少使用此方法，因为您会丢失一些有关可能的死锁情况的信息，等等。

- System Call: void **exit** (int status)

  终止当前用户程序，将status返回到内核。如果进程的父进程等待它（参见下文），则将返回此状态。按照惯例，状态状态为0表示成功，非零值表示错误。

- System Call: pid_t **exec** (const char *cmd\_line)

  运行其名称在 cmd_line 中给出的可执行文件，并传递任何给定的参数，返回新进程的进程ID（pid）。 如果程序由于任何原因无法加载或运行，则必须返回pid -1，否则不应为有效pid。 因此，父进程在知道子进程是否成功加载其可执行文件之前不能从exec返回。您必须使用适当的同步来确保这一点。

- System Call: int **wait** (pid_t pid)

  - 等待子进程pid并检索子进程的退出状态.

  - 如果pid仍然有效，请等待直到终止。然后，返回pid传递给`exit`的状态。如果pid没有调用`exit()`，而是被内核终止（例如，由于异常而终止），则`wait(pid)`必须返回-1。父进程等待在父进程调用wait之前已经终止的子进程是完全合法的，但是内核仍必须允许父进程检索其子进程的退出状态，或者得知该子进程已被终止。

  - 如果满足以下任一条件，则“wait”必须失败并立即返回-1:

    - pid不引用调用过程的直接子级。pid是调用过程的直接子级，当且仅当调用过程从成功调用exec收到pid作为返回值时。

      请注意，子级不是继承的：如果A生成子级B和B生成子进程C，则即使B已死,A也无法等待C。进程A对`wait(C)`的调用必须失败。同样，如果孤立进程的父进程先退出，则它们也不会分配给新父进程。

    - 调用“wait”的进程已经在pid上调用过wait了。也就是说，一个过程最多可以等待任何给定的孩子一次.

  - 进程可以生成任意数量的子代，以任何顺序等待它们，甚至可以退出而无需等待部分或全部子代。您的设计应考虑所有可能发生等待的方式。无论父进程是否等待它，无论子进程是在其父进程之前还是之后退出，都必须释放该进程的所有资源，包括其“结构线程”。

  - 您必须确保Pintos在初始进程退出之前不会终止。提供的Pintos代码尝试通过从main()（在“threads/init.c”中）调用“process_wait()”（在“userprog/process.c”中）来实现此目的。 我们建议您根据函数顶部的注释实现`process_wait()`，然后再根据`process_wait()`实现`wait`系统调用。

  - 实现此系统调用所需的工作比其余任何工作都要多得多。

#### 3.4.2 实验过程

##### DATA STRUCTURES

- 需要在thread结构体中增加成员变量满足子线程管理的需求

  - \<threads.h\>

  ```C
  struct thread
    {
      ...
          
      /*---------------my code-------------*/
      struct list children;
      struct child * thread_child;
      int st_exit;
      struct semaphore sema;
      bool success;
      struct thread* parent;
      /*---------------my code-------------*/
    };
  
  struct child
    {
      tid_t tid;
      bool is_run;
      struct list_elem child_elem;
      struct semaphore sema;
      int store_exit;
    };
  ```

#####  ALGORITHMS

- 检查地址合法性

  - \<syscall.c\>

  ```C
  // 判断指针指向的地址是否合法
  void * 
  ckptr2(const void *vaddr)
  {
    // 是否为用户地址
    if (!is_user_vaddr(vaddr)) {
      exit_spe ();
    }
    // 检查页
    void *ptr = pagedir_get_page (thread_current()->pagedir, vaddr);
    if (!ptr) {
      exit_spe ();
    }
    // 检查内容
    uint8_t *check_byteptr = (uint8_t *) vaddr;
    for (uint8_t i = 0; i < 4; i++) {
      if (get_user(check_byteptr + i) == -1) {
        exit_spe ();
      }
    }
  
    return ptr;
  }
  ```

  实现*get_user*函数

  ```C
  static int 
  get_user (const uint8_t *uaddr)
  {
    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
    return result;
  }
  ```

  这里采用了pintos文档**3.1.5 Accessing User Memory**的第二种方法

  > 第二种方法是仅检查用户指针是否指向“PHYS_BASE”下方，然后解引用。无效的用户指针将导致“页面错误”，您可以通过修改“userprog/exception.c”中的“page_fault()”的代码来处理。该技术通常更快，因为它利用了处理器的MMU,因此倾向于在实际内核（包括Linux）中使用。

  实现*exit_spe*函数，用来在发生错误时返回-1

  ```C
  void 
  exit_spe (void)
  {
    thread_current()->st_exit = -1;
    thread_exit ();
  }
  ```

- 初始化系统调用

  - \<syscall.c\>

  ```C
  void
  syscall_init (void)
  {
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    /*---------------my code-------------*/
    syscalls[SYS_HALT] = &syscall_halt;
    syscalls[SYS_EXIT] = &syscall_exit;
    syscalls[SYS_EXEC] = &syscall_exec;
    syscalls[SYS_WAIT] = &syscall_wait;
    syscalls[SYS_CREATE] = &syscall_create;
    syscalls[SYS_REMOVE] = &syscall_remove;
    syscalls[SYS_OPEN] = &syscall_open;
    syscalls[SYS_WRITE] = &syscall_write;
    syscalls[SYS_SEEK] = &syscall_seek;
    syscalls[SYS_TELL] = &syscall_tell;
    syscalls[SYS_CLOSE] =&syscall_close;
    syscalls[SYS_READ] = &syscall_read;
    syscalls[SYS_FILESIZE] = &syscall_fsize;
    /*---------------my code-------------*/
  }
  ```

  `SYS_HALT`等系统调用号已经被说明在\<syscall-nr.h\>中，拿来用即可。

- 完成函数*syscall_handler*

  - \<syscall.c\>

  由*syscall_init*可知，中断0x30发生时，系统将根据系统调用号选择相应的函数指针，从而调用对应函数。

  ```C
  static void
  syscall_handler (struct intr_frame *f UNUSED)
  {
    /*---------------my code-------------*/
    int * p = f->esp;
    ckptr2 (p + 1);
    int type = *(int *)f->esp;
    if(type >= max_syscall || type <= 0) {
      exit_spe ();
    }
    syscalls[type](f);
    /*---------------my code-------------*/
  }
  ```

- 对新定义的数据结构进行初始化

  - \<threads.c\>

  在*init_thread*中增加

  ```C
  t->st_exit = UINT32_MAX;
  t->success = true;
  t->parent = (t != initial_thread) ? thread_current () : NULL;
  
  list_init (&t->children);
  
  sema_init (&t->sema, 0);
  ```

  对新创建的进程，在*thread_create*中增加

  ```C
  t->thread_child = malloc(sizeof(struct child));
  t->thread_child->tid = tid;
  sema_init (&t->thread_child->sema, 0);
  list_push_back (&thread_current()->children, &t->thread_child->child_elem);
  t->thread_child->store_exit = UINT32_MAX;
  t->thread_child->is_run = false;
  ```

- 实现系统调用

  - \<syscall.c\>

  实现***halt***，只需调用`shutdown_power_off()`

  ```C
  void 
  syscall_halt (struct intr_frame* f)
  {
    shutdown_power_off();
  }
  ```

  - \<syscall.c\>

  实现***exit***

  ```C
  void 
  syscall_exit (struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 1);
    *user_ptr++;
    thread_current()->st_exit = *user_ptr;
    thread_exit ();
  }
  ```

  - \<syscall.c\>

  实现***exec***

  ```C
  void 
  syscall_exec (struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 1);  // 检查地址
    ckptr2 (*(user_ptr + 1));  // 检查值
    *user_ptr++;
    f->eax = process_execute((char*)* user_ptr);
  }
  ```

  - \<syscall.c\>

  实现***wait***，根据

  > 我们建议您根据函数顶部的注释实现`process_wait()`，然后再根据`process_wait()`实现`wait`系统调用。

  ```C
  void 
  syscall_wait (struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 1);
    *user_ptr++;
    f->eax = process_wait(*user_ptr);
  }
  ```

  - \<process.c\>

  补全*process_wait*函数

  ```C
  int
  process_wait (tid_t child_tid UNUSED)
  {
    struct list *l = &thread_current()->children;
    struct list_elem *temp;
    temp = list_begin (l);
    struct child *temp2 = NULL;
    while (temp != list_end (l)) {
      temp2 = list_entry (temp, struct child, child_elem);
      if (temp2->tid == child_tid) {
        if (!temp2->is_run) {
          temp2->is_run = true;
          sema_down (&temp2->sema);  // 阻塞，等待子线程结束
          break;
        } 
        else {
          return -1;
        }
      }
      temp = list_next (temp);
    }
    if (temp == list_end (l)) {
      return -1;
    }
    list_remove (temp);
    return temp2->store_exit;
  }
  ```

  - \<threads.c\>

  在子线程退出时执行*sema_up*，需要在*thread_exit*中加入

  ```C
  thread_current ()->thread_child->store_exit = thread_current()->st_exit;
  sema_up (&thread_current()->thread_child->sema);
  ```

#### 3.4.3 实验结果

通过了halt、exit、exec\*、wait\*。

### 3.5 任务3：File Operation Syscalls

#### 3.5.1 任务描述

在“userprog/syscall.c”中实现系统调用处理程序。它将需要检索系统调用号，然后检索系统调用参数，并执行适当的操作。

需要完成的系统调用如下：

- System Call: bool **create** (const char *file, unsigned initial\_size)

  创建一个名为file的新文件，其初始大小为 initial_size个字节。如果成功，则返回true，否则返回false。创建新文件不会打开它：打开新文件是一项单独的操作，需要系统调用“open”。

- System Call: bool **remove** (const char *file)

  删除名为file的文件。如果成功，则返回true，否则返回false。不论文件是打开还是关闭，都可以将其删除，并且删除打开的文件不会将其关闭。

- System Call: int **open** (const char *file)

  - 打开名为 file 的文件。 返回一个称为“文件描述符”（fd）的非负整数句柄；如果无法打开文件，则返回-1

  - 为控制台保留编号为0和1的文件描述符：fd 0（`STDIN_FILENO`）是标准输入，fd 1（`STDOUT_FILENO`)是标准输出。 open系统调用将永远不会返回这两个文件描述符中的任何一个，这些文件描述符仅在以下明确描述的情况下才作为系统调用参数有效

  - 每个进程都有一组独立的文件描述符。 文件描述符不被子进程继承

  - 当单个文件多次打开（无论是通过单个进程还是通过不同进程）时，每个“open”都将返回一个新的文件描述符。 单个文件的不同文件描述符在对`close`的单独调用中独立关闭，并且它们不共享文件位置。

- System Call: int **filesize** (int fd)

  返回以fd打开的文件的大小（以字节为单位）

- System Call: int **read** (int fd, void *buffer, unsigned size)

  从打开为fd的文件中读取size个字节到buffer中。返回实际读取的字节数（文件末尾为0），如果无法读取文件（由于文件末尾以外的条件），则返回-1。 fd 0使用input_getc()从键盘读取。

- System Call: int **write** (int fd, const void *buffer, unsigned size)

  - 将size个字节从buffer写入打开的文件fd。返回实际写入的字节数，如果某些字节无法写入，则可能小于size。

  - 在文件末尾写入通常会扩展文件，但是基本文件系统无法实现文件增长。预期的行为是在文件末尾写入尽可能多的字节并返回实际写入的数字，如果根本无法写入任何字节，则返回0。

  - fd 1写入控制台。您写入控制台的代码应在一次调用`putbuf()`中写入所有 buffer ，至少只要 size 不超过几百个字节。（打破较大的缓冲区是合理的。）否则，不同进程输出的文本行可能最终会在控制台上交错出现，从而使人类读者和我们的评分脚本感到困惑。

- System Call: void **seek** (int fd, unsigned position)

  - 将打开文件fd中要读取或写入的下一个字节更改为position，以从文件开头开始的字节表示。（因此，position为0是文件的开始。）

  - 越过当前文件的末尾进行查找不是错误。后续的读操作将获得0字节，表示文件结束。后续的写操作将扩展文件，并用零填充所有未写入的间隙。（但是，在Pintos中，文件在项目4完成之前都具有固定长度，因此文件末尾的写入将返回错误。）这些语义在文件系统中实现，并且在系统调用实现中不需要任何特殊的工作。

- System Call: unsigned **tell** (int fd)

  返回打开文件fd中要读取或写入的下一个字节的位置，以从文件开头开始的字节数表示。

- System Call: void **close** (int fd)

  关闭文件描述符fd。退出或终止进程会隐式关闭其所有打开的文件描述符，就像通过为每个进程调用此函数一样。

#### 3.5.2 实验过程

##### DATA STRUCTURES

需要在thread结构体中增加成员变量满足文件管理的需求

- \<threads.h\>

```C
struct thread
  {
    ...
        
    /*---------------my code-------------*/
    ...
        
    struct list files;
    int f_folder;
    struct file * f_owned;
    /*---------------my code-------------*/
  };

// 存在files里
struct thread_file
  {
    int fd;
    struct file* file;
    struct list_elem file_elem;
  };
```

#####  ALGORITHMS

- 建立文件锁，保证文件读写时只有一个线程在操作

  - \<threads.c\>

  ```C
  static struct lock lock_f;
  
  void 
  acquire_lock_f ()
  {
    lock_acquire(&lock_f);
  }
  
  void 
  release_lock_f ()
  {
    lock_release(&lock_f);
  }
  ```

- 初始化新定义的数据结构

  - \<threads.c\>

  在*thread_init*中加入

  ```C
  lock_init(&lock_f);
  ```

  在*init_thread*中加入

  ```C
  list_init (&t->files);
  ```

- 线程退出时应该释放所有文件

  - \<threads.c\>

  ```C
  void
  thread_exit (void) 
  {
    ASSERT (!intr_context ());
  
  #ifdef USERPROG
    process_exit ();
  #endif
  
    intr_disable ();
  
    /*---------------my code-------------*/
    printf ("%s: exit(%d)\n", thread_name(), thread_current()->st_exit);
    
    thread_current ()->thread_child->store_exit = thread_current()->st_exit;
    sema_up (&thread_current()->thread_child->sema);
  
    // 退出时自动关闭文件
    file_close (thread_current ()->f_owned);
  
    struct list *files = &thread_current()->files;
    while(!list_empty (files)) {
      struct list_elem *e = list_pop_front (files);
      struct thread_file *f = list_entry (e, struct thread_file, file_elem);
      acquire_lock_f ();
      file_close (f->file);
      release_lock_f ();
      
      list_remove (e);
      
      free (f);
    }
    /*---------------my code-------------*/
  
    /* Remove thread from all threads list, set our status to dying,
       and schedule another process.  That process will destroy us
       when it calls thread_schedule_tail(). */
    list_remove (&thread_current()->allelem);
    thread_current ()->status = THREAD_DYING;
    schedule ();
    NOT_REACHED ();
  }
  ```

- 实现系统调用

  - \<syscall.c\>

  实现**create**

  ```C
  void 
  syscall_create(struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 1);
    ckptr2 (*(user_ptr + 1));
    *user_ptr++;
    acquire_lock_f ();
    f->eax = filesys_create ((const char *)*user_ptr, *(user_ptr+1));
    release_lock_f ();
  }
  ```

  - \<syscall.c\>

  实现**remove**

  ```C
  void 
  syscall_remove(struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 1);
    ckptr2 (*(user_ptr + 1));
    *user_ptr++;
    acquire_lock_f ();
    f->eax = filesys_remove ((const char *)*user_ptr);
    release_lock_f ();
  }
  ```

  - \<syscall.c\>

  实现**open**

  ```C
  void 
  syscall_open (struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 1);
    ckptr2 (*(user_ptr + 1));
    *user_ptr++;
    acquire_lock_f ();
    struct file *file_opened = filesys_open((const char *)*user_ptr);
    release_lock_f ();
    struct thread *t = thread_current();
    if (file_opened) {
      struct thread_file *thread_file_temp = malloc(sizeof(struct thread_file));
      thread_file_temp->file = file_opened;
      thread_file_temp->fd = t->f_folder++;
      list_push_back (&t->files, &thread_file_temp->file_elem);
      f->eax = thread_file_temp->fd;
    }
    else {
      f->eax = -1;
    }
  }
  ```

  - \<syscall.c\>

  实现**filesize**

  ```C
  void 
  syscall_fsize (struct intr_frame* f) {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 1);
    *user_ptr++;
    struct thread_file * thread_file_temp = find_f_name (*user_ptr);
    if (thread_file_temp) {
      acquire_lock_f ();
      f->eax = file_length (thread_file_temp->file);
      release_lock_f ();
    } 
    else {
      f->eax = -1;
    }
  }
  ```

  - \<syscall.c\>

  实现**read**。读size大小的文件fd到buffer中，对fd0，认为是标准输入，调用`input_getc()`。返回值是size，失败则返回-1。

  ```C
  void 
  syscall_read (struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    *user_ptr++;
    int fd = *user_ptr;
    int i;
    uint8_t * buffer = (uint8_t*)*(user_ptr + 1);
    off_t size = *(user_ptr + 2);
    if (!is_valid_pointer (buffer + size, 1) || !is_valid_pointer (buffer, 1)) {
      exit_spe ();
    }
  
    if (fd == 0) {
      // 标准输入
      for (i = 0; i < size; i++)
        buffer[i] = input_getc();
      f->eax = size;
    }
    else {
      struct thread_file * thread_file_temp = find_f_name (*user_ptr);
      if (thread_file_temp) {
        acquire_lock_f ();
        f->eax = file_read (thread_file_temp->file, buffer, size);
        release_lock_f ();
      }
      else {
        f->eax = -1;
      }
    }
  }
  ```

  - \<syscall.c\>

  实现**write**。写size大小的文件fd到buffer中，对fd1，认为是标准输出，调用`putbuf()`。返回值是size，失败则返回0。

  ```C
  void 
  syscall_write (struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 7);
    ckptr2 (*(user_ptr + 6));
    *user_ptr++;
    int fd = *user_ptr;
    const char * buffer = (const char *)*(user_ptr + 1);
    off_t size = *(user_ptr + 2);
    if (fd != 1) {
      struct thread_file * thread_file_temp = find_f_name (*user_ptr);
      if (thread_file_temp) {
        acquire_lock_f ();
        // 返回写入的字节数
        f->eax = file_write (thread_file_temp->file, buffer, size);
        release_lock_f ();
      } 
      else {
        // 无法写入
        f->eax = 0;
      }
    }
    else {
      // 打印到控制台 fd1
      putbuf(buffer, size);
      f->eax = size;
    }
  }
  ```

  - \<syscall.c\>

  实现**seek**

  ```C
  void 
  syscall_seek(struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 5);
    *user_ptr++;
    struct thread_file *file_temp = find_f_name (*user_ptr);
    if (file_temp) {
      acquire_lock_f ();
      file_seek (file_temp->file, *(user_ptr+1));
      release_lock_f ();
    }
  }
  ```

  - \<syscall.c\>

  实现**tell**

  ```C
  void 
  syscall_tell (struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 1);
    *user_ptr++;
    struct thread_file *thread_file_temp = find_f_name (*user_ptr);
    if (thread_file_temp) {
      acquire_lock_f ();
      f->eax = file_tell (thread_file_temp->file);
      release_lock_f ();
    }
    else {
      f->eax = -1;
    }
  }
  ```

  - \<syscall.c\>

  实现**close**

  ```C
  void 
  syscall_close (struct intr_frame* f)
  {
    uint32_t *user_ptr = f->esp;
    ckptr2 (user_ptr + 1);
    *user_ptr++;
    struct thread_file * opened_file = find_f_name (*user_ptr);
    if (opened_file) {
      acquire_lock_f ();
      file_close (opened_file->file);
      release_lock_f ();
      list_remove (&opened_file->file_elem);
      free (opened_file);
    }
  }
  ```

#### 3.5.3 实验结果

80个测试点全部pass

## 4 总结及展望

pintos难度很高，单人完成不管是从工作量上还是从学习成本上都非常可怕，不过也正应如此学习和完成pintos的过程才格外地具有挑战性和价值。不过说实话我个人认为教师对课程设计缺少必要的指导，导致部分同学（比如说我）在前期的探索过程中不必要地浪费了大量时间——这些时间原本可以用来更好地学习和钻研操作系统。

对于这学期的操作系统课程，经过一学期学习，我对操作系统的架构、原理有了一定了解，也具体学习了操作系统的线程、调度、内存管理、虚存技术、磁盘管理等的具体算法与策略。我认为我们应该学习前人们的探索和创新精神，对面对复杂工程问题大胆假设、小心求证，利用理论知识建立分析模型，从而设计出良好的方案，对环境和社会的可持续发展做出贡献。

回顾这学期的学习, 我收获了许多知识, 也有不小的遗憾, 还有很多东西需要我去学习、应用和探索。
