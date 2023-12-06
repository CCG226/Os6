#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <errno.h>
#include "oss.h"
#include <string.h>
#include "stdarg.h"
//Author: Connor Gilmore
//Purpose: Program task is to scheudle, handle and launch x amount of worker processes 
//The Os will terminate no matter what after 10 seconds
//the OS operates its own system clock it stores in shared memory to control worker runtimes
//the OS stores worker status information in a process table
//program takes arguments: 
//-h for help,
//-s for number of child processes to run simultanously
//-t time interval to launch nxt child
//-f for the name of the  log file you want os to log too. (reccommend .txt)

//os gets 10 seconds of life
void Begin_OS_LifeCycle()
{
	printf("\n\nBooting Up Operating System...\n\n");	
	//set alarm to trigger a method to kill are program after 10 seconds via signal
	signal(SIGALRM, End_OS_LifeCycle);

	alarm(10);

}
//os is done
void End_OS_LifeCycle()
{
	//clear shared memmory and message queue as the program failed to reack EOF
	int shm_id = shmget(SYS_TIME_SHARED_MEMORY_KEY, sizeof(struct Sys_Time), 0777);
	shmctl(shm_id, IPC_RMID, NULL);
	int msqid = msgget(MSG_SYSTEM_KEY, 0777);
	msgctl(msqid, IPC_RMID, NULL);
	//kill system
	printf("\n\nOS Has Reached The End Of Its Life Cycle And Has Terminated.\n\n");	
	exit(1);

}	

