// Examples borrowed from http://www.practicalsynthesis.org/PSynSyn.html

/*
 * Dekker's critical section algorithm, implemented with fences.
 *
 * URL:
 *   http://www.justsoftwaresolutions.co.uk/threading/
 */

#include <atomic>
#include "threads.h"
//#include <assert.h>
#include <stdlib.h>/* //srand, rand */
#include <time.h>       /* time */

#include "librace.h"
#include "mem_op_macros.h"
#include "model-assert.h"

atomic_int que, deq;

// std::atomic<int> que("queu"), deq("deq");

uint32_t critical_section = 0;

void * p0(void *arg)
{
    int t1_loop_itr_bnd = 3;
    int i_t1 = 0;
    while(++i_t1 <= t1_loop_itr_bnd){
        
        int t1q = fetch_add(&que, 1, std::memory_order_relaxed);
        while (t1q != load(&deq, std::memory_order_relaxed)){
            thrd_yield();
        }
        // critical section
        store_32(&critical_section, 1);

        int t1deq = load(&deq,  std::memory_order_relaxed);
        ++t1deq;
        store(&deq, t1deq, std::memory_order_relaxed);
    }
return NULL;}

void * p1(void *arg)
{
    int t2_loop_itr_bnd = 3;
    int i_t2 = 0;
    while(++i_t2 <= t2_loop_itr_bnd){

        int t2q = fetch_add(&que, 1, std::memory_order_relaxed);
        while (t2q != load(&deq, std::memory_order_relaxed)){
            thrd_yield();
        }
        // critical section
        store_32(&critical_section, 1);

        int t2deq = load(&deq,  std::memory_order_relaxed);
        ++t2deq;
        store(&deq, t2deq, std::memory_order_relaxed);
    }
return NULL;}

int main(int argc, char **argv)
{
    thrd_t a, b;

    store(&deq, 0, std::memory_order_seq_cst);
    store(&que, 0, std::memory_order_seq_cst);

    thrd_create(&a, p0, NULL);
    thrd_create(&b, p1, NULL);

    thrd_join(a);
    thrd_join(b);

    return 0;
}
