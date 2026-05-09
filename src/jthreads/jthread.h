#pragma once

#include <string>

struct jthread_t;
struct jthread_mutex_t;
struct jthread_cond_t;

using jthread_start_t = void* (*)(void*);

void jthread_init(const std::string& trace_path = "trace1.json");

jthread_t* jthread_create(jthread_start_t start_proc, void* start_arg = nullptr);
jthread_t* jthread_self();
void jthread_block();
void jthread_unblock(jthread_t* thread);
void jthread_join(jthread_t* thread, void** value_ptr = nullptr);
void jthread_detach(jthread_t* thread);
void jthread_yield();
void jthread_sleep(int worker_id, int seconds);

jthread_mutex_t* jthread_mutex_create(const char* debug_name = nullptr);
void jthread_mutex_lock(jthread_mutex_t* mutex);
void jthread_mutex_unlock(jthread_mutex_t* mutex);
void jthread_mutex_destroy(jthread_mutex_t* mutex);

jthread_cond_t* jthread_cond_create(jthread_mutex_t* mutex = nullptr, const char* debug_name = nullptr);
void jthread_cond_wait(jthread_cond_t* cond);
void jthread_cond_wait(jthread_cond_t* cond, jthread_mutex_t* mutex);
void jthread_cond_signal(jthread_cond_t* cond);
void jthread_cond_broadcast(jthread_cond_t* cond);
void jthread_cond_destroy(jthread_cond_t* cond);
