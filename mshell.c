#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#include "string.h"
#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"

#define true 1
#define false 0
#define bool int


#define ChildStatusBuffer 1000

#define DEBUG
#include "include/siparse.h"
#include "include/builtins.h"


typedef struct spawnedProcessData{
	pid_t pid;
	int exitNotTerminated;
	int exitTerminationCode;
	int hasRunInBG;
	int stillRuning;
} spawnedProcessData;

volatile int amountSpawnedProcess = 0;
spawnedProcessData BGexitStatus[ChildStatusBuffer];
int findProcessData(pid_t pid);
void addProcessData(struct spawnedProcessData pData);
void removeProcessData(pid_t pid);
void askForChildStatus();
void printChildStatus();

typedef struct redirDetails{
	char *in;
	char *out;
	int inFlag;
	int outFlag;
} redirDetails;
void redirDetailsInit(redirDetails* data);


char linebuf[MAX_LINE_LENGTH + 1];
char buffer[2 * MAX_LINE_LENGTH + 2]; // bufor dla read
char* bufferPtr = buffer;



int findEndOfLine(char* tab, int pos, int size);
int sinkRead(void);

redirDetails getRedirDetails(command _command);

void runLineInBG(line *l);

void runLine(line* l, int isBG);
void runPipeLine(pipeline* p, int isBG);
int runCommand(command _command, int *fd, int hasNext, int isBG); // returns 1 when child was spawned, otherwise 0 eg. number of spawned processes

int countPipelineInLine(line* l);
int countCommandInPipeLine(pipeline* p);
int ifEmptyPipeline(line* l);
int ifEmptyCommand(command* c);

void registerHandlers();
volatile int aborting;
void registerDefaultSignalhandler();

int (*(findFunction)(char*))(char **);

volatile int child_can_start;

int
main(int argc, char *argv[]){
	line * ln;
	command *com;
	int printPrompt = 1;


	char* bufferptr = buffer;
	char* lineBegin;
	char* lineEnd;
	registerHandlers();
	struct stat statbuffer;
	int status = fstat(0, &statbuffer);
	if (status != 0){}
	else{
		if (!S_ISCHR(statbuffer.st_mode))
			printPrompt = 0;
	}

	while (1){
		int bytesRead;
		askForChildStatus();
		if (printPrompt){
			printChildStatus();
			write(1, PROMPT_STR, sizeof(PROMPT_STR));
		}

		int r = sinkRead();
		if (r == 0){
			break;
		} if (r == -2){
			write(1, "\n", 1);
			continue;
		}
		ln = parseline(linebuf);
		if (!ln || ifEmptyPipeline(ln)){
			write(2, SYNTAX_ERROR_STR, sizeof(SYNTAX_ERROR_STR));
			continue;
		}

		runLine(ln, ln->flags == LINBACKGROUND);
	}
}


