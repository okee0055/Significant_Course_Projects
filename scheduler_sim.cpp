#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;


//Info the scheduler needs to know
struct schedulerInfo{
	int numbCores; //1-4 cores
	int numbProcesses; //1-24 processes
	int algorithm; //choose which algorithm 0,1,2,3
	int contextSwitchNum; //context switching overhead in ms (100-1000)
	int sliceVal; //time slice for round robin (200-2000)
};

//All the info we need for each process
struct processInfo{
	int index; //this is the start of the list if index equals zero

	//stats that vary and are defined on the go
	int startedFlag; //1 if flag is started, otherwise 0
	int readyFlag; //when a process is ready to be grabbed by a core ==1
	int waitFlag; //when a process is undergoing an io burst ==1
	int doneFlag; //when a process terminates ==1
	int readyReady; //when something is ready to be put on the ready queue ==1
	
    char* state; //ready, running, i/o, terminated
    char* core; //if running, give a core number, otherwise --
    double turnTime; //total time since creation (until finished)
    double waitTime; //amount of time waiting in ready queue
    double cpuTime; //amount of time on the cpu core
    double remainTime; //amount of time to finish the algorithm
    double switchingTime; //amount of time context switching
    
    int burstsLeft; //number of bursts left
    double cpuBursts[11]; //random total number 2-10 for IO burst (CPU=IO+1)
    double ioBursts[10]; //random between 2-10 values (int values 1000 to 6000)
    int ioBindex; //io burst index in our double array
    int cpuBindex; //cpu burst index in our double array
    
    //set variables and don't change
    int pid; // start at 1024 and increment by 1
    double startTime; //random between 500 and 8000
    timeval initialTime; //time set when first created
    double initialDouble; //storing timeval of initial time
    int priority; //random number between 0 and 4 
    
    timeval startState; //timeval at the beginning of any state
};

//Information All the Threads have access too --> created before thread creation
struct threadInfo{
	int myCoreIndex; //individual core number
	int algorithm; //which algorithm is being used
	double contextSwitchNum; //context switching overhead in ms (100-1000) GRABBED FROM INFO
	int numProcess; //list of total number of processes
	int numReady; //number of processes ready
	int numWait; //number of processes waiting
	int numDone; //number of process done
	double sliceVal; //round robin value for 'slices' of operations
	double totalSwitch; //total time spent context switching
	double totalCPU; //total time of CPU operations (set at beginning)
	processInfo *allQ; //list of all the processes
	
	timeval initial_t; //when cores are originally started
	timeval half_t; //when half the processes are done
	timeval end_t; //when all the processes are done
};

struct indvThread{
	int myCore;//my core number 0 to 3 depending on setup
	int myProcess; //what process is running on core
	int isEmpty;//is there a process running on me
	timeval ctime;//current time
};


void* scheduler (void* info);
void* core(void* all_procs);
void prioritySort (processInfo processes[], int numProc, int numReady);
void shortSort (processInfo processes[], int numProc, int numReady);
double randNum(int min, int max);
void printerer(processInfo processes[], int numbProcesses, int numbDone);

