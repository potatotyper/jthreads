import { FlagPoint, LockWaitInterval, TaskInterval, ThreadSnapshot, TraceEvent } from "./types";

type FunctionPoint = {
  currentFunction: string;
};

const EVENT_FUNCTION_MAP: Record<string, FunctionPoint> = {
  worker_start: {
    currentFunction: "jthread worker start",
  },
  lock_attempt: {
    currentFunction: "jthread_mutex_lock (wait)",
  },
  worker_queue_lock_acquired: {
    currentFunction: "jthread_mutex_lock",
  },
  lock_acquired: {
    currentFunction: "jthread_mutex_lock",
  },
  worker_wait_begin: {
    currentFunction: "jthread_cond_wait",
  },
  worker_wait_end: {
    currentFunction: "jthread_cond_wait (wake)",
  },
  task_dequeued: {
    currentFunction: "task dequeue",
  },
  lock_released: {
    currentFunction: "jthread_mutex_unlock",
  },
  task_start: {
    currentFunction: "task callback entry",
  },
  task_end: {
    currentFunction: "task callback exit",
  },
  worker_stop: {
    currentFunction: "jthread worker stop",
  },
  task_submit: {
    currentFunction: "jthread_create",
  },
  submit_lock_acquired: {
    currentFunction: "jthread_mutex_lock",
  },
  task_enqueued: {
    currentFunction: "task enqueue",
  },
  worker_notified: {
    currentFunction: "jthread_cond_signal",
  },
  shutdown_begin: {
    currentFunction: "jthread shutdown",
  },
  shutdown_notify_all: {
    currentFunction: "jthread_cond_broadcast",
  },
  shutdown_join_wait: {
    currentFunction: "jthread_join",
  },
  shutdown_join_done: {
    currentFunction: "jthread_join",
  },
  shutdown_complete: {
    currentFunction: "jthread shutdown",
  },
  task_sleep_begin: {
    currentFunction: "sleep",
  },
  task_sleep_end: {
    currentFunction: "sleep (return)",
  },
  task_print: {
    currentFunction: "print",
  },
  jthread_init: {
    currentFunction: "jthread_init",
  },
  jthread_register: {
    currentFunction: "jthread runtime registration",
  },
  jthread_create: {
    currentFunction: "jthread_create",
  },
  jthread_self: {
    currentFunction: "jthread_self",
  },
  jthread_start: {
    currentFunction: "jthread start procedure entry",
  },
  jthread_finish: {
    currentFunction: "jthread start procedure exit",
  },
  jthread_block_begin: {
    currentFunction: "jthread_block",
  },
  jthread_block_end: {
    currentFunction: "jthread_block (wake)",
  },
  jthread_unblock: {
    currentFunction: "jthread_unblock",
  },
  jthread_join_wait: {
    currentFunction: "jthread_join",
  },
  jthread_join_done: {
    currentFunction: "jthread_join",
  },
  jthread_join: {
    currentFunction: "jthread_join",
  },
  jthread_detach: {
    currentFunction: "jthread_detach",
  },
  jthread_yield: {
    currentFunction: "jthread_yield",
  },
  jthread_yield_done: {
    currentFunction: "jthread_yield (return)",
  },
  jthread_sleep_begin: {
    currentFunction: "jthread_sleep",
  },
  jthread_sleep_end: {
    currentFunction: "jthread_sleep (return)",
  },
  jthread_mutex_create: {
    currentFunction: "jthread_mutex_create",
  },
  jthread_mutex_lock: {
    currentFunction: "jthread_mutex_lock",
  },
  jthread_mutex_unlock: {
    currentFunction: "jthread_mutex_unlock",
  },
  jthread_mutex_destroy: {
    currentFunction: "jthread_mutex_destroy",
  },
  jthread_cond_create: {
    currentFunction: "jthread_cond_create",
  },
  jthread_cond_wait_begin: {
    currentFunction: "jthread_cond_wait",
  },
  jthread_cond_wait_end: {
    currentFunction: "jthread_cond_wait (wake)",
  },
  jthread_cond_wait: {
    currentFunction: "jthread_cond_wait",
  },
  jthread_cond_signal: {
    currentFunction: "jthread_cond_signal",
  },
  jthread_cond_broadcast: {
    currentFunction: "jthread_cond_broadcast",
  },
  jthread_cond_destroy: {
    currentFunction: "jthread_cond_destroy",
  },
  sample_begin: {
    currentFunction: "sample setup",
  },
  sample_sleep_begin: {
    currentFunction: "sleep_for",
  },
  sample_sleep_end: {
    currentFunction: "sleep_for (return)",
  },
  sample_counter_change_begin: {
    currentFunction: "change_counter",
  },
  sample_counter_changed: {
    currentFunction: "change_counter",
  },
  sample_string_change_begin: {
    currentFunction: "change_string",
  },
  sample_word_changed: {
    currentFunction: "change_string",
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

function getFunctionPoint(event: TraceEvent): FunctionPoint {
  if (event.event === "flag_set" && event.flag === "jthreads") {
    return {
      currentFunction: "jthread_init",
    };
  }

  if (event.event === "lock_released" && event.state === "cond_wait_release") {
    return {
      currentFunction: "jthread_cond_wait (release mutex)",
    };
  }

  if (event.event === "lock_acquired" && event.state === "cond_wait_reacquired") {
    return {
      currentFunction: "jthread_cond_wait (reacquire mutex)",
    };
  }

  return (
    EVENT_FUNCTION_MAP[event.event] ?? {
      currentFunction: "(unknown function)",
    }
  );
}

export function buildTaskIntervals(events: TraceEvent[]): TaskInterval[] {
  const starts = new Map<string, { event: TraceEvent; label: string; kind: "task" | "jthread" }>();
  const intervals: TaskInterval[] = [];

  for (const event of events) {
    if (event.event === "task_start" && event.task_id !== undefined) {
      starts.set(`task:${event.task_id}`, {
        event,
        label: `task ${event.task_id}`,
        kind: "task",
      });
    }

    if (event.event === "jthread_start" && event.task_id !== undefined) {
      starts.set(`jthread:${event.task_id}`, {
        event,
        label: `jthread ${event.task_id}`,
        kind: "jthread",
      });
    }

    if (event.event === "task_end" && event.task_id !== undefined) {
      const start = starts.get(`task:${event.task_id}`);
      if (!start) {
        continue;
      }
      const startEvent = start.event;
      const durationUs = event.duration_us ?? Math.max(0, (event.time_since_started_s - startEvent.time_since_started_s) * 1e6);
      intervals.push({
        taskId: event.task_id,
        thread: startEvent.thread,
        start: startEvent.time_since_started_s,
        end: event.time_since_started_s,
        durationUs,
        label: start.label,
        kind: start.kind,
      });
      starts.delete(`task:${event.task_id}`);
    }

    if (event.event === "jthread_finish" && event.task_id !== undefined) {
      const start = starts.get(`jthread:${event.task_id}`);
      if (!start) {
        continue;
      }
      const startEvent = start.event;
      const durationUs = event.duration_us ?? Math.max(0, (event.time_since_started_s - startEvent.time_since_started_s) * 1e6);
      intervals.push({
        taskId: event.task_id,
        thread: startEvent.thread,
        start: startEvent.time_since_started_s,
        end: event.time_since_started_s,
        durationUs,
        label: start.label,
        kind: start.kind,
      });
      starts.delete(`jthread:${event.task_id}`);
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
    const functionPoint = latest ? getFunctionPoint(latest) : { currentFunction: "(idle)" };
    const waitingOnLock = waitingByThread.get(thread) ?? null;
    return {
      thread,
      state: latest?.state ?? "unknown",
      activeTaskId: activeTaskByThread.get(thread) ?? null,
      waitingOnLock,
      currentEvent: latest?.event ?? "none",
      currentFunction: functionPoint.currentFunction,
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