int runCommand(command _command, int *fd, int hasNext, int isBG){
	child_can_start = 0;
	// command from builtins table
	if(findFunction(_command.argv[0]) != NULL){
		int result = (findFunction(_command.argv[0]))(_command.argv);
		if(result == -1){
			char text[MAX_LINE_LENGTH];
			strcpy(text, "Builtin ");
			strcat(text, _command.argv[0]);
			strcat(text, " error.\n");
			write(1, text, strlen(text));
		}
		return 0;
	} else { 
		/* ******************* */
		/* FORK FORK FORK FORK */
		/* ******************* */
		int childpid = fork(); 
		if (childpid == 0){ // KID
			//while (child_can_start == 0)
			//{
			//}
			// registering default signal handlers

			registerDefaultSignalhandler();
			if (isBG) {
				setsid();
			}
			// switching fd
			if(fd[0] != -1)
			{
				dup2(fd[0], 0); 
				close(fd[0]);
			}
			if(fd[1] != -1 && (hasNext || isBG))
			{
				dup2(fd[1], 1);
				close(fd[1]);
			}
			if (fd[2] != -1)
			{
				close(fd[2]);
			}			
			if (fd[3] != -1)
			{
				close(fd[3]);
			}
			
			// REDIRS handling
			if(_command.redirs != NULL){
				redirDetails details = getRedirDetails(_command);
				if(details.inFlag == 1){
					errno = 0;
					int fd_open;
					fd_open = open(details.in, O_RDONLY);

					if(errno == 0) {
						dup2(fd_open ,0);
						close(fd_open);
					} else {
						char buffer[128];
						buffer[0] = 0;
						strcat(buffer, details.in);
						if(errno == EACCES) {
							strcat(buffer, ": permission denied\n");
						} else if(errno == ENOENT || errno == ENOTDIR) {
							strcat(buffer, ": no such file or directory\n");
						}
						write(2, buffer, strlen(buffer));
						exit(EXEC_FAILURE);
					}
				}
				if(details.outFlag == 2){
					int fd_append = open(details.out, O_WRONLY | O_CREAT | O_APPEND, S_IRWXG);
					dup2(fd_append, 1); 
					close(fd_append);
				}
				if(details.outFlag == 1){
					int fd_out = open(details.out, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXG);
					dup2(fd_out, 1);
					close(fd_out);
				}
			} // end redirs handling
	
			errno = 0;
			execvp(*_command.argv, _command.argv);
			
			/* Program shouldn't go below */
			char buffer[MAX_LINE_LENGTH + 33];
			buffer[0] = 0;
			strcat(buffer, *_command.argv);
			if (errno == ENOENT){
				strcat(buffer, ": no such file or directory\n");
			}
			else if (errno == EACCES){
				strcat(buffer, ": permission denied\n");
			}
			else{
				strcat(buffer, ": exec error\n");
			}
			write(2, buffer, strlen(buffer));
	
			exit(EXEC_FAILURE);
		}
		// parent
		struct spawnedProcessData spawned;
		spawned.pid = childpid;
		spawned.hasRunInBG = isBG;
		spawned.stillRuning = 1;
		addProcessData(spawned);
		//kill(childpid, SIGUSR1);
	}
	return 1;
}

int findEndOfLine(char* tab, int pos, int size){

	int p = pos;
	while (p < size){
		if (tab[p] == '\n')
			return p;
		p++;
	}
	return -1;
}

// returns 0 if EOF, -1 if signal, 1 else, -2 if sigint
int sinkRead(){
	int pos = 0;
	while(1){
		char buff;
		int status;
		errno = 0;
		status = read(0, &buff, 1);
		if(status == -1){
			if (errno == EINTR)	{
				return -2;
			} else if (errno == EAGAIN){
				continue;
			} else {
				return -1;
			}
		}
		if(status == 0){
			linebuf[pos++] = '\n';
			linebuf[pos] = 0;
			return 0;
		}
		if(buff == '\n'){
			linebuf[pos++] = buff;
			linebuf[pos] = 0;
			return 1;
		}
		linebuf[pos++] = buff;
		if(pos >= MAX_LINE_LENGTH)
		{
			write(2, SYNTAX_ERROR_STR, strlen(SYNTAX_ERROR_STR)); 
			linebuf[0] = '\n';
			linebuf[1] = 0;
			while(1){
				errno = 0;
				status = read(0, &buff, 1);
				if(status == -1){
					if(errno == EAGAIN){
						continue;
					} else {
						return -1;
					}
				}
				if(status == 0){
					linebuf[0] = '\n';
					linebuf[1] = 0;
					return 0;
				}
				if(buff == '\n'){
					linebuf[0] = '\n';
					linebuf[1] = 0;
					return 1;
				}
			}
		} // if end buffer overflow
	}
	return -1;
}

int (*(findFunction)(char* name))(char **) {
	if(name == NULL || name[0] == '\0')
		return NULL;
	builtin_pair* pointer = builtins_table;
	while(pointer->name != NULL) {
		if(strcmp(pointer->name, name) == 0) {
			return pointer->fun;
		}
		pointer++;
	}
	return NULL;
}