int main (int argc, char **argv)
{
	srand(time(NULL));
	//GET USER INPUT
	char *tempvalue = NULL;
  	int cval = -1;
  	int pval = -1;
  	int sval = -1;
  	int oval = -1;
  	int tval = -1;
  	int index;
  	int c;
  	int flag_val=0;

	opterr = 0;
	while ((c = getopt (argc, argv, "c:p:s:o:t:")) != -1)
		switch (c){
			case 'c':
				tempvalue = optarg; cval = atoi(tempvalue); break;
			case 'p':
				tempvalue = optarg; pval = atoi(tempvalue); break;
			case 's':
				tempvalue = optarg; sval = atoi(tempvalue); break;
			case 'o':
				tempvalue = optarg; oval = atoi(tempvalue); break;
			case 't':
				tempvalue = optarg; tval = atoi(tempvalue); break;
			case '?':
				if (optopt == 'c'|| optopt == 'p'|| optopt == 's'|| optopt == 'o'|| optopt == 't')
				  	fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				else if (isprint (optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf (stderr,"Unknown option character `\\x%x'.\n",optopt); return 1;
			  	default: abort ();
		}
	
	//c,p,s,o needed
	//t for time slice for Round-Robin
	if(cval<1 || cval>4){printf("Invalid -c input. Must be between 1-4..\n");flag_val = 1;}
	if(pval<1 || pval>24){printf("Invalid -p input. Must be between 1-24..\n");flag_val = 1;}
	if(sval<0 || sval>3){printf("Invalid -s input. Must be between 0-3..\n");flag_val = 1;}
	if(oval<100 || sval>1000){printf("Invalid -o input. Must be between 100-1000..\n");flag_val = 1;;}
	if((tval<200 || tval>2000)&& sval==0){printf("Invalid -t input. Must be between 200-2000\n");flag_val = 1;}
	
	if(flag_val == 1){return 1;}//if we got an error, return

	schedulerInfo info;
	info.numbCores = cval;
	info.numbProcesses = pval;
	info.algorithm = sval;
	info.contextSwitchNum = oval;
	info.sliceVal = tval;
	
	pthread_t sched;
	pthread_create(&sched, NULL, scheduler, &info);
	pthread_join(sched, NULL);
	
	return 0;
}

void* scheduler (void* vinfo){
	//printf("In fcfs scheduler\n");
	schedulerInfo* info = (schedulerInfo*)vinfo;
	int size = info->numbProcesses;
	processInfo processes[size];
    int firstThird =  (info->numbProcesses)/3; //rounds down
    int numDelayed = info->numbProcesses - firstThird;
    int currPID = 1024;
    
    char coreEmpty[3]="--";
    char ready[]="Ready";
    int ioNum;
    int cpuNum;

	for(int i=0; i<info->numbProcesses; i++){
		
		processes[i].cpuBindex=0;
		processes[i].ioBindex=0;
		processes[i].waitFlag=0;
		processes[i].doneFlag=0;
        processes[i].core=coreEmpty;
        processes[i].turnTime=0.0;
        processes[i].waitTime=0.0;
        processes[i].cpuTime=0.0;
        processes[i].switchingTime=0.0;
        //setup bursts////////////////////////
        ioNum = randNum(2,10);
        cpuNum = ioNum + 1;
        double temp0[ioNum];
        double temp1[cpuNum];
        processes[i].burstsLeft = ioNum+cpuNum;
        
        for(int io = 0; io < ioNum; io++)
        	processes[i].ioBursts[io]=((double)(randNum(1000,6000))/1000.0);
        
        //processes[i].cpuBursts=temp1;
        for(int cpu = 0; cpu < cpuNum; cpu++)
        	processes[i].cpuBursts[cpu]=((double)(randNum(1000,6000))/1000.0);


        double cpuTotal=0.0;
        processes[i].remainTime=0.0;
        for(int remain = 0; remain < cpuNum; remain++){
        	cpuTotal=cpuTotal+temp1[remain];
        	processes[i].remainTime = (processes[i].remainTime) + processes[i].cpuBursts[remain];
        }

        processes[i].pid=currPID;
        currPID++;
        gettimeofday(&processes[i].initialTime,NULL);
        gettimeofday(&processes[i].startState,NULL);
        processes[i].priority=0;//random number between 0 and 4
    	processes[i].readyFlag=0;
    	processes[i].startedFlag=0;
        processes[i].state= NULL;
        processes[i].index=-1;
	}
	//assign first third of processes
	for(int fth=0; fth< firstThird; fth++){
        processes[fth].startTime=0;//put in the queue right away, set to current time
	}
	//assign second third of processes
    for(int sth=firstThird; sth< numDelayed+firstThird; sth++){
        processes[sth].startTime=(randNum(500, 8000)/1000.0);//random between 500 and 8000 -- used for last 2/3rds
    }
    
	pthread_t* cores = (pthread_t*)malloc(info->numbCores * sizeof(pthread_t));
	
	threadInfo data_in;
	data_in.contextSwitchNum = (double(info->contextSwitchNum)/1000.0);
	data_in.myCoreIndex = 0;
	data_in.numProcess = info->numbProcesses;
	data_in.numDone = 0;
	data_in.numReady = 0;
	data_in.numWait = 0;
	data_in.allQ = processes;
	data_in.algorithm = info->algorithm;
	data_in.totalSwitch = 0.0;
	
	for(int t257=0; t257<data_in.numProcess; t257++)
		data_in.totalCPU=data_in.totalCPU + processes[t257].remainTime;
	
	gettimeofday(&data_in.initial_t,NULL);
	
	//ROUND ROBIN STUFF////////////////////////////////////
	if(info->algorithm == 0){
		//enter this if round robin
		////set slice val to input
		data_in.sliceVal = ((double)(info->sliceVal)/1000.0);
	}
	else{
		//if it's not in round robin
		////set slice val to large number
		data_in.sliceVal = 1000.0; //large 1000 seconds
	}
	
	//PRIORITY STUFF////////////////////////////////////
	if(info->algorithm == 3){
		//enter this if preemptive priority
		////set priority to random number between 0 & 4
		for(int pp=0; pp<info->numbProcesses; pp++){
			processes[pp].priority=(randNum(0,4));
		}
	}///////////////////////////////////////////////////

	pthread_mutex_lock(&my_mutex);
	for (int q=0; q<info->numbCores; q++){
		pthread_create(&cores[q], NULL, core, &data_in);
	}
	pthread_mutex_unlock(&my_mutex);
	timeval lastPrint;
	double lprint;
	timeval currTime;
	double ctime;
	
	gettimeofday(&lastPrint,NULL);
	lprint=(double)lastPrint.tv_sec +((double)lastPrint.tv_usec/1000000);

	//SCHEDULER LOOP
	while(1){
		pthread_mutex_lock(&my_mutex);
		
		//lock (precious allQ data)
		for(int lp1=0; lp1<data_in.numProcess; lp1++){
			if(data_in.allQ[lp1].index == -42){}
			else if(data_in.allQ[lp1].startedFlag == 1){//test if readyQ,waitQ, done
			
				//do stuff to readyQ
				if(data_in.allQ[lp1].readyFlag ==1){
					gettimeofday(&currTime,NULL);
					ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
					double t293 = (double)data_in.allQ[lp1].startState.tv_sec +((double)data_in.allQ[lp1].startState.tv_usec/1000000);
					data_in.allQ[lp1].waitTime = (data_in.allQ[lp1].waitTime) + (ctime-t293);
					
					double t296 = (double)data_in.allQ[lp1].initialTime.tv_sec +((double)data_in.allQ[lp1].initialTime.tv_usec/1000000);
					data_in.allQ[lp1].turnTime = (ctime - t296);
					
					gettimeofday(&(data_in.allQ[lp1].startState),NULL);
				}
				
				//do stuff to waitQ
				else if(data_in.allQ[lp1].waitFlag ==1){
					
					double tempIOwait= (double)data_in.allQ[lp1].startState.tv_sec +((double)data_in.allQ[lp1].startState.tv_usec/1000000);
					gettimeofday(&currTime,NULL);
					ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
					
					double t307 = (double)(data_in.allQ[lp1].ioBursts[data_in.allQ[lp1].ioBindex]);
					if(tempIOwait + t307 <=ctime){
					//if IO burst is done
						data_in.allQ[lp1].waitFlag = 0;
						data_in.numWait--;
						//update index for bursts
						data_in.allQ[lp1].ioBindex++;
						//update bursts remaining for the previous cpu and io bursts
						data_in.allQ[lp1].burstsLeft--;
						data_in.allQ[lp1].readyReady=1;
						//set its startState timval
						gettimeofday(&(data_in.allQ[lp1].startState),NULL);
					}
					gettimeofday(&currTime,NULL);
					ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
					double t338 = (double)data_in.allQ[lp1].initialTime.tv_sec +((double)data_in.allQ[lp1].initialTime.tv_usec/1000000);
					data_in.allQ[lp1].turnTime=(ctime - t338);
					//check to see if it's ready to go on the ready queue (currently doing IO)
					//IO burst check
					//if (time since entering burst)-(current time)>= IO burst number (in ms)
				}
				
				else if(data_in.allQ[lp1].doneFlag ==1){}//do nothing
			}
			
			else{
				//operations on stuff that is waiting to be "started"
				double tempInitial= (double)data_in.allQ[lp1].initialTime.tv_sec +((double)data_in.allQ[lp1].initialTime.tv_usec/1000000);
				gettimeofday(&currTime,NULL);
				ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
				if(tempInitial+(data_in.allQ[lp1].startTime)<=ctime){
						data_in.allQ[lp1].readyReady = 1;

				}
			}
		}
		
		for(int lp2=0; lp2<data_in.numProcess; lp2++){
			//for scheduling into the ready queue
			//if something is ready to be placed in queue
				//put into the queue
			if(data_in.allQ[lp2].readyReady==1 && data_in.allQ[lp2].index != -42){
				char ready[]="Ready";
				data_in.allQ[lp2].state=ready;
				if(data_in.allQ[lp2].startTime !=0 && data_in.allQ[lp2].startedFlag !=1){
					gettimeofday(&processes[lp2].initialTime,NULL);
				}//only affects the 2nd third of delayed processes
				
				//we enter here when it's ready to go onto the ready queue
				data_in.allQ[lp2].readyFlag = 1;//indicate location
				data_in.allQ[lp2].index = data_in.numReady;//indicate new index
				data_in.numReady++;//add to total number ready
				data_in.allQ[lp2].startedFlag = 1;
				gettimeofday(&(data_in.allQ[lp2].startState),NULL);
				data_in.allQ[lp2].readyReady=0;
				
			}
		}
		if(info->algorithm == 2){
		//order based on the shortest time first
			shortSort(data_in.allQ, data_in.numProcess, data_in.numReady);
		}
		else if(info->algorithm == 3){
		//order based on the priority
			prioritySort(data_in.allQ, data_in.numProcess, data_in.numReady);
		}
		

		//if 2 seconds passed, print new stuff
		gettimeofday(&currTime,NULL);
		ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
		
		
		
		if(ctime-lprint > 0.1){
			//pthread_mutex_lock(&my_mutex);
			//printf("current: %f,   last: %f\n",ctime,lprint);
			for (int k=0; k<info->numbProcesses+2; k++) {
				fputs("\033[A\033[2K", stdout);
			}
			rewind(stdout);
			//print stats
			printerer(processes, info->numbProcesses, data_in.numDone);
			gettimeofday(&lastPrint,NULL);
			lprint=(double)lastPrint.tv_sec +((double)lastPrint.tv_usec/1000000);
			if(data_in.numProcess == data_in.numDone){break;}
		}

		
		pthread_mutex_unlock(&my_mutex);
		
		timeval temptime;
		double tmptime;
		gettimeofday(&temptime,NULL);
		tmptime=(double)temptime.tv_sec +((double)temptime.tv_usec/1000000);
		gettimeofday(&currTime,NULL);
		ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
		while(ctime-tmptime < 0.000001){
			gettimeofday(&currTime,NULL);
			ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
		}
		
	}
	
	
	////////////////////////////
	//PRINT STATS///////////////
	/*
		CPU utilization
		Throughput
			Average for first 50% of processes finished
			Average for second 50% of processes finished
			Overall average
		Average turnaround time
		Average waiting time
		
		Throughput: jobs per time taken
		   1st half = ie. 8jobs/52seconds
		   2nd half = ie. 8jobs/130seconds
	*/
	double avgWait = 0.0;
	double avgTurn = 0.0;
	double f_thru = 0.0;
	double l_thru = 0.0;
	double t_thru = 0.0;
	double utilization = 0.0;
	double total_switch = 0.0;
	
	for(int t449=0; t449<data_in.numProcess; t449++){
		avgWait=avgWait + data_in.allQ[t449].waitTime;
		avgTurn=avgTurn + data_in.allQ[t449].turnTime;
		total_switch = total_switch + data_in.allQ[t449].switchingTime;
	}
	avgWait = avgWait/((double)(data_in.numProcess));
	avgTurn = avgTurn/((double)(data_in.numProcess));
	

	double itime=(double)data_in.initial_t.tv_sec +((double)data_in.initial_t.tv_usec/1000000);
	double htime=(double)data_in.half_t.tv_sec +((double)data_in.half_t.tv_usec/1000000);
	double etime=(double)data_in.end_t.tv_sec +((double)data_in.end_t.tv_usec/1000000);
	
	int second = data_in.numProcess-(data_in.numProcess/2);
	f_thru= double(data_in.numProcess/2)/(htime-itime);
	l_thru= double(data_in.numProcess-(data_in.numProcess/2))/(etime-itime);
	t_thru= double(data_in.numProcess) / (etime-itime);
	
	utilization = 100.0*((data_in.totalCPU)/(data_in.totalCPU+total_switch));
	
	printf("\n------------------STATISTICS------------------\n\n");
	printf("CPU UTILIZATION:\t\t%.1f percent\n",utilization);
	printf("THROUGHPUT:\n     FIRST HALF:\t\t%.4f jobs/sec\n     SECOND HALF:\t\t%.4f jobs/sec\n     TOTAL:\t\t\t%.4f jobs/sec\n",f_thru,l_thru,t_thru);
	printf("AVERAGE TURNAROUND TIME:\t%.3f seconds\n",avgTurn);
	printf("AVERAGE WAITING TIME:\t\t%.3f seconds\n",avgWait);
	
	for (int q=0; q<info->numbCores; q++){
		pthread_join(cores[q],NULL);
	}
	//print statistics
}

void* core(void* data_in){
	threadInfo * info = (threadInfo*)data_in;
	indvThread myInfo;
	pthread_mutex_lock(&my_mutex);
	myInfo.myCore = info->myCoreIndex;
	info->myCoreIndex++;
	myInfo.isEmpty=1;
	pthread_mutex_unlock(&my_mutex);
	
	timeval currTime;
	double ctime;
	double sliceDone=0.0;
	int contextSwitch=0;
	//int decramentFlag;
	//CORE LOOP
	while(1){
		pthread_mutex_lock(&my_mutex);
		
		if(myInfo.isEmpty==1){ //If it's free find next process to put on core
			for(int o = 0; o < info->numProcess; o++){//for all processes
				if(info->allQ[o].index == 0){ //Grab next item in Q

					myInfo.isEmpty = 0; //No longer empty
					myInfo.myProcess = o; //index of process currently on core					
					
					info->allQ[o].index = -1; //Setting next process to -1 to show busy
					
					info->allQ[o].switchingTime = info->allQ[o].switchingTime + info->contextSwitchNum;
					contextSwitch=1;
					
					for(int t493 = 0; t493< o; t493++)
						if(info->allQ[t493].index > 0){info->allQ[t493].index=(info->allQ[t493].index - 1);}
					for(int t495 = (o+1); t495 < info->numProcess; t495++)
						if(info->allQ[t495].index > 0){info->allQ[t495].index=(info->allQ[t495].index - 1);}
						
						
					if(myInfo.myCore == 0){info->allQ[o].core = (char*)" 0";} //Saving core number to process
					else if(myInfo.myCore == 1){info->allQ[o].core = (char*)" 1";}
					else if(myInfo.myCore == 2){info->allQ[o].core = (char*)" 2";}
					else if(myInfo.myCore == 3){info->allQ[o].core = (char*)" 3";}
					info->allQ[o].state = (char*)"Running";
					
					info->numReady--;
					info->allQ[o].readyFlag=0;
					info->allQ[o].waitFlag=0;
					info->allQ[o].doneFlag=0;
					//turntime,waittime,cputime,remaintime

					double tempStart= (double)info->allQ[o].startState.tv_sec +((double)info->allQ[o].startState.tv_usec/1000000);
					
					//turntime
					gettimeofday(&currTime,NULL);
					ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
					double t508 = (double)info->allQ[o].initialTime.tv_sec +((double)info->allQ[o].initialTime.tv_usec/1000000);
					info->allQ[o].turnTime=(ctime-t508);
					
					
					pthread_mutex_unlock(&my_mutex);
					double tmptime;
					timeval temptime;
					double cttime;
					timeval currtTime;
					gettimeofday(&currtTime,NULL);
					//////////////////////////////////////////////////////////////////////
					if(contextSwitch==1){
						gettimeofday(&temptime,NULL);
						tmptime=(double)temptime.tv_sec +((double)temptime.tv_usec/1000000);
						while(cttime-tmptime < info->contextSwitchNum){
							gettimeofday(&currtTime,NULL);
							cttime=(double)currtTime.tv_sec +((double)currtTime.tv_usec/1000000);
						}
						contextSwitch = 0;
					}/////////////////////////////////////////////////////////////////////
					
					pthread_mutex_lock(&my_mutex);
					info->totalSwitch = info->totalSwitch + cttime-tmptime;
					
					gettimeofday(&(info->allQ[o].startState),NULL);
					break;
				}
			}
			
			
			//pthread_mutex_unlock(&my_mutex);
		}//end of empty processor
		
		//(tempIOwait+(double)((data_in.allQ[lp1].ioBursts[data_in.allQ[lp1].ioBindex])/1000.0))
		else if(myInfo.isEmpty == 0){//something is on the processor and we should do time checks
			//if startState + cpuBurstTime <= ctime
			//kick off the process and set values indicating such
			double tempStart= (double)info->allQ[myInfo.myProcess].startState.tv_sec +((double)info->allQ[myInfo.myProcess].startState.tv_usec/1000000);
			gettimeofday(&currTime,NULL);
			ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
			int kickOffFlag = 0;
			
			double t539 = (double(info->allQ[myInfo.myProcess].cpuBursts[info->allQ[myInfo.myProcess].cpuBindex]));
			
			if(info->algorithm == 3){	
				processInfo* idx1;
				processInfo* idxC = &(info->allQ[myInfo.myProcess]);

				for(int i=0; i<((info->numReady)-1); i++){
					//find index[i]
					for(int j=0; j<(info->numProcess); j++){
						if(info->allQ[j].index==i){
						
							idx1=&(info->allQ[j]);
							if(idx1->priority < idxC->priority){
								kickOffFlag = 1;
								break;
							}
						}
					}
				}
				
			}
			
			
			
			if(tempStart+t539 <= ctime){
				if(info->allQ[myInfo.myProcess].remainTime <= 0){
					//terminate the process
					info->allQ[myInfo.myProcess].index=-42;
					info->allQ[myInfo.myProcess].waitFlag=0;
					info->allQ[myInfo.myProcess].readyFlag=0;
					info->allQ[myInfo.myProcess].doneFlag=1;
					info->allQ[myInfo.myProcess].burstsLeft--;
					char done[]="terminated";
					info->allQ[myInfo.myProcess].state=done;
					char emT[]="--";
					info->allQ[myInfo.myProcess].core=emT;
					
					myInfo.isEmpty = 1;
					

					
					
					//turntime
					gettimeofday(&currTime,NULL);
					ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
					double t551=(double)info->allQ[myInfo.myProcess].initialTime.tv_sec +((double)info->allQ[myInfo.myProcess].initialTime.tv_usec/1000000);
					info->allQ[myInfo.myProcess].turnTime=(ctime-t551);
					
					//cputime & remaintime
					double t555=(double)info->allQ[myInfo.myProcess].startState.tv_sec +((double)info->allQ[myInfo.myProcess].startState.tv_usec/1000000);
					info->allQ[myInfo.myProcess].cpuTime = (info->allQ[myInfo.myProcess].cpuTime) + (ctime-t555);
					//info->allQ[myInfo.myProcess].remainTime = (info->allQ[myInfo.myProcess].remainTime) - (ctime-t555);
					info->allQ[myInfo.myProcess].remainTime = 0.000;
					
					
					info->numDone++;
					gettimeofday(&(info->allQ[myInfo.myProcess].startState),NULL);//not necessary because it has completed
					myInfo.myProcess = -1;
					sliceDone = 0.0;
					
					if(info->numDone == (info->numProcess/2)){
					//halfway point
						gettimeofday(&info->half_t,NULL);
					}
					else if(info->numDone == (info->numProcess)){
					//end point
						gettimeofday(&info->end_t,NULL);
					}
					
				}
				else if(info->allQ[myInfo.myProcess].remainTime > 0){
					//there are more cpu bursts to come so we should put this in the wait Q
					//if(info->allQ[myInfo.myProcess].index == -42){printf("Hey Hey Hey\n");break;}
					info->allQ[myInfo.myProcess].waitFlag=1;
					info->allQ[myInfo.myProcess].readyFlag=0;
					char ioStr[]="i/o";
					info->allQ[myInfo.myProcess].state=ioStr;
					char emT[]="--";
					info->allQ[myInfo.myProcess].core=emT;
					info->allQ[myInfo.myProcess].cpuBindex++;
					info->allQ[myInfo.myProcess].burstsLeft--;
					myInfo.isEmpty=1;
					info->numWait++;
					//turntime,waittime,cputime,remaintime

					gettimeofday(&currTime,NULL);
					ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
					double t591 = (double)info->allQ[myInfo.myProcess].initialTime.tv_sec +((double)info->allQ[myInfo.myProcess].initialTime.tv_usec/1000000);
					info->allQ[myInfo.myProcess].turnTime=(ctime-t591);
					
					//cputime & remaintime
					double t555=(double)info->allQ[myInfo.myProcess].startState.tv_sec +((double)info->allQ[myInfo.myProcess].startState.tv_usec/1000000);
					info->allQ[myInfo.myProcess].cpuTime = (info->allQ[myInfo.myProcess].cpuTime) + (ctime-t555);
					info->allQ[myInfo.myProcess].remainTime = (info->allQ[myInfo.myProcess].remainTime) - (ctime-t555);
					
					//waittime edit in scheduler
					gettimeofday(&(info->allQ[myInfo.myProcess].startState),NULL);
					myInfo.myProcess = -1;
					sliceDone = 0.0;
				}

			}
			
			else if((tempStart + info->sliceVal - sliceDone <= ctime) && ((info->myCoreIndex+1)<info->numReady)){
			//implement what happens if it kicks off due to slice val time being up
				info->allQ[myInfo.myProcess].readyReady = 1;
				info->allQ[myInfo.myProcess].readyFlag = 1;
				char readyStr[]="ready";
				info->allQ[myInfo.myProcess].state=readyStr;
				char emT[]="--";
				info->allQ[myInfo.myProcess].core=emT;
				myInfo.isEmpty=1;

				//TURNTIME
				double t653 = (double)info->allQ[myInfo.myProcess].initialTime.tv_sec +((double)info->allQ[myInfo.myProcess].initialTime.tv_usec/1000000);
				info->allQ[myInfo.myProcess].turnTime=(ctime-t653);
				
				//REMAINTIME, CPUTIME, BURSTTIME
				double t657=(double)info->allQ[myInfo.myProcess].startState.tv_sec +((double)info->allQ[myInfo.myProcess].startState.tv_usec/1000000);
				double t658= ctime-t657;
				info->allQ[myInfo.myProcess].remainTime = (info->allQ[myInfo.myProcess].remainTime) - (t658);
				info->allQ[myInfo.myProcess].cpuTime = (info->allQ[myInfo.myProcess].cpuTime) + (t658);
				
				info->allQ[myInfo.myProcess].cpuBursts[info->allQ[myInfo.myProcess].cpuBindex] = info->allQ[myInfo.myProcess].cpuBursts[info->allQ[myInfo.myProcess].cpuBindex]-t658;
				gettimeofday(&(info->allQ[myInfo.myProcess].startState),NULL);
				sliceDone = 0.0;
				myInfo.myProcess = -1;
			
			}
			else if(kickOffFlag == 1){
			//if something has a smaller priority, kick the current off
			////should implement much like the sliceVal because we want it back on the ready Q
				
				//basically compare the current process with every single process
				////if another process is ready and has a smaller priority, kick the current process off
				
				kickOffFlag = 0;
				//kick the current process off
				info->allQ[myInfo.myProcess].readyReady = 1;
				info->allQ[myInfo.myProcess].readyFlag = 1;
				char readyStr[]="ready";
				info->allQ[myInfo.myProcess].state=readyStr;
				char emT[]="--";
				info->allQ[myInfo.myProcess].core=emT;
				myInfo.isEmpty=1;

				//TURNTIME
				double t653 = (double)info->allQ[myInfo.myProcess].initialTime.tv_sec +((double)info->allQ[myInfo.myProcess].initialTime.tv_usec/1000000);
				info->allQ[myInfo.myProcess].turnTime=(ctime-t653);
			
				//REMAINTIME, CPUTIME, BURSTTIME
				double t657=(double)info->allQ[myInfo.myProcess].startState.tv_sec +((double)info->allQ[myInfo.myProcess].startState.tv_usec/1000000);
				double t658= ctime-t657;
				info->allQ[myInfo.myProcess].remainTime = (info->allQ[myInfo.myProcess].remainTime) - (t658);
				info->allQ[myInfo.myProcess].cpuTime = (info->allQ[myInfo.myProcess].cpuTime) + (t658);
			
				info->allQ[myInfo.myProcess].cpuBursts[info->allQ[myInfo.myProcess].cpuBindex] = info->allQ[myInfo.myProcess].cpuBursts[info->allQ[myInfo.myProcess].cpuBindex]-t658;
				gettimeofday(&(info->allQ[myInfo.myProcess].startState),NULL);
				sliceDone = 0.0;
				myInfo.myProcess = -1;
			}
			
			else{
				gettimeofday(&currTime,NULL);
				ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
				double t653 = (double)info->allQ[myInfo.myProcess].initialTime.tv_sec +((double)info->allQ[myInfo.myProcess].initialTime.tv_usec/1000000);
				info->allQ[myInfo.myProcess].turnTime=(ctime-t653);
				
				
				double t657=(double)info->allQ[myInfo.myProcess].startState.tv_sec +((double)info->allQ[myInfo.myProcess].startState.tv_usec/1000000);
				double t658= ctime-t657;
				
				info->allQ[myInfo.myProcess].remainTime = (info->allQ[myInfo.myProcess].remainTime) - (t658);
				info->allQ[myInfo.myProcess].cpuTime = (info->allQ[myInfo.myProcess].cpuTime) + (t658);
				sliceDone = sliceDone + t658;
				
				info->allQ[myInfo.myProcess].cpuBursts[info->allQ[myInfo.myProcess].cpuBindex] = info->allQ[myInfo.myProcess].cpuBursts[info->allQ[myInfo.myProcess].cpuBindex]-t658;
				gettimeofday(&(info->allQ[myInfo.myProcess].startState),NULL);
			
			}
		}
		pthread_mutex_unlock(&my_mutex);
		
		timeval temptime;
		double tmptime;
		gettimeofday(&temptime,NULL);
		tmptime=(double)temptime.tv_sec +((double)temptime.tv_usec/1000000);
		gettimeofday(&currTime,NULL);
		ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
		while(ctime-tmptime < 0.000001){
			gettimeofday(&currTime,NULL);
			ctime=(double)currTime.tv_sec +((double)currTime.tv_usec/1000000);
		}
	}
}

void shortSort (processInfo processes[], int numProc, int numReady){
	//find first index[0]
	////compare with index[1]
	//////if index [0] has a longer time than index [1], swap the two indexes
	//////then compare index[1] and index[2]
	//index can only be as big as (numReady-1)
	processInfo* idx1;
	processInfo* idx2;
	int temp;
	
	for(int i=0; i<(numReady-1); i++){
		//find index[i]
		for(int j=0; j<numProc; j++){
			if(processes[j].index==i){idx1=&processes[j];}
			else if(processes[j].index==(i+1)){idx2=&processes[j];}
		}
		//now that idx1 and idx2 have the ones we want to compare, let's compare
		if(idx1->remainTime > idx2->remainTime){
			temp = idx1->index;
			idx1->index = idx2->index;
			idx2->index = temp;
		}
	}
}

void prioritySort (processInfo processes[], int numProc, int numReady){
	processInfo* idx1;
	processInfo* idx2;
	int temp;
	
	for(int i=0; i<(numReady-1); i++){
		//find index[i]
		for(int j=0; j<numProc; j++){
			if(processes[j].index==i){idx1=&processes[j];}
			else if(processes[j].index==(i+1)){idx2=&processes[j];}
		}
		//now that idx1 and idx2 have the ones we want to compare, let's compare
		if(idx1->priority > idx2->priority){
			temp = idx1->index;
			idx1->index = idx2->index;
			idx2->index = temp;
		}
	}
}


double randNum(int min, int max){
	//requires stdlib.h
	int returnVal;
	double temp;
	temp=(rand()%((max-min)+1)+(double)min);
	returnVal= (int)temp;
	return temp;
}

void printerer(processInfo process[], int numbProcesses, int numbDone){
	char* stateVar;
	int printFlag=1;
	printf("|   PID | Priority |      State | Core | Turn Time |  Wait Time |  CPU Time |  Remain Time |\n");
	printf("+-------+----------+------------+------+-----------+------------+-----------+--------------+\n");
	for(int i = 0; i < numbProcesses; i++){
		if(process[i].doneFlag){stateVar=(char*)"terminated";}
		else if(process[i].readyFlag){stateVar=(char*)"ready";}
		else if(process[i].waitFlag){stateVar=(char*)"i/o";}
		else if(process[i].startedFlag == 0){stateVar=(char*)"--";printFlag=0;}
		else{stateVar=(char*)"running";}
		
		if(printFlag == 1){
		printf("|  %4i |        %1i | %10s |   %2s |   %7.3f |    %7.3f |   %7.3f |     %8.3f |\n",
			process[i].pid,process[i].priority,stateVar,process[i].core,process[i].
			turnTime,process[i].waitTime,process[i].cpuTime,process[i].remainTime);
		}
		printFlag=1;
	}
}