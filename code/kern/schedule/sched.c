#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
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
void schedule(void)
{
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        do
        {
            if ((le = list_next(le)) != &proc_list)
            {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE)
                {
                    break;
                }
            }
        } while (le != last);
        // 循环正常结束时 next 是最后检查过的那个线程，未必可运行，所以要再判断一次状态
        if (next == NULL || next->state != PROC_RUNNABLE)
        {
            next = idleproc;
        }
        next->runs++;
        if (next != current)
        {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}


