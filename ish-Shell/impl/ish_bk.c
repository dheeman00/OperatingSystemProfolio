#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include "command.h"
#include "parser.tab.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <termios.h>

#define QUIT 1
#define SUCCESS 2
#define FAIL 3

#define WORD_LIMIT 1024
#define WORD_SIZE 1024
#define JOB_LIMIT 1024

#define PROCESS_PER_JOB 100
#define RUNNING 0
#define STOPPED 1
#define COMPLETED 2

char host_name[HOST_NAME_MAX];
char home_directory[WORD_SIZE];
char curr_directory[WORD_SIZE];
int num_env = 0; // number of environment variables
int num_alias = 0; //  number of current aliases

// contains all aliases
struct Alias
{
    char from[WORD_SIZE];
    char to[WORD_SIZE];
} arr_aliases[WORD_LIMIT];
//int num_alias = 0; // total number of current aliases

// contains environment variables
struct UnixEnv
{
    char key[WORD_SIZE];
    char value[WORD_SIZE];
} UnixEnv[WORD_LIMIT];
//int num_env = 0;     // total number of environment variables

//keeps track of processes
struct Process
{
	pid_t pid;
	int status;
	int stopped;
	int completed;
	struct Process *next;
};

// keeps track of jobs
struct Job
{
    int job_num;
    char *job_name;
	struct Process *process;
	pid_t process_gid; /* process group ID */
	int notified;
	int run_status;
	int status;
	int foreground;
	struct termios tmodes; /* saved terminal modes */
	struct Job *next;
};

struct Job *job_head = NULL;
int n_job = 0;

/* Keep track of attributes of the terminal.*/
pid_t terminal_pgid;
struct termios terminal_tmodes;
int terminal;

/*
this function copies UnixEnv variables to envs[] according to the
required format of execve() function.
*/
// void getEnvironmentVariables(char *envs[])
// {
//     for(int i = 0; i < num_env; i++)
//     {
//         envs[i] = (char*) malloc(WORD_SIZE*sizeof(char));
//         strcpy(envs[i], UnixEnv[i].key);
//         strcat(envs[i],"=");
//         char val[WORD_SIZE];
//         strcpy(val,UnixEnv[i].value);
//         strcat(envs[i],val);
//     }
//     envs[num_env] = NULL;
//     return;
// }

/*This functions replicate the functionality of execve()*/
// LOOKUP: getEnvironmentVariables
void EnvGetVars(char *env_vars[])
{
    for (int i = 0; i < num_env; i++)
    {
        env_vars[i] = (char *)malloc(WORD_SIZE * sizeof(char));
        strcpy(env_vars[i], UnixEnv[i].key);
        strcat(env_vars[i], "=");
        char val[WORD_SIZE];
        strcpy(val, UnixEnv[i].value);
        strcat(env_vars[i], val);
    }
    env_vars[num_env] = NULL;
    return;
}

// void issuePrompt(FILE *pf)
// {
//     fprintf(pf, "%s%% ", host_name);
//     fflush(pf);
// }

/*Prompt warning for any unexecutable commands*/
// LOOKUP: issuePrompt
void WarningPrompt(FILE *pf)
{
    fprintf(pf, "%s", host_name);
    fflush(pf);
}

/*
fixing direcotory path like /dir1/dir2/../dir3 to /dir1/dir3
*/
// void fixDirectoryPath()
// {
//     char tokens[WORD_SIZE][WORD_SIZE];
//     char *token;
//     token = strtok(curr_directory, "/");
//     int cnt = 0;
//     while(token != NULL)
//     {
//         if(strcmp(token,"..") == 0)
//         {
//             cnt = cnt-1;
//         }
//         else
//         {
//             strcpy(tokens[cnt++],token);
//         }
//         token = strtok(NULL, "/");
//     }
//     memset(curr_directory,0,sizeof(curr_directory));
//     curr_directory[0] = '/';
//     for(int i = 0; i < cnt; i++)
//     {
//         strcat(curr_directory,tokens[i]);
//         strcat(curr_directory,"/");
//     }
// }

/*fixing directory path eg /path1/path2/../path3 to /path1/path3*/
// LOOKUP: fixDirectoryPath
void FixDirPath()
{
    char tokens[WORD_SIZE][WORD_SIZE];
    char *token;
    token = strtok(curr_directory, "/");
    int counter = 0;
    while (token != NULL)
    {
        if (strcmp(token, "..") == 0)
        {
            counter = counter - 1;
        }
        else
        {
            strcpy(tokens[counter++], token);
        }
        token = strtok(NULL, "/");
    }
    memset(curr_directory, 0, sizeof(curr_directory));
    curr_directory[0] = '/';
    for (int i = 0; i < counter; i++)
    {
        strcat(curr_directory, tokens[i]);
        strcat(curr_directory, "/");
    }
}

// free allocated memory for a job
void freeJob(struct Job *job)
{
	struct Process *next;
	for(struct Process *p = job->process; p != NULL; p = next)
    {
		next = p->next;
		free(p);
	}
	free(job->job_name);
	free(job);
}

// print job info. The base layout of this function is taken from the code provided by Professor Bridges.
// Thanks to him.
void printJobInfo(struct Job *job)
{
    if(job->run_status == RUNNING) fprintf(stdout, "[%d] Running\n", job->job_num);
    else if(job->run_status == STOPPED) fprintf(stdout, "[%d] %s %s\n", job->job_num, job->job_name,strsignal(WSTOPSIG(job->status)));
	else
    {
        if(WIFSIGNALED(job->status))
        {
            fprintf(stdout, "[%d] %s %s", job->job_num, job->job_name, strsignal(WTERMSIG(job->status)));
            if(WCOREDUMP(job->status)) fprintf(stdout, " (core dumped)\n");
            else fprintf(stdout, "\n");
        }
        else fprintf(stdout, "[%d] %s Exit %d\n", job->job_num, job->job_name, WEXITSTATUS(job->status));
    }
    fflush(stdout);
}

