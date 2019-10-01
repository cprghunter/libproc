# PolySat Process Library (libproc)

libproc is a lightweight event loop and inter-process communication library built by PolySat.
It powers every one of our satellite core processes!

## Building

libproc is a POSIX C library.
It has been tested on Linux and MacOS, but it will probably run on any unix system.

It can be built with `make` and installed with `make install`.

## Event loop functionality

The event loop allows programs to react to specific events that happen in the operating system.

Some of the event types supported by libproc are:
- File actions: file reading, writing, and errors
- Scheduled events: schedule events to run at specific times
- Incoming commands: React to incoming commands via UDP packets
- Signals: Handle operating system signals, such as `SIGINT`
- Pending power-off

## Inter-process communication

libproc makes it easy to communicate with other processes. 
Using a simple schema the XDR system generates C code to allow easy use of libprocs interproccess communication capability.
Under the hood, proccesses send and receive commands on UDP ports. If a user had no access to libproc they would have to deal with the
allocation of UDP ports, the packaging of data into UDP packets, and the handling of any necesarry response from the commanded process. 
However the libproc library abstracts this would-be problem by auto-generating most of this code using the XDR architecture.

With the XDR architecture, process commands and the data structures they will use can be defined in a simple schema.

## The XDR Schema

Every proccess using the xdr schema to manager its inter-process communication will contain a .xp file in the same directory.

There are three things that need to be defined in the .xp file of a process, the types (structs) it will use in inter-process communication, the commands associated with it, and the errors associated with it.

At the top of the .xp file, each element of these things is assigned a unique hex number to allow for easier debugging. To ensure each process has unique hex codes for there types, commands and errors each process numbers their objects off of a different base. Bases are kept track of and allocated here <insert base doc>.

A working example of the numbering might look like this:
```c
const CMD_BASE =       0x400;
const TYPE_BASE = 0x01000400;
const ERR_BASE =  0x02000400;

enum types {
   STATUS = TYPE_BASE + 0,
   PRINT_PARAMS = TYPE_BASE + 1,
};

enum Cmds {
   PRINT = CMD_BASE + 0,
};

enum Errs {
   BAD_NUM = ERR_BASE + 0,
};
```
Where each element of the enums is a type, command, or error used by the process.


## Open-Source Programs Using libproc

- [ADCS Sensor Reader](https://github.com/PolySat/adcs-sensor-reader): Used to read attitude determination sensors on PolySat missions without full ADCS.

## Hello World using libproc

```c
#include <stdio.h>
#include <polysat/polysat.h>

/*
 * The event callback function. Will be called once a second.
 */
int my_timed_event(void *arg)
{
   printf("Hello World\n");
   /* Returning EVENT_KEEP will reschedule the event to run again on the event loop */
   return EVENT_KEEP;
}

int main(void)
{
   /* Where libproc stores its state */
   struct ProcessData *proc;

   /* Initialize the process */
   proc = PROC_init("test1", WD_DISABLED);
   if (!proc) {
      printf("error: failed to initialize process\n");
      return -1;
   }

   /* Schedule my_timed_event to run once a second on the event loop */
   EVT_sched_add(PROC_evt(proc), EVT_ms2tv(1000), &my_timed_event, NULL);

   /* Start the event loop */
   printf("Starting the event loop...\n");
   EVT_start_loop(PROC_evt(proc));

   /* Clean up libproc */
   printf("Cleaning up process...\n");
   PROC_cleanup(proc);

   printf("Done.\n");
   return 0;
}
```
## Additional Examples
[Stopwatch using libproc](https://github.com/PolySat/libproc/tree/master/programs/stopwatch_example)
