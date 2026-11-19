#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

//设置 proc_strucut->state 为 runnalbe. 
void wakeup_proc(struct proc_struct *proc) 
{
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

//LAB4 function3 2411264
/*
schedule - 最简单的 FIFO / 轮转调度：
1) 把 current->need_resched 清 0；
2) 在 proc_list 中找下一个 PROC_RUNNABLE 的线程 next：
   - current 不是 idleproc：从 current 的下一个位置开始找，转一圈回到 current 为止（current 自己也在候选之列，
     所以只剩它一个可运行时会选回它自己），实现轮转；
   - current 是 idleproc：idleproc 不在 proc_list 中，从链表头开始找；
   遍历时跳过链表头 proc_list 本身（它不是某个 proc_struct 的成员，不能用 le2proc 转换）；
3) 找不到就用 idleproc 保底；
4) next->runs++，next 不是 current 时调用 proc_run 切换。
整个过程关中断：遍历链表和切换期间不能被打断。
*/
// ![image](../../../.vscode/image-comment/Screenshot-2026-11-19-162858.png)
void schedule(void)
{
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    local_intr_save(intr_flag); // 关中断：遍历 proc_list 和切换期间不能被时钟中断打断
    
    {
        current->need_resched = 0; // 这次调度已经在处理“要不要让出 CPU”的请求，先清掉标志
        // last 是遍历的起点，同时也是终点：
        //   idleproc 不在链表里，以链表头 proc_list 为起点，一圈就是整条链表；
        //   其他线程以自己的 list_link 为起点，从它的下一个节点开始，转回自己时结束（自己也会被检查到一次）。
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        do
        {
            // proc_list 是链表头（哨兵），不嵌在任何 proc_struct 里，不能做 le2proc；遇到它就跳过
            if ((le = list_next(le)) != &proc_list)
            {
                next = le2proc(le, list_link); // 从链表节点反推出所在的 proc_struct
                if (next->state == PROC_RUNNABLE)
                {
                    break; // 找到第一个可运行的线程，选它
                }
            }
        } while (le != last); // 回到起点说明整圈都看过了
        if (next == NULL || next->state != PROC_RUNNABLE) next = idleproc;  // 循环正常结束时 next 是最后检查过的那个线程，未必可运行，所以要再判断一次状态
        
        next->runs++; // 统计这个线程被调度运行的次数
        if (next != current) proc_run(next); // 切换到 next；等到 current 再次被调度时，才会从这里返回
        // next == current（例如只剩它自己可运行）时什么也不做，继续运行
    }

    local_intr_restore(intr_flag); // 恢复调用 schedule 之前的中断状态
}