// // execution of cd command
// void _cd(command *pc)
// {
//     if(pc->pa->nArgs == 1)
//     {
//         memset(curr_directory,0,sizeof(curr_directory));
//         strcpy(curr_directory,home_directory);
//         chdir(curr_directory);
//         return;
//     }
//     if(pc->pa->nArgs > 2)
//     {
//         fprintf(stderr,"cd : Too many arguments\n");
//         fflush(stderr);
//         return;
//     }
//     char path[WORD_SIZE];
//     memset(path,0,sizeof(path));
//     if(pc->pa->azArgs[1][0] == '/')
//     {
//         strcpy(path,pc->pa->azArgs[1]);
//     }
//     else
//     {
//         char path1[WORD_SIZE];
//         memset(path1,0,sizeof(path1));
//         strcpy(path1,pc->pa->azArgs[1]);
//         strcpy(path, curr_directory);
//         strcat(path, path1);
//     }
//     int len = strlen(path);
//     if(path[len-1] != '/') strcat(path,"/");
//     if(chdir(path) == 0)
//     {
//         strcpy(curr_directory, path);
//         FixDirPath();
//     }
//     else
//     {
//         fprintf(stderr,"cd : %s\n", strerror(errno));
//         fflush(stderr);
//     }
//     return;
// }

/*function perform cd command*/
// LOOKUP: _cd
void func_cd(command *pc)
{
    if (pc->pa->nArgs == 1)
    {
        memset(curr_directory, 0, sizeof(curr_directory));
        strcpy(curr_directory, home_directory);
        chdir(curr_directory);
        return;
    }
    // when the required arguments are more
    if (pc->pa->nArgs > 2)
    {
        fprintf(stderr, "cd : Too many arguments\n");
        fflush(stderr);
        return;
    }

    char file_path[WORD_SIZE];
    memset(file_path, 0, sizeof(file_path));
    // execting the command for "/"
    if (pc->pa->azArgs[1][0] == '/')
    {
        strcpy(file_path, pc->pa->azArgs[1]);
    }
    else
    {
        char update_path[WORD_SIZE];
        memset(update_path, 0, sizeof(update_path));
        strcpy(update_path, pc->pa->azArgs[1]);

        int str_length = strlen(update_path);
        if (update_path[str_length - 1] != '/')
        {
            update_path[str_length] = '/';
        }
        strcpy(file_path, curr_directory);
        strcat(file_path, update_path);
    }
    if (chdir(file_path) == 0)
    {
        strcpy(curr_directory, file_path);
        FixDirPath();
    }
    else
    {
        fprintf(stderr, "cd %s : %s\n", pc->zCmd, strerror(errno));
        fflush(stderr);
    }
    return;
}

// execution of alias command
// void _alias(command *pc)
// {
//     if(pc->pa->nArgs == 1)
//     {
//         if(num_alias == 0)
//         {
//             return;
//         }
//         for(int i = 0; i < num_alias; i++)
//         {
//             fprintf(stdout, "alias %s = %s\n", arr_aliases[i].from, arr_aliases[i].to);
//         }
//         fflush(stdout);
//         return;
//     }
//     if(pc->pa->nArgs == 2)
//     {
//         for(int i = 0; i < num_alias; i++)
//         {
//             if(strcmp(arr_aliases[i].from, pc->pa->azArgs[1]) == 0)
//             {
//                 fprintf(stdout, "alias %s = %s\n", arr_aliases[i].from, arr_aliases[i].to);
//                 fflush(stderr);
//                 return;
//             }
//         }
//         fflush(stderr);
//         return;
//     }
//     if(pc->pa->nArgs == 3)
//     {
//         for(int i = 0; i < num_alias; i++)
//         {
//             if(strcmp(arr_aliases[i].from, pc->pa->azArgs[1]) == 0)
//             {
//                 memset(arr_aliases[i].to,0,sizeof(arr_aliases[i].to));
//                 strcpy(arr_aliases[i].to,pc->pa->azArgs[2]);
//                 return;
//             }
//         }
//         if(num_alias == WORD_LIMIT)
//         {
//             fprintf(stderr,"alias : Maximum alias limit reached\n");
//             fflush(stderr);
//             return;
//         }
//         memset(arr_aliases[num_alias].from,0,sizeof(arr_aliases[num_alias].from));
//         memset(arr_aliases[num_alias].to,0,sizeof(arr_aliases[num_alias].to));
//         strcpy(arr_aliases[num_alias].from,pc->pa->azArgs[1]);
//         strcpy(arr_aliases[num_alias].to,pc->pa->azArgs[2]);
//         num_alias++;
//         return;
//     }
//     fprintf(stderr,"alias : Too many arguments\n");
//     fflush(stderr);
//     return;
// }

/*function performing alias */
// LOOKUP: _alias
void func_alias(command *pc)
{
    if (pc->pa->nArgs == 1)
    {
        if (num_alias == 0)
        {
            return;
        }
        for (int i = 0; i < num_alias; i++)
        {
            fprintf(stdout, "aliasing %s = %s\n", arr_aliases[i].from, arr_aliases[i].to);
        }
        fflush(stdout);
        return;
    }
    if (pc->pa->nArgs == 2)
    {
        for (int i = 0; i < num_alias; i++)
        {
            if (strcmp(arr_aliases[i].from, pc->pa->azArgs[1]) == 0)
            {
                fprintf(stdout, "aliasing %s = %s\n", arr_aliases[i].from, arr_aliases[i].to);
                fflush(stderr);
                return;
            }
        }
        fflush(stderr);
        return;
    }
    if (pc->pa->nArgs == 3)
    {
        for (int i = 0; i < num_alias; i++)
        {
            if (strcmp(arr_aliases[i].from, pc->pa->azArgs[1]) == 0)
            {
                memset(arr_aliases[i].to, 0, sizeof(arr_aliases[i].to));
                strcpy(arr_aliases[i].to, pc->pa->azArgs[2]);
                return;
            }
        }
        if (num_alias == WORD_LIMIT)
        {
            fprintf(stderr, "aliasing : Maximum limit reached\n");
            fflush(stderr);
            return;
        }
        memset(arr_aliases[num_alias].from, 0, sizeof(arr_aliases[num_alias].from));
        memset(arr_aliases[num_alias].to, 0, sizeof(arr_aliases[num_alias].to));
        strcpy(arr_aliases[num_alias].from, pc->pa->azArgs[1]);
        strcpy(arr_aliases[num_alias].to, pc->pa->azArgs[2]);
        num_alias++;
        return;
    }
    else
    {
        fprintf(stderr, "aliasing : Too many arguments\n");
        fflush(stderr);
        return;
    }
}

// // execution of unalias command
// void _unalias(command *pc)
// {
//     if(pc->pa->nArgs == 1)
//     {
//         fprintf(stderr, "unalias: Too few arguments.\n");
//         fflush(stderr);
//         return;
//     }
//     if(pc->pa->nArgs == 2)
//     {
//         for(int i = 0; i < num_alias; i++)
//         {
//             if(strcmp(arr_aliases[i].from, pc->pa->azArgs[1]) == 0)
//             {
//                 for(int j = i+1; j < num_alias; j++)
//                 {
//                     strcpy(arr_aliases[j-1].from,arr_aliases[j].from);
//                     strcpy(arr_aliases[j-1].to,arr_aliases[j].to);
//                 }
//                 num_alias--;
//                 return;
//             }
//         }
//         return;
//     }
//     fprintf(stderr,"unalias : Too many arguments\n");
//     fflush(stderr);
//     return;
// }

