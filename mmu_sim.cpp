#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <string>
#include <string.h>
#include <vector>
#include <iostream>
#include <fstream>

struct Mem_alloc {
    std::string name;
    int size;
    int v_addr;
    //page_num = v_adder / page_size
    //offset = v_adder % page_size
    std::vector<std::string> values;
};
// entry in page table
struct entry {
    int pid;
    int page_num;
    int frame_num;
};
struct Proc {
    int pid;
    std::vector<Mem_alloc> mem;

};


// compile: g++ -o mmu main.cpp -std=c++11

using namespace std;
#define STACK_SIZE 65536
#define RESET   "\033[0m"
#define RED     "\033[31m"      /* Red */
int RAM_SIZE = 67108864;    //67108864 bytes
int HARD_DISC_SIZE = 536870912;
int cur_pid = 1023;
int page_size = 0;
//Create RAM and Hard Disc file
uint8_t *RAM; //init 64MB RAM, physical memory
uint8_t *HD; //init 448MB Hard disk, backing store
std::vector<entry> page_table;

//physical address is (frame_num * page_size)+offset
//      **if this exceeds the 67108864 (64MB) then the frame is in the backing store
//          and must be loaded loaded into mem (and something from mem loaded into
//          the backing store) before it can be used/accessed

//Functions
int create(std::vector<Proc>* procs);
int randomBetweenRange(int, int);
int allocate_mem(int PID, std::vector<Mem_alloc>* mem, std::string name, int size);
int allocate(std::vector<Proc>* procs, int PID, std::string var_name, std::string data_type, int number_of_elements);
int set (std::vector<Proc>* procs, int PID, string var_name, int offset, vector<string> values);
int terminate(std::vector<Proc>* procs, int PID);
int free(std::vector<Proc>* procs, int PID, string var_name);
int print_variable(std::vector<Proc>* procs, int PID, string var_name);
int print_mmu(std::vector<Proc>* procs);
int print_processes(std::vector<Proc>* procs);
int print_page (std::vector<Proc>* procs);
int find_free_frame(int PID);
int load_to_RAM(int PID, int page_num, int num_pages, int page_table_idx);
vector<std::string> str_split(std::string input);
/*
 * ---------- Begin main() ----------
 */
