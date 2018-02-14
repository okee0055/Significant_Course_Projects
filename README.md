# Significant_Course_Projects
Repository of biggest course projects completed.

OSShell.cpp:

GRADE RECEIVED: A

compile: g++ -o <executable_name> OSShell.cpp -lpthread -std=c++11

run: ./OSShell

  This was an assignment in which we were to wrote c++ code which would use fork() and exec() to create a new shell. The new shell would have all the normal functionality of the usual Linux shell as well as have access to our custome lsr command. lsr is a lot like ls and will show the conents of files, color the files according to type (Directory, executable, other) and will print subdirectories and files tabbed in.
  
  A partner an I wrote the lsr function, and the team of four all worked on fitting the lsr into the OSShell code. I also wrote the str_split function at the bottom of the file.
  
  
 scheduler_sim.cpp:
 
 GRADE RECEIVED: A
 
 compile: g++ -o <executable_name> scheduler_sim.cpp -lpthread -std=c++11
 
 run: ./scheduler_sim -c <num_processors> -p <num_processes> -s <schedular_algorithm> -o <context_switch_lenth_in> -t <time_slice_length>
 
 	-c input must be between 1-4
	-p input. Must be between 1-24
	-s input. Must be between 0-3
      0: first come first serve
      1: round robin
      2: shortest job first (shortest time remaining)
      3: Premptive priority
	-o input. Must be between 100-1000
	-t input. Must be between 200-2000 (only needed if round robin algorithm is chosen)
 
   This is a program which simulates different scheduler algorithms with different number of cores, processes, context switch times, and if round robin, time slice values. Running this will show you stats on the processes being simulated which updates periodically. At the end it will print out final statistics on CPU utilization, average throughput, average turnaround time, and average wait time. This was done to see the benefits of the different scheduling algorithms.
   
   This was written by myself and a partner. The "printerer" function at the end was written by a third member of the team who was gone for the last half of the project.
   
   
mmu_sim.cpp:

GRADE RECEIVED: B

compile: g++ -o <executable_name> mmu.cpp -lpthread -std=c++11

run: ./mmu <power_of_two>

	-it is not checked for, but the program worked best if the power of two is somewhere around 8,192-65,536 (2^13-2^16)

  This progam was meant to simulate a memory managment unit, and give us hands on experience implementing a paging algorithm. We had to simulate what happens when there is a page fault (finding open space on hard disc, loading information from another process there, updating the page table, and vitual address tables, load the needed memory in from the hard disc).
  
  This was a group assingment, I wrote everything past line 120 on my own due to the other members of my team being unavailable near the end of the assignment. Unfortunately this program does not work perfectly and it does not do a few things such as find free space between allocations within a program (For example if you had process 1024 and allocated 3 variables, x, y, z and deleted x, then you wanted to allocate a new variable t, which is smaller than y. Although there is space left over where y was to be used, it would be left untouched. This causes some extra internal fragmentation of memory within the pages, but this program does correctly de-allocate pages who have all memory freed on from them).
  
  This grade would have been an A without a missreading of the instructions. The instructor wanted us to create a file which would contain the actual 1's and 0's of the memory allocations for the RAM and hard disc. The wording of it though did not make this clear. From the outside, when one uses the program it has all the functionalities that were desired though, and without looking at the code, one would never know that it was implemented differently than what was expected.
   
   
 