redirDetails getRedirDetails(command _command){ 
	redirDetails result;
	redirDetailsInit(&result);
	int i = 0;
	while(_command.redirs[i] != NULL){
		if(IS_RIN(_command.redirs[i]->flags)){
			result.inFlag = 1;
			result.in = _command.redirs[i]->filename;
		}
		if(IS_ROUT(_command.redirs[i]->flags)){
			result.outFlag = 1;
			result.out = _command.redirs[i]->filename;
		}
		if(IS_RAPPEND(_command.redirs[i]->flags)){
			result.outFlag = 2;
			result.out = _command.redirs[i]->filename;
		}
		i++;
	}
	return result;
}

void redirDetailsInit(redirDetails* data){
	data->inFlag = 0;
	data->outFlag = 0;
	data->in = NULL;
	data->out = NULL;
}

void runPipeLine(pipeline* p, int isBG){
	int curpipe[2];
	int prevpipe[2]; 
	prevpipe[0] = prevpipe[1] = -1;
	curpipe[0] = curpipe[1] = -1;
	int i;
	int spawnedProccesses = 0;
	for(i = 0; (*p)[i] != NULL && !ifEmptyCommand((*p)[i]); i++) {
		prevpipe[0] = curpipe[0];
		prevpipe[1] = curpipe[1];
		if((*p)[i+1] != NULL || isBG) {
			pipe(curpipe);
		} else {
			curpipe[0] = -1;
			curpipe[1] = -1;
		}
		if(prevpipe[1] != -1){
			close(prevpipe[1]);
			prevpipe[1] = -1;
		}
		int fd[4];
		fd[0] = prevpipe[0];
		fd[1] = curpipe[1];
		fd[2] = prevpipe[1];
		fd[3] = curpipe[0];
		spawnedProccesses += runCommand(*((*p)[i]), fd, (*p)[i+1] != NULL, isBG);
		if(prevpipe[0] != -1) {
			close(prevpipe[0]);
		}
		if(prevpipe[1] != -1) {
			close(prevpipe[1]);
		}
	}
	if (curpipe[0] != -1) {
		close(curpipe[0]);
	}
	if (curpipe[1] != -1) {
		close(curpipe[1]);
	}
	if (isBG)
		return;
	int terminatedSpawnedChilds = 0;
	while (spawnedProccesses>terminatedSpawnedChilds) {
		int status;
		pid_t pid = 0;
		aborting = 0;
		int fault = 0;
		pid = waitpid(-1, &status, 0);
		if (aborting)
		{
			//write(1, "a", 1);
			//if (fault)
			//	write(1, "pos", 3);
			break;
		}

		if(pid == 0 || pid == -1)
		{
			continue;
		}
		int pos = findProcessData(pid);
		if (pos == -1) // bug nie dla tego ze pos sie jakos ustawi
		{
			fault = 1;
			continue;
		}

		struct spawnedProcessData* pData = &(BGexitStatus[pos]);
		if (pData->hasRunInBG == 0)	{
			removeProcessData(pid);
			terminatedSpawnedChilds++;
		}
		else{
			if (WIFEXITED(status)){
				pData->exitNotTerminated = 1;
				pData->exitTerminationCode = WEXITSTATUS(status);
			} else	{
				pData->exitNotTerminated = 0;
				pData->exitTerminationCode = WTERMSIG(status);
			}
			pData->stillRuning = 0;
		}
	}
}

void runLine(line* l, int isBG){
	int i;
	for(i = 0; l->pipelines[i] != NULL; i++){
		runPipeLine(l->pipelines + i, isBG);
	}
}

int countPipelineInLine(line* l){
	int i;
	for(i = 0; (l->pipelines)[i] != NULL; i++){}
	
	return i;
}

