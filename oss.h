#include <sys/types.h>
#include <stdio.h>
//represents shared memory Operating system clock
struct Sys_Time {
	int seconds;//current second
	int nanoseconds; //current nanosecond
	int rate;//increment rate
};
//process table
struct PCB {
	int state;//is or is not working (1/0)
	pid_t pid; //worker process id
	int startSeconds;//second launched
	int startNano; //nanosecond launched
};
//message data sent through message queue
typedef struct msgbuffer {
	long mtype;
	int address;
        int offset;	
	int action;
	int workerID;
} msgbuffer;


const int STATE_BLOCKED = 3;
//worker state running
const int STATE_RUNNING = 1;
//worker state terminated
const int STATE_TERMINATED = 0;
//table size of process table
const int TABLE_SIZE = 100;
//time statics 
const int HALF_SEC = 500000000;

const int MAX_NANOSECOND = 1000000000;

//shrd memory and msg queue key
const int MSG_SYSTEM_KEY = 5303;

const int SYS_TIME_SHARED_MEMORY_KEY = 63131;

//help information
void Help();

//parses arguments to get -n value (workerAmount), -s value (workerSimLimit), -t value for (timeInterval), -f value for logfile name 
//workerAmount = amount of workers os will launch in total
//workerSimLimit = amount of workers allowed to run on os at once
//timeInterval = every timeInterval maount of time passes in the system clock a new porcess is to be launched if possible
//fileName = file information will be logged too
void ArgumentParser(int argc, char** argv, int* workerSimLimit, int* timeInterval,char** fileName);

//validates command line arguments input and shuts down software if argument is invalid
int ValidateInput(int workerAmount, int workerSimLimit, int timeInterval, char* fileName);

//sets up shared memory location and ties the address to are os's system clock
int StartSystemClock(struct Sys_Time **Clock);

//detaches shared memory from systemc clock and removes shared memory location
void StopSystemClock(struct Sys_Time *Clock, int sharedMemoryId);

//increments clock based on incRate
void RunSystemClock(struct Sys_Time *Clock, int incRate);

//handles how many workers are laucnhed,if its time to launch a new worker as well as if its ok too
//runs the system clock
//prints process table and handles logger
//calls scheduler to schedule a process from ready queue to run  and WakeUpProcess to see if a blocked worker doesnt have to be blocked anymore
//updates process table info
//handles response messages from workers after they run
//workAmount = n value, workerSimLimit = -s value, timeInterval = -t value, logFile = -f value, OsClock = shared memory clock, table is process pcb table 
void WorkerHandler(int workerAmount, int workerSimLimit, int timeInterval,char* logFile, struct Sys_Time* OsClock, struct PCB table[]);
//logs a particular message to logfile 
int LogMessage(FILE* logger, const char* format,...);

// await to see if a worker is done
//return 0 if no workers done
//returns id of worker done if a worker is done
int AwaitWorker();

void UpdateWorkerStateInProcessTable(struct PCB table[], pid_t workerId, int state);

//launches worker processes
//amount launched at once is based on simLimit, adds workers id and state to ready as well as start clock time to process table post launch, 
void WorkerLauncher(int amount, struct PCB table[], struct Sys_Time* clock, FILE* logger);
//starts alarm clock for 10 seconds
void Begin_OS_LifeCycle();
//kills os after 10 second event signal is triggered
void End_OS_LifeCycle();

void AddWorkerToProcessTable(struct PCB table[], pid_t workerId, int secondsCreated, int nanosecondsCreated);

int GetWorkerIndexFromProcessTable(struct PCB table[], pid_t workerId);

void PrintProcessTable(struct PCB processTable[],int curTimeSeconds, int curTimeNanoseconds, FILE *logger);

void BuildProcessTable(struct PCB table[]);

int CanEvent(int curSec,int curNano,int eventSecMark,int eventNanoMark);

int ConstructMsgQueue();

void DestructMsgQueue(int msqid);

int RequestHandler(int msqid, msgbuffer *msg);
void ResponseHandler(int msqid, int workerId, msgbuffer *msg);
void GenerateTimeToEvent(int currentSecond,int currentNano,int timeIntervalNano,int timeIntervalSec, int* eventSec, int* eventNano);

int WakeUpProcess(struct PCB table[], int msqid);

void Report(struct PCB table[]);

