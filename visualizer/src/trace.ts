import { FlagPoint, LockWaitInterval, TaskInterval, ThreadSnapshot, TraceEvent } from "./types";

type CodePoint = {
  currentCode: string;
  sourceLocation: string;
};

const EVENT_CODE_MAP: Record<string, CodePoint> = {
  worker_start: {
    currentCode: "enter worker loop",
    sourceLocation: "src/jthreads/threadpool.cpp:113",
  },
  lock_attempt: {
    currentCode: "attempt tasks_mtx lock",
    sourceLocation: "src/jthreads/threadpool.cpp:117",
  },
  worker_queue_lock_acquired: {
    currentCode: "acquired queue mutex",
    sourceLocation: "src/jthreads/threadpool.cpp:125",
  },
  lock_acquired: {
    currentCode: "holding tasks_mtx",
    sourceLocation: "src/jthreads/threadpool.cpp:126",
  },
  worker_wait_begin: {
    currentCode: "begin condition wait",
    sourceLocation: "src/jthreads/threadpool.cpp:128",
  },
  worker_wait_end: {
    currentCode: "woke from condition wait",
    sourceLocation: "src/jthreads/threadpool.cpp:136",
  },
  task_dequeued: {
    currentCode: "pop task from queue",
    sourceLocation: "src/jthreads/threadpool.cpp:143",
  },
  lock_released: {
    currentCode: "release tasks_mtx",
    sourceLocation: "src/jthreads/threadpool.cpp:152",
  },
  task_start: {
    currentCode: "execute task callback",
    sourceLocation: "src/jthreads/threadpool.cpp:166",
  },
  task_end: {
    currentCode: "finish task callback",
    sourceLocation: "src/jthreads/threadpool.cpp:172",
  },
  worker_stop: {
    currentCode: "exit worker thread",
    sourceLocation: "src/jthreads/threadpool.cpp:140",
  },
  task_submit: {
    currentCode: "submit task into pool",
    sourceLocation: "src/jthreads/threadpool.h:27",
  },
  submit_lock_acquired: {
    currentCode: "submit acquired tasks_mtx",
    sourceLocation: "src/jthreads/threadpool.h:41",
  },
  task_enqueued: {
    currentCode: "enqueue task into queue",
    sourceLocation: "src/jthreads/threadpool.h:54",
  },
  worker_notified: {
    currentCode: "notify waiting worker",
    sourceLocation: "src/jthreads/threadpool.h:55",
  },
  shutdown_begin: {
    currentCode: "begin pool shutdown",
    sourceLocation: "src/jthreads/threadpool.cpp:92",
  },
  shutdown_notify_all: {
    currentCode: "notify all workers",
    sourceLocation: "src/jthreads/threadpool.cpp:96",
  },
  shutdown_join_wait: {
    currentCode: "join worker thread",
    sourceLocation: "src/jthreads/threadpool.cpp:100",
  },
  shutdown_join_done: {
    currentCode: "worker joined",
    sourceLocation: "src/jthreads/threadpool.cpp:102",
  },
  shutdown_complete: {
    currentCode: "shutdown complete",
    sourceLocation: "src/jthreads/threadpool.cpp:105",
  },
};

export function parseTraceJsonLines(raw: string): TraceEvent[] {
  const lines = raw
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter((line) => line.length > 0);

  const events: TraceEvent[] = [];
  for (const line of lines) {
    try {
      const parsed = JSON.parse(line) as Partial<TraceEvent>;
      if (
        typeof parsed.event === "string" &&
        typeof parsed.state === "string" &&
        typeof parsed.time_since_started_s === "number" &&
        typeof parsed.thread === "number"
      ) {
        events.push(parsed as TraceEvent);
      }
    } catch {
      // Skip malformed line and continue parsing remaining events.
    }
  }

  return events.sort((a, b) => a.time_since_started_s - b.time_since_started_s);
}

export function getTraceBounds(events: TraceEvent[]): { min: number; max: number } {
  if (events.length === 0) {
    return { min: 0, max: 0 };
  }
  return {
    min: events[0].time_since_started_s,
    max: events[events.length - 1].time_since_started_s,
  };
}

export function getThreads(events: TraceEvent[]): number[] {
  return [...new Set(events.map((e) => e.thread))].sort((a, b) => a - b);
}

function getCodePoint(event: TraceEvent): CodePoint {
  return (
    EVENT_CODE_MAP[event.event] ?? {
      currentCode: `event: ${event.event}`,
      sourceLocation: "trace event (no direct source mapping)",
    }
  );
}

