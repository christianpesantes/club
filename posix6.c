
//																	posix6.c
//----------------------------------------------------------------------------
//	Christian Pesantes, 1231218
//	COSC 4330 - Principles of Operating Systems, Summer 2014
//	Programming Assignments #3 : Managing a Club

//	Description:
//	This program simulates a club that wants to enforce a female-male patrons
//	ratio; this requires the use of semaphores and shared memory segments.

//	Input:
//	This programs require a .txt file with the list of patrons coming to
//	the club; it must follow the following format:
//	'serial_num  gender  time(delay)  time(stay)'
//	The name of the file should be provided as an argument for main!
//	NOTE!
//	The provided input must have a solution! A wrong input file with no
//	solution could cause the program to behave erratically. For example:
//	An input file with only males will result in n processes doing a P
//	operation on a semaphore that will never increase due to lack of
//	female patrons coming to the club

//	Output:
//	The status of each patron will be printed to the screen

//	Instructions:
//	Compile >> 	gcc posix6.c -lrt -o posix6
//	Run		>>	./posix6 input3.txt

//	Sources:
//	This program is based on the material provided by Dr. Paris
//----------------------------------------------------------------------------


//																	includes
//----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>


//																	constants
//----------------------------------------------------------------------------
#define NUM_ARGS_REQ 1
#define LIST_BUFFER 100

#define SUCCEEDED 1
#define FAILED -1
#define TRUE 1
#define FALSE 0

#define MALE 'M'
#define FEMALE 'F'
#define RATIO 3

#define KEY_MAX 9000000
#define KEY_MIN 6000000
#define KEY_ATTEMPTS 3

#define SEM_NAME_BUFFER 128
#define SEM_NAME_MUTEX "capt_mutex"
#define SEM_NAME_ADMIT "capt_admit"
	
//															struct : Patron
//----------------------------------------------------------------------------
typedef struct _Patron
{
	int serial_num;		//	id for the patron
	char gender;		//	either M or F
	int delay;			//	delay between incoming patrons
	int time;			//	party time at the club
	
} Patron;

//														struct : Statistics
//----------------------------------------------------------------------------
typedef struct _Statistics
{
	int club_female;	//	number of females in club
	int club_male;		//	number of males in club
	
} Statistics;

//													struct : SharingManager
//----------------------------------------------------------------------------

typedef struct _SharingManager
{
	long  key_mem;			//	key to memory segment
	int sid_mem;			//	id to memory segment
	Statistics * stats;		//	data to be shared
	
} SharingManager;

//														struct : Semaphore
//----------------------------------------------------------------------------
typedef struct _Semaphore
{
	sem_t * smphr;					//	semaphore
	char name [SEM_NAME_BUFFER];	//	name of semaphore
	
} Semaphore;


//										function prototypes : major functions
//----------------------------------------------------------------------------
int Setup (	Patron ** list, int * total, SharingManager * shared, 
			Semaphore * mutex, Semaphore * admit, int * argc, char ** argv);
		
void RunSimulation ( 	Patron ** list, int * total, SharingManager * shared, 
						Semaphore * mutex, Semaphore * admit);
						
void Cleanup (	Patron ** list, SharingManager * shared, 
				Semaphore * mutex, Semaphore * admit);

				
//										function prototypes : minor functions
//----------------------------------------------------------------------------

int ReadingInput (	Patron ** list, int * total, int * argc, char ** argv);
void PrintList (Patron ** list, int * total);
int CreateSharedMemory (SharingManager * shared);
long GetRandomKey();
int CreateSemaphore (Semaphore * smphr, char * name, int value);
void FreeSharedMemory (SharingManager * shared);
void FreeSemaphore (Semaphore * smphr);
void MaleProcess (	Patron * male, SharingManager * shared, 
					Semaphore * mutex, Semaphore * admit);