/*function performing unalias */
// LOOKUP: _unalias
void func_unalias(command *pc)
{
    if (pc->pa->nArgs == 1)
    {
        fprintf(stderr, "unaliasing: Too few arguments.\n");
        fflush(stderr);
        return;
    }
    if (pc->pa->nArgs == 2)
    {
        for (int i = 0; i < num_alias; i++)
        {
            if (strcmp(arr_aliases[i].from, pc->pa->azArgs[1]) == 0)
            {
                for (int j = i + 1; j < num_alias; j++)
                {
                    strcpy(arr_aliases[j - 1].from, arr_aliases[j].from);
                    strcpy(arr_aliases[j - 1].to, arr_aliases[j].to);
                }
                num_alias--;
                return;
            }
        }
        return;
    }
    else
    {
        fprintf(stderr, "unaliasing : Too many arguments\n");
        fflush(stderr);
        return;
    }
}

/*performs aliasing on the given command*/
// LOOKUP: aliasing
void func_aliasing(command *pc)
{
    for (int i = 0; i < num_alias; i++)
    {
        if (strcmp(arr_aliases[i].from, pc->zCmd) == 0)
        {
            free(pc->zCmd);
            pc->zCmd = (char *)malloc(WORD_SIZE * sizeof(char));
            strcpy(pc->zCmd, arr_aliases[i].to);
            break;
        }
    }
}

// execution of setenv command
// void _setenv(command *pc)
// {
//     if(pc->pa->nArgs == 1)
//     {
//         if(num_env == 0)
//         {
//             return;
//         }
//         for(int i = 0; i < num_env; i++)
//         {
//             fprintf(stdout, "%s = %s\n", UnixEnv[i].key, UnixEnv[i].value);
//         }
//         fflush(stdout);
//         return;
//     }
//     if(pc->pa->nArgs == 2)
//     {
//         for(int i = 0; i < num_env; i++)
//         {
//             if(strcmp(UnixEnv[i].key, pc->pa->azArgs[1]) == 0)
//             {
//                 memset(UnixEnv[i].value,0,sizeof(UnixEnv[i].value));
//                 return;
//             }
//         }
//         strcpy(UnixEnv[num_env].key,pc->pa->azArgs[1]);
//         memset(UnixEnv[num_env].value,0,sizeof(UnixEnv[num_env].value));
//         num_env++;
//         return;
//     }
//     if(pc->pa->nArgs == 3)
//     {
//         for(int i = 0; i < num_env; i++)
//         {
//             if(strcmp(UnixEnv[i].key, pc->pa->azArgs[1]) == 0)
//             {
//                 strcpy(UnixEnv[i].value,pc->pa->azArgs[2]);
//                 return;
//             }
//         }
//         if(num_env == WORD_LIMIT)
//         {
//             fprintf(stderr,"setenv : Maximum environment limit reached\n");
//             fflush(stderr);
//             return;
//         }
//         strcpy(UnixEnv[num_env].key,pc->pa->azArgs[1]);
//         strcpy(UnixEnv[num_env].value,pc->pa->azArgs[2]);
//         num_env++;
//         return;
//     }
//     fprintf(stderr,"setenv : Too many arguments\n");
//     fflush(stderr);
//     return;
// }

/* function perform setenv command*/
// LOOKUP: _setenv
void func_setenv(command *pc)
{
    if (pc->pa->nArgs == 1)
    {
        if (num_env == 0)
        {
            return;
        }
        for (int i = 0; i < num_env; i++)
        {
            fprintf(stdout, "%s = %s\n", UnixEnv[i].key, UnixEnv[i].value);
        }
        fflush(stdout);
        return;
    }
    if (pc->pa->nArgs == 2)
    {
        for (int i = 0; i < num_env; i++)
        {
            if (strcmp(UnixEnv[i].key, pc->pa->azArgs[1]) == 0)
            {
                memset(UnixEnv[i].value, 0, sizeof(UnixEnv[i].value));
                return;
            }
        }
        strcpy(UnixEnv[num_env].key, pc->pa->azArgs[1]);
        memset(UnixEnv[num_env].value, 0, sizeof(UnixEnv[num_env].value));
        num_env++;
        return;
    }
    if (pc->pa->nArgs == 3)
    {
        for (int i = 0; i < num_env; i++)
        {
            if (strcmp(UnixEnv[i].key, pc->pa->azArgs[1]) == 0)
            {
                strcpy(UnixEnv[i].value, pc->pa->azArgs[2]);
                return;
            }
        }
        if (num_env == WORD_LIMIT)
        {
            fprintf(stderr, "setenv : Maximum limit reached\n");
            fflush(stderr);
            return;
        }
        strcpy(UnixEnv[num_env].key, pc->pa->azArgs[1]);
        strcpy(UnixEnv[num_env].value, pc->pa->azArgs[2]);
        num_env++;
        return;
    }
    fprintf(stderr, "setenv : Too many arguments\n");
    fflush(stderr);
    return;
}

// execution of unsetenv command
// void _unsetenv(command *pc)
// {
//     if(pc->pa->nArgs == 1)
//     {
//         fprintf(stderr, "unsetenv: Too few arguments\n");
//         fflush(stderr);
//         return;
//     }
//     if(pc->pa->nArgs == 2)
//     {
//         for(int i = 0; i < num_env; i++)
//         {
//             if(strcmp(UnixEnv[i].key, pc->pa->azArgs[1]) == 0)
//             {
//                 for(int j = i+1; j < num_env; j++)
//                 {
//                     strcpy(UnixEnv[j-1].key,UnixEnv[j].key);
//                     strcpy(UnixEnv[j-1].value,UnixEnv[j].value);
//                 }
//                 num_env--;
//                 return;
//             }
//         }
//         return;
//     }
//     fprintf(stderr,"unsetenv : Too many arguments\n");
//     fflush(stderr);
//     return;
// }

