# CPU Scheduling Simulator | C++

## Overview
This is a command-line educational simulator for evaluating and comparing CPU scheduling algorithms. It accurately simulates scheduling decisions, process state transitions, CPU bursts, and I/O wait times rather than managing actual operating-system processes.

## Features
- **Accurate CPU and I/O Burst Simulation:** Simulates full process life-cycles including I/O waits and dynamic queue transitions.
- **Three Scheduling Algorithms:** Includes Round Robin, Shortest Job First (SJF), and Priority Scheduling.
- **Metrics Tracking:** Calculates Completion Time (CT), Turnaround Time (TAT), Response Time (RT), Waiting Time (WT), and overall CPU utilization.
- **Comparison Mode:** Automatically evaluates all three algorithms against a single workload sequentially without state leakage.
- **Robust Input Validation:** Rejects malformed burst sequences (e.g. consecutive CPU bursts, missing terminators), negative arrival times, and malformed CLI arguments cleanly without crashing.
- **Gantt Chart Visualization:** Produces a timeline of CPU execution history, properly tracking exact PIDs to differentiate duplicate process names.

## Scheduling Algorithms
1. **Round Robin (RR)**
   - Preemptive scheduling with a configurable time quantum. Processes are given a fixed time slice. If they do not finish their burst, they are preempted and placed at the back of the FIFO ready queue.
2. **Shortest Job First (SJF) (Non-Preemptive)**
   - The CPU selects the process in the ready queue with the shortest next CPU burst. Once scheduled, it runs until the CPU burst completes. Ties are broken using FIFO arrival order.
3. **Priority Scheduling (Non-Preemptive)**
   - The CPU selects the process with the highest priority (lowest numerical priority value). Once scheduled, it runs until the burst completes. Ties are broken using FIFO arrival order.

## Tech Stack
- C++ (C++17)
- Standard Template Library (STL): `std::vector`, `std::deque`, `std::string`
- File I/O: `<fstream>`, `<sstream>`

## Installation and Execution
Ensure you have a C++17 compliant compiler installed (like `g++`).

### Compilation
**Windows/Linux:**
```bash
make
# Or manually:
g++ -std=c++17 cpu-scheduler-simulation.cpp -o simulator
```

### Execution
Run the compiled executable with the input file and chosen algorithm. (On Windows PowerShell, use `.\simulator.exe`).

```bash
# Round Robin (default quantum is 5)
./simulator input/mixed-workload.txt RR

# Round Robin with custom quantum 5
./simulator input/mixed-workload.txt RR 5

# Shortest Job First
./simulator input/mixed-workload.txt SJF

# Priority Scheduling
./simulator input/mixed-workload.txt PRIO

# Compare all algorithms
./simulator input/mixed-workload.txt COMPARE 5
```

## Input Format
Workload files use plain text with space-separated values. Each process must be completely defined on a single line.

**Format:**
`<ProcessName> <ArrivalTime> [Priority] <BurstType> <Duration> ... N 0`

- **Priority (Optional):** If a number is provided before the first burst type, it is treated as the process priority (lower number = higher priority). Defaults to `10`.
- **Burst Types:** `C` (CPU), `I` (Input), `O` (Output). A process must begin with a CPU burst. Consecutive bursts of the same type are invalid.
- **Termination:** `N 0` indicates the end of a process's bursts.

**Example Input:**
```txt
EDITOR 0 C 5 I 8 C 4 O 6 C 3 N 0
BROWSER 2 1 C 8 I 12 C 7 N 0
STOPHERE 0
```
*(BROWSER has priority 1, EDITOR defaults to 10)*

## Sample Output
```text
========== Final Simulation Summary ==========
Algorithm: Round Robin (Q=5)
Final Timer: 50
Processes Terminated: 2
CPU Idle Time: 3
CPU Utilization: 94.00%

--- Per-Process Metrics ---
Process   AT    CT    TAT   RT    WT    
EDITOR    0     50    50    0     18    
BROWSER   2     47    45    3     18    

Average Waiting Time: 18.00
Average Turnaround Time: 47.50
Average Response Time: 1.50

--- Gantt Chart ---
0--EDITOR(PID:101)--5--BROWSER(PID:102)--10--EDITOR(PID:101)--14--IDLE--17--BROWSER(PID:102)--22--EDITOR(PID:101)--25--BROWSER(PID:102)--27--IDLE--39--BROWSER(PID:102)--47--EDITOR(PID:101)--50

Total CPU Time: 27
Total Input Time: 20
Total Output Time: 6
Processes Left in Entry Queue: 0
Processes Left in Ready Queue: 0
Processes Left in Input Queue: 0
Processes Left in Output Queue: 0
===============================================
```

## CS Fundamentals Covered
- **Operating Systems & CPU Scheduling:** Practical application of classical OS scheduling heuristics.
- **Preemption & Process States:** Accurately models real state transitions (Entry -> Ready -> Running -> I/O Wait -> Terminated).
- **Queues and Deques:** Leverages STL `deque` for O(1) front/back queue manipulation crucial to RR scheduling.
- **Scheduling Metrics:** Demonstrates how OS theory evaluates performance bottlenecks (TAT vs WT vs RT).

## Limitations
- **Fixed System Constraints:** The simulation terminates at a hardcoded `MAX_TIME = 500` ticks, and limits concurrent active processes to `IN_USE = 5`.
- **No Multithreading:** The simulator runs in a single-threaded loop to model OS scheduling theoretically; it does not perform actual OS thread context switching.
- **Priority Starvation:** As Priority Scheduling is strictly non-preemptive without an "aging" mechanism, low-priority processes can theoretically starve if blocked by an infinite stream of high-priority arrivals.