int main(int argc, char **argv) {
    srand(time(NULL));

    std::vector<Proc> procs;

    //Check user arguments
    if (argc == 2) {
        page_size = atoi(argv[1]);//Cast input to int
        if ((page_size == 0) || ((page_size & (page_size - 1)) != 0)) {//Check if input is power of 2
            printf("Error: %d is not a power of 2.\n", page_size);
            exit(1);
        }
    } else { //Either missing page size arg, or too many args received.
        printf("Error: invalid arguments.\n");
        exit(1);
    }
    //Create Backing store and RAM, set each to zero
    //Each entry in these array will contain a 0 if the frame is free
    //or a 1 if the frame is not.
    RAM = new uint8_t[RAM_SIZE/page_size];
    HD = new uint8_t[HARD_DISC_SIZE/page_size];
    memset(RAM, 0, sizeof(RAM)); //set all values to zero
    memset(HD, 0, sizeof(HD)); //set all values to zero

    //Print greeting and command list
    cout << "Welcome to the Memory Allocation Simulator! Using a page size of " << page_size << " bytes.\n";
    cout << "Commands:\n";
    cout << "   * create (initializes a new process)\n";
    cout << "   * allocate <PID> <var_name> <data_type> <number_of_elements> (allocated memory on the heap)\n";
    cout << "   * set <PID> <var_name> <offset> <value_0> <value_1> ... <value_N> (set the value for a variable)\n";
    cout << "   * free <PID> <var_name> (deallocate memory on the heap that is associated with <var_name>)\n";
    cout << "   * terminate <PID> (kill the specified process)\n";
    cout << "   * print <object> (prints data)\n";
    cout << "      - If <object> is \"mmu\", print the MMU memory table\n";
    cout << "      - if <object> is \"page\", print the page table\n";
    cout << "      - if <object> is \"processes\", print a list of PIDs for processes that are still running\n";
    cout << "      - if <object> is a \"<PID>:<var_name>\", print the value of the variable for that process\n";
    cout << "   * exit (quit this program)\n\n";

    //Begin prompting for command
    string input;
    while (1) {
        cout << "> ";
        getline(cin, input);
        vector<string> inputs = str_split(input);
        if (inputs[0] == "exit") {
            cout << "Goodbye!\n";
            break;
        } else if (inputs[0] == "create") {
            int new_proc = create(&procs);
            cout << new_proc << endl;
        } else if (inputs[0] == "allocate") {
            if(inputs.size() == 5) {
                int V_Addr = allocate(&procs, stoi(inputs[1]), inputs[2], inputs[3], stoi(inputs[4]));
                if(V_Addr != -1) cout << V_Addr << endl;
            }else{
                cout << "Invalid allocate options: " << input << endl;
                cout << "allocate <PID> <var_name> <data_type> <number_of_elements>" << endl;
            }
        } else if (inputs[0] == "set") {
            if(inputs.size() >= 4){
                set(&procs, stoi(inputs[1]), inputs[2], stoi(inputs[3]), inputs);
            }else {
                cout << "Invalid set options: " << input << endl;
                cout << "set <PID> <var_name> <offset> <value_0> <value_1> ... <value_N>" << endl;
            }
        } else if (inputs[0] == "free") {
                if(inputs.size() == 3) {
                    free(&procs, stoi(inputs[1]), inputs[2]);
                }else{
                    cout << "Invalid free options: " << input << endl;
                    cout << "free <PID> <var_name>" << endl;
                }
        } else if (inputs[0] == "terminate") {
            terminate(&procs, stoi(inputs[1]));
        } else if (inputs[0] == "print") {
            if(inputs[1] == "mmu"){
                print_mmu(&procs);
            } else if (inputs[1] == "processes") {
                print_processes(&procs);
            } else if (inputs[1] == "page") {
                print_page(&procs);
            } else if(inputs.size() == 3){
                print_variable(&procs, stoi(inputs[1]), inputs[2]);
            }

        } else {
            cout << "Invalid command: " << input << endl;
        }
    }
    return 0;
}

/*
 *Use this to create a new process when the "create" command is intput
 *	It will create a new process stuct, allocate the data, text, and stack
 *  then put the new process at the end of the "procs" vector
 */
int create(std::vector<Proc>* procs) {
    Proc ret;
    int flag = 1;
    int i;
    while(flag){
        flag=0;
        cur_pid++;
        for(i=0; i<procs->size();i++){
            if(procs->at(i).pid == cur_pid){flag=1;}
        }
    }
    ret.pid = cur_pid;
    int data_size = randomBetweenRange(2048, 16384);
    int text_size = randomBetweenRange(0, 1024);
    //do the memory checking handling stuff in the allocate_mem method
    allocate_mem(ret.pid, &ret.mem, "<DATA>", data_size);
    allocate_mem(ret.pid, &ret.mem, "<TEXT>", text_size);
    allocate_mem(ret.pid, &ret.mem, "<STACK>", STACK_SIZE);
    procs->push_back(ret);
    return cur_pid;
}
//generates a random number between "min" and "max"
int randomBetweenRange(int min, int max) {
    return rand() % (max - min + 1) + min;
}
//This is used only for allocating the data, text, and stack when creating a process
int allocate_mem(int PID, std::vector<Mem_alloc>* mem, std::string name, int size) {
    Mem_alloc unit;
    unit.name = name;
    unit.size = size;
    unit.v_addr = 0;
    if (mem->size() > 0) {
        unit.v_addr = mem->back().size + mem->back().v_addr;
    }
    if((unit.v_addr + unit.size - 1)/page_size > (unit.v_addr-1)/page_size || unit.v_addr == 0){
        //if more room is needed or this is the first memory allocation, allocate frames.
        //load the new memory to RAM (load_to_RAM will find a free frame automatically,
        //this includes freeing up RAM frames and sending them to the backing store)
        int last_page_idx = (unit.v_addr + unit.size - 1)/page_size; //last to be allocated
        int first_page_idx = 0;
        if(unit.v_addr > 0){ first_page_idx = ((unit.v_addr-1)/page_size)+1; }
        int num_pages = 1+last_page_idx-first_page_idx; //number being allocated
        load_to_RAM(PID, first_page_idx, num_pages, -1); //allocated pages to RAM
    }

    mem->push_back(unit);
    return 0;
}
/*
 *This function will be called when a user inputs the "allocate" command.
 *	Use the data type and number of elements to find the size of
 *	the variable. Create the Mem_alloc struct for this new variable
 *	and put it in the specified processes mem vector.
 */