/* function perform unsetenv command*/
// LOOKUP: _unsetenv
void func_unsetenv(command *pc)
{
    if (pc->pa->nArgs == 1)
    {
        fprintf(stderr, "unsetenv: Too few arguments\n");
        fflush(stderr);
        return;
    }
    if (pc->pa->nArgs == 2)
    {
        for (int i = 0; i < num_env; i++)
        {
            if (strcmp(UnixEnv[i].key, pc->pa->azArgs[1]) == 0)
            {
                for (int j = i + 1; j < num_env; j++)
                {
                    strcpy(UnixEnv[j - 1].key, UnixEnv[j].key);
                    strcpy(UnixEnv[j - 1].value, UnixEnv[j].value);
                }
                num_env--;
                return;
            }
        }
        return;
    }
    fprintf(stderr, "unsetenv : Too many arguments\n");
    fflush(stderr);
    return;
}

// update the process status returned by waitpid
int updateProcessStatus(pid_t pid, int status)
{
	struct Job *job;
	struct Process *p;
	if(pid > 0)
    {
		for(job = job_head; job != NULL; job = job->next)
		{
			for(p = job->process; p != NULL; p = p->next)
			{
				if(p->pid == pid)
				{
					p->status = status;
					if(WIFSTOPPED(status)) p->stopped = 1;
					else p->completed = 1;
					return SUCCESS;
				}
			}
		}
	}
	return FAIL;
}
// update job status after waitpid returns some process status
int updateJobStatus(struct Job *job)
{
    struct Process *p;
    int status;
    int is_stopped = 0;
	for(p = job->process; p != NULL; p = p->next)
    {
		if(p->completed == 0 && p->stopped == 0)
        {
            if(job->run_status != RUNNING) job->notified = 0;
            job->run_status = RUNNING;
			return RUNNING;
		}
		if(p->stopped == 1)
        {
            is_stopped = 1;
            status = p->status;
        }
        else status = p->status;
	}
	if(is_stopped == 1)
    {
        if(job->run_status != STOPPED) job->notified = 0;
        job->run_status = STOPPED;
        job->status = status;
        job->foreground = 0;
        return STOPPED;
    }
    if(job->run_status != COMPLETED) job->notified = 0;
    job->run_status = COMPLETED;
    job->status = status;
    return COMPLETED;
}

// put job in foreground and give the terminal to the process groupid
void makeJobForeground(struct Job *job, int cont)
{

	tcsetpgrp(terminal, job->process_gid);
	job->foreground = 1;

	// sending continue signal
	if(cont)
    {
		tcsetattr(terminal, TCSADRAIN, &job->tmodes);
		if(kill(-job->process_gid, SIGCONT) < 0)
		{
		    fprintf(stderr,"kill (SIGCONT): %s\n",strerror(errno));
		    fflush(stderr);
		}
	}

	// Wait for the processes to report
	int status;
	pid_t pid;
	do
    {
		pid = waitpid(-1, &status, WUNTRACED);
	} while(updateProcessStatus(pid, status) == SUCCESS && updateJobStatus(job) == RUNNING);

    tcgetattr(terminal, &job->tmodes);
	tcsetpgrp(terminal, terminal_pgid);

	tcsetattr(terminal, TCSADRAIN, &job->tmodes);
	tcgetattr(terminal, &terminal_tmodes);
}

// put job in background
void sendJobBackground(struct Job *job, int cont)
{

	job->foreground = 0;
	if(cont)
    {
		if(kill(-job->process_gid, SIGCONT) < 0)
		{
			fprintf(stderr,"kill (SIGCONT): %s\n",strerror(errno));
			fflush(stderr);
		}
	}
}

// printing active job info
void printActiveJobs(struct Job *job)
{
    if(job == NULL) return;
    printActiveJobs(job->next);
    if(job->foreground == 0) printJobInfo(job);
}

void _fg(command *pc)
{
    if(pc->pa->nArgs == 1)
    {
        fprintf(stderr,"fg : Too few arguments\n");
        fflush(stderr);
        return;
    }
    if(pc->pa->nArgs > 2)
    {
        fprintf(stderr,"fg : Too many arguments\n");
        fflush(stderr);
        return;
    }
    struct Job *job = job_head;
    while(job != NULL)
    {
        if(job->job_num == atoi(pc->pa->azArgs[1]+1)) break;
        job = job->next;
    }
    if(job != NULL)
    {
        for(struct Process *p = job->process; p != NULL; p = p->next)
        {
            p->stopped = 0;
        }
        job->notified = 1;
        fprintf(stdout,"%s\n", job->job_name);
        makeJobForeground(job, 1);
    }
    else
    {
        fprintf(stderr,"fg : No job with the specified id found.\n");
        fflush(stderr);
    }
}
// printing process ids of a job
void printProcessIds(struct Process *p)
{
    if(p == NULL) return;
    printProcessIds(p->next);
    fprintf(stdout," %d", p->pid);
}

void _bg(command *pc)
{
    if(pc->pa->nArgs > 2)
    {
        fprintf(stderr,"bg : Too many arguments\n");
        fflush(stderr);
        return;
    }
    struct Job *job = job_head;
    if(pc->pa->nArgs == 1)
    {
        while(job != NULL)
        {
            if(job->run_status == STOPPED) break;
            job = job->next;
        }
    }
    else
    {
        while(job != NULL)
        {
            if(job->job_num == atoi(pc->pa->azArgs[1]+1)) break;
            job = job->next;
        }
    }
    if(job != NULL)
    {
        for(struct Process *p = job->process; p != NULL; p = p->next)
        {
            p->stopped = 0;
        }
        job->notified = 0;
        fprintf(stdout,"[%d]", job->job_num);
        printProcessIds(job->process);
        fprintf(stdout,"\n");
        fflush(stdout);
        sendJobBackground(job, 1);
    }
    else
    {
        fprintf(stderr,"bg : No job found.\n");
        fflush(stderr);
    }
}

void _kill(command *pc)
{
    if(pc->pa->nArgs == 1)
    {
        fprintf(stderr,"kill : Too few arguments\n");
        fflush(stderr);
        return;
    }
    for(int i = 1; i < pc->pa->nArgs; i++)
    {
        struct Job *job = job_head;
        while(job != NULL)
        {
            if(job->job_num == atoi(pc->pa->azArgs[i]+1)) break;
            job = job->next;
        }
        if(job != NULL)
        {
            for(struct Process *p = job->process; p != NULL; p = p->next)
            {
                p->stopped = 0;
            }
            job->notified = 0;
            sendJobBackground(job, 1);
            if(kill(-job->process_gid, SIGTERM) < 0)
            {
                fprintf(stderr,"kill (SIGTERM): %s\n",strerror(errno));
                fflush(stderr);
                return;
            }
        }
    }
}
// this function tests whether a command is built in or not
// int isBuiltIn(command *pc)
// {
//     if(strcmp(pc->zCmd, "cd") == 0 || strcmp(pc->zCmd, "bg") == 0 || strcmp(pc->zCmd, "fg") == 0 ||
//        strcmp(pc->zCmd, "jobs") == 0 || strcmp(pc->zCmd, "kill") == 0 || strcmp(pc->zCmd, "setenv") == 0 ||
//        strcmp(pc->zCmd, "unsetenv") == 0 || strcmp(pc->zCmd, "alias") == 0 || strcmp(pc->zCmd, "unalias") == 0 ||
//        strcmp(pc->zCmd, "quit") == 0 )
//     {
//         return 1;
//     }
//     return 0;
// }

