# C++ Operating System Process and Memory Simulator

A C++ simulation of core operating system concepts, including process management, CPU scheduling, instruction execution, virtual memory, paging, and memory statistics.

This project was developed as a personal/course project to explore how operating-system components interact through a multithreaded simulation.

## Features

* **Multicore CPU simulation**

  * Configurable number of CPU cores
  * Concurrent process execution using C++ threads
  * CPU utilization and core status tracking

* **Process management**

  * Process creation and lifecycle management
  * Process states:

    * `READY`
    * `RUNNING`
    * `IDLE`
    * `FINISHED`
  * Per-process instruction counters and execution logs
  * Configurable process memory limits

* **CPU scheduling**

  * Round-robin scheduling
  * Configurable quantum size
  * Automatic process dispatching across available cores
  * Scheduler stress testing through randomly generated processes

* **Instruction execution**

  * Custom instruction format and parser
  * Supported instructions:

    * `PRINT`
    * `DECLARE`
    * `ADD`
    * `SUBTRACT`
    * `SLEEP`
    * `FOR`
    * `READ`
    * `WRITE`

* **Virtual memory simulation**

  * Per-process page tables
  * Virtual-to-physical address translation
  * Page faults
  * Backing store simulation
  * FIFO page replacement
  * Configurable frame size and total memory
  * Per-process memory limits
  * Page-in and page-out statistics

* **System monitoring**

  * Process information through `process-smi`
  * Virtual memory statistics through `vmstat`
  * Backing-store snapshots
  * CPU utilization and memory usage reporting

## Architecture

The simulator is divided into several components:

```text
src/
├── configs/
│   ├── Initialize.hpp
│   └── config.txt
│
├── instructions/
│   ├── Instruction.hpp
│   ├── OpCode.hpp
│   └── Parser.hpp
│
├── process/
│   ├── Dispatcher.hpp
│   ├── Logger.hpp
│   ├── Memory.hpp
│   ├── Process.hpp
│   └── ProcessHelper.hpp
│
├── utils/
│   └── helper.hpp
│
├── MainController.hpp
└── main.cpp
```

### Main Components

**MainController**

Handles the command-line interface and connects user commands to the scheduler and process manager.

**Dispatcher**

Responsible for scheduling processes, assigning them to CPU cores, managing ready/finished queues, and collecting CPU statistics.

**Process**

Represents an individual simulated process. It contains its instructions, program counter, state, memory limit, and execution history.

**MemoryManager**

Simulates physical memory and virtual memory. It manages page tables, frames, page faults, backing storage, and FIFO page replacement.

**Instruction Parser**

Converts the custom instruction syntax into executable `Instruction` objects and handles nested `FOR` instructions.

## Configuration

Simulation parameters are stored in:

```text
src/configs/config.txt
```

Example configuration:

```text
num_cpu 4
scheduler rr
quantum_cycles 4
batch_process_freq 1
min_ins 1
max_ins 200
delays_per_exec 1000
max_overall_mem 32768
mem_per_frame 32
min_mem_per_proc 8
max_mem_per_proc 8
```

The configuration controls:

| Setting              | Description                               |
| -------------------- | ----------------------------------------- |
| `num_cpu`            | Number of simulated CPU cores             |
| `scheduler`          | Scheduling algorithm                      |
| `quantum_cycles`     | Number of cycles allocated per process    |
| `batch_process_freq` | Frequency of automatic process generation |
| `min_ins`            | Minimum generated instructions            |
| `max_ins`            | Maximum generated instructions            |
| `delays_per_exec`    | Delay between execution cycles            |
| `max_overall_mem`    | Total simulated physical memory           |
| `mem_per_frame`      | Size of each memory frame                 |
| `min_mem_per_proc`   | Minimum generated process memory          |
| `max_mem_per_proc`   | Maximum generated process memory          |

## Building and Running

### Requirements

* Linux
* `g++`
* C++17 or newer
* POSIX threads

The included build script uses:

* C++17
* pthreads
* AddressSanitizer
* UndefinedBehaviorSanitizer

Run the simulator with:

```bash
./run.sh
```

Or compile manually:

```bash
g++ -g -O0 -pthread -std=c++17 \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -o a.out src/*.cpp
```

Then:

```bash
./a.out
```

