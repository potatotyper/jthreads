# jthreads

A spinoff of CPSC 213 Uthreads threadpool program (no code copied) with a vizualiser to help students further undestand the basics of threadpools and multihreadding :)

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
nvm use || nvm install
npm install
npm run dev
```

Open the local Vite URL, then either:

- click "Load sample trace1.json" (copied to `visualizer/public/trace1.json`), or
- upload a trace file from your own run.

### WSL npm fix (if install fails with UNC/CMD path errors)

If `npm install` shows UNC path errors (for example `\\wsl.localhost\...` and `CMD.EXE ... UNC paths are not supported`), your shell is using Windows npm instead of Linux node.

Use Linux Node inside WSL via nvm:

```bash
export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] || curl -fsSL https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.3/install.sh | bash
. "$NVM_DIR/nvm.sh"
nvm install --lts
nvm use --lts

cd visualizer
rm -rf node_modules package-lock.json
npm install
```

### Supported features

- time cursor `t` via slider, playback controls, and numeric input
- per-thread swimlane task timeline
- point-in-time thread state table
- lock contention overlays and current lock owner view
- current flag values at time `t`
