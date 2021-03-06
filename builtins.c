#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>

#include "builtins.h"
#include "utils.h"
#include "include/builtins.h" // may be deleted
extern char **environ;
int echo(char*[]);
int exit_function(char * argv[]);
int lcd_function(char * argv[]);
int lkill_function(char * argv[]);
int lls_function(char * argv[]);


char* findHOME(); // return pointer to text like 'HOME=blah blah blah'
char* findPWD(); // same as findHome
int adjustPWD(char* path);


builtin_pair builtins_table[]={
	{"exit",	&exit_function},
	{"lcd",		&lcd_function},
	{"lkill",	&lkill_function},
	{"lecho",	&echo},
	{"lls",		&lls_function},
	{NULL,NULL} 
};

str_int_pair signal_mapping[]={
	{"SIGQUIT", SIGQUIT},
	{"SIGWINCH", SIGWINCH},
	{"SIGTERM", SIGTERM},
	{"SIGTRAP", SIGTRAP},
	{"SIGTSTP", SIGTSTP},
	{"SIGTTIN", SIGTTIN},
	{"SIGTTOU", SIGTTOU},
	{"SIGUSR1", SIGUSR1},
	{"SIGUSR2", SIGUSR2},
	{"SIGILL", SIGILL},
	{"SIGINT", SIGINT},
	{"SIGPIPE", SIGPIPE},
	{"SIGPROF", SIGPROF},
	{"SIGABRT", SIGABRT},
	{"SIGALRM", SIGALRM},
	{"SIGSEGV", SIGSEGV},
	{"SIGSTKSZ", SIGSTKSZ},
	{"SIGSTOP", SIGSTOP},
	{"SIGFPE", SIGFPE},	
	{"SIGHUP", SIGHUP},
	{"SIGKILL", SIGKILL},
	{"SIGCHLD", SIGCHLD},
	{"SIGCONT", SIGCONT},
	{"SIGVTALRM", SIGVTALRM},	
	{"SIGBUS", SIGBUS},
	//{"SIGSYS", SIGSYS},
	//{"SIGSTKFLT", SIGSTKFLT},	
	//{"SIGCLD", SIGCLD},
	//{"SIGXCPU", SIGXCPU},
	//{"SIGXFSZ", SIGXFSZ},	
	//{"SIGPWR", SIGPWR},	
	//{"SIGRTMAX", SIGRTMAX},
	//{"SIGRTMIN", SIGRTMIN},
	//{"SIGUNUSED", SIGUNUSED},
	//{"SIGURG", SIGURG},
	//{"SIGPOLL", SIGPOLL},
	//{"SIGIO", SIGIO},
	//{"SIGIOT", SIGIOT},	
	{NULL, 0}
};

int echo( char * argv[]){
	int i =1;
	if (argv[i]) write(1, argv[i], strlen(argv[i++]));
	while  (argv[i])
		write(1, argv[i], strlen(argv[i++]));
	write(1, "\n", 1);
	return 0;
}

int exit_function(char * argv[]){
	int canexit = 1;
	if(argv[1] != 0)
		canexit = 0;
	if(canexit)
		exit(0);
	return -1;
}

int lcd_function(char * argv[]){
	char _b[32] = "Builtin lcd error.\n";
	if(argv[2] != 0){
		write(2, _b, strlen(_b));
	}
	if(argv[1] == 0){
		char buffer[512];
		chdir(findHOME() + strlen("HOME="));
	} else	{
		int status;
		status = chdir(argv[1]);
		if(status == -1) {
			write(2, _b, strlen(_b));
		}
	}
	return 0;
}

int find_signal_int_code(char* signal_name, int* signal_number){
	int i;
	for(i = 0; signal_mapping[i].name != 0; i++){
		if(strcmp(signal_name, signal_mapping[i].name) == 0){
			*signal_number = signal_mapping[i].sig;
			return 0;
		}
	}
	return -1;
}

int lkill_error(){
	char _b[32] = "Builtin lkill error.\n" ;
	write(2, _b, strlen(_b));
	return BUILTIN_ERROR;
}
int lkill_function(char * argv[]){
	if(argv[1] == 0){
		return lkill_error();
	}
	
	// pid obtaining
	pid_t pid;
	char* pid_text;
	int i;
	for(i = 1; argv[i] != 0; i++){}
	i--;
	if(i > 2)
		return lkill_error();
	pid_text = argv[i];
	
	int converting_result;
	char to_big_buffer[256];
	converting_result = sscanf(pid_text, "%d%s", &pid, &(to_big_buffer[0]));
	if(converting_result != 1){
		return lkill_error();
	}
	
	i--;
	int sig_number = SIGTERM;
	if(i == 1){ // there is custom signal
		converting_result = sscanf(argv[i], "-%d%s", &sig_number, &(to_big_buffer[0]));
		if(converting_result != 1){
			char signal_text[64];
			if(find_signal_int_code(&(signal_text[0]), &sig_number) != 0){
				return lkill_error();
			}
		}
	}
	kill(pid, sig_number);
	return 0;
}

char* findHOME(){
	int i = 0;
	while(environ[i] != 0){
		char* home_env = "HOME";
		if(strncmp(environ[i], home_env, strlen(home_env)) == 0){
			return environ[i];
		}
		i++;
	}
	return NULL;
}

char* findPWD(){
	int i = 0;
	while(environ[i] != 0){
		char* home_env = "PWD";
		if(strncmp(environ[i], home_env, strlen(home_env)) == 0){
			return environ[i];
		}
		i++;
	}
	return NULL;
}

int lls_error(){
	char errortext[64] = "Builtins lls failed.\n";
	write(2, errortext, strlen(errortext));
	return -1;
}

int lls_function(char * argv[]){
	if (argv[1] != 0){
		return lls_error();
	}
	
	DIR* dir;
	dir = opendir(".");
	struct dirent* entry_ptr;
	while((entry_ptr = readdir(dir)) != 0){
		if(*(entry_ptr->d_name) == '.')
			continue;
		write(1, entry_ptr->d_name, strlen(entry_ptr->d_name));
		char newlinechar = '\n';
		write(1, &newlinechar, 1);
	}
	closedir(dir);
	return 0;
}

