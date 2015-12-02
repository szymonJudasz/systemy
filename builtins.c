#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>

#include "builtins.h"
#include "utils.h"
extern char **environ;
int echo(char*[]);
int undefined(char *[]);
int exit_function(char * argv[]);
int lcd_function(char * argv[]);
int lkill_function(char * argv[]);
char* findPWD();

builtin_pair builtins_table[]={
	{"exit",	&exit_function},
	{"lcd",		&lcd_function},
	{"lkill",	&lkill_function},
	{"lecho",	&echo},
	{"lls",		&undefined},
	{NULL,NULL} 
};

str_int_pair signal_mapping[]=
{
	{"SIGQUIT", SIGQUIT},
	{"SIGWINCH", SIGWINCH},
	//{"SIGRTMAX", SIGRTMAX},
	//{"SIGRTMIN", SIGRTMIN},
	{"SIGTERM", SIGTERM},
	{"SIGTRAP", SIGTRAP},
	{"SIGTSTP", SIGTSTP},
	{"SIGTTIN", SIGTTIN},
	{"SIGTTOU", SIGTTOU},
	{"SIGUNUSED", SIGUNUSED},
	{"SIGURG", SIGURG},
	{"SIGUSR1", SIGUSR1},
	{"SIGUSR2", SIGUSR2},
	{"SIGILL", SIGILL},
	{"SIGINT", SIGINT},
	{"SIGIO", SIGIO},
	{"SIGIOT", SIGIOT},	
	{"SIGPIPE", SIGPIPE},
	{"SIGPOLL", SIGPOLL},
	{"SIGPROF", SIGPROF},
	{"SIGPWR", SIGPWR},	
	{"SIGABRT", SIGABRT},
	{"SIGALRM", SIGALRM},
	{"SIGSEGV", SIGSEGV},
	{"SIGSTKFLT", SIGSTKFLT},	
	{"SIGSTKSZ", SIGSTKSZ},
	{"SIGSTOP", SIGSTOP},
	{"SIGSYS", SIGSYS},
	{"SIGFPE", SIGFPE},	
	{"SIGHUP", SIGHUP},
	{"SIGKILL", SIGKILL},
	{"SIGXCPU", SIGXCPU},
	{"SIGXFSZ", SIGXFSZ},	
	{"SIGCHLD", SIGCHLD},
	{"SIGCLD", SIGCLD},
	{"SIGCONT", SIGCONT},
	{"SIGVTALRM", SIGVTALRM},	
	{"SIGBUS", SIGBUS},
	{NULL, 0}
};

int echo( char * argv[]){
	int i =1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);
	printf("\n");
	fflush(stdout);
	return 0;
}

int exit_function(char * argv[]){
	int canexit = 1;
	if(argv[1] != 0)
		canexit = 0;
	if(canexit)
		exit(0);
	return -1;


	// wrong, exit doesn't take parameters
	/*int canexit = 1;

	int codeofexit;
	if(cstringtoint(argv[1], &codeofexit) == -1)
		canexit 0;
	if(canexit)
		return exit(codeofexit);
	return -1;*/
}

int lcd_function(char * argv[]){
	if(argv[1] == 0){
		char buffer[512];
		chdir(findPWD() + strlen("HOME="));
	} else
	{
		chdir(argv[1]);
	}
	return 0;
}

int find_signal_int_code(char* signal_name, int* signal_number)
{
	//printf("\n\nlooking for: %s\n\n", signal_name);
	//fflush(stdout);
	int i;
	for(i = 0; signal_mapping[i].name != 0; i++)
	{
		//printf("lookup: %s\n", signal_mapping[i].name);
		//fflush(stdout);
		if(strcmp(signal_name, signal_mapping[i].name) == 0)
		{
			*signal_number = signal_mapping[i].sig;
			return 0;
		}
	}
	return -1;
}

int lkill_error(){
	fprintf(stderr, "Command kill error.\n" );
	return BUILTIN_ERROR;
}
int lkill_function(char * argv[]){
	// get rid of invalid call
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
	
	// signal determining
	// argv[i] points to pid_t which is the last argument
	i--;
	int sig_number = SIGTERM;
	if(i == 1){ // there is custom signal
		converting_result = sscanf(argv[i], "-%d%s", &sig_number, &(to_big_buffer[0]);
		if(converting_result != 1){
			char signal_text[64];
			//sscanf(argv[i]+1, "%s", signal_text);
			//printf("signal text: %s", signal_text);
			fflush(stdout);
			if(find_signal_int_code(&(signal_text[0]), &sig_number) != 0)
			{
				return lkill_error();
			}
		}
	}
	printf("pid: %d\nsig: %d", pid, sig_number);
	fflush(stdout);
	// kill(pid, sig_number);
	
}

char* findPWD(){
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



int undefined(char * argv[]){
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}