export function buildTaskIntervals(events: TraceEvent[]): TaskInterval[] {
  const starts = new Map<number, TraceEvent>();
  const intervals: TaskInterval[] = [];

  for (const event of events) {
    if (event.event === "task_start" && event.task_id !== undefined) {
      starts.set(event.task_id, event);
    }

    if (event.event === "task_end" && event.task_id !== undefined) {
      const start = starts.get(event.task_id);
      if (!start) {
        continue;
      }
      const durationUs = event.duration_us ?? Math.max(0, (event.time_since_started_s - start.time_since_started_s) * 1e6);
      intervals.push({
        taskId: event.task_id,
        thread: start.thread,
        start: start.time_since_started_s,
        end: event.time_since_started_s,
        durationUs,
      });
      starts.delete(event.task_id);
    }
  }

  return intervals.sort((a, b) => a.start - b.start);
}

export function buildLockWaitIntervals(events: TraceEvent[]): LockWaitInterval[] {
  const waiting = new Map<string, TraceEvent>();
  const waits: LockWaitInterval[] = [];

  for (const event of events) {
    const lockName = event.lock;
    if (!lockName) {
      continue;
    }

    const key = `${event.thread}:${lockName}`;
    if (event.event === "lock_attempt") {
      waiting.set(key, event);
    }

    if (event.event === "lock_acquired") {
      const start = waiting.get(key);
      if (!start) {
        continue;
      }
      waits.push({
        lock: lockName,
        thread: event.thread,
        start: start.time_since_started_s,
        end: event.time_since_started_s,
        waitUs: event.duration_us ?? Math.max(0, (event.time_since_started_s - start.time_since_started_s) * 1e6),
      });
      waiting.delete(key);
    }
  }

  return waits.sort((a, b) => a.start - b.start);
}

export function buildFlags(events: TraceEvent[]): FlagPoint[] {
  return events
    .filter((event) => event.event === "flag_set" && typeof event.flag === "string" && typeof event.flag_value === "number")
    .map((event) => ({
      flag: event.flag as string,
      value: event.flag_value as number,
      time: event.time_since_started_s,
      state: event.state,
    }))
    .sort((a, b) => a.time - b.time);
}

export function getQueueSizeAt(events: TraceEvent[], time: number): number {
  let size = 0;
  for (const event of events) {
    if (event.time_since_started_s > time) {
      break;
    }
    if (typeof event.queue_size === "number") {
      size = event.queue_size;
    }
  }
  return size;
}

export function getThreadSnapshotsAt(events: TraceEvent[], taskIntervals: TaskInterval[], time: number): ThreadSnapshot[] {
  const latestByThread = new Map<number, TraceEvent>();
  for (const event of events) {
    if (event.time_since_started_s > time) {
      break;
    }
    latestByThread.set(event.thread, event);
  }

  const waitingByThread = new Map<number, string | null>();
  const lockOwners = new Map<string, number | null>();
  for (const event of events) {
    if (event.time_since_started_s > time || !event.lock) {
      continue;
    }

    if (event.event === "lock_attempt") {
      waitingByThread.set(event.thread, event.lock);
    }
    if (event.event === "lock_acquired") {
      waitingByThread.set(event.thread, null);
      lockOwners.set(event.lock, event.thread);
    }
    if (event.event === "lock_released") {
      const owner = lockOwners.get(event.lock);
      if (owner === event.thread) {
        lockOwners.set(event.lock, null);
      }
    }
  }

  const activeTaskByThread = new Map<number, number | null>();
  for (const interval of taskIntervals) {
    if (interval.start <= time && interval.end >= time) {
      activeTaskByThread.set(interval.thread, interval.taskId);
    }
  }

  const threads = [...latestByThread.keys()].sort((a, b) => a - b);
  return threads.map((thread) => {
    const latest = latestByThread.get(thread);
    const codePoint = latest ? getCodePoint(latest) : { currentCode: "idle", sourceLocation: "n/a" };
    const waitingOnLock = waitingByThread.get(thread) ?? null;
    return {
      thread,
      state: latest?.state ?? "unknown",
      activeTaskId: activeTaskByThread.get(thread) ?? null,
      waitingOnLock,
      currentEvent: latest?.event ?? "none",
      currentCode: codePoint.currentCode,
      sourceLocation: codePoint.sourceLocation,
    };
  });
}

export function getLockOwnersAt(events: TraceEvent[], time: number): Map<string, number | null> {
  const owners = new Map<string, number | null>();
  for (const event of events) {
    if (event.time_since_started_s > time || !event.lock) {
      continue;
    }
    if (event.event === "lock_acquired") {
      owners.set(event.lock, event.thread);
    }
    if (event.event === "lock_released") {
      const owner = owners.get(event.lock);
      if (owner === event.thread) {
        owners.set(event.lock, null);
      }
    }
  }
  return owners;
}
