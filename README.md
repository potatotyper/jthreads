# jthreads

A spinoff of CPSC 213 uthreads threadpool program (no code used from uthread library for this program) with a vizualiser to help students further undestand the basics of threadpools and multihreadding :)

## Jthreads library how to use
#### Thread functions
- jthread_init()
- jthread_create()
- jthread_self()
- jthread_block()
- jthread_unblock()
- jthread_join()
- jthread_detach()
- jthread_yield()

#### Mutex functions
- jthread_mutex_create()
- jthread_mutex_lock()
- jthread_mutex_unlock()
- jthread_mutex_destroy()

#### Condition variable functions
- jthread_cond_create()
- jthread_cond_wait()
- jthread_cond_signal()
- jthread_cond_broadcast()
-jthread_cond_destroy()

## Build and run C++ example

```bash
cmake -S . -B build
cmake --build build
./build/basicprogram1
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
