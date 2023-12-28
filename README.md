# CG Runner

Runner for Codingame AI challenges. Spawn multiple processes running games in succession and aggregate the results.
Inspired by [cg-brutaltester](https://github.com/dreignier/cg-brutaltester).

## Disclaimers
Using the POSIX API to manipulate processes and file descriptors. This won't work on Windows.
I also assume that this will run in a VT100 compatible terminal with true colors enabled.

Development is not considered complete yet, I'll keep updating the software as I need.

## Usage
```bash
runner -1 /path/to/player1 -2 /path/to/player2 -r /path/to/referee
```

Additional options:
+ `-c` number of processes to run in total
+ `-p` number of processes to run in parallel
+ `-G` do not generate output files for each game. By default a file named `output-<timestamp>-<run nb>.json` will be created for each game.

## Installation

No automated installation for now. Clone the repo and compile it, then copy the executable somewhere in your PATH.
For example:
```bash
git clone https://github.com/de-passage/cg-runner.git &&\
    cd cg-runner &&\
    make build CXXARGS='-O2' &&
    cp build/runner ~/.local/bin
```

## Requirements
A C++20 compiler (I developped it using clang 12, anything more recent should work).