int allocate(std::vector<Proc>* procs, int PID, std::string var_name, std::string data_type, int number_of_elements) {
    //create the new variable
    Mem_alloc var;
    var.name = var_name;
    if (data_type == "char") {
        var.size = number_of_elements;
    } else if (data_type == "short") {
        var.size = number_of_elements * 2;
    } else if (data_type == "int" || data_type == "float") {
        var.size = number_of_elements * 4;
    } else if (data_type == "long" || data_type == "double") {
        var.size = number_of_elements * 8;
    } else {
        cout << "Invalid data type: " << data_type << "\n";
        return -1;
    }
    //find the specified process, put new variable into mem vector
    int i, c;
    c = -1;
    for(i=0; i<procs->size(); i++) {
        if (procs->at(i).pid == PID) c = i;
    }
    if (c == -1) {
        cout << "Invalid PID: " << PID << "\n";
        return -1;
    }
    var.v_addr = 0;
    if(procs->at(c).mem.size() > 0){
        var.v_addr = procs->at(c).mem.back().size + procs->at(c).mem.back().v_addr;
    }
    var.values.clear();
    for(i=0; i<number_of_elements; i++){
        var.values.push_back("");
    }
    if((var.v_addr + var.size - 1)/page_size > (var.v_addr-1)/page_size || var.v_addr == 0){
        //if more room is needed or this is the first memory allocation, allocate frames.
        //load the new memory to RAM (load_to_RAM will find a free frame automatically,
        //this includes freeing up RAM frames and sending them to the backing store)
        int last_page_idx = (var.v_addr + var.size - 1)/page_size; //last to be allocated
        int first_page_idx = 0;
        if(var.v_addr > 0){
            first_page_idx = ((var.v_addr-1)/page_size)+1;
        }
        int num_pages = 1+last_page_idx-first_page_idx; //number being allocated
        load_to_RAM(PID, first_page_idx, num_pages, -1); //allocated pages to RAM
    }
    procs->at(c).mem.push_back(var);
    return var.v_addr;
}
int set (std::vector<Proc>* procs, int PID, string var_name, int offset, vector<string> values) {
    //find the specified process
    int i, p, v;
    p = -1;
    v = -1;
    for(i=0; i<procs->size(); i++) {
        if (procs->at(i).pid == PID) p = i;
    }
    if (p == -1) {
        cout << "Invalid PID: " << PID << "\n";
        return -1;
    }
    //find the variable that has var_name
    for(i=0; i<procs->at(p).mem.size(); i++){
        if(procs->at(p).mem.at(i).name == var_name) v = i;
    }
    if (v == -1) {
        cout << "Invalid Variable Name: " << var_name << "\n";
        return -1;
    }
    //now we have the variable (procs->at(p).mem.at(v))
    //make sure it is in RAM
    //find its page #'s
    int first = procs->at(p).mem.at(v).v_addr/page_size;
    int last = (procs->at(p).mem.at(v).v_addr+procs->at(p).mem.at(v).size-1)/page_size;
    vector<int> idxs; //idx's of page_table entries that correspond to this processes variable
    for(i=0; i<page_table.size(); i++){
        //if this is an entry corresponding to this variable of this process...
        if(page_table[i].pid == PID && page_table[i].page_num >= first && page_table[i].page_num <= last){
            //and if this entry says the variable is on Hard Disk...
            if(page_table[i].frame_num > (RAM_SIZE/page_size)){
                //then save this idx
                idxs.push_back(i);
            }
        }
    }
    //load the pages to RAM
    for(i=0;i<idxs.size();i++){
        load_to_RAM(PID, -1, 0, idxs[i]);
    }
    //set it's value(s)
    //**values start at values[4] because it is the whole inputs vector
    if((offset + values.size()-4) > procs->at(p).mem.at(v).values.size()){
        cout << "Invalid Offset/Number of elements: " << offset << "/" << values.size()-4 << endl;
        cout << "Variable " << procs->at(p).mem.at(v).name << " has size " << procs->at(p).mem.at(v).values.size() << endl;
        return -1;
    }
    vector<string>::iterator it;
    for(i=4; i<values.size(); i++) {
        it = procs->at(p).mem.at(v).values.begin();
        it = procs->at(p).mem.at(v).values.erase(it+offset+i-4);
        procs->at(p).mem.at(v).values.insert(it, values[i]);
    }

    return 0;
}
int print_variable(std::vector<Proc>* procs, int PID, string var_name){
    //find the specified process
    int i, p, v;
    p = -1;
    v = -1;
    for(i=0; i<procs->size(); i++) {
        if (procs->at(i).pid == PID) p = i;
    }
    if (p == -1) {
        cout << "Invalid PID: " << PID << "\n";
        return -1;
    }
    //find the variable that has var_name
    for(i=0; i<procs->at(p).mem.size(); i++){
        if(procs->at(p).mem.at(i).name == var_name) v = i;
    }
    if (v == -1) {
        cout << "Invalid Variable Name: " << var_name << "\n";
        return -1;
    }
    //Now I have the variable (procs->at(p).mem.at(v))
    //make sure it is in RAM
    //find its page #'s
    int first = procs->at(p).mem.at(v).v_addr/page_size;
    int last = (procs->at(p).mem.at(v).v_addr+procs->at(p).mem.at(v).size-1)/page_size;
    vector<int> idxs; //idx's of page_table entries that correspond to this processes variable
    for(i=0; i<page_table.size(); i++){
        //if this is an entry corresponding to this variable of this process...
        if(page_table[i].pid == PID && page_table[i].page_num >= first && page_table[i].page_num <= last){
            //and if this entry says the variable is on Hard Disk...
            if(page_table[i].frame_num > (RAM_SIZE/page_size)){
                //then save this idx
                idxs.push_back(i);
            }
        }
    }
    //load the pages to RAM
    for(i=0;i<idxs.size();i++){
        load_to_RAM(PID, -1, 0, idxs[i]);
    }
    if(procs->at(p).mem.at(v).values.size() > 4){
        for(i=0; i<4; i++){
            cout << procs->at(p).mem.at(v).values.at(i) << ", ";
        }
        cout << "... [" << procs->at(p).mem.at(v).values.size() << " items]" << endl;
    } else {
        for(i=0; i<procs->at(p).mem.at(v).values.size()-1; i++) {
            cout << procs->at(p).mem.at(v).values.at(i) << ", ";
        }
        cout << procs->at(p).mem.at(v).values.at(procs->at(p).mem.at(v).values.size()-1) << endl;
    }
    return 0;
}
int print_mmu(std::vector<Proc>* procs){
    printf(" PID  | Variable Name | Virtual Addr | Size     \n");
    printf("------+---------------+--------------+------------\n");
    //Now for each process
    int i, j;
    for(i=0; i<procs->size(); i++){
        //and for each variable of each process
        for(j=0; j<procs->at(i).mem.size(); j++){
            //print its information
            // EX. )1024 | <TEXT>        |   0x00000000 |       5992
            printf(" %4d | %13s |   0x%08X | %d\n", procs->at(i).pid, procs->at(i).mem.at(j).name.c_str(), procs->at(i).mem.at(j).v_addr, procs->at(i).mem.at(j).size);
        }

    }
    return 0;
}
int print_processes(std::vector<Proc>* procs){
    int i;
    for(i=0; i<procs->size(); i++){
        cout << procs->at(i).pid << endl;
    }
    return 0;
}
int print_page (std::vector<Proc>* procs){
    printf(" PID  | Page Number | Frame Number\n");
    printf("------+-------------+--------------\n");
    //Now print each entry in the table
    int i, j;
    for(j=0; j<procs->size(); j++) {
        for (i = 0; i < page_table.size(); i++) {
            if(procs->at(j).pid == page_table.at(i).pid) {
                if (page_table.at(i).frame_num > (RAM_SIZE / page_size) - 1) {
                    printf(RED " %4d | %11d | %12d \n" RESET, page_table.at(i).pid, page_table.at(i).page_num,
                           page_table.at(i).frame_num);
                } else {
                    printf(" %4d | %11d | %12d \n", page_table.at(i).pid, page_table.at(i).page_num,
                           page_table.at(i).frame_num);
                }
            }
        }
    }
    return 0;
}
int free(std::vector<Proc>* procs, int PID, string var_name){
    //find the specified process
    int i, p, v;
    p = -1;
    v = -1;
    for(i=0; i<procs->size(); i++) {
        if (procs->at(i).pid == PID) p = i;
    }
    if (p == -1) {
        cout << "Invalid PID: " << PID << "\n";
        return -1;
    }
    //find the variable that has var_name
    for(i=0; i<procs->at(p).mem.size(); i++){
        if(procs->at(p).mem.at(i).name == var_name) v = i;
    }
    if (v == -1) {
        cout << "Invalid Variable Name: " << var_name << "\n";
        return -1;
    }
    //now we have the variable (procs->at(p).mem.at(v))
    //does this de-allocation free up any frames?
    if(procs->at(p).mem.size() >= 1) {
        int first;
        int last;
        if (v == 0) {
            first = procs->at(p).mem.at(v).v_addr / page_size;
            last = procs->at(p).mem.at(v + 1).v_addr / page_size;
        } else if (v == procs->at(p).mem.size()-1) {
            first = (procs->at(p).mem.at(v - 1).v_addr + procs->at(p).mem.at(v - 1).size - 1) / page_size;
            first = first + 1;
            last = 2000000000; //2 billion pages is impossible;
        } else {
            first = (procs->at(p).mem.at(v - 1).v_addr + procs->at(p).mem.at(v - 1).size - 1) / page_size;
            first = first + 1;
            last = procs->at(p).mem.at(v + 1).v_addr / page_size;
        }

        //first = 0;
        //if(procs->at(p).mem.at(v).v_addr > 0){first = ((procs->at(p).mem.at(v).v_addr-1)/page_size)+1;}
        //last = (procs->at(p).mem.at(v).v_addr+procs->at(p).mem.at(v).size-1)/page_size;
        //num_to_free = 1 + last - first;

        //if so de-allocate the memory associated with this variable
        i = 0;
        int flag = 1;
        while (flag) {
            if (page_table[i].pid == PID && page_table[i].page_num >= first && page_table[i].page_num < last) {
                //free RAM and HD allocations
                if (page_table[i].frame_num < (RAM_SIZE / page_size)) { RAM[page_table[i].frame_num] = 0; }
                else { HD[page_table[i].frame_num - RAM_SIZE / page_size] = 0; }
                //erase entry with PID from page table
                page_table.erase(page_table.begin() + i);
                i = i - 1;
            }
            i++;
            unsigned long length = page_table.size();
            if (i >= length) { flag = 0; }
        }
    }else{
        //this might as well terminate the process, but the process is still running
        i = 0;
        int flag = 1;
        while (flag) {
            if (page_table[i].pid == PID) {
                //free RAM and HD allocations
                if (page_table[i].frame_num < (RAM_SIZE / page_size)) { RAM[page_table[i].frame_num] = 0; }
                else { HD[page_table[i].frame_num - RAM_SIZE / page_size] = 0; }
                //erase entry with PID from page table
                page_table.erase(page_table.begin() + i);
                i = i - 1;
            }
            i++;
            unsigned long length = page_table.size();
            if (i >= length) { flag = 0; }
        }
    }
    //now remove the variable from the process memory
    procs->at(p).mem.erase(procs->at(p).mem.begin() + v);
    return 0;
}
int terminate(std::vector<Proc>* procs, int PID) {

    //erase the proc from the proc array
    int i;
    int c = -1;
    for (i = 0; i < procs->size(); i++) {
        if(procs->at(i).pid == PID){
            procs->erase(procs->begin() + i);
            c = 1;
        }
    }
    if(c == -1){
        cout << "Invalid PID: " << PID << endl;
        return -1;
    }

    i=0;
    int flag=1;
    while(flag) {
        if (page_table[i].pid == PID) {
            //free RAM and HD allocations
            if(page_table[i].frame_num < (RAM_SIZE/page_size)){RAM[page_table[i].frame_num] = 0;}
            else{HD[page_table[i].frame_num - RAM_SIZE/page_size] = 0;}
            //erase entry with PID from page table
            page_table.erase(page_table.begin() + i);
            i = i - 1;
        }
        i++;
        unsigned long length = page_table.size();
        if(i>=length){flag=0;}
    }
    //update cur_pid so that PID's can be reused
    cur_pid = PID - 1;
    return 0;
}
/*
 * Parameter: The PID of the process needing a free frame
 * Return; index of the freed RAM frame
 */