/*Check for BuiltIn Commands*/
// LOOKUP: isBuiltIn
int CheckBuiltInCommands(command *pc)
{
    if (strcmp(pc->zCmd, "alias") == 0 || strcmp(pc->zCmd, "unalias") == 0 || strcmp(pc->zCmd, "unsetenv") == 0 ||
        strcmp(pc->zCmd, "cd") == 0 || strcmp(pc->zCmd, "setenv") == 0 || strcmp(pc->zCmd, "quit") == 0 ||
        strcmp(pc->zCmd, "bg") == 0 || strcmp(pc->zCmd, "fg") == 0 ||
        strcmp(pc->zCmd, "jobs") == 0 || strcmp(pc->zCmd, "kill") == 0)
    {
        return 1;
    }
    return 0;
}

// get next command from the pipeline
command* getNextCommandInPipeline(command *pc)
{
    redirect* redir = pc->prRedirects;
    while(redir != NULL)
    {
        if(redir->fType == PIPE || redir->fType == PIPE_ERROR)
        {
            return redir->u.pcPipe;
        }
        redir = redir->prNext;
    }
    return NULL;
}

// // this function performs aliasing on the given command
// void aliasing(command *pc)
// {
//     while(pc != NULL)
//     {
//         for(int i = 0; i < num_alias; i++)
//         {
//             if(strcmp(arr_aliases[i].from,pc->zCmd) == 0)
//             {
//                 free(pc->zCmd);
//                 pc->zCmd = (char*) malloc(WORD_SIZE*sizeof(char));
//                 strcpy(pc->zCmd,arr_aliases[i].to);
//                 break;
//             }
//         }
//         pc = getNextCommandInPipeline(pc);
//     }
// }

// this function will be implemented to check ambiguity in pipeline commands
int checkAmbiguity(command *pc, int *is_pipe)
{
    redirect* redir = pc->prRedirects;
    int is_redir_out = 0;
    while(redir != NULL)
    {
        if(redir->fType == PIPE || redir->fType == PIPE_ERROR)
        {
            if(is_redir_out == 1)
            {
                fprintf(stderr,"Ambiguous output redirect.\n");
                fflush(stderr);
                return FAIL;
            }
            *is_pipe = 1;
            redir = redir->u.pcPipe->prRedirects;
        }
        else if(redir->fType == REDIRECT_IN || redir->fType == REDIRECT_ERROR)
        {
            if(*is_pipe == 1)
            {
                fprintf(stderr,"Ambiguous input redirect.\n");
                fflush(stderr);
                return FAIL;
            }
            redir = redir->prNext;
        }
        else
        {
            is_redir_out = 1;
            redir = redir->prNext;
        }
    }
    return SUCCESS;
}

// this function handles I/O redirections
// int handleIORedirection(redirect *redir, int *fd_in, int *fd_out, int *is_stderr)
// {
//     while(redir != NULL)
//     {
//         if(redir->fType == REDIRECT_IN)
//         {
//             *fd_in = open(redir->u.pzFile,O_RDONLY,0);
//             if(*fd_in == -1)
//             {
//                 fprintf(stderr,"%s : Error : %s\n",redir->u.pzFile, strerror(errno));
//                 fflush(stderr);
//                 if(*fd_out != -1) close(*fd_out);
//                 return FAIL;
//             }
//         }
//         else if(redir->fType == REDIRECT_OUT || redir->fType == REDIRECT_ERROR || redir->fType == APPEND || redir->fType == APPEND_ERROR)
//         {
//             int flag = O_WRONLY;
//             if(redir->fType == REDIRECT_OUT) flag = flag | O_CREAT;
//             else if(redir->fType == REDIRECT_ERROR)
//             {
//                 flag = flag | O_CREAT;
//                 *is_stderr = 1;
//             }
//             else if(redir->fType == APPEND)
//             {
//                 flag = flag | O_APPEND;
//             }
//             else
//             {
//                 flag = flag | O_APPEND;
//                 *is_stderr = 1;
//             }
//             *fd_out = open(redir->u.pzFile, flag, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
//             if(*fd_out == -1)
//             {
//                 fprintf(stderr,"%s : Error : %s\n",redir->u.pzFile, strerror(errno));
//                 fflush(stderr);
//                 if(*fd_in != -1) close(*fd_in);
//                 return FAIL;
//             }
//         }
//         redir = redir->prNext;
//     }
//     return SUCCESS;
// }

