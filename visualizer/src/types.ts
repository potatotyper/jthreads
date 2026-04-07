export type TraceEvent = {
  event: string;
  state: string;
  time_since_started_s: number;
  thread: number;
  task_id?: number;
  queue_size?: number;
  duration_us?: number;
  lock?: string;
  flag?: string;
  flag_value?: number;
};

export type TaskInterval = {
  taskId: number;
  thread: number;
  start: number;
  end: number;
  durationUs: number;
};

export type LockWaitInterval = {
  lock: string;
  thread: number;
  start: number;
  end: number;
  waitUs: number;
};

export type FlagPoint = {
  flag: string;
  value: number;
  time: number;
  state: string;
};

export type ThreadSnapshot = {
  thread: number;
  state: string;
  activeTaskId: number | null;
  waitingOnLock: string | null;
  currentEvent: string;
  currentCode: string;
  sourceLocation: string;
};
