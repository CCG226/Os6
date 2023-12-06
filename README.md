# Os4
compile: oss will takes in 4 arguments: 
-s for the maximum amount of workers the os is allowed to launch at once 
-t is the time interval to launch next worker if allowed to.
-f for the name of the log file to use for logging.
-h OPTIONAL lists help info

run: ./oss -s 1 -t 7 -f test.txt
compile: make all
clean: make clean

unique: -Os Clock is a struct type called Sys_Time made up of three members to make it easier to store and access clock info in shared memory Sys_Time members: 
rate: the speed the system clock will 'tick'/'iterate' at 'x'  nanoseconds (the rate 'x' changes based on how long it takes os to do a task). 
seconds: contains the current time of the system clock in seconds nanoseconds: 
contains the current time of the system clock in nanoseconds

- tables printed every 5 seconds instead of every half a second 

-for the 10 second timer (5 second timer was too short), I used alarm function from signal.h that allows you to genertae a signal (SIGALRM) after 10 seconds in which the program will call a method that will kill the program. this behavior is handled functions: Begin_OS_LifeCycle() and End_OS_LifeCycle()

-process Table:
-replaced occupied and blocked fields with singular field called state that is used to determine whether a worker is blocked, ready, running, or terminated/DNE

possible values for state:

-3: STATE_BLOCKED = meaning the worker picked the task to wait for event and has been put in blocked queue and must wait in the blocked queue until the os wakes it up and puts it in the ready queue when its eventWait time is passed

-1: STATE_RUNNNING = worker has been scheduled to run by os for a specific timeslcie

-0: STATE_TERMINATED = the worker has finished its task and notifies os that is will terminate OR this process table slot is empty (DNE)
action messages: action = 1, then worker is releasing resource else if action = 0 worker is claimingi


page table is a field of the process table so each process has size 32 page table
page table is array of ints of 0 or 1. 1 means page address is in memory, 0 means its not

frame  properties: occupied = 1 or 0. pageAddress = 0 -31, owner = worker pid that requested address, dirtyBit is 0 for read and 1 for write

msgbuffer: address = page address, action = dirt bit, workerID = child pid

both blocked queue and frame table use the frame struct and its fields.

the blocked queue uses frames to remeber the entire request a blocked worker made so we know what exactly to swap when a frame slot becomes empty 

fullfillments done via SwapOutFrame method

SwapInFrames to add in blocked requests when a frame table slot becomes empty from fullfillment or process terminated 