/*function performing IO Redirection*/
// LOOKUP: handleIORedirection
int func_IORedirection(redirect *redir, int *check_stderror, int *fd_in, int *fd_out)
{
    while (redir != NULL)
    {
        // perform REDIRECT_IN
        if (redir->fType == REDIRECT_IN)
        {
            *fd_in = open(redir->u.pzFile, O_RDONLY, 0);
            if (*fd_in == -1)
            {
                fprintf(stderr, "%s : Error : %s\n", redir->u.pzFile, strerror(errno));
                fflush(stderr);
                if (*fd_out != -1)
                {
                    close(*fd_out);
                }
                return FAIL;
            }
        }
        else if (redir->fType == REDIRECT_OUT || redir->fType == APPEND || redir->fType == APPEND_ERROR || redir->fType == REDIRECT_ERROR)
        {
            int flag = O_WRONLY;
            // check for APPEND
            if (redir->fType == APPEND)
            {
                flag = flag | O_APPEND;
            }
            // check for REDIRECT_ERROR
            else if (redir->fType == REDIRECT_ERROR)
            {
                /* create if nonexistant */
                flag = flag | O_CREAT;
                *check_stderror = 1;
            }
            // check for REDIRECT_OUT
            else if (redir->fType == REDIRECT_OUT)
            {
                /* create if nonexistant */
                flag = flag | O_CREAT;
            }
            else
            {
                flag = flag | O_APPEND;
                *check_stderror = 1;
            }

            // file mode:
            // binary operation
            *fd_out = open(redir->u.pzFile, flag, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (*fd_out == -1)
            {
                fprintf(stderr, "%s : Error : %s\n", redir->u.pzFile, strerror(errno));
                fflush(stderr);
                if (*fd_in != -1)
                {
                    close(*fd_in);
                }
                return FAIL;
            }
        }
        redir = redir->prNext;
    }
    return SUCCESS;
}

// get execution path from PATH variable of not absolute path
int getPath(command *pc, char *path)
{
    int ret = SUCCESS;
    if(pc->zCmd[0] == '/')
    {
        strcpy(path,pc->zCmd);
        path[strlen(pc->zCmd)] = '\0';
    }
    else
    {
        if(pc->zCmd[0] == '.' && pc->zCmd[1] == '/')
        {
            strcpy(path,curr_directory);
            int str_length = strlen(pc->zCmd);
            int j = strlen(curr_directory);
            if(path[j-1] != '/') path[j++] = '/';
            for(int i = 2; i < str_length; i++)
            {
                path[j++] = pc->zCmd[i];
            }
            path[j] = '\0';
        }
        else
        {
            char PATH[WORD_SIZE];
            memset(PATH,0,sizeof(PATH));
            for(int i = 0; i < num_env; i++)
            {
                if(strcmp(UnixEnv[i].key,"PATH") == 0)
                {
                    strcpy(PATH,UnixEnv[i].value);
                    break;
                }
            }
            if(strlen(PATH) > 0)
            {
                char* token = strtok(PATH,":");
                int found = 0;
                while(token != NULL)
                {
                    char tmp_path[WORD_SIZE];
                    memset(tmp_path,0,sizeof(tmp_path));
                    strcpy(tmp_path,token);
                    int str_length = strlen(tmp_path);
                    if(tmp_path[str_length-1] != '/')
                    {
                        tmp_path[str_length] = '/';
                    }
                    char tmp_cmd[WORD_SIZE];
                    strcpy(tmp_cmd,pc->zCmd);
                    strcat(tmp_path,tmp_cmd);
                    if(access(tmp_path, X_OK) == 0)
                    {
                        strcpy(path, tmp_path);
                        found = 1;
                        break;
                    }
                    token = strtok(NULL,":");
                }
                if(found == 0) ret = FAIL;
            }
            else ret = FAIL;
        }
    }
    return ret;
}

// send job notification to the user
void checkForJobNotifications()
{
	struct Job *job, *last, *next;

	int status;
	pid_t pid;
	do
    {
		pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
	} while(updateProcessStatus(pid, status) == SUCCESS);

	last = NULL;
	for(job = job_head; job != NULL; job = next)
    {
		next = job->next;

        int run_status = updateJobStatus(job);
		if(run_status == COMPLETED)
		{
			if(job->foreground == 0 && job->notified == 0) fprintf(stdout, "[%d] done %s\n", job->job_num, job->job_name);
			if(last != NULL) last->next = next;
			else job_head = next;
			freeJob(job);
		}
		else if(run_status == STOPPED)
		{
			if(job->foreground == 0 && job->notified == 0) fprintf(stdout, "[%d] stopped\n", job->job_num);
			job->notified = 1;
			last = job;
		}
		else last = job;
	}
	if(job_head == NULL) n_job = 0;
}
// this function handles built-in commands
// int executeBuiltInCommand(command *pc)
// {
//     if(strcmp(pc->zCmd, "cd") == 0)
//     {
//         func_cd(pc);
//         return SUCCESS;
//     }
//     if(strcmp(pc->zCmd, "bg") == 0)
//     {
//         _bg(pc);
//         return SUCCESS;
//     }
//     if(strcmp(pc->zCmd, "fg") == 0)
//     {
//         _fg(pc);
//         return SUCCESS;
//     }
//     if(strcmp(pc->zCmd, "jobs") == 0)
//     {
//         printActiveJobs(job_head);
//         return SUCCESS;
//     }
//     if(strcmp(pc->zCmd, "kill") == 0)
//     {
//         _kill(pc);
//         return SUCCESS;
//     }
//     if(strcmp(pc->zCmd, "setenv") == 0)
//     {
//         func_setenv(pc);
//         return SUCCESS;
//     }
//     if(strcmp(pc->zCmd, "unsetenv") == 0)
//     {
//         func_unsetenv(pc);
//         return SUCCESS;
//     }
//     if(strcmp(pc->zCmd, "alias") == 0)
//     {
//         func_alias(pc);
//         return SUCCESS;
//     }
//     if(strcmp(pc->zCmd, "unalias") == 0)
//     {
//         func_unalias(pc);
//         return SUCCESS;
//     }
//     if(strcmp(pc->zCmd, "quit") == 0)
//     {
//         return QUIT;
//     }
//     return FAIL;
// }

/*function perform user built-in commands*/
// LOOKUP: executeBuiltInCommand
int ExecuteBuiltInComds(command *pc)
{
    if (strcmp(pc->zCmd, "cd") == 0)
    {
        func_cd(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "alias") == 0)
    {
        func_alias(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "unalias") == 0)
    {
        func_unalias(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "setenv") == 0)
    {
        func_setenv(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "unsetenv") == 0)
    {
        func_unsetenv(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "bg") == 0)
    {
        _bg(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "fg") == 0)
    {
        _fg(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "jobs") == 0)
    {
        printActiveJobs(job_head);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "kill") == 0)
    {
        _kill(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "quit") == 0)
    {
        return QUIT;
    }
    return FAIL;
}

void runProcess(command *pc,char *envs[],pid_t pgid,int fd_in,int fd_out,int is_stderr,int background,int interactive)
{
	pid_t pid;
	if(interactive == 1)
    {
		pid = getpid();
		if(pgid == 0) pgid = pid;
		setpgid(pid, pgid);
		if(background == 0) tcsetpgrp(terminal, pgid);

		/* Set the handling for job control signals back to the default.  */
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);
		signal(SIGTTIN, SIG_DFL);
		signal(SIGTTOU, SIG_DFL);
	}

	int fd_in1 = -1, fd_out1 = -1, is_stderr1 = 0;
    int ret = func_IORedirection(pc->prRedirects, &fd_in1, &fd_out1, &is_stderr1);
    if(ret == SUCCESS)
    {
        if(fd_in1 != -1) fd_in = fd_in1;
        if(fd_out1 != -1)
        {
            fd_out = fd_out1;
            is_stderr = is_stderr1;
        }
        /* Set the standard input/output channels of the new process.  */
        if(fd_in != STDIN_FILENO)
        {
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        if(fd_out != STDOUT_FILENO)
        {
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
            if(is_stderr == 1) dup2(STDOUT_FILENO, STDERR_FILENO);
        }

        /* Exec the new process.  Make sure we exit.  */
        if(CheckBuiltInCommands(pc) == 1)
        {
            ExecuteBuiltInComds(pc);
            exit(0);
        }
        else
        {
            char* path = (char*) calloc(WORD_SIZE,sizeof(char));
            if(getPath(pc, path) == FAIL)
            {
                fprintf(stderr, "%s : Command not found\n", pc->zCmd);
                fflush(stderr);
                free(path);
                exit(1);
            }
            if(access(path, X_OK) == 0)
            {
                execve(path, pc->pa->azArgs, envs);
                fprintf(stderr,"Error: %s\n",strerror(errno));
                fflush(stderr);
                exit(1);
            }
            else
            {
                fprintf(stderr,"%s : Error : %s\n",pc->zCmd, strerror(errno));
                fflush(stderr);
                free(path);
                exit(1);
            }
        }
    }
    exit(1);
}

