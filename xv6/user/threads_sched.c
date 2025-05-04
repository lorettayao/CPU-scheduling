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

struct threads_sched_result schedule_dm(struct threads_sched_args args) {
    struct thread *selected = NULL;
    struct thread *th;
    struct release_queue_entry *cur, *nxt;

    // Step 1: release threads to run_queue
    list_for_each_entry_safe(cur, nxt, &release_queue, thread_list) {
        if (args.now >= cur->release_time) {
            cur->thrd->remaining_time = cur->thrd->processing_time;
            cur->thrd->current_deadline = cur->release_time + cur->thrd->deadline;
            list_add_tail(&cur->thrd->thread_list, &run_queue);
            list_del(&cur->thread_list);
            free(cur);
        }
    }

    // Step 2: check deadline miss
    struct thread *miss = __check_deadline_miss(&run_queue, args.now);
    if (miss != NULL) {
        return (struct threads_sched_result){
            .selected = NULL,
            .missed = miss
        };
    }

    // Step 3: select highest priority thread using DM
    list_for_each_entry(th, &run_queue, thread_list) {
        if (selected == NULL || __dm_thread_cmp(th, selected) < 0) {
            selected = th;
        }
    }

    // Step 4: remove selected from run_queue
    if (selected != NULL) {
        list_del(&selected->thread_list);
    }

    return (struct threads_sched_result){
        .selected = selected,
        .missed = NULL
    };
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