int main(int argc, char** argv)
{
	srand(time(NULL));

	//set 10 second timer	
	Begin_OS_LifeCycle();

	//process table
	struct PCB processTable[TABLE_SIZE];

	//assigns default values to process table, page table, waitlist queue, frame table. constructor
	BuildProcessTable(processTable);

	BuildPageTable(processTable);

	frame waitList[WAIT_TABLE_SIZE];

	BuildBlockedQueue(waitList);

	frame frameTable[FRAME_TABLE_SIZE];

	BuildFrameTable(frameTable);

	//stores shared memory system clock info (seconds passed, nanoseconds passed, clock speed(rate))
	struct Sys_Time *OS_Clock;

	//default values for argument variables are defaults
	int workerAmount = 100;//max limit of workers is 100
	int simultaneousLimit = 0;//-s simul value (number of workers to run in sync)
	int timeInterval = 0;//-t iter value (time interval to launch nxt worker every t time)
	char* logFileName = NULL;//-f value to store file name of log file


	//stored id to access shared memory for system clock
	int shm_ClockId = -1;

	//pass  simultaneousLimit (-s),timeLimit(-t) by reference and assigns coresponding argument values from the command line to them
	ArgumentParser(argc, argv,&simultaneousLimit, &timeInterval, &logFileName);

	//'starts' aka sets up shared memory system clock and assigns default values for clock 
	shm_ClockId = StartSystemClock(&OS_Clock);

	//handles how os laumches, tracks, and times workers
	WorkerHandler(workerAmount, simultaneousLimit, timeInterval,logFileName, OS_Clock, processTable, frameTable, waitList);
	//removes system clock from shared mem
	StopSystemClock(OS_Clock ,shm_ClockId);
	printf("\n\n\n");
	return EXIT_SUCCESS;

}
//create os message queue system for inter process communication
int ConstructMsgQueue()
{
	int id = -1;
	//create a message queue and return id of queue
	if((id = msgget(MSG_SYSTEM_KEY, 0777 | IPC_CREAT)) == -1)
	{
		printf("Failed To Construct OS's Msg Queue\n");
		exit(1);

	}
	return id;
}
//destroy message queue at end of life
void DestructMsgQueue(int msqid)
{
	//delete message queue from system
	if(msgctl(msqid, IPC_RMID, NULL) == -1)
	{
		printf("Failed To Destroy Message Queue\n");
		exit(1);
	}
}
//handles incoming worker requests
int RequestHandler(int msqid, msgbuffer *msg) 
{
	//see if workers sent a message, return 1 if worker did

	ssize_t result = msgrcv(msqid, msg, sizeof(msgbuffer), 1, IPC_NOWAIT);
	if(result != -1)
	{


		return 1;
	}
	return 0;
}
//sends a response back to worker when their page adddress has been added to the frame table
void ResponseHandler(int msqid, int workerId, msgbuffer *msg)
{
	msg->mtype = workerId;	
	if(msgsnd(msqid,msg, sizeof(msgbuffer)-sizeof(long),0) == -1)
	{
		printf("Worker terminated without resource response\n");
		exit(1);
	}
}
//start shared mem clock
int StartSystemClock(struct Sys_Time **Clock)
{
	//sets up shared memory location for system clock
	int shm_id = shmget(SYS_TIME_SHARED_MEMORY_KEY, sizeof(struct Sys_Time), 0777 | IPC_CREAT);
	if(shm_id == -1)
	{
		printf("Failed To Create Shared Memory Location For OS Clock.\n");

		exit(1);
	}
	//attaches system clock to shared memory
	*Clock = (struct Sys_Time *)shmat(shm_id, NULL, 0);
	if(*Clock == (struct Sys_Time *)(-1)) {
		printf("Failed to connect OS Clock To A Slot In Shared Memory\n");
		exit(1);
	}
	//set default clock values
	//clock speed is 50000 nanoseconds per 'tick'
	(*Clock)->rate = 0;
	(*Clock)->seconds = 0;
	(*Clock)->nanoseconds = 0;

	return shm_id;
}
//end shared memory clock
void StopSystemClock(struct Sys_Time *Clock, int sharedMemoryId)
{ //detach shared memory segement from our system clock
	if(shmdt(Clock) == -1)
	{
		printf("Failed To Release Shared Memory Resources..");
		exit(1);
	}
	//remove shared memory location
	if(shmctl(sharedMemoryId, IPC_RMID, NULL) == -1) {
		printf("Error Occurred When Deleting The OS Shared Memory Segmant");
		exit(1);
	}
}
//increment system clock
void RunSystemClock(struct Sys_Time *Clock, int incRate) {

	//clock iteration based on incRate value, if negative (worker terminating time slice return) change value to positive
	Clock->rate = incRate;
	//handles clock iteration / ticking
	if((Clock->nanoseconds + Clock->rate) >= MAX_NANOSECOND)
	{
		Clock->nanoseconds = (Clock->nanoseconds + Clock->rate) - MAX_NANOSECOND;	
		Clock->seconds = Clock->seconds + 1;
	}
	else {
		Clock->nanoseconds = Clock->nanoseconds + Clock->rate;
	}


}
//skip time 
void FastForwardClock(struct Sys_Time *Clock,int ffSec,int ffNano)
{
	Clock->seconds = ffSec;
	Clock->nanoseconds = ffNano;
}
//pase arguments
void ArgumentParser(int argc, char** argv,  int* workerSimLimit, int* timeInterval, char** fileName) {
	//assigns argument values to workerAmount, simultaneousLimit, workerArg variables in main using ptrs 
	int option;
	int tempIgnore = 0;
	//getopt to iterate over CL options
	while((option = getopt(argc, argv, "hn:s:t:f:")) != -1)
	{
		switch(option)
		{//junk values (non-numerics) will default to 0 with atoi which is ok
			case 'h'://if -h
				Help();
				break;
			case 'n'://if -n int_value
				printf("n argument has been depricated. default amount of workers is 100. Do NOT use -n\n");
				exit(1);
				break;
			case 's'://if -s int_value
				*(workerSimLimit) = atoi(optarg);
				break;
			case 't'://if t int_value
				*(timeInterval) = atoi(optarg);
				break;
			case 'f':
				*(fileName) = optarg;
				break;
			default://give help info
				Help();
				break; 
		}

	}
	//check if arguments are valid 
	int isValid = ValidateInput(tempIgnore, *workerSimLimit, *timeInterval, *fileName);

	if(isValid == 0)
	{//valid arguments 
		return;	
	}
	else
	{
		exit(1);
	}
}
//ensure input args are valid
int ValidateInput(int workerAmount, int workerSimLimit, int timeInterval, char* fileName)
{
	//acts a bool	
	int isValid = 0;
	printf("file: %s, s: %d, t: %d", fileName, workerSimLimit, timeInterval);
	FILE* tstPtr = fopen(fileName, "w");
	//if file fails to open then throw error
	//likely invald extension
	if(tstPtr == NULL)
	{
		printf("\nFailed To Access Input File\n");
		exit(1);
	}
	fclose(tstPtr);
	//arguments cant be negative 
	if(/*workerAmount < 0 ||*/ workerSimLimit < 0 || timeInterval < 0)
	{
		printf("\nInput Arguments Cannot Be Negative Number!\n");	 
		isValid = 1;	
	}
	/*	//args cant be zero
		if(workerAmount < 1)
		{
		printf("\nNo Workers To Launch!\n");
		isValid = 1;
		}
	//no more than 20 workers
	if(workerAmount > 20)
	{
	printf("\nTo many Workers!\n");
	isValid = 1;
	}*/
	//need to launch 1 worker at a time mun
	if(workerSimLimit < 1)
	{
		printf("\nWe Need To Be Able To Launch At Least 1 Worker At A Time!\n");
		isValid = 1;
	}
	if(workerSimLimit > 18)
	{
		printf("\nWe Can Only Run Up To 18 Workers Simultanously!\n");
		isValid = 1;

	}
	//time interval must be less than a second
	if(timeInterval > MAX_NANOSECOND)
	{
		printf("\nTime Interval must be less than 1 second!\n");
		isValid = 1;
	}
	return isValid;

}
//block worker becuase page fault occurred, address requested aint in memory and frame table full
void BlockWorker(struct PCB table[], frame waitList[],int pageAddress,int workerID, int dirtBit)
{

	UpdateWorkerStateInProcessTable(table, workerID, STATE_BLOCKED);

	for(int i = 0; i < WAIT_TABLE_SIZE;i++)
	{
		if(waitList[i].occupied == 0)
		{
			AddToQueue(i, pageAddress, workerID, waitList, dirtBit);
			//printf("blocked id: %d , address: %d, dirtyBit: %d, \n",workerID, pageAddress,dirtBit);

			break;
		}	

	}
}
//used to move head ptr of waitlist queue or frame table
int MoveQueueHead(int oldHead, int size)
{
	int newHead = oldHead;	
	if(oldHead == (size - 1))
	{
		newHead = 0;
	}
	else
	{
		newHead++;
	}

	return newHead;
}
//take the first frame request from the waitlist queue out to put into frame table
frame FreeBlockedProcessRequest(frame table[], int* head)
{
	frame selected;

	for(int i = 0; i < WAIT_TABLE_SIZE;i++)
	{
		if(table[*(head)].occupied == 0)
		{
			*(head) = MoveQueueHead(*(head), WAIT_TABLE_SIZE);

		}
		else
		{

			int HIndex = *(head);
			selected = table[HIndex];
			RemoveFrame(HIndex, table[HIndex].pageAddress, table[HIndex].owner, table);
			return selected;


		}		  


	}
	return selected;
}
//logging
int LogMessage(FILE* logger, const char* format,...)
{//logging to file
	static int lineCount = 0;
	lineCount++;
	if(lineCount > 10000)
	{
		return 1;
	}

	va_list args;

	va_start(args,format);

	vfprintf(logger,format,args);

	va_end(args);

	return 0;
}
//conver offset of request page address to address block between 0 - 31
int CalculatePageAddress(int address)
{
	int pAddr;	
	if(address < 1000)
	{
		pAddr = 0;
	}
	else if(address > 32000)
	{
		pAddr = 31;
	}
	else
	{
		pAddr = address / 1000;
	}
	return pAddr;
}
//paging alg
////return 0 for no page fault and 1 for page fault happened
int Pager(struct PCB processTable[],frame frameTable[],int address,int workerID)
{

	for(int i = 0; i < FRAME_TABLE_SIZE; i++)
	{
		if((frameTable[i].pageAddress == address) && (frameTable[i].owner == workerID))
		{//if request frame already in frame table, return no page fault
			return 0;
		}
	}	 
	for(int i = 0; i < FRAME_TABLE_SIZE; i++)
	{
		if(frameTable[i].occupied == 0)
		{//if slot in page table is empty add requested frame to it, no page fault occured
			AddFrame(i, address, workerID, frameTable); 	  
			UpdateProcessPageTable(GetWorkerIndexFromProcessTable(processTable,workerID),processTable,address, MEM_SLOT_ACTIVE); 
			return 0;	   	   
		}


	}
	//page table full and requested page address not in memory, so page fault
	return 1; 
}
//update processes apge table
//memorySlotState is either set to MEM_SLOT_ACTIVE meaning page has been stored in frame table or MEM_SLOT_EMPTY meaning page address not in memory 
void UpdateProcessPageTable(int workerIndex, struct PCB table[],int pageAddress, int memorySlotState)
{
	for(int i = 0; i < PAGE_TABLE_SIZE;i++)
	{
		if(i == pageAddress)
		{

			table[workerIndex].page[i] = memorySlotState;	
		}
	}

}
//used to delete a frame from blocked queue if its a proccess gets to leave blocked queue 
//OR  we are deleteing frame from frame table becuase its process owner terminated or its frame request has been fullfulled
void RemoveFrame(int frameIndex, int pageAddress, int workerID, frame table[])
{

	table[frameIndex].occupied = 0;
	table[frameIndex].pageAddress = -1;
	table[frameIndex].owner = 0;
	table[frameIndex].dirtyBit = 0;
}
//used to add frame to frame table
void AddFrame(int frameIndex, int pageAddress, int workerID, frame table[])
{
	table[frameIndex].occupied = 1;
	table[frameIndex].pageAddress = pageAddress;
	table[frameIndex].owner = workerID;
}
//used to add frame to blocked queue so we remember which request should be retrieved when a frame slot in the table is available in the future
void AddToQueue(int frameIndex, int pageAddress, int workerID, frame table[], int dirtBit)
{
	table[frameIndex].occupied = 1;
	table[frameIndex].pageAddress = pageAddress;
	table[frameIndex].owner = workerID;
	table[frameIndex].dirtyBit = dirtBit;
}
//change dirtybit from read to write
////return 1 if change occurred, 0 if dirty bit stayed the same
int AlterDirtyBit(frame frameTable[],int dirtyBit,int workerID,int pageAddress)
{
	if(dirtyBit == 0)
	{
		return 0;
	}	
	for(int i = 0; i < FRAME_TABLE_SIZE;i++)
	{
		if((frameTable[i].pageAddress == pageAddress) && (frameTable[i].owner == workerID))
		{
			if(frameTable[i].dirtyBit == 0)
			{
				frameTable[i].dirtyBit = 1;
			}

		}

	}
	return 1;
}
//occurs during fullfillemnt when address is no longer needed to be in frame table
void SwapOutFrame(frame frameTable[],struct PCB processTable[], int* head, FILE* logger)
{
	for(int i = 0; i < FRAME_TABLE_SIZE;i++)
	{
		if(frameTable[*(head)].occupied == 0)
		{
			*(head) = MoveQueueHead(*(head), FRAME_TABLE_SIZE);

		}
		else
		{
			int HIndex = *(head);
			LogMessage(logger, "Removing data in frame %d, containing page address %d from process %d.\n", HIndex, frameTable[HIndex].pageAddress, frameTable[HIndex].owner);
			UpdateProcessPageTable(GetWorkerIndexFromProcessTable(processTable, frameTable[HIndex].owner), processTable, frameTable[HIndex].pageAddress, MEM_SLOT_EMPTY);
			RemoveFrame(HIndex, frameTable[HIndex].pageAddress, frameTable[HIndex].owner, frameTable);
			break;

		}
	}
	*(head) = MoveQueueHead(*(head), FRAME_TABLE_SIZE);

}
//when process terminates clear its page table to make it empty
void CleanPageTable(int workerID, struct PCB processTable[])
{
	int workerIndex = GetWorkerIndexFromProcessTable(processTable, workerID);

	for(int i = 0; i < PAGE_TABLE_SIZE;i++)
	{
		if(processTable[workerIndex].page[i] == MEM_SLOT_ACTIVE)
		{
			processTable[workerIndex].page[i] = MEM_SLOT_EMPTY;
		}
	}

}
//when process termianted get rid of all its addresses stored in the frame table
int EmptyWorkerFrames(int workerID, frame frameTable[])
{
	int amountEmptied = 0;	
	for(int i = 0; i < FRAME_TABLE_SIZE;i++)
	{
		if(frameTable[i].owner == workerID)
		{
			amountEmptied++;
			RemoveFrame(i, frameTable[i].pageAddress, workerID, frameTable);	  
		}

	}
	return amountEmptied; 
}
//returns 1 if blocked queue is empty
int IsBlockedQueueEmpty(frame waitList[])
{
	int count = 0;

	for(int i = 0; i < WAIT_TABLE_SIZE;i++)
	{
		if(waitList[i].occupied == 0)
		{
			count++;
		}
	}
	if(count == WAIT_TABLE_SIZE)
	{
		return 1;
	}
	return 0;
}
//return 1 if blocked queue is full
int IsBlockedQueueFull(frame waitList[], int simLimit)
{
	int count = 0;

	for(int i = 0; i < WAIT_TABLE_SIZE;i++)
	{
		if(waitList[i].occupied == 1)
		{
			count++;
		}
	}
	// printf("fast %d\n",count);
	if(count == simLimit)
	{
		return 1;
	}
	return 0;
}
//add in new addresses when a process termiantes or after a frame has been fullfilled
//amountEmpty is number of empty frames that need to be filled since when a worker terminates many frames are released
int SwapInFrames(frame frameTable[],frame waitList[],int* waitListHead, int amountEmpty, int msqid, msgbuffer* msg, struct PCB processTable[], FILE* logger)
{
	int dirtBitAlter = 0;
	for(int i = 0; i < amountEmpty;i++)
	{
		if(IsBlockedQueueEmpty(waitList) == 1)
		{//if blocked queue is empty, exit b/c no new frames to add
			break;
		}
		//select a new request to be added to frame table
		frame selected = FreeBlockedProcessRequest(waitList, waitListHead);
		LogMessage(logger, "Swaping In Blocked Request for memory location %d from process %d\n",selected.pageAddress,selected.owner);
		//send message to wake up worker whos reques has been added to frame table
		ResponseHandler(msqid, selected.owner, msg);
		//update info
		UpdateWorkerStateInProcessTable(processTable, selected.owner, STATE_RUNNING);

		AddFrame(FindEmptyFrame(frameTable),selected.pageAddress, selected.owner, frameTable);

		UpdateProcessPageTable(GetWorkerIndexFromProcessTable(processTable, selected.owner),processTable, selected.pageAddress, MEM_SLOT_EMPTY);

		int didAlterOccur = AlterDirtyBit(frameTable, selected.dirtyBit, selected.owner,selected.pageAddress);	

		if(didAlterOccur == 1)
		{//to count how many dirty bit alters occurid during swap ins to inc clock after func
			dirtBitAlter++;
		}		
		//add frame to frame table,
		//send message to worker
		//update worker state to running in pcb
		//update process page table
	}
	return dirtBitAlter;
}
//find an empty fram to put a address in
int FindEmptyFrame(frame table[])
{
	for(int i = 0; i < FRAME_TABLE_SIZE;i++)
	{
		if(table[i].occupied == 0)
		{
			return i;
		}

	}
	printf("Error Swapping In New Frame\n");
	exit(1);
}
void WorkerHandler(int workerAmount, int workerSimLimit,int timeInterval, char* logFile, struct Sys_Time* OsClock, struct PCB processTable[], frame frameTable[], frame waitList[])
{	//access logfile
	FILE *logger = fopen(logFile, "w");
	//get id of message queue after creating it
	int msqid = ConstructMsgQueue();

	//tracks amount of workers finished
	int workersComplete = 0;

	//tracks amount of workers left to be launched
	int workersLeftToLaunch = workerAmount;

	//amount of workers in ready, blocked, or running state	
	int workersInSystem = 0;
	//increment clock to let us launch first worker
	RunSystemClock(OsClock, timeInterval);
	//holds next time we are allowed to launch a new worker after t time interval		
	int timeToLaunchNxtWorkerSec = OsClock->seconds;
	int timeToLaunchNxtWorkerNano = OsClock->nanoseconds;
	//holds next time we can output porcess table every half seoond
	int timeToOutputSec = 0;
	int timeToOutputNano = 0;
	//calcualtes time to print table for timeToOutput varables
	GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,0,5, &timeToOutputSec, &timeToOutputNano);

	int timeToFullFillSec = 0;
	int timeToFullFillNano = 0;
	//time to next fullfillment
	GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,FULLFILLMENT_TIME,0,&timeToFullFillSec, &timeToFullFillNano); 

	msgbuffer msg;

	//heads fro frame table and wait list	
	int frameTableHead = 0;
	int waitListHead = 0;
	//statistic info for report
	int totalMemAccess = 0;
	int totalPageFaults = 0;
	//keep looping until all workers (-n) have finished working
	while(workersComplete != workerAmount)
	{
		//if thier are still workers left to launch
		if(workersLeftToLaunch != 0)
		{//if thier are less workers in the system the the allowed simultanous value of amount of workers that can run at once

			if(workersInSystem < workerSimLimit)
			{ 	
				//it timeInterval t has passed
				if(CanEvent(OsClock->seconds, OsClock->nanoseconds, timeToLaunchNxtWorkerSec, timeToLaunchNxtWorkerNano) == 1)
				{	
					//laucnh new worker
					WorkerLauncher(1, processTable, OsClock, logger);

					RunSystemClock(OsClock, 500000);

					workersLeftToLaunch--;

					workersInSystem++;

					//determine nxt time to launch worker
					GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,timeInterval,0, &timeToLaunchNxtWorkerSec, &timeToLaunchNxtWorkerNano);

				}
			}
		}		

		//if 5 second passed, print process table
		if(CanEvent(OsClock->seconds,OsClock->nanoseconds, timeToOutputSec, timeToOutputNano) == 1)
		{

			RunSystemClock(OsClock, 5000);				
			PrintProcessTable(processTable, OsClock->seconds, OsClock->nanoseconds,logger);

			PrintFrameTable(frameTableHead,frameTable,  OsClock->seconds, OsClock->nanoseconds,logger);

			GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,0, 5, &timeToOutputSec, &timeToOutputNano);
		}
		//ig fullfillment time occured
		if(CanEvent(OsClock->seconds, OsClock->nanoseconds, timeToFullFillSec, timeToFullFillNano) == 1)
		{
			//take out frma at head of frame table queue
			SwapOutFrame(frameTable,processTable, &frameTableHead, logger);

			//swap in a new fram from blocked queue and see if dirty bit alter occured	
			int didAlterOccur = SwapInFrames(frameTable, waitList,&waitListHead, 1, msqid, &msg, processTable, logger);

			if(didAlterOccur  == 1)
			{
				LogMessage(logger, "Dirty Bit swap occurred during swap in of a new frame\n");
				RunSystemClock(OsClock, 5000000);
			}


			RunSystemClock(OsClock, 50000000);

			GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,FULLFILLMENT_TIME,0,&timeToFullFillSec, &timeToFullFillNano); 

		}
		//see if we got msg from a worker
		int gotMsg = RequestHandler(msqid,&msg);	

		RunSystemClock(OsClock, 50000);
		//1 means we did get request from worker
		if(gotMsg == 1)
		{//if we got a worker message
			//calc page address
			int pageAddress = CalculatePageAddress(msg.address);
			//do paging alg
			int pageFault =	Pager(processTable, frameTable, pageAddress, msg.workerID);
			//if no page fault
			if(pageFault == 0)
			{
				RunSystemClock(OsClock, 100000); 
				int didAlterOccur = AlterDirtyBit(frameTable, msg.action, msg.workerID, pageAddress);	
				LogMessage(logger, "Successful memory access from worker %d's request. No Page fault\n", msg.workerID);
				//if dirty bit was altered
				if(didAlterOccur == 1)
				{


					LogMessage(logger, "Dirty Bit swap occurred during Non page faulted memory access\n");
					RunSystemClock(OsClock, 5000000);

				}
				//send msg back to worker to indicate successful completion of adding request to frame table
				ResponseHandler(msqid, msg.workerID, &msg);

			}
			//page fault occured so block worker request until frame table slot available
			if(pageFault == 1)
			{
				LogMessage(logger, "Page Fault occurred during request of worker %d, blocking worker until frame table spot is open.\n", msg.workerID); 	
				BlockWorker(processTable, waitList, pageAddress, msg.workerID, msg.action);

				RunSystemClock(OsClock, 500000);

				totalPageFaults++; 
			}
			totalMemAccess++;
		}		

		//see if worker has teminated
		int workerFinishedId = AwaitWorker();


		if(workerFinishedId != 0)
		{//worker has terminated
			workersComplete++;
			//empty is page table anf frame table slots
			CleanPageTable(workerFinishedId, processTable);

			int amountEmpty = EmptyWorkerFrames(workerFinishedId, frameTable);

			LogMessage(logger, "process %d is terminating, removing its %d memory frames in the frame table and clearing its page table\n",workerFinishedId, amountEmpty);	
			//add new addresses from blocked queue to replace newly emptied frame table slots
			int didAlterOccur = SwapInFrames(frameTable, waitList,&waitListHead, amountEmpty, msqid, &msg, processTable,logger);
			//inc clock based on amount of dirty bits swapped during adding new frames 	
			if(didAlterOccur > 0)
			{


				LogMessage(logger, "%d Dirty Bit swaps occurred during swap in of new frame(s) after process %d terminated\n", didAlterOccur, workerFinishedId);
				RunSystemClock(OsClock, 500000 * didAlterOccur);

			}
			RunSystemClock(OsClock, 5000000);


			LogMessage(logger, "Process %d terminated naturally\n",workerFinishedId);


			UpdateWorkerStateInProcessTable(processTable, workerFinishedId, STATE_TERMINATED);

			workersInSystem--;

		}		
		if(IsBlockedQueueFull(waitList, workerSimLimit) == 1)
		{//if wait queue is full of all workers currently running skip time to next fullfillment to prevent deadlocks and free a worker in b-queue
			LogMessage(logger,"Block Queue is full, fastforwarding clock to next fullfillment\n");
			FastForwardClock(OsClock,timeToFullFillSec, timeToFullFillNano); 
		}



	}

	Report(OsClock->seconds, totalMemAccess,totalPageFaults);
	DestructMsgQueue(msqid);
	fclose(logger);
}

