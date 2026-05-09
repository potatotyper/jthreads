# jthreads

A spinoff of CPSC 213 uthreads threadpool program (no code used from uthread library for this program) with a vizualiser to help students further undestand the basics of threadpools and multihreadding :)

## Jthreads library how to use
#### Thread functions
- jthread_init(trace_path = "trace1.json")
- jthread_create(start_proc, start_arg)
- jthread_self()
- jthread_block()
- jthread_unblock(thread)
- jthread_join(thread, &return_value)
- jthread_detach(thread)
- jthread_yield()
- jthread_sleep(worker_id, seconds)

#### Mutex functions
- jthread_mutex_create(mutex_name = nullptr)
- jthread_mutex_lock(mutex)
- jthread_mutex_unlock(mutex)
- jthread_mutex_destroy(mutex)

#### Condition variable functions
- jthread_cond_create(mutex, cond_name = nullptr)
- jthread_cond_wait(cond)
- jthread_cond_wait(cond, mutex)
- jthread_cond_signal(cond)
- jthread_cond_broadcast(cond)
- jthread_cond_destroy(cond)

#### Thread pool, task, tracing, and spinlock helpers
- FixedThreadPool(num_workers)
- FixedThreadPool::submit(callable)
- FixedThreadPool::shutdown()
- Task
- Tracer::emit_event(...)
- Spinlock

```cpp
#include "jthreads/jthread.h"

void* worker(void* arg) {
  auto* mutex = static_cast<jthread_mutex_t*>(arg);
  jthread_mutex_lock(mutex);
  jthread_mutex_unlock(mutex);
  return nullptr;
}

int main() {
  jthread_init("trace1.json");
  auto* mutex = jthread_mutex_create("demo_mutex");
  auto* thread = jthread_create(worker, mutex);
  jthread_join(thread);
  jthread_mutex_destroy(mutex);
}
```

All `jthread_*`, `jthread_mutex_*`, and `jthread_cond_*` calls emit visualizer trace events through `Tracer`.

## Build and run C++ example

```bash
cmake -S . -B build
cmake --build build
./build/sample
```

The sample program is a simple five-thread jthread showcase. It defines five
separate worker functions, and each one follows the same pattern: call
`jthread_sleep` for a random 4-5 seconds, update either the shared counter or
the shared string, then sleep again and update the other kind of value.
`counter_mutex` protects the counter, `word_mutex` protects the string, and each
update emits a condition-variable signal for the visualizer. It writes
`trace1.json` in the directory where you run it.

## Visualizer

The visualizer is in `visualizer/` and uses React + TypeScript + Vite.

```bash
cd visualizer
npm install
npm run dev
```

The Vite dev and preview servers are pinned to `0.0.0.0:5173` with strict port
checking, so deploys fail clearly if that port is unavailable instead of moving
to a different one. For a production-style local check:

```bash
cd visualizer
npm run build
npm run preview
```

To run the visualizer under PM2, build it and start the ecosystem config:

```bash
cd visualizer
npm install
npm run build
pm2 start ecosystem.config.cjs
pm2 save
```

If PM2 logs show npm looking for `/home/mkasa/apps/jthreads/package.json`, delete
the old npm/Vite preview process and start the ecosystem config instead:

```bash
pm2 delete jthreads
pm2 delete jthreads-visualizer
pm2 delete potatotyper-site
pm2 start /home/mkasa/apps/jthreads/visualizer/ecosystem.config.cjs
pm2 save
```

### Supported features

- time cursor `t` via slider, playback controls, and numeric input
- per-thread swimlane task timeline
- point-in-time thread state table
- lock contention overlays and current lock owner view
- current flag values at time `t`