int find_free_frame(int PID){
    // if there is a free frame, return it
    int i;
    for(i=0; i<RAM_SIZE/page_size; i++){
        if(RAM[i]==0){
            return i;
        }
    }
    //otherwise...
    //1. find a frame on RAM not in use by the specified process
    int target_frame = -1;
    int page_table_entry = -1;
    i=0;
    while(target_frame == -1 && i<page_table.size()){
        if(page_table[i].pid != PID && page_table[i].frame_num < RAM_SIZE/page_size){
            target_frame = page_table[i].frame_num;
            page_table_entry = i;
        }
        i++;
    }
    if(target_frame == -1){
        return -2; //this process needs more RAM than there is
                    //Why would one process need more RAM than a system has???
    }
    //2. find a free frame on the hard disk
    int HD_frame = -1;
    for(i=0; i<sizeof(HD); i++){
        if(HD[i]==0){
            HD_frame = i;
        }
    }
    if(HD_frame == -1) {
        return -1; //There is no room left on Hard Disc
    }
    //3. set the HD frame to allocated
    HD[HD_frame] = 1;
    //4. update the page_table
    page_table[page_table_entry].frame_num = HD_frame + RAM_SIZE/page_size;
    //5. set the RAM frame to free
    RAM[target_frame] = 0;
    // return the newly freed frame
    return target_frame;
}
/*
 * Parameters: PID of the process that needs a page loaded
 *             page_num: first new page number for this process (-1 if this is existing memory)
 *             num_pages: number of pages to be put into RAM
 *             page_table_idx: page table index for the information of this existing memory
 *                             (-1 if this is new memory)
 *
 * Return: 0: no errors
 *        -1: if there is not enough room in hard disc to free a RAM frame
 *        -2: if there is not enough RAM for this process
 */
