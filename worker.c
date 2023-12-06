#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "worker.h"
#include <errno.h>
#include <time.h>
//Author: Connor Gilmore
//Purpose: takes in a numeric argument
//Program will take in two arguments that represent a random time in seconds and nanoseconds
//are program will then work for that amoount of time by printing status messages
//are worker will constantly check the os's system clock to see if it has reached the end of its work time, if it has it will terminate

int main(int argc, char** argv)
{
	srand(getpid());

	TaskHandler();//do worker task


	return EXIT_SUCCESS;
}
//creates message queue
int AccessMsgQueue()
{
	//access message queue create by workinh using key constant
	int msqid = -1;
	if((msqid = msgget(MSG_SYSTEM_KEY, 0777)) == -1)
	{
		perror("Failed To Join Os's Message Queue.\n");
		exit(1);
	}
	return msqid;
}
//cretes access to system clock
struct Sys_Time* AccessSystemTime()
{//access system clock from shared memory (read-only(
	int shm_id = shmget(SYS_TIME_SHARED_MEMORY_KEY, sizeof(struct Sys_Time), 0444);
	if(shm_id == -1)
	{
		perror("Failed To Access System Clock");

		exit(1);	
	}
	return (struct Sys_Time*)shmat(shm_id, NULL, 0);

}
//disconeects from ahred clock memory
void DisposeAccessToShm(struct Sys_Time* clock)
{//detach system clock from shared memory
	if(shmdt(clock) == -1)
	{
		perror("Failed To Release Shared Memory Resources.\n");
		exit(1);
	}

}

//workers task
void TaskHandler(int workerResources[])
{
	//get access to msg queue and clock
	int msqid = AccessMsgQueue();

	struct Sys_Time* Clock = AccessSystemTime();

	//determines of worker can keep working, if 0 worker must terminate
	int status = RUNNING;

	msgbuffer msg;
	//trackes total amount of requests worker makes
	int totalRequests = 0;
	//generate a request goal, if goal is reached worker considers terminating
	int requestGoal = GenerateNextRequestGoal();
	//while status is NOT 0, keep working
	while(status != TERMINATING)
	{

		//determine to request read or write to address
		int action = MemoryAction();

		int addressSlot = GetAddress();
		//generates specifc address reques	
		int address = GetOffset(addressSlot);

		//send request to os for page address
		SendRequest(msqid,&msg,address,  action);

		totalRequests++;
		//get response
		GetResponse(msqid, &msg);



		if(TerminateEvent(totalRequests, &requestGoal) == 1)
		{//determines if worker should quit after reaching its goal number of amount of requests to os
			status = TERMINATING;	   
		}	   

	}

	DisposeAccessToShm(Clock);

}
//calculate time to next worker event
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
//check time to see if event time has been passed so we know when to do event
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
//deciding to terminate
int ShouldTerminate()
{//when to terminate
	int option = (rand() % 100) + 1;

	if(option <= 75)
	{
		return 1;
	}
	return 0;
}
//handl checking to see if we should terminate
////if terminate is 0 dont termiante
//if termiante is 1 then terminate worker
int TerminateEvent(int totalAmountOfRequests, int *requestGoal)
{
	int terminate;	
	if(totalAmountOfRequests < *(requestGoal))
	{//if we are below request goal dont terminate
		terminate = 0;

	}
	else
	{
		//if we are above request randomly decide if we should terminate
		terminate = ShouldTerminate();	

		if(terminate == 0)
		{	//if we are not terminating  then generate a new request goal
			*(requestGoal) = GenerateNextRequestGoal();
		}

	}
	return terminate;
}
//generate a goal of every 1000 +/- memory references. if goal is reached decide to terminate
int GenerateNextRequestGoal()
{
	int random = (rand() % 100) + 1;

	return (900 + random);
}
//get os repsosne
void GetResponse(int msqid, msgbuffer *msg)
{

	//wait for os to send message with amount of time we can run for

	if(msgrcv(msqid, msg, sizeof(msgbuffer), getpid(), 0) == -1)
	{
		//		printf("Worker %d did recieve resource", getpid());
		exit(1);
	}

	return;
}
//send mesage to os with page address we want, if we want to read or write, and who we are (pid)
void SendRequest(int msqid, msgbuffer *msg,int address, int requestAction)
{//send amount of time worker ran and  amount of time worker wait to access external resource (if it doesnt, eventWaitTime = 0)
	msg->address = address;
	msg->action = requestAction;
	msg->mtype = 1;
	msg->workerID = getpid();
	//send message back to os
	if(msgsnd(msqid, msg, sizeof(msgbuffer)-sizeof(long),0) == -1) {
		perror("Failed To Generate Response Message Back To Os.\n");
		exit(1);
	}

}
//decide to read or write
//read = 60%
//write = 40%
int MemoryAction()
{//wdevide to claim or release
	int option = (rand() % 100) + 1;

	if(option <= 40)
	{
		return 1;
	}
	return 0;
}
//get page address block 0 -31 at random
int GetAddress()
{

	return (rand() % 32);

}
//calculate specifc address in memmory block to request
int GetOffset(int addr)
{
	int loc = addr * 1024;

	int randOffset = (rand() % 1024);

	return (loc + randOffset);

}

