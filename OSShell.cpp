#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <vector>
#include <fstream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define RST "\x1B[0m"
#define BLU "\x1B[34m"
#define LGRN "\x1B[92m"


struct mystring{
	char ent[128];
};

using namespace std;
bool fileExist(const char *fileName);
vector<string> splitUserInput(string str, char* path);
int lsr(DIR* dir, char* path, int indent);
bool isDir(char* path);
bool isExe(char* path);
long long get_file_size(char* path);
char* get_new_path(char* path, char* next_file);
void print_entry(char* entry, int type, long long size, int indent);
vector<mystring> str_split(char* array);

int main(int argc, char **argsv)
{

	cout << "Welcome to OSShell! Please enter your commands. ('exit' to quit)" << endl;
	
	char 	*uInput = new char[128];
	char 	*path;
	char 	*parts;
	char	*backup;
	char 	*backupParts;
	char	*inparts;
	int 	 pid;
	int 	*status;
	const char *programName;
	const char **argv;
	vector<string> vect;
	bool worked;
	char lsr_str[3];
	lsr_str[0] = 'l';
	lsr_str[1] = 's';
	lsr_str[2] = 'r';

	while(true)
	{
		cout << "osshell> ";
		worked = false;
		fgets (uInput, 128, stdin);
		string uInp(uInput);
		char myInput[128];
		strcpy(myInput, uInput);
		vector<mystring> inputs = str_split(myInput);

		if (uInp == "exit\n")
			{break;}
		else if (uInp == "\n"){}

		else if (0 == strcmp(inputs[0].ent,lsr_str))//
//		else if (uInp == "lsr\n")//
		{
//			cout << lsr_str << '\n';
//			cout << inputs[0].ent << '\n';
			DIR* root;
			char cwd[256];
			getcwd(cwd, sizeof(cwd));
			if(inputs.size() == 3){
				perror("too many args");
			}
			if(inputs.size() == 1){
				root = opendir(cwd);
			if(root == NULL){
				perror("cannot open the current working directory");
			} else {
				lsr(root, cwd, 0);
			}
			}else if(inputs.size() == 2){
				root = opendir(inputs[1].ent);
			if(root == NULL){
				perror("cannot open directory");
			} else {
				lsr(root, inputs[1].ent, 0);
			}
		}
		}
		else
		{
			backup = getenv("PATH");
			char path[strlen(backup)+1];
			strcpy(path, backup);
			parts = path;
			parts = strtok(path, ":");
			
			pid = 1;

			while(parts != NULL)
			{
				vect = splitUserInput(uInp, parts); //this stays here
				//-------conversion---
				programName = vect[0].c_str();
				argv = new const char* [vect.size()+1];
				argv[0] = programName;
				for(int j = 0; j < vect.size() ; j++)
				{
					argv[j+1] = vect[j+1].c_str();
				}
				argv[vect.size()] = NULL;
				//--------------------
				parts = strtok (NULL, ":"); //iterates
				if(fileExist(programName))//if the file doesnt exist then dont try to run it
				{
					worked = true; //since it ran update the worked boolean
					if (pid > 0)
					{
					
						pid = fork();
						if (pid != 0)
							{wait(&status);} 
					}

					if(pid == 0)
					{
						int test;
						test =0;
						test = execv(programName, (char**)argv);
						return 0; //kill the child process
					}
				}
			}
			if(!worked) //if it didnt work
			{
				printf("%s:Error running command\n" , uInp.erase(uInp.size()-1).c_str()); //print error message without the newline at the end of uInp
			} 
		}
	}	
	return 0 ;
}

bool fileExist(const char *fileName) //tests if the file exists
{
    std::ifstream infile(fileName);
    return infile.good();
}

vector<string> splitUserInput(string str, char* path)
{
	string theCopy = str;
	istringstream stream(theCopy);
	std::vector<string> vect;
	while(stream)
	{
		string substr;
		stream >> substr;
		vect.push_back(substr);
	}
	stream.clear();
	vect.pop_back();
	string temp = vect[0];
	string way(path);
	vect[0] = way + "/" + temp;
	return vect;
}
int lsr(DIR* dir, char* path, int indent){

	struct dirent* direntp;
	direntp = readdir(dir);
	if(direntp == NULL){
		closedir(dir);
		return 0;
	}
	if(direntp->d_name[0] == '.'){
		lsr(dir, path, indent);
	} else {
		char* new_path = get_new_path(path, direntp->d_name);
		DIR* new_dir;
		if(isDir(new_path)){
			print_entry(direntp->d_name, 2, 0, indent);
			new_dir = opendir(new_path);
			indent++;
			lsr(new_dir, new_path, indent);
			indent--;
		}else if(isExe(new_path)){
			long long size = get_file_size(new_path);
			print_entry(direntp->d_name, 1, size, indent);
		}else{
			long long size = get_file_size(new_path);
			print_entry(direntp->d_name, 0, size, indent);
		}
		delete[] new_path;
		lsr(dir, path, indent);
	}
	return 0;

}


//check if a file is a directory
bool isDir(char* path){

	struct stat filestats;
	stat(path, &filestats);
	return S_ISDIR(filestats.st_mode);
}

//check if a file is an executable
bool isExe(char* path){

	struct stat filestats;
	stat(path, &filestats);
	return (filestats.st_mode & S_IEXEC);
}

long long get_file_size(char* path){
	struct stat filestats;
	stat(path, &filestats);
	return filestats.st_size;
}

//create the path that includes the new possible directory
char* get_new_path(char* path, char* next_file){

	char* new_path = new char[256];
	strcpy(new_path, path);
	if(new_path[strlen(new_path)-1] == '/'){
		strcat(new_path, next_file);
	} else {
		strcat(new_path, "/");
		strcat(new_path, next_file);
	}
	return new_path;
}

//prints the enrty to the screen
void print_entry(char* entry, int type, long long size, int indent){
		
	for(int i=0; i<indent; i++){
		std::cout << "    ";
	}
	if(type == 2){
		printf(BLU "%s (directory)\n" RST, entry);
	}else if(type == 1){
		printf(LGRN "%s (%lld bytes)\n" RST, entry, size);
	}else{
		std::cout << entry << " (" << size << " bytes)" << "\n";
	}
}


vector<mystring> str_split(char* array){

	
	vector<mystring> ret;
	int i = 0;
	int j = 0;
	mystring str;
	while(array[i] != '\0'){
		
		str.ent[j] = array[i];
		if(array[i] == ' ' || array[i] == '\n'){
			str.ent[j] = '\0';
			ret.push_back(str);
			j = -1;
		}
		j++;
		i++;
	}
	//str.ent[j] = '\0';
	//ret.push_back(str);

	return ret;
}