int countCommandInPipeLine(pipeline* p){
	int i;
	for(i = 0; (*p)[i] != NULL; i++){}
	
	return i;
}

int ifEmptyCommand(command* c){
	return c->argv[0] == 0;
}

int findProcessData(pid_t pid){
	int i;
	for (i = 0; i < amountSpawnedProcess; i++){
		if (BGexitStatus[i].pid == pid){
			return i;
		}
	}
	return -1;
}
void addProcessData(struct spawnedProcessData pData){
	BGexitStatus[amountSpawnedProcess++] = pData;
}
void removeProcessData(pid_t pid){
	int pos = findProcessData(pid);
	if (pos == -1){
		return;
	}
	if (pos + 1 == amountSpawnedProcess){
		amountSpawnedProcess--;
		return;
	}
	BGexitStatus[pos] = BGexitStatus[--amountSpawnedProcess];
	return;
}
void askForChildStatus(){
	pid_t pid;
	int status;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		int pos = findProcessData(pid);
		if (pos == -1)
			break;
		struct spawnedProcessData* pData = &(BGexitStatus[pos]);
		if (WIFEXITED(status))
		{
			pData->exitNotTerminated = 1;
			pData->exitTerminationCode = WEXITSTATUS(status);
		}
		else
		{
			pData->exitNotTerminated = 0;
			pData->exitTerminationCode = WTERMSIG(status);
		}
		pData->stillRuning = 0;
	}

}
void printChildStatus(){
	int index = amountSpawnedProcess - 1;
	while (index >= 0)
	{
		if (BGexitStatus[index].stillRuning == 0)
		{
			if (BGexitStatus[index].hasRunInBG == 0)
			{
				removeProcessData(BGexitStatus[index].pid);
				index--;
				continue;
			}
			char txtbuffer[100];
			sprintf(txtbuffer, "Background process %i terminated. ", BGexitStatus[index].pid);
			int len = strlen(txtbuffer);
			if (BGexitStatus[index].exitNotTerminated == 1)
			{
				sprintf(txtbuffer + len, "(exited with status %i)\n", BGexitStatus[index].exitTerminationCode);
			}
			else
			{
				sprintf(txtbuffer + len, "(killed by signal %i)\n", BGexitStatus[index].exitTerminationCode);
			}
			write(1, txtbuffer, strlen(txtbuffer));
			removeProcessData(BGexitStatus[index].pid);
		}
		index--;
	}
}



void SIGCHLD_handler(int i){
	//askForChildStatus();
}

void SIGINT_handler(int i){
	aborting = 1;
}


void registerHandlers(){
	struct sigaction s;

	s.sa_handler = SIGINT_handler; 
	s.sa_flags = 0;
	sigemptyset(&s.sa_mask);
	sigaction(SIGINT, &s, NULL);

	s.sa_handler = SIGCHLD_handler;
	s.sa_flags = SA_NOCLDSTOP;
	sigemptyset(&s.sa_mask);
	sigaction(SIGCHLD, &s, NULL);

}
void registerDefaultSignalhandler()
{
	struct sigaction s;
	s.sa_handler = SIG_DFL;
	s.sa_flags = 0;
	sigemptyset(&s.sa_mask);
	sigaction(SIGINT, &s, NULL);

	s.sa_handler = SIG_DFL;
	s.sa_flags = 0;
	sigemptyset(&s.sa_mask);
	sigaction(SIGCHLD, &s, NULL);
}

int ifEmptyPipeline(line* l)
{
	int i;
	for (i = 0; (l->pipelines)[i] != NULL; i++)
	{
		pipeline* pipelineptr = &((l->pipelines)[i]);
		if (countCommandInPipeLine(pipelineptr) < 2)
			continue;
		int j;
		for (j = 0; (*pipelineptr)[j] != NULL; j++)
		{
			command* commandptr = (*pipelineptr)[j];
			if (ifEmptyCommand(commandptr))
			{
				return 1;
			}

		}
	}
	return 0;
}