//check if we passed a time for an event
int CanEvent(int curSec,int curNano,int eventSecMark,int eventNanoMark)
{
	if(eventSecMark == curSec && eventNanoMark <= curNano)
	{
		return 1;
	}
	else if(eventSecMark < curSec)
	{
		return 1;	
	}
	else
	{
		return 0;
	}

}
//generate time to next event, any event
void GenerateTimeToEvent(int currentSecond,int currentNano,int timeIntervalNano,int timeIntervalSec, int* eventSec, int* eventNano)
{//adds timeInterval time to current time (which is system clock) and stores result in event time ptr variables to be used to know when a particular event will occur on the system clock
	*(eventSec) = currentSecond + timeIntervalSec;
	if(currentNano + timeIntervalNano >= MAX_NANOSECOND)
	{
		*(eventNano) = (currentNano + timeIntervalNano) - MAX_NANOSECOND;
		*(eventSec) = *(eventSec) + 1;
	}
	else
	{
		*(eventNano) = (currentNano + timeIntervalNano);
	}
}
//aunch worker
void WorkerLauncher(int amount, struct PCB table[], struct Sys_Time* clock, FILE* logger)
{


	//keep launching workers until limit (is reached
	//to create workers, first create a new copy process (child)
	//then replace that child with a worker program using execlp

	for(int i = 0 ; i < amount; i++)
	{


		pid_t newProcess = fork();




		if(newProcess < 0)
		{
			printf("\nFailed To Create New Process\n");
			exit(1);
		}
		if(newProcess == 0)
		{//child process (workers launching and running their software)

			execlp("./worker", "./worker", NULL);

			printf("\nFailed To Launch Worker\n");
			exit(1);
		}
		//add worker launched to process table 
		AddWorkerToProcessTable(table,newProcess, clock->seconds, clock->nanoseconds);
		LogMessage(logger, "OSS: Generating process with PID %d and putting it in ready queue at time %d:%d\n",newProcess, clock->seconds, clock->nanoseconds);
	}	
	return;
}
//see if worker has terminated
int AwaitWorker()
{	
	int pid = 0;

	int stat = 0;
	//nonblocking await to check if any workers are done
	pid = waitpid(-1, &stat, WNOHANG);

	if(pid == -1)
	{
		perror("error occured awaiting worker\n");
		exit(1);
	}	
	if(WIFEXITED(stat)) {

		if(WEXITSTATUS(stat) != 0)
		{
			exit(1);
		}	
	}

	//return pid of finished worker
	//pid will equal 0 if no workers done
	return pid;
}
//using worker id, return its index from process table
int GetWorkerIndexFromProcessTable(struct PCB table[], pid_t workerId)
{//find the index (0 - 19) of worker with pid passed in params using table
	for(int i = 0; i < TABLE_SIZE;i++)
	{

		if(table[i].pid == workerId)
		{

			return i;
		}

	}
	printf("Invalid Worker Id: %d\n", workerId);
	while(1)
	{}
}
//add worker to process table
void AddWorkerToProcessTable(struct PCB table[], pid_t workerId, int secondsCreated, int nanosecondsCreated)
{
	for(int i = 0; i < TABLE_SIZE; i++)
	{
		//look for empty slot of process table bt checking each rows pid	
		if(table[i].pid == 0)
		{

			table[i].pid = workerId;
			table[i].startSeconds = secondsCreated;
			table[i].startNano = nanosecondsCreated;
			table[i].state = STATE_RUNNING;
			//break out of loop, prevents assiginged this worker to every empty row
			break;
		}

	}	
}
//change worker state
void UpdateWorkerStateInProcessTable(struct PCB table[], pid_t workerId, int state)
{ //using pid of worker, update status state of theat work. currently used to set a running workers state to terminated (aka 0 value)

	for(int i = 0; i < TABLE_SIZE;i++)
	{
		if(table[i].pid == workerId)
		{
			table[i].state = state;
		}
	}
}	
//initalize
void BuildProcessTable(struct PCB table[])
{ 
	for(int i = 0; i < TABLE_SIZE;i++)
	{ //give default values to process table so it doesnt output junk
		table[i].pid = 0;
		table[i].state = 0;
		table[i].startSeconds = 0;
		table[i].startNano = 0;
	}	

}	
void BuildBlockedQueue(frame waitList[])
{
	for(int i = 0; i < WAIT_TABLE_SIZE;i++)
	{

		waitList[i].occupied = 0;
		waitList[i].pageAddress = -1;
		waitList[i].owner = 0;
		waitList[i].dirtyBit = 0;

	}
}
void BuildPageTable(struct PCB table[])
{

	for(int i = 0; i < TABLE_SIZE;i++)
	{	 
		for(int j = 0; j < PAGE_TABLE_SIZE;j++)
		{
			table[i].page[j] = MEM_SLOT_EMPTY;
		}

	}

}
void BuildFrameTable(frame frameTable[])
{

	for(int i = 0; i < FRAME_TABLE_SIZE;i++)
	{
		frameTable[i].occupied = 0;
		frameTable[i].pageAddress = -1;
		frameTable[i].owner = 0;
		frameTable[i].dirtyBit = 0;
	}
}
void PrintFrameTable(int head, frame frameTable[], int curTimeSeconds, int curTimeNanoseconds, FILE* logger)
{
	LogMessage(logger,"Current memory layout at time %d:%d is:\n", curTimeSeconds, curTimeNanoseconds);
	LogMessage(logger,"Current head: %d\n", head);
	LogMessage(logger,"    Occupied    Address     Owner    DirtyBit \n");

	printf("Current memory layout at time %d:%d is:\n", curTimeSeconds, curTimeNanoseconds);
	printf("Current head: %d\n", head);
	printf("    Occupied    Address     Owner    DirtyBit \n");
	for(int i = 0 ; i < FRAME_TABLE_SIZE; i++)
	{
		LogMessage(logger,"%d       %d        %d       %d       %d\n", i, frameTable[i].occupied, frameTable[i].pageAddress, frameTable[i].owner, frameTable[i].dirtyBit);

		printf("%d       %d        %d       %d       %d\n", i, frameTable[i].occupied, frameTable[i].pageAddress, frameTable[i].owner, frameTable[i].dirtyBit);
	}
}
void PrintWaitListTable(int head, frame waitList[], int curTimeSeconds, int curTimeNanoseconds, FILE* logger)
{
	LogMessage(logger,"Current memory layout at time %d:%d is:\n", curTimeSeconds, curTimeNanoseconds);
	LogMessage(logger,"Current head: %d\n", head);
	LogMessage(logger,"    Occupied    Address     Owner    DirtyBit \n");

	printf("Current memory layout at time %d:%d is:\n", curTimeSeconds, curTimeNanoseconds);
	printf("Current head: %d\n", head);
	printf("    Occupied    Address     Owner    DirtyBit \n");
	for(int i = 0 ; i < WAIT_TABLE_SIZE; i++)
	{
		LogMessage(logger,"%d       %d        %d       %d       %d\n", i, waitList[i].occupied, waitList[i].pageAddress, waitList[i].owner, waitList[i].dirtyBit);

		printf("%d       %d        %d       %d       %d\n", i, waitList[i].occupied, waitList[i].pageAddress, waitList[i].owner, waitList[i].dirtyBit);
	}
}
//pirnt table
void PrintProcessTable(struct PCB processTable[],int curTimeSeconds, int curTimeNanoseconds, FILE* logger)
{ //printing 
	int os_id = getpid();	
	LogMessage(logger, "\nOSS PID:%d SysClockS: %d SysclockNano: %d\n",os_id, curTimeSeconds, curTimeNanoseconds);
	LogMessage(logger, "Process Table:\n");
	LogMessage(logger,"      State      PID       StartS       StartN  \n");

	printf("\nOSS PID:%d SysClockS: %d SysclockNano: %d\n",os_id, curTimeSeconds, curTimeNanoseconds);
	printf("Process Table:\n");
	printf("      State      PID       StartS       StartN \n");
	for(int i = 0; i < TABLE_SIZE; i++)
	{
		if(processTable[i].pid != 0)
		{
			LogMessage(logger, "%d        %d          %d          %d         %d  \n", i, processTable[i].state, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano);
			printf("%d        %d          %d          %d         %d    \n", i, processTable[i].state, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano);

		}
	}	
}

//print report
void Report(int curSec, int totalMemAcc, int totalPageFaults)
{
	printf("\nOS REPORT:\n");
	printf("Number of memory accesses per second overall in the system: %d\n", totalMemAcc / curSec);
	if(totalPageFaults != 0)
	{	
		printf("For every %d memory accesses their was approximantly 1 page fault.\n", totalMemAcc / totalPageFaults);
	}
	printf("Total number of memory accesses: %d\n", totalMemAcc);
	printf("Total number of page faults: %d\n", totalPageFaults);

}

//help info
void Help() {
	printf("When executing this program, please provide three numeric arguments");
	printf("Set-Up: oss [-h] [-s simul] [-t time]");
	printf("The [-h] argument is to get help information");
	printf("The [-s int_value] argument to specify how many workers are allowed to run simultaneously");
	printf("The [-t int_value] argument for a time interval  to specify that a worker will be launched every t amount of nanoseconds. (MUST BE IN NANOSECONDS)");
	printf("The [-f string_value] argument is used to specify the file name of the file that will be used as log a file");
	printf("Example: ./oss -s 3 -t 10000 -f test.txt");
	printf("\n\n\n");
	exit(1);
}