## Usage

After starting the program, initialize the simulator:

```text
root:\> initialize
```

The configuration is loaded and the scheduler starts.

### Create a Process

Create a randomly generated process:

```text
root:\> screen -s PROC-1 64
```

Create a process with a custom name and instruction sequence:

```text
root:\> screen -c PROC-1 64 "DECLARE x 10; ADD x x 5; PRINT \"Value: \" x"
```

### View Processes

```text
root:\> screen -ls
```

This displays active CPU cores, running processes, finished processes, and their instruction progress.

### Start Process Generation

Start automatic process generation:

```text
root:\> scheduler-start
```

Stop automatic process generation:

```text
root:\> scheduler-stop
```

### View Process Statistics

```text
root:\> process-smi
```

Displays information about CPU utilization, memory usage, and processes currently executing on the simulated cores.

### View Virtual Memory Statistics

```text
root:\> vmstat
```

Reports:

* Total memory
* Used memory
* Free memory
* Active CPU ticks
* Idle CPU ticks
* Total CPU ticks
* Pages loaded into memory
* Pages evicted from memory

### Generate a Backing Store Snapshot

```text
root:\> backing-store
```

This generates:

```text
csopesy-backing-store.txt
```

containing a snapshot of memory and virtual-memory statistics.

### Exit

```text
root:\> exit
```

## Custom Instruction Set

The simulator uses a small instruction language for process execution.

### DECLARE

Creates a variable with an initial value.

```text
DECLARE x 10
```

### PRINT

Prints a message or variable value.

```text
PRINT "Hello"
PRINT "Value: " x
```

### ADD

Adds two values and stores the result.

```text
ADD result x 5
```

### SUBTRACT

Subtracts one value from another.

```text
SUBTRACT result x 5
```

### READ

Reads a value from a virtual memory address into a variable.

```text
READ x 0x10
```

### WRITE

Writes a value to a virtual memory address.

```text
WRITE 0x10 42
```

### SLEEP

Places the process into an idle state for a specified duration.

```text
SLEEP 1000
```

### FOR

Repeats a group of instructions.

```text
FOR([PRINT "Hello", ADD x x 1], 3)
```

## Virtual Memory

The memory subsystem models a simplified paged virtual-memory system.

Each process receives its own virtual address space. When a process accesses a page that is not currently resident in physical memory, a **page fault** occurs.

The memory manager then:

1. Checks whether the process has exceeded its memory limit.
2. Searches for a free physical frame.
3. If no frame is available, selects a frame using **FIFO page replacement**.
4. Writes the evicted page to the simulated backing store.
5. Loads the requested page into the selected frame.
6. Updates the process page table.
7. Continues the memory access.

Memory accesses are synchronized with mutexes so that multiple simulated CPU cores can safely access the memory manager concurrently.

## Concurrency

The simulator uses C++ threading primitives to model concurrent execution.

Key synchronization mechanisms include:

* `std::thread`
* `std::mutex`
* `std::lock_guard`
* `std::atomic`
* `std::shared_ptr`

The dispatcher can execute processes across multiple simulated cores while the memory manager protects shared memory structures from concurrent access.

## Project Goals

This project focuses on understanding the implementation of operating-system concepts rather than building a production operating system.

The main goals are to experiment with:

* Process scheduling
* Multithreaded execution
* CPU utilization
* Instruction parsing and execution
* Virtual memory
* Paging and page replacement
* Memory isolation
* Process state management
* System monitoring

## Future Improvements

Potential improvements include:

* Additional scheduling algorithms such as FCFS and SJF
* More sophisticated page replacement algorithms such as LRU
* Improved process termination and cleanup
* More robust memory protection
* A more complete process shell
* Persistent process information
* Better instruction-language error handling
* Unit and integration tests
* CMake-based build configuration
* Interactive visualization of CPU and memory state

## Technologies

* **C++17**
* **STL**
* **Multithreading**
* **Mutexes and atomics**
* **AddressSanitizer**
* **UndefinedBehaviorSanitizer**
* **Custom instruction parser**
* **Virtual memory simulation**

## Project Status

This project is an educational simulator and is actively open to experimentation and further development. The implementation prioritizes demonstrating OS concepts and providing a working simulation environment over faithfully reproducing a production operating system.