/*check if the file exists and if the it is executable*/
int func_CheckFilePermission(command *pc, char *file_path)
{
    if (pc->zCmd[0] == '/')
    {
        // checking if access is available
        if (access(pc->zCmd, X_OK) == 0)
        {
            strcpy(file_path, pc->zCmd);
            file_path[strlen(pc->zCmd)] = '\0';
            return SUCCESS;
        }
        // unable to access
        else
        {
            fprintf(stderr, "%s : Error : %s\n", pc->zCmd, strerror(errno));
            fflush(stderr);
            return FAIL;
        }
    }
    else
    {
        if (pc->zCmd[0] == '.' && pc->zCmd[1] == '/')
        {
            strcpy(file_path, curr_directory);
            int str_length = strlen(pc->zCmd);
            int curr_dict_len = strlen(curr_directory);
            if (file_path[curr_dict_len - 1] != '/')
            {
                file_path[curr_dict_len++] = '/';
            }
            for (int i = 2; i < str_length; i++)
            {
                file_path[curr_dict_len++] = pc->zCmd[i];
            }
            file_path[curr_dict_len] = '\0';
            if (access(file_path, X_OK) == 0)
            {
                return SUCCESS;
            }
            else
            {
                fprintf(stderr, "%s : Error : %s\n", pc->zCmd, strerror(errno));
                fflush(stderr);
                return FAIL;
            }
            return SUCCESS;
        }
        else
        {
            char PATH[WORD_SIZE];
            memset(PATH, 0, sizeof(PATH));
            for (int i = 0; i < num_env; i++)
            {
                if (strcmp(UnixEnv[i].key, "PATH") == 0)
                {
                    strcpy(PATH, UnixEnv[i].value);
                    break;
                }
            }
            if (strlen(PATH) > 0)
            {
                char *token = strtok(PATH, ":");
                int found = 0;
                while (token != NULL)
                {
                    char tmp_path[WORD_SIZE];
                    memset(tmp_path, 0, sizeof(tmp_path));
                    strcpy(tmp_path, token);
                    int str_length = strlen(tmp_path);
                    if (tmp_path[str_length - 1] != '/')
                    {
                        tmp_path[str_length] = '/';
                    }
                    char tmp_cmd[WORD_SIZE];
                    strcpy(tmp_cmd, pc->zCmd);
                    strcat(tmp_path, tmp_cmd);
                    if (access(tmp_path, X_OK) == 0)
                    {
                        strcpy(file_path, tmp_path);
                        found = 1;
                        break;
                    }
                    token = strtok(NULL, ":");
                }
                if (found == 0)
                {
                    fprintf(stderr, "%s : Command not found\n", pc->zCmd);
                    fflush(stderr);
                    return FAIL;
                }
            }
            else
            {
                fprintf(stderr, "%s : Command not found\n", pc->zCmd);
                fflush(stderr);
                return FAIL;
            }
        }
    }
    return SUCCESS;
}

/*function to execute the command*/
// LOOKUP: executeCommand
// This function is different from Phase-I
int ExecCommand(command *pc, int interactive)
{
    int is_pipe = 0;
    int ret = checkAmbiguity(pc, &is_pipe);
    if(ret == FAIL)
    {
        return ret;
    }
    if(is_pipe == 0 && CheckBuiltInCommands(pc) == 1 && pc->fBackground == 0)
    {
        int fd_in = -1, fd_out = -1, is_stderr = 0;
        ret = func_IORedirection(pc->prRedirects, &fd_in, &fd_out, &is_stderr);
        if(ret == SUCCESS)
        {
            int saved_stdin = dup(STDIN_FILENO);
            int saved_stdout = dup(STDOUT_FILENO);
            int saved_stderr = dup(STDERR_FILENO);
            if(fd_in != -1)
            {
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            if(fd_out != -1)
            {
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
                if(is_stderr == 1) dup2(STDOUT_FILENO, STDERR_FILENO);
            }
            ret = ExecuteBuiltInComds(pc);
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stderr);
            return ret;
        }
        return ret;
    }
    char* envs[num_env+1];
    EnvGetVars(envs);
    struct Job *job = calloc(1, sizeof(struct Job));
    job->job_num = ++n_job;
    job->job_name = (char*) calloc(WORD_SIZE,sizeof(char));
    strcpy(job->job_name, pc->zCmd);
    struct Job *tmp = job_head;
    job_head = job;
    job_head->next = tmp;
    int fd_pipe[2], fd_in = STDIN_FILENO, fd_out = STDOUT_FILENO, is_stderr = 0;
    command *tmp_pc = pc;
    while(pc != NULL)
    {
        is_pipe = 0;
        redirect* redir = pc->prRedirects;
        while(redir != NULL)
        {
            if(redir->fType == PIPE || redir->fType == PIPE_ERROR)
            {
                is_pipe = 1;
                if(redir->fType == PIPE_ERROR) is_stderr = 1;
                break;
            }
            redir = redir->prNext;
        }
        if(is_pipe == 1)
        {
            pipe(fd_pipe);
            fd_out = fd_pipe[1];
        }
        else fd_out = STDOUT_FILENO;
        pid_t pid = fork();
        if(pid == 0)
        {
            runProcess(pc,envs,job->process_gid,fd_in,fd_out,is_stderr,tmp_pc->fBackground,interactive);
        }
        else if(pid < 0)
        {
            fprintf(stderr,"Fork failed %s\n", strerror(errno));
            fflush(stderr);
            exit(1);
        }
        else
        {
            struct Process *p = calloc(1, sizeof(struct Process));
            p->pid = pid;
            struct Process *tmp = job->process;
            job->process = p;
            job->process->next = tmp;
            if(interactive)
            {
                if(job->process_gid == 0) job->process_gid = pid;
                setpgid(pid, job->process_gid);
            }
        }
        if(fd_in != STDIN_FILENO) close(fd_in);
        if(fd_out != STDOUT_FILENO) close(fd_out);
        pc = getNextCommandInPipeline(pc);
        fd_in = fd_pipe[0];
    }
    if(interactive == 0)
    {
        int status;
        pid_t pid;
        do
        {
            pid = waitpid(-1, &status, WUNTRACED);
        } while(updateProcessStatus(pid, status) == SUCCESS && updateJobStatus(job) == RUNNING);
    }
    else if(tmp_pc->fBackground == 0)
    {
        job->foreground = 1;
        makeJobForeground(job, 0);
    }
    else
    {
        job->notified = 1;
        tcgetattr(terminal, &job->tmodes);
        fprintf(stdout,"[%d]", job->job_num);
        printProcessIds(job->process);
        fprintf(stdout,"\n");
        fflush(stdout);
        sendJobBackground(job, 0);
    }
    for(int i = 0; i < num_env+1; i++)
    {
        if(envs[i] != NULL) free(envs[i]);
    }
    return ret;
}