void FemaleProcess (Patron * female, SharingManager * shared, 
					Semaphore * mutex, Semaphore * admit);

					
//															main function
//----------------------------------------------------------------------------
//	This is the core of the simulation; first we create the necessary
//	objects, then we send them to Setup in order to initialize their values;
//	the simulation will only continue if we have a successful Setup.
//	RunSimulation will then create the child processes, and finally
//	Cleanup will free all memory allocations and semaphores.
//----------------------------------------------------------------------------
int main(int argc, char ** argv)
{
	Patron * list;
	int total;
	
	SharingManager shared;
	
	Semaphore mutex;
	Semaphore admit;
	
	if (Setup (&list, &total, &shared, &mutex, &admit, &argc, argv) == FAILED) 
		{ return FAILED; }
		
	RunSimulation (&list, &total, &shared, &mutex, &admit);
	
	Cleanup (&list, &shared, &mutex, &admit);

	return 0;
}

//													Major function : Setup
//----------------------------------------------------------------------------
//	Sets the seed for the random generator to be used later on; then it
//	allocates the array to be used as container for the patrons coming
//	to the club. The it creates the shared memory segment and the 2
//	semaphores needed. If at any step, there is an error, the function
//	deallocates and unlinks everything previously used.
//----------------------------------------------------------------------------
int Setup (	Patron ** list, int * total, SharingManager * shared, 
			Semaphore * mutex, Semaphore * admit, int * argc, char ** argv)
{
	srand(time(NULL));
	
	*list = malloc(LIST_BUFFER * sizeof(Patron));
	if (*list == NULL) 
		{ printf("ERROR! allocating list!\n"); return FAILED; }
		
	if (ReadingInput (list, total, argc, argv) == FAILED)
		{ free(*list); return FAILED; }
		
	printf("\n");
	PrintList(list, total);
	
	if (CreateSharedMemory (shared) == FAILED)
		{ free(*list); return FAILED; }
		
	if (CreateSemaphore (mutex, SEM_NAME_MUTEX, 1) == FAILED)
		{ free(*list); FreeSharedMemory(shared); return FAILED; }
		
	if (CreateSemaphore (admit, SEM_NAME_ADMIT, 0) == FAILED)
		{ free(*list); FreeSharedMemory(shared); 
		FreeSemaphore(mutex); return FAILED; }
		
	printf("\n");
	return SUCCEEDED;
}

//											Major function : RunSimulation
//----------------------------------------------------------------------------
//	For every patron in our list, it spawns a new process; depending on
//	the gender, we call the respective functions, and we free the copy
//	of list that every child process has in order to avoid memory leaks.
//	Finally, we wait for every children to be over before moving on.
//----------------------------------------------------------------------------			
void RunSimulation ( 	Patron ** list, int * total, SharingManager * shared, 
						Semaphore * mutex, Semaphore * admit)
{
	int pid;
	int i;
	
	for(i=0; i<*total; i++)
	{
		sleep((*list)[i].delay);	
		if ((pid = fork()) == 0) 
		{
			if ((*list)[i].gender == MALE) 
				{ MaleProcess(&(*list)[i], shared, mutex, admit); }
			else 
				{ FemaleProcess(&(*list)[i], shared, mutex, admit); }
			free(*list);
			_exit(0);
		}
	}
	
	for(i=0; i<*total; i++) { wait(0); }
}

//													Major function : Cleanup
//----------------------------------------------------------------------------
//	Deallocates the list, frees the shared memory segment, and closes and
//	unlinks the two semaphores.
//----------------------------------------------------------------------------
void Cleanup (	Patron ** list, SharingManager * shared, 
				Semaphore * mutex, Semaphore * admit)
{
	free(*list);
	FreeSharedMemory(shared);
	FreeSemaphore(mutex);
	FreeSemaphore(admit);
	
}

