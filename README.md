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

#### Mutex functions
- jthread_mutex_create(debug_name = nullptr)
- jthread_mutex_lock(mutex)
- jthread_mutex_unlock(mutex)
- jthread_mutex_destroy(mutex)

#### Condition variable functions
- jthread_cond_create(mutex = nullptr, debug_name = nullptr)
- jthread_cond_wait(cond)
- jthread_cond_wait(cond, mutex)
- jthread_cond_signal(cond)
- jthread_cond_broadcast(cond)
- jthread_cond_destroy(cond)

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
./build/basicprogram1
./build/jthreads_demo
```

## Visualizer

The visualizer is in `visualizer/` and uses React + TypeScript + Vite.

```bash
cd visualizer
npm install
npm run dev
```

### Supported features

- time cursor `t` via slider, playback controls, and numeric input
- per-thread swimlane task timeline
- point-in-time thread state table
- lock contention overlays and current lock owner view
- current flag values at time `t`