int load_to_RAM(int PID, int page_num, int num_pages, int page_table_idx){
    //if these are new pages to be loaded into RAM
    if(page_table_idx == -1) {
        //create the new entry for each
        int page;
        for(page = page_num; page < page_num+num_pages; page++) {
            //1. find free frame in RAM
            int free_frame = find_free_frame(PID);
            if(free_frame == -1){
                return -1; //not enough room in hard disc to free a RAM frame
            }
            if(free_frame == -2) {
                return -2; //not enough RAM for this process
            }
            //2. set that frame in RAM to allocated
            RAM[free_frame] = 1;
            //3. update page table
            entry ent;
            ent.pid = PID;
            ent.frame_num = free_frame;
            ent.page_num = page;
            page_table.push_back(ent);
        }
    }else{
        //if this is an existing page for the process
        //1. find free frame in RAM
        int free_frame = find_free_frame(PID);
        if(free_frame == -1){
            return -1; //not enough room in hard disc to free a RAM frame
        }
        if(free_frame == -2) {
            return -2; //not enough RAM for this process
        }
        //2. set that frame in RAM to allocated
        RAM[free_frame] = 1;
        //3. get the Hard Disc idx
        int HD_frame = page_table[page_table_idx].frame_num - RAM_SIZE/page_size;
        //4. update page table
        page_table[page_table_idx].frame_num = free_frame;
        //free Hard Disc frame
        HD[HD_frame] = 0;
    }
    //return the frame that just allocated
    return 0;
}
/*
 * This takes a string as input and returns a vector of strings that come from splitting
 * the input on space characters. (will not include space characters in the items)
 */
vector<std::string> str_split(std::string input) {
    vector<std::string> ret;
    std::string str = "";
    int i;
    for (i = 0; i < input.length(); i++) {
        if (input[i] != ' ') str += input[i];
        if (input[i] == ' ' || input[i] == '\n' || i == input.length() - 1) {
            if (str.length() > 0) ret.push_back(str);
            str.clear();
        }
    }
    if(ret.size() < 1) ret.push_back("");
    return ret;
}