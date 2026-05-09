import { ChangeEvent, useEffect, useMemo, useRef, useState } from "react";
import type { CSSProperties } from "react";
import {
  buildFlags,
  buildLockWaitIntervals,
  buildTaskIntervals,
  getLockOwnersAt,
  getQueueSizeAt,
  getThreadSnapshotsAt,
  getThreads,
  getTraceBounds,
  parseTraceJsonLines,
} from "./trace";
import { FlagPoint, LockWaitInterval, TaskInterval, TraceEvent } from "./types";

type PlaybackSpeed = 0.25 | 0.5 | 1 | 2 | 4;
type TaskVisualState = "blocked" | "waiting" | "sleeping" | "yielding" | "signaling" | "running" | "idle";

const SPEED_OPTIONS: PlaybackSpeed[] = [0.25, 0.5, 1, 2, 4];

function fmtSeconds(value: number): string {
  return `${value.toFixed(3)} s`;
}

function fmtUs(value: number): string {
  return `${Math.round(value)} us`;
}

function threadColor(thread: number): string {
  const palette = ["#38bdf8", "#34d399", "#f59e0b", "#a78bfa", "#fb7185", "#22d3ee"];
  return palette[(thread - 1) % palette.length];
}

function classifyTaskEvent(event: TraceEvent): TaskVisualState {
  if (event.event === "jthread_sleep_begin" || event.state === "sleeping") {
    return "sleeping";
  }

  if (
    event.event.includes("signal") ||
    event.event.includes("broadcast") ||
    event.state.includes("signaling") ||
    event.event === "worker_notified"
  ) {
    return "signaling";
  }

  if (event.event.includes("yield")) {
    return "yielding";
  }

  if (event.event === "jthread_block_begin") {
    return "blocked";
  }

  if (
    event.event === "lock_attempt" ||
    event.event === "worker_wait_begin" ||
    event.event === "jthread_cond_wait_begin" ||
    event.event === "jthread_join_wait" ||
    event.event === "shutdown_join_wait" ||
    event.state === "cond_wait_release" ||
    event.state.includes("waiting") ||
    event.state.includes("joining") ||
    event.state.includes("blocked_on_condition")
  ) {
    return "waiting";
  }

  if (
    event.event === "task_start" ||
    event.event === "jthread_start" ||
    event.event === "jthread_block_end" ||
    event.event === "jthread_cond_wait_end" ||
    event.event === "jthread_self" ||
    event.event === "jthread_unblock" ||
    event.event === "worker_wait_end" ||
    event.event === "lock_acquired" ||
    event.event === "lock_released" ||
    event.event === "task_dequeued" ||
    event.state === "running" ||
    event.state === "current" ||
    event.state === "held" ||
    event.state === "released" ||
    event.state === "woken" ||
    event.state === "ready_to_run"
  ) {
    return "running";
  }

  return "idle";
}

function getTaskVisualState(task: TaskInterval, threadEvents: TraceEvent[], playhead: number): TaskVisualState {
  if (playhead < task.start || playhead > task.end) {
    return "idle";
  }

  let latest: TraceEvent | null = null;
  for (const event of threadEvents) {
    if (event.time_since_started_s < task.start) {
      continue;
    }
    if (event.time_since_started_s > playhead) {
      break;
    }
    latest = event;
  }

  return latest === null ? "running" : classifyTaskEvent(latest);
}

