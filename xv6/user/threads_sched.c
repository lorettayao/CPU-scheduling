#include "kernel/types.h"
#include "user/user.h"
#include "user/list.h"
#include "user/threads.h"
#include "user/threads_sched.h"
#include <limits.h>
#define NULL 0

/* default scheduling algorithm */
#ifdef THREAD_SCHEDULER_DEFAULT
struct threads_sched_result schedule_default(struct threads_sched_args args)
{
    struct thread *thread_with_smallest_id = NULL;
    struct thread *th = NULL;
    list_for_each_entry(th, args.run_queue, thread_list) {
        if (thread_with_smallest_id == NULL || th->ID < thread_with_smallest_id->ID)
            thread_with_smallest_id = th;
    }

    struct threads_sched_result r;
    if (thread_with_smallest_id != NULL) {
        r.scheduled_thread_list_member = &thread_with_smallest_id->thread_list;
        r.allocated_time = thread_with_smallest_id->remaining_time;
    } else {
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = 1;
    }

    return r;
}
#endif

/* MP3 Part 1 - Non-Real-Time Scheduling */

// HRRN
#ifdef THREAD_SCHEDULER_HRRN
struct threads_sched_result schedule_hrrn(struct threads_sched_args args)
{
    struct threads_sched_result r;
    // TO DO
    struct thread *th = NULL;
    struct thread *selected = NULL;

    int best_n = -1;
    int best_d = 1;

    list_for_each_entry(th,args.run_queue, thread_list){
        int waiting_time = args.current_time - (th->arrival_time);
        int response_ratio_n = waiting_time + th->processing_time;
        int response_ratio_d = th->processing_time;

        if (selected == NULL || 
            response_ratio_n * best_d > best_n * response_ratio_d ||
            (response_ratio_n * best_d == best_n * response_ratio_d && th->ID < selected->ID)) {

            selected = th;
            best_n = response_ratio_n;
            best_d = response_ratio_d;
        }
    }
    if (selected != NULL) {
        r.scheduled_thread_list_member = &selected->thread_list;
        r.allocated_time = selected->remaining_time;
    } else {
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = 1;
    }
    

    return r;
}
#endif

#ifdef THREAD_SCHEDULER_PRIORITY_RR
// priority Round-Robin(RR)
struct threads_sched_result schedule_priority_rr(struct threads_sched_args args) 
{
    // struct threads_sched_result r;
    // TO DO
    struct threads_sched_result r;
    struct thread *th;

    // 1) Find highest priority among runnable threads
    int highest_priority = INT_MAX;
    list_for_each_entry(th, args.run_queue, thread_list) {
        if (th->remaining_time > 0 && th->priority < highest_priority) {
            highest_priority = th->priority;
        }
    }

    // 2) If none runnable, idle
    if (highest_priority == INT_MAX) {
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = 1;
        return r;
    }

    // 3) Count how many threads share that highest priority
    int count = 0;
    list_for_each_entry(th, args.run_queue, thread_list) {
        if (th->remaining_time > 0 && th->priority == highest_priority) {
            count++;
        }
    }

    // 4) Pick the first runnable thread at that priority
    list_for_each_entry(th, args.run_queue, thread_list) {
        if (th->remaining_time > 0 && th->priority == highest_priority) {
            r.scheduled_thread_list_member = &th->thread_list;

            // 5) If only one thread in this group, run it to completion;
            //    otherwise, use one quantum.
            if (count == 1) {
                r.allocated_time = th->remaining_time;
            } else {
                r.allocated_time = (th->remaining_time < args.time_quantum)
                                   ? th->remaining_time
                                   : args.time_quantum;
            }
            return r;
        }
    }

    // 6) Fallback (shouldn’t happen)
    r.scheduled_thread_list_member = args.run_queue;
    r.allocated_time = 1;
    return r;
}
#endif

/* MP3 Part 2 - Real-Time Scheduling*/

#if defined(THREAD_SCHEDULER_EDF_CBS) || defined(THREAD_SCHEDULER_DM)
static struct thread *__check_deadline_miss(struct list_head *run_queue, int current_time)
{
    struct thread *th = NULL;
    struct thread *thread_missing_deadline = NULL;
    list_for_each_entry(th, run_queue, thread_list) {
        if (th->current_deadline <= current_time) {
            if (thread_missing_deadline == NULL)
                thread_missing_deadline = th;
            else if (th->ID < thread_missing_deadline->ID)
                thread_missing_deadline = th;
        }
    }
    return thread_missing_deadline;
}
#endif

#ifdef THREAD_SCHEDULER_DM
/* Deadline-Monotonic Scheduling */
// [Loretta]
static int __dm_thread_cmp(struct thread *a, struct thread *b)
{
    //To DO
    if(a->period > b->period)
        return 1;
    else if(a->period < b->period)
        return -1;
    else
        return a->ID - b->ID;
    
}
// [Loretta]
// extern struct list_head run_queue;
// extern struct list_head release_queue;

