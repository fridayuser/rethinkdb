// Copyright 2010-2013 RethinkDB, all rights reserved.
#ifndef ARCH_RUNTIME_MESSAGE_HUB_HPP_
#define ARCH_RUNTIME_MESSAGE_HUB_HPP_

#include <pthread.h>
#include <strings.h>

#include "arch/runtime/event_queue.hpp"
#include "arch/runtime/runtime_utils.hpp"
#include "arch/runtime/system_event.hpp"
#include "arch/spinlock.hpp"
#include "config/args.hpp"
#include "containers/intrusive_list.hpp"
#include "utils.hpp"

class linux_thread_pool_t;

/* There is one message hub per thread, NOT one message hub for the entire program.

Each message hub stores messages that are going from that message hub's home thread to
other threads. It keeps a separate queue for messages destined for each other thread. */

class linux_message_hub_t : private linux_event_callback_t {
public:
    typedef intrusive_list_t<linux_thread_message_t> msg_list_t;

    linux_message_hub_t(linux_event_queue_t *queue, linux_thread_pool_t *thread_pool,
                        threadnum_t current_thread);

    /* For each thread, transfer messages from our msg_local_list for that thread to our
    msg_global_list for that thread */
    void push_messages();

    /* Schedules the given message to be sent to the given thread by pushing it onto our
    msg_local_list for that thread */
    void store_message_ordered(threadnum_t nthread, linux_thread_message_t *msg);

    // Schedules the given message to be sent to the given thread.  However, these are not
    // guaranteed to be called in the same order relative to one another.
    void store_message_sometime(threadnum_t nthread, linux_thread_message_t *msg);

    // Called by the thread pool when it needs to deliver a message from the main thread
    // (which does not have an event queue)
    void insert_external_message(linux_thread_message_t *msg);

    ~linux_message_hub_t();

private:
    // Does store_message or store_message_sometime, only without setting the reloop_count_ in
    // debug mode.
    void do_store_message(threadnum_t nthread, linux_thread_message_t *msg);
    
    // TODO! Document
    void deliver_local_messages();


    // TODO! Remove? This doesn't seem to be implemented.
    /* pull_messages should be called on thread N with N as its argument. (The argument is
    partially redundant.) It will cause the actual delivery of messages that originated
    on this->current_thread and are destined for thread N. It is (almost) the only method on
    linux_message_hub_t that is not called on the thread that the message hub belongs to. */
    void pull_messages(threadnum_t thread);
    
    msg_list_t &get_priority_msg_list(int priority);

    linux_event_queue_t *const queue_;
    linux_thread_pool_t *const thread_pool_;

    /* Queue for messages going from this->current_thread to other threads */
    struct thread_queue_t {
        //TODO this doesn't need to be a class anymore

        /* Messages are cached here before being pushed to the global list so that we don't
        have to acquire the spinlock as often */
        msg_list_t msg_local_list;
    } queues_[MAX_THREADS];

    // Must only be used with acquired incoming_messages_lock_
    bool check_and_set_is_woken_up();
    bool is_woken_up_;
    msg_list_t incoming_messages_;
    spinlock_t incoming_messages_lock_;
    
    // TODO! Document
    msg_list_t priority_msg_lists_[
            MESSAGE_SCHEDULER_MAX_PRIORITY - MESSAGE_SCHEDULER_MIN_PRIORITY + 1];

    void on_event(int events);

    // The eventfd (or pipe-based alternative) notified after the first incoming
    // message is put onto incoming_messages_.
    system_event_t event_;

    /* The thread that we queue messages originating from. (Recall that there is one
    message_hub_t per thread.) */
    const threadnum_t current_thread_;

    DISABLE_COPYING(linux_message_hub_t);
};

#endif // ARCH_RUNTIME_MESSAGE_HUB_HPP_