function App() {
  const [events, setEvents] = useState<TraceEvent[]>([]);
  const [playhead, setPlayhead] = useState(0);
  const [isPlaying, setIsPlaying] = useState(false);
  const [speed, setSpeed] = useState<PlaybackSpeed>(1);
  const [error, setError] = useState<string | null>(null);
  const rafRef = useRef<number | null>(null);
  const lastTickRef = useRef<number | null>(null);

  const bounds = useMemo(() => getTraceBounds(events), [events]);
  const threads = useMemo(() => getThreads(events), [events]);
  const taskIntervals = useMemo(() => buildTaskIntervals(events), [events]);
  const lockWaits = useMemo(() => buildLockWaitIntervals(events), [events]);
  const flags = useMemo(() => buildFlags(events), [events]);

  useEffect(() => {
    if (events.length === 0) {
      setPlayhead(0);
      return;
    }
    setPlayhead(bounds.min);
  }, [events, bounds.min]);

  useEffect(() => {
    if (!isPlaying || events.length === 0) {
      if (rafRef.current !== null) {
        cancelAnimationFrame(rafRef.current);
      }
      rafRef.current = null;
      lastTickRef.current = null;
      return;
    }

    const loop = (frameNow: number) => {
      if (lastTickRef.current === null) {
        lastTickRef.current = frameNow;
      }
      const elapsedMs = frameNow - lastTickRef.current;
      lastTickRef.current = frameNow;

      setPlayhead((prev: number) => {
        const next = prev + (elapsedMs / 1000) * speed;
        if (next >= bounds.max) {
          setIsPlaying(false);
          return bounds.max;
        }
        return next;
      });

      rafRef.current = requestAnimationFrame(loop);
    };

    rafRef.current = requestAnimationFrame(loop);
    return () => {
      if (rafRef.current !== null) {
        cancelAnimationFrame(rafRef.current);
      }
      rafRef.current = null;
      lastTickRef.current = null;
    };
  }, [isPlaying, events.length, speed, bounds.max]);

  const snapshots = useMemo(
    () => getThreadSnapshotsAt(events, taskIntervals, playhead),
    [events, taskIntervals, playhead],
  );
  const queueSize = useMemo(() => getQueueSizeAt(events, playhead), [events, playhead]);
  const lockOwners = useMemo(() => getLockOwnersAt(events, playhead), [events, playhead]);

  const currentFlags = useMemo(() => {
    const latest = new Map<string, FlagPoint>();
    for (const point of flags) {
      if (point.time <= playhead) {
        latest.set(point.flag, point);
      }
    }
    return [...latest.values()];
  }, [flags, playhead]);

  const totalContentionUs = useMemo(
    () => lockWaits.reduce((acc: number, wait: LockWaitInterval) => acc + wait.waitUs, 0),
    [lockWaits],
  );

  const handleFileUpload = async (file: File) => {
    try {
      const text = await file.text();
      const parsed = parseTraceJsonLines(text);
      if (parsed.length === 0) {
        setError("No valid JSONL events found in file.");
        return;
      }
      setEvents(parsed);
      setError(null);
      setIsPlaying(false);
    } catch {
      setError("Unable to read file.");
    }
  };

  const loadSample = async () => {
    try {
      const res = await fetch("/trace1.json");
      if (!res.ok) {
        setError("Sample trace not found.");
        return;
      }
      const text = await res.text();
      const parsed = parseTraceJsonLines(text);
      setEvents(parsed);
      setError(null);
      setIsPlaying(false);
    } catch {
      setError("Failed to load sample trace.");
    }
  };

  return (
    <>
      <header className="site-header">
        <div className="site-header-inner">
          <span className="brand">
            Potatotyper
          </span>
          <a className="home-button" href="https://potatotyper.page/home">
            Home
          </a>
        </div>
      </header>

      <div className="app-shell">
        <header className="hero">
          <p className="eyebrow">HELLO, I AM A THREAD VISUALIZER</p>
          <h1>jthreads Trace Visualizer</h1>
          <p>
            Move t to inspect how each jthread changes state, which library function is active, and where lock contention appears.
          </p>
        </header>

        <section className="card sample-program">
          <h2>Sample Program</h2>
          <p>
            The bundled sample uses five simple worker functions. Each one calls jthread_sleep for a random 4-5 seconds,
            updates either the shared counter or shared word, then sleeps again and updates the other kind of value.
          </p>
          <div className="sample-points">
            <span>thread_one through thread_five have matching sleep/update steps</span>
            <span>jthread_sleep has a gray fill with a green outline</span>
            <span>counter_mutex protects the shared counter</span>
            <span>word_mutex protects the shared word</span>
            <span>each step changes counter or word, not both</span>
            <span>main joins all five workers</span>
          </div>
        </section>

        <section className="card controls">
        <div className="control-row">
          <button onClick={loadSample}>Load Sample</button>
          <label className="file-input">
            Upload trace JSON
            <input
              type="file"
              accept=".json,.jsonl,.txt"
              onChange={(e: ChangeEvent<HTMLInputElement>) => {
                const file = e.target.files?.[0];
                if (file) {
                  void handleFileUpload(file);
                }
              }}
            />
          </label>
        </div>

        <div className="control-row">
          <button
            onClick={() => {
              if (events.length === 0) {
                return;
              }
              if (playhead >= bounds.max) {
                setPlayhead(bounds.min);
              }
              setIsPlaying((v: boolean) => !v);
            }}
            disabled={events.length === 0}
          >
            {isPlaying ? "Pause" : "Play"}
          </button>

          <label>
            Speed
            <select
              value={speed}
              onChange={(e: ChangeEvent<HTMLSelectElement>) => setSpeed(Number(e.target.value) as PlaybackSpeed)}
            >
              {SPEED_OPTIONS.map((option) => (
                <option key={option} value={option}>
                  {option}x
                </option>
              ))}
            </select>
          </label>

          <label>
            t (seconds)
            <input
              type="number"
              step="0.001"
              value={playhead.toFixed(3)}
              min={bounds.min}
              max={bounds.max}
              onChange={(e: ChangeEvent<HTMLInputElement>) => {
                const next = Number(e.target.value);
                if (Number.isFinite(next)) {
                  setPlayhead(Math.max(bounds.min, Math.min(bounds.max, next)));
                }
              }}
              disabled={events.length === 0}
            />
          </label>

          <span className="pill">events: {events.length}</span>
          <span className="pill">threads: {threads.length}</span>
          <span className="pill">queue size @ t: {queueSize}</span>
        </div>

        <input
          className="time-slider"
          type="range"
          min={bounds.min}
          max={bounds.max}
          step="0.0001"
          value={playhead}
          onChange={(e: ChangeEvent<HTMLInputElement>) => setPlayhead(Number(e.target.value))}
          disabled={events.length === 0}
        />

        <div className="time-labels">
          <span>{fmtSeconds(bounds.min)}</span>
          <strong>Current t: {fmtSeconds(playhead)}</strong>
          <span>{fmtSeconds(bounds.max)}</span>
        </div>

        {error && <p className="error">{error}</p>}
      </section>

      <section className="card">
        <h2>Thread Swimlanes</h2>
        {threads.length === 0 && <p>Load a trace file to render timeline.</p>}
        {threads.map((thread) => (
          <ThreadLane
            key={thread}
            thread={thread}
            bounds={bounds}
            playhead={playhead}
            tasks={taskIntervals.filter((interval) => interval.thread === thread)}
            lockWaits={lockWaits.filter((wait) => wait.thread === thread)}
            threadEvents={events.filter((event) => event.thread === thread)}
          />
        ))}
      </section>

      <section className="card grid-two">
        <div>
          <h2>Thread State @ t</h2>
          <table>
            <thead>
              <tr>
                <th>Thread</th>
                <th>State</th>
                <th>Function</th>
                <th>Task</th>
                <th>Lock Wait</th>
              </tr>
            </thead>
            <tbody>
              {snapshots.map((snapshot) => (
                <tr key={snapshot.thread}>
                  <td>#{snapshot.thread}</td>
                  <td>{snapshot.state}</td>
                  <td className="function-name" title={snapshot.currentEvent}>
                    {snapshot.currentFunction}
                  </td>
                  <td>{snapshot.activeTaskId === null ? "-" : snapshot.activeTaskId}</td>
                  <td>{snapshot.waitingOnLock ?? "-"}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        <div>
          <h2>Locks and Flags</h2>
          <p className="metric">Total lock contention: {fmtUs(totalContentionUs)}</p>
          <h3>Current lock owners</h3>
          {[...lockOwners.entries()].length === 0 && <p>No lock events in current trace.</p>}
          <ul>
            {[...lockOwners.entries()].map(([lockName, owner]) => (
              <li key={lockName}>
                {lockName}: {owner === null ? "unlocked" : `thread #${owner}`}
              </li>
            ))}
          </ul>

          <h3>Current flags</h3>
          {currentFlags.length === 0 && <p>No flag transitions available.</p>}
          <ul>
            {currentFlags.map((flag) => (
              <li key={flag.flag}>
                {flag.flag}={flag.value} ({flag.state})
              </li>
            ))}
          </ul>
        </div>
      </section>

      <section className="card state-legend">
        <h2>State Legend</h2>
        <div className="state-grid">
          <StateLegendItem
            state="blocked"
            title="Blocked"
            description="The thread has called jthread_block and is unavailable until another thread unblocks it."
          />
          <StateLegendItem
            state="waiting"
            title="Waiting"
            description="The thread is waiting for a lock, condition variable, or join target before it can continue."
          />
          <StateLegendItem
            state="sleeping"
            title="Sleeping"
            description="The thread is inside jthread_sleep; it is paused by time, not waiting on a synchronization primitive."
          />
          <StateLegendItem
            state="yielding"
            title="Yielding"
            description="The thread voluntarily gave the scheduler a chance to run other work."
          />
          <StateLegendItem
            state="running"
            title="Running"
            description="The task or jthread is actively executing or has just resumed useful work."
          />
          <StateLegendItem
            state="signaling"
            title="Signaling"
            description="The thread is waking one or more waiting threads with signal, broadcast, or notify."
          />
        </div>
      </section>
      </div>
    </>
  );
}

type LaneProps = {
  thread: number;
  bounds: { min: number; max: number };
  playhead: number;
  tasks: TaskInterval[];
  lockWaits: LockWaitInterval[];
  threadEvents: TraceEvent[];
};

type StateLegendItemProps = {
  state: Exclude<TaskVisualState, "idle">;
  title: string;
  description: string;
};

function StateLegendItem({ state, title, description }: StateLegendItemProps) {
  return (
    <article className="state-item">
      <div className={`state-marker state-marker--${state}`} aria-hidden="true" />
      <div>
        <h3>{title}</h3>
        <p>{description}</p>
      </div>
    </article>
  );
}

function ThreadLane({ thread, bounds, playhead, tasks, lockWaits, threadEvents }: LaneProps) {
  const span = Math.max(bounds.max - bounds.min, 0.000001);
  const color = threadColor(thread);

  const leftPct = ((playhead - bounds.min) / span) * 100;

  return (
    <div className="lane">
      <div className="lane-label" style={{ color }}>
        Thread #{thread}
      </div>
      <div className="lane-track">
        {tasks.map((task) => {
          const left = ((task.start - bounds.min) / span) * 100;
          const width = ((task.end - task.start) / span) * 100;
          const visualState = getTaskVisualState(task, threadEvents, playhead);
          const style = {
            left: `${left}%`,
            width: `${Math.max(width, 0.2)}%`,
            "--task-color": color,
          } as CSSProperties;

          return (
            <div
              key={`${task.taskId}-${task.start}`}
              className={`task-bar task-bar--${visualState}`}
              style={style}
              title={`${task.label ?? `task ${task.taskId}`} | ${visualState} | ${fmtSeconds(task.start)} -> ${fmtSeconds(task.end)}`}
            >
              <span>{task.label ?? `task ${task.taskId}`}</span>
            </div>
          );
        })}

        {lockWaits.map((wait) => {
          const left = ((wait.start - bounds.min) / span) * 100;
          const width = ((wait.end - wait.start) / span) * 100;
          return (
            <div
              key={`${wait.lock}-${wait.start}`}
              className="lock-wait"
              style={{ left: `${left}%`, width: `${Math.max(width, 0.15)}%` }}
              title={`${wait.lock} wait ${fmtUs(wait.waitUs)}`}
            />
          );
        })}

        <div className="playhead" style={{ left: `${leftPct}%` }} />
      </div>
    </div>
  );
}

export default App;