// [Loretta]
struct threads_sched_result schedule_dm(struct threads_sched_args args) {
    struct threads_sched_result r;
    int now = args.current_time;
    struct thread *th, *selected = NULL;
    struct release_queue_entry *cur, *nxt;

    // --- [Loretta][step 1] Release step 开始 ---
    // printf("[Loretta][step 1] now=%d Release step\n", now);
    // 打印 release_queue
    // printf("[Loretta] release_queue:");
    // list_for_each_entry(cur, args.release_queue, thread_list) {
    //     printf(" (#%d@%d)", cur->thrd->ID, cur->release_time);
    // }
    // printf("\n");
    // 执行 release
    list_for_each_entry_safe(cur, nxt, args.release_queue, thread_list) {
        if (now >= cur->release_time) {
            cur->thrd->remaining_time   = cur->thrd->processing_time;
            cur->thrd->current_deadline = cur->release_time + cur->thrd->deadline;
            list_add_tail(&cur->thrd->thread_list, args.run_queue);
            list_del(&cur->thread_list);
            free(cur);
        }
    }
    // 打印 run_queue 状态
    // printf("[Loretta] after release, run_queue:");
    // list_for_each_entry(th, args.run_queue, thread_list) {
    //     printf(" (#%d rt=%d dl=%d)", th->ID, th->remaining_time, th->current_deadline);
    // }
    // printf("\n");

    // --- [Loretta][step 2] Deadline‐miss check ---
    // printf("[Loretta][step 2] Deadline‐miss check\n");
    struct thread *missed = __check_deadline_miss(args.run_queue, now);
    if (missed) {
        // printf("[Loretta] missed thread#%d (dl=%d)\n",
        //        missed->ID, missed->current_deadline);
        r.scheduled_thread_list_member = &missed->thread_list;
        r.allocated_time = 0;
        return r;
    }

    // --- [Loretta][step 3] Select runnable threads ---
    // printf("[Loretta][step 3] Selecting runnable threads\n");
    int runnable_count = 0;
    list_for_each_entry(th, args.run_queue, thread_list) {
        if (th->remaining_time > 0) {
            runnable_count++;
            if (!selected || __dm_thread_cmp(th, selected) < 0)
                selected = th;
        }
    }
    // if (runnable_count > 0) {
    //     printf("[Loretta] runnable_count=%d, selected thread#%d (rt=%d dl=%d)\n",
    //            runnable_count,
    //            selected->ID,
    //            selected->remaining_time,
    //            selected->current_deadline);
    // } else {
    //     printf("[Loretta] runnable_count=0, no selected thread\n");
    // }


    // --- [Loretta][step 4] Idle or sleep? ---
    if (runnable_count == 0) {
        // printf("[Loretta][step 4] run_queue empty, computing sleep\n");
        int sleep_time = 1;
        list_for_each_entry(cur, args.release_queue, thread_list) {
            int dt = cur->release_time - now;
            if (dt > 0 && (sleep_time == 1 || dt < sleep_time))
                sleep_time = dt;
        }
        if (sleep_time < 1) sleep_time = 1;
        // printf("[Loretta] sleep_time=%d\n", sleep_time);
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = sleep_time;
        return r;
    }

    // --- [Loretta][step 5] Preemption check ---
    // printf("[Loretta][step 5] Preemption check for thread#%d\n", selected->ID);
    if (now + 1 > selected->current_deadline) {
        // printf("[Loretta] will miss deadline if run 1 tick\n");
        r.scheduled_thread_list_member = &selected->thread_list;
        r.allocated_time = 0;
        return r;
    }

    // --- [Loretta][step 6] Dispatch ---
    // printf("[Loretta][step 6] Dispatching thread#%d for %s ticks\n",
    //        selected->ID,
    //        (runnable_count == 1) ? "all remaining" : "1");
    r.scheduled_thread_list_member = &selected->thread_list;
    r.allocated_time = (runnable_count == 1)
                       ? selected->remaining_time
                       : 1;
    return r;
}

    
#endif


#ifdef THREAD_SCHEDULER_EDF_CBS
// EDF with CBS comparation
static int __edf_thread_cmp(struct thread *a, struct thread *b)
{
    // TO DO
}
//  EDF_CBS scheduler
struct threads_sched_result schedule_edf_cbs(struct threads_sched_args args)
{
    struct threads_sched_result r;

    // notify the throttle task
    // TO DO

    // first check if there is any thread has missed its current deadline
    // TO DO

    // handle the case where run queue is empty
    // TO DO

    return r;
}
#endif