//												Minor function : MaleProcess
//----------------------------------------------------------------------------
//	Prints the status of the process when it arrives, enters, and 
//	later leaves; it asks permission before accessing any shared memory
//	segment to the mutex semaphore, and it waits for admission with the
//	admit semaphore.
//----------------------------------------------------------------------------
void MaleProcess (	Patron * male, SharingManager * shared, 
					Semaphore * mutex, Semaphore * admit)
{
	printf("patron #%d [M] arrives\n", male->serial_num);
	
	sem_wait(admit->smphr);
		sem_wait(mutex->smphr);
			shared->stats->club_male++;
		sem_post(mutex->smphr);
	
		printf("patron #%d [M] enters\n", male->serial_num);
		sleep(male->time);
	
		sem_wait(mutex->smphr);
			shared->stats->club_male--;
			if((shared->stats->club_female * RATIO - 
				shared->stats->club_male) > 0) 
				{ sem_post(admit->smphr); }
		sem_post(mutex->smphr);
	
	printf("patron #%d [M] leaves\n", male->serial_num);
}

//												Minor function : FemaleProcess
//----------------------------------------------------------------------------
//	Prints the status of the process when it arrives, enters, and 
//	later leaves; it doesn't need to ask for permission before it enters the
//	club, and depending on the number of males already at the club, it
//	increases the admit semaphore.
//----------------------------------------------------------------------------
void FemaleProcess (Patron * female, SharingManager * shared, 
					Semaphore * mutex, Semaphore * admit)
{
	int i;
	int sem;
	int diff;
	
	printf("patron #%d [F] arrives\n", female->serial_num);
	
	sem_wait(mutex->smphr);

		sem_getvalue(admit->smphr, &sem);
		printf("%d  %d/%d  %d\n", shared->stats->club_female, 
			shared->stats->club_male, RATIO*shared->stats->club_female, sem);

		shared->stats->club_female++;
		sem_getvalue(admit->smphr, &sem);
		for(i=0; i<((RATIO * shared->stats->club_female) - 
			shared->stats->club_male - sem); i++) { sem_post(admit->smphr); }

		sem_getvalue(admit->smphr, &sem);
		printf("%d  %d/%d  %d\n", shared->stats->club_female, 
			shared->stats->club_male, RATIO*shared->stats->club_female, sem);
	sem_post(mutex->smphr);
	
	printf("patron #%d [F] enters\n", female->serial_num);
	sleep(female->time);
		
	sem_wait(mutex->smphr);

		sem_getvalue(admit->smphr, &sem);
		printf("%d  %d/%d  %d\n", shared->stats->club_female, 
			shared->stats->club_male, RATIO*shared->stats->club_female, sem);
		
		shared->stats->club_female--;
		
		diff = RATIO * shared->stats->club_female - shared->stats->club_male;
		if (diff < 0) { diff = 0; }
		sem_getvalue(admit->smphr, &sem);
		for (i=diff; i<sem; i++) { sem_wait(admit->smphr); }
		
		sem_getvalue(admit->smphr, &sem);
		printf("%d  %d/%d  %d\n", shared->stats->club_female, 
			shared->stats->club_male, RATIO*shared->stats->club_female, sem);

	sem_post(mutex->smphr);
	
	printf("patron #%d [F] leaves\n", female->serial_num);
}

//												Minor function : ReadingInput
//----------------------------------------------------------------------------
//	Reads the file specified when running the program and extracts the
//	information so it can later store in the list
//----------------------------------------------------------------------------
int ReadingInput (	Patron ** list, int * total, int * argc, char ** argv)
{
	FILE * file_in;
	Patron p;
	*total = 0;	

	if (*argc != NUM_ARGS_REQ + 1) 
		{ printf("ERROR! not enough arguments provided!\n"); return FAILED; }

	file_in = fopen(argv[1], "r");
	
	if (file_in == NULL) 
		{ printf("ERROR! file '%s' not found!\n", argv[1]); return FAILED; }

	while (fscanf(file_in, "%d %c %d %d", 	&p.serial_num, &p.gender, 
											&p.delay, &p.time) == 4)
	{
		if (*total == LIST_BUFFER-1) 
			{ printf("WARNING! max number of elements read!\n"); break; }
			
		(*list)[*total].serial_num = p.serial_num;
		(*list)[*total].gender = p.gender;
		(*list)[*total].delay = p.delay;
		(*list)[*total].time = p.time;
		
		(*total)++;
	}
	
	fclose (file_in);
	return SUCCEEDED;
}