/*
interactive = 1 means the parser is reading from terminal
interactive = 0 means the parser is reading from a file
*/
void processCommands(FILE *pf, int interactive)
{
    resetParser(pf);
    int ret;
    while(1)
    {
        if(interactive == 1)
        {
            checkForJobNotifications();
            WarningPrompt(stdout);
        }
        int eof = 1;
        command* pc = nextCommand(&eof);
        if(eof == 1)
        {
            if(interactive == 1)
            {
                fprintf(stdout,"\nquit\n");
                fflush(stdout);
            }
            break;
        }
        if(pc != NULL)
        {
            command *tmp_pc = pc;
            while(pc != NULL)
            {
                func_aliasing(pc);
                ret = ExecCommand(pc, interactive);
                if(ret == QUIT) break;
                pc = pc->pcNext;
            }
            destroyCommand(tmp_pc);
        }
        if(ret == QUIT) break;
    }
}

// void freeJobs()
// {
//     while(job_head != NULL)
//     {
//         struct Job *job = job_head;
//         job_head = job_head->next;
//         freeJob(job);
//     }
// }

/*
this function is called at the start of the shell.
It finds host name, home directory, and current directory and
saves them in appropriate variables. This function also tries to read ~/.ishrc,
if it exists.
*/
// void init()
// {
//     int ret = gethostname(host_name, HOST_NAME_MAX);
//     if(ret)
//     {
//         fprintf(stderr,"Host name not found!\n");
//         fflush(stderr);
//         return;
//     }
//     struct passwd* user_info = getpwuid(getuid());
//     memset(home_directory,0,sizeof(home_directory));
//     memset(curr_directory,0,sizeof(curr_directory));
//     strcpy(home_directory,user_info->pw_dir);
//     strcat(home_directory,"/");
//     char* tmp = (char*) malloc(WORD_SIZE*sizeof(char));
//     getcwd(tmp, WORD_SIZE);
//     strcpy(curr_directory, tmp);
//     free(tmp);
//     int len = strlen(curr_directory);
//     if(curr_directory[len-1] != '/')
//     {
//         curr_directory[len] = '/';
//         curr_directory[len+1] = 0;
//     }
//     char path[WORD_SIZE];
//     strcpy(path, home_directory);
//     strcat(path,".ishrc");
//     FILE* pf = fopen(path,"r");
//     if(pf != NULL)
//     {
//         processCommands(pf, 0);
//         fclose(pf);
//     }

//     terminal = STDIN_FILENO;
// 	int interactive = isatty(terminal);

// 	if(interactive)
// 	{

// 		signal(SIGINT, SIG_IGN);
// 		signal(SIGQUIT, SIG_IGN);
// 		signal(SIGTSTP, SIG_IGN);
// 		signal(SIGTTIN, SIG_IGN);
// 		signal(SIGTTOU, SIG_IGN);

// 		terminal_pgid = getpid();
// 		if(setpgid(terminal_pgid, terminal_pgid) < 0)
//         {
// 			fprintf(stderr,"Error setting terminal process group id: %s\n", strerror(errno));
//             fflush(stderr);
// 			exit(1);
// 		}

// 		tcsetpgrp(terminal, terminal_pgid);

// 		tcgetattr(terminal, &terminal_tmodes);
// 	}
//     processCommands(stdin, interactive);
// }

/*main function*/
int main(int argc, char **argv)
{
    //init();
    int ret = gethostname(host_name, HOST_NAME_MAX);
    if (ret)
    {
        fprintf(stderr, "Host name not found!\n");
        fflush(stderr);
        return 0;
    }
    struct passwd *user_info = getpwuid(getuid());
    memset(home_directory, 0, sizeof(home_directory));
    memset(curr_directory, 0, sizeof(curr_directory));
    strcpy(home_directory, user_info->pw_dir);
    strcat(home_directory, "/");
    char *tmp = (char *)malloc(WORD_SIZE * sizeof(char));
    getcwd(tmp, WORD_SIZE);
    strcpy(curr_directory, tmp);
    free(tmp);

    int str_length = strlen(curr_directory);
    if (curr_directory[str_length - 1] != '/')
    {
        curr_directory[str_length] = '/';
        curr_directory[str_length + 1] = 0;
    }
    char path[WORD_SIZE];
    strcpy(path, home_directory);
    strcat(path, ".ishrc");
    FILE *pf = fopen(path, "r");
    if (pf != NULL)
    {
        processCommands(pf, 0);
        fclose(pf);
    }
    /*
    if (isatty(fileno(stdin)))
    {
        processCommands(stdin, 1);
    }
    else
    {
        processCommands(stdin, 0);
    }
    */

    terminal = STDIN_FILENO;
    int interactive = isatty(terminal);

    if (interactive)
    {

        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        terminal_pgid = getpid();
        if (setpgid(terminal_pgid, terminal_pgid) < 0)
        {
            fprintf(stderr, "Error setting terminal process group id: %s\n", strerror(errno));
            fflush(stderr);
            exit(1);
        }

        tcsetpgrp(terminal, terminal_pgid);

        tcgetattr(terminal, &terminal_tmodes);
    }

    processCommands(stdin, interactive);

    //freeJobs();
    while (job_head != NULL)
    {
        struct Job *job = job_head;
        job_head = job_head->next;
        freeJob(job);
    }

    return 0;
}