//												Minor function : GetRandomKey
//----------------------------------------------------------------------------
//	Returns a random key value to be used
//----------------------------------------------------------------------------
long GetRandomKey()
{
	long diff = KEY_MAX - KEY_MIN + 1;
	return (rand()%diff + KEY_MIN);
}

//												Minor function : PrintList
//----------------------------------------------------------------------------
//	Prints the list
//----------------------------------------------------------------------------
void PrintList (Patron ** list, int * total)
{
	int i;
	for(i=0; i<*total; i++)
	{
		printf("%3d %3c %3d %3d\n", 
			(*list)[i].serial_num, 	(*list)[i].gender, 
			(*list)[i].delay, 		(*list)[i].time);
	}
	printf("\n");
}

//										Minor function : CreateSharedMemory
//----------------------------------------------------------------------------
//	IT attempts to create a shared memory segment up to n times using a
//	randomly generated key.
//----------------------------------------------------------------------------
int CreateSharedMemory (SharingManager * shared)
{
	int i = 0;
	
	while (i < KEY_ATTEMPTS)
	{
		shared->key_mem = GetRandomKey();
		shared->sid_mem = shmget(shared->key_mem, 
								sizeof(Statistics), 0600 | IPC_CREAT);
		i++;
		
		if (shared->sid_mem == -1) { continue; }
		
		shared->stats = (Statistics *) shmat(shared->sid_mem, NULL, 0);
		
		if (shared->stats == (void *) -1) 
			{ shmctl(shared->sid_mem, 0, IPC_RMID); continue; }
			
		shared->stats->club_female = 0;
		shared->stats->club_male = 0;
			
		return SUCCEEDED;
	}
	
	printf ("ERROR! shared memory allocation failed after %d attempts!", i);
	return FAILED;
}

//										Minor function : CreateSemaphore
//----------------------------------------------------------------------------
//	Creates a semaphore with the desired name and initial value
//----------------------------------------------------------------------------
int CreateSemaphore (Semaphore * smphr, char * name, int value)
{
	int x;
	memset(smphr->name, 0, sizeof(smphr->name));
	strcpy(smphr->name, name);
	
	smphr->smphr = sem_open(smphr->name, O_CREAT, 0600, value);
	
	if (smphr->smphr == SEM_FAILED)
	{
		printf("ERROR! unable to create %s semaphore!\n", smphr->name);
		sem_unlink(smphr->name);
		return FAILED;
	}
	
	sem_getvalue(smphr->smphr, &x);
	printf("The initial value of %s is %d\n", smphr->name, x);

	return SUCCEEDED;
}

//										Minor function : FreeSharedMemory
//----------------------------------------------------------------------------
//	Clears the shared memory segment
//----------------------------------------------------------------------------
void FreeSharedMemory (SharingManager * shared)
{
	shmdt(shared->stats);
	shmctl(shared->sid_mem, 0, IPC_RMID);
}


//											Minor function : FreeSemaphore
//----------------------------------------------------------------------------
//	Closes the semaphore, and later checks if it was able to unlink properly
//----------------------------------------------------------------------------
void FreeSemaphore (Semaphore * smphr)
{
	sem_close(smphr->smphr);
	if (sem_unlink(smphr->name) != 0)
		{ printf("WARNING: semaphore %s was not deleted\n", smphr->name); }
	
}
