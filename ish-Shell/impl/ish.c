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
int num_jobs = 0;  // number of jobs

/* Keep track of attributes of the terminal.*/
pid_t pid_terminal;
struct termios terminal_tmodes;
int terminal;

// contains environment variables
struct UnixEnv
{
    char key[WORD_SIZE];
    char value[WORD_SIZE];
} UnixEnv[WORD_LIMIT];

// contains all aliases
struct Alias
{
    char from[WORD_SIZE];
    char to[WORD_SIZE];
} arr_aliases[WORD_LIMIT];

// contains process tracking
struct Process
{
	pid_t process_id;
    
    int completed;
    int status;
	int stopped;
	
	struct Process *next;
};

// contain job variables
struct Job
{
    int job_num;
    char *job_name;
	
	int notified;
    int status;
    int run_status;
    int foreground;

    struct termios tmodes; 
	struct Job *next;

    struct Process *process;
    pid_t process_group_id; 
};

struct Job *job_head = NULL;



/*This functions replicate the functionality of execve()*/
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

/*Prompt warning for any unexecutable commands*/
void WarningPrompt(FILE *pf)
{
    fprintf(pf, "%s", host_name);
    fflush(pf);
}

/*fixing directory path eg /path1/path2/../path3 to /path1/path3*/
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

/*allocate memory for Job*/
void AllocJobMem(struct Job *job)
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

/*print job information*/
void PrintInfoJobs(struct Job *job)
{
    if (job->run_status == RUNNING)
    {
        fprintf(stdout, "[%d] Running\n", job->job_num);
    }
    else if (job->run_status == STOPPED)
    {
        fprintf(stdout, "[%d] %s %s\n", job->job_num, job->job_name, strsignal(WSTOPSIG(job->status)));
    }
    else
    {
        if (WIFSIGNALED(job->status))
        {
            fprintf(stdout, "[%d] %s %s", job->job_num, job->job_name, strsignal(WTERMSIG(job->status)));
            if (WCOREDUMP(job->status))
            {
                fprintf(stdout, " (Core Dumped)\n");
            }
            else
            {
                fprintf(stdout, "\n");
            }
        }
        else
        {
            fprintf(stdout, "[%d] %s Exit %d\n", job->job_num, job->job_name, WEXITSTATUS(job->status));
        }
    }
    fflush(stdout);
}

/*Check for BuiltIn Commands*/
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

/*function performing alias */
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

/*function performing unalias */
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

/* function perform setenv command*/
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


/* function perform unsetenv command*/
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

/*function perform cd command*/
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

/*Update Process Status based on wait pid*/
int UpdatedProcessStatus(pid_t process_id, int status)
{
	struct Job *job;
	struct Process *p;
	if(process_id > 0)
    {
		for(job = job_head; job != NULL; job = job->next)
		{
			for(p = job->process; p != NULL; p = p->next)
			{
				if(p->process_id == process_id)
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
int UpdatedJobStatus(struct Job *job)
{
    struct Process *procs;
    int status;
    int is_stopped = 0;
	for(procs = job->process; procs != NULL; procs = procs->next)
    {
		if(procs->completed == 0 && procs->stopped == 0)
        {
            if(job->run_status != RUNNING) 
            {
                job->notified = 0;
            }
            job->run_status = RUNNING;
			return RUNNING;
		}
		if(procs->stopped == 1)
        {
            is_stopped = 1;
            status = procs->status;
        }
        else status = procs->status;
	}

	if(is_stopped == 1)
    {
        if(job->run_status != STOPPED) 
        {
            job->notified = 0;
        }

        job->run_status = STOPPED;
        job->status = status;
        job->foreground = 0;

        return STOPPED;
    }
    // check for incomplete jobs
    if(job->run_status != COMPLETED) 
    {
        job->notified = 0;
    }

    job->run_status = COMPLETED;
    job->status = status;
    
    return COMPLETED;
}

/*make Job background*/
void MakeJobBackGround(struct Job *job, int cont)
{

    job->foreground = 0;
    if(cont)
    {
        if (kill(-job->process_group_id, SIGCONT) < 0)
        {
            fprintf(stderr, "kill (SIGCONT): %s\n", strerror(errno));
            fflush(stderr);
        }
    }
}

/*make Job foreground process groupid given to terminal*/
void MakeJobForeGround(struct Job *job, int cont)
{

	tcsetpgrp(terminal, job->process_group_id);
	job->foreground = 1;

    // sending info to terminal
    if(cont)
    {
		tcsetattr(terminal, TCSADRAIN, &job->tmodes);
		if(kill(-job->process_group_id, SIGCONT) < 0)
		{
		    fprintf(stderr,"kill (SIGCONT): %s\n",strerror(errno));
		    fflush(stderr);
		}
	}

    // report process id
    int status;
	pid_t process_id;
	do
    {
		process_id = waitpid(-1, &status, WUNTRACED);
    } while (UpdatedJobStatus(job) == RUNNING && UpdatedProcessStatus(process_id, status) == SUCCESS);

    tcgetattr(terminal, &job->tmodes);
	tcsetpgrp(terminal, pid_terminal);

	tcsetattr(terminal, TCSADRAIN, &job->tmodes);
	tcgetattr(terminal, &terminal_tmodes);
}

/*Print Active Jobs*/
void DisplayActiveJobs(struct Job *job)
{
    if (job == NULL)
    {
        return;
    }

    DisplayActiveJobs(job->next);

    if (job->foreground == 0)
    {
        PrintInfoJobs(job);
    }
}

/*Print Process ID*/
void DisplayProcessIDs(struct Process *proc_ptr)
{
    if (proc_ptr == NULL)
    {
        return;
    }
    DisplayProcessIDs(proc_ptr->next);
    fprintf(stdout, " %d", proc_ptr->process_id);
}

/*Check the Job Status*/
void func_JobNotification()
{
    int status;
    pid_t process_id;
    struct Job *job;
    struct Job *last; 
    struct Job *next;
    last = NULL;

    do
    {
        process_id = waitpid(-1, &status, WUNTRACED | WNOHANG);
    } while (UpdatedProcessStatus(process_id, status) == SUCCESS);

    
    for (job = job_head; job != NULL; job = next)
    {
        next = job->next;

        int run_status = UpdatedJobStatus(job);
        if (run_status == COMPLETED)
        {
            if (job->foreground == 0 && job->notified == 0)
            {
                fprintf(stdout, "[%d] done %s\n", job->job_num, job->job_name);
            }
                
            if (last != NULL)
            {
                last->next = next;
            }
                
            else
            {
                job_head = next;
            }
                
            AllocJobMem(job);
        }
        else if (run_status == STOPPED)
        {
            if (job->foreground == 0 && job->notified == 0)
            {
                fprintf(stdout, "[%d] stopped\n", job->job_num);
            }
                
            job->notified = 1;
            last = job;
        }
        else
        {
            last = job;
        }
            
    }
    if (job_head == NULL)
    {
        num_jobs = 0;
    }
        
}

/*function perform fg (foreground) command*/
void func_fg(command *pc)
{
    struct Job *job = job_head;

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
        MakeJobForeGround(job, 1);
    }
    else
    {
        fprintf(stderr,"fg : No job with the specified id found.\n");
        fflush(stderr);
    }
}

/*function perform bg (background) command*/
void func_bg(command *pc)
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
        DisplayProcessIDs(job->process);
        fprintf(stdout,"\n");
        fflush(stdout);
        MakeJobBackGround(job, 1);
    }
    else
    {
        fprintf(stderr,"bg : No job found.\n");
        fflush(stderr);
    }
}

/*function perform kill command*/
void func_kill(command *pc)
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
            MakeJobBackGround(job, 1);
            
            if(kill(-job->process_group_id, SIGTERM) < 0)
            {
                fprintf(stderr,"kill (SIGTERM): %s\n",strerror(errno));
                fflush(stderr);
                return;
            }
        }
    }
}

/*Next Pipeline Command*/
command* NextPipelineCommand(command *pc)
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

/*Check Ambiguity in Pipeline Command*/
int CheckPipelineAmbiguity(command *pc, int *is_pipe)
{
    redirect* redir = pc->prRedirects;
    int is_redir_out = 0;
    while(redir != NULL)
    {
        if(redir->fType == PIPE || redir->fType == PIPE_ERROR)
        {
            if(is_redir_out == 1)
            {
                fprintf(stderr,"ambiguous output redirect.\n");
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

/*function perform user built-in commands*/
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
        func_bg(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "fg") == 0)
    {
        func_fg(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "jobs") == 0)
    {
        DisplayActiveJobs(job_head);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "kill") == 0)
    {
        func_kill(pc);
        return SUCCESS;
    }
    if (strcmp(pc->zCmd, "quit") == 0)
    {
        return QUIT;
    }
    return FAIL;
}

/*function performing IO Redirection*/
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

/*Get the Execution Path Variable*/
int GetExecPath(command *pc, char *path)
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
            if(path[j-1] != '/') 
            {
                path[j++] = '/';
            }
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

/* function to execute Process*/
void ExecProcess(command *pc, char *envs[], pid_t pgid, int fd_out, int fd_in,
                 int is_stderr, int interactive, int background)
{
	pid_t process_id;
    int lookup_fd_out = -1;
    int lookup_fd_in = -1; 
    int lookup_stderr = 0;

    if(interactive == 1)
    {
		process_id = getpid();
		if(pgid == 0) 
        {
            pgid = process_id;
        }
		setpgid(process_id, pgid);

		if(background == 0) 
        {
            tcsetpgrp(terminal, pgid);
        }

        // default job control signals
        signal(SIGINT, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
    }
	
    int ret = func_IORedirection(pc->prRedirects, &lookup_fd_in, &lookup_fd_out, &lookup_stderr);
    if(ret == SUCCESS)
    {
        if(lookup_fd_in != -1) 
        {
            fd_in = lookup_fd_in;
        }
        if(lookup_fd_out != -1)
        {
            fd_out = lookup_fd_out;
            is_stderr = lookup_stderr;
        }
        // set the standard input/output signals for new process
        if(fd_in != STDIN_FILENO)
        {
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        if(fd_out != STDOUT_FILENO)
        {
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
            if(is_stderr == 1) 
            {
                dup2(STDOUT_FILENO, STDERR_FILENO);
            }

        }

        // execute process
        if(CheckBuiltInCommands(pc) == 1)
        {
            ExecuteBuiltInComds(pc);
            exit(0);
        }
        else
        {
            char* path = (char*) calloc(WORD_SIZE,sizeof(char));
            if(GetExecPath(pc, path) == FAIL)
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
int ExecCommand(command *pc, int interactive)
{
    int check_pipeline = 0;
    int ret = CheckPipelineAmbiguity(pc, &check_pipeline);
    
    // checking for ambiguity for pipeline
    if(ret == FAIL)
    {
        return ret;
    }
    // ambiguity not present
    if (check_pipeline == 0 && pc->fBackground == 0 && CheckBuiltInCommands(pc) == 1)
    {
        int fd_out = -1;
        int fd_in = -1;
        int is_stderr = 0;
        ret = func_IORedirection(pc->prRedirects, &fd_in, &fd_out, &is_stderr);
        
        if(ret == SUCCESS)
        {
            int saved_stdout = dup(STDOUT_FILENO);
            int saved_stdin = dup(STDIN_FILENO);            
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
                if(is_stderr == 1) 
                {
                    dup2(STDOUT_FILENO, STDERR_FILENO);
                }
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
    // performing execve()
    char* envs[num_env+1];
    EnvGetVars(envs);

    // performing job control
    struct Job *job = calloc(1, sizeof(struct Job));
    job->job_num = ++num_jobs;
    job->job_name = (char*) calloc(WORD_SIZE,sizeof(char));
    
    strcpy(job->job_name, pc->zCmd);
    struct Job *tmp = job_head;
    // change in job control
    job_head = job;
    job_head->next = tmp;
    
    int fd_pipe[2];
    int fd_in = STDIN_FILENO;
    int fd_out = STDOUT_FILENO;
    int is_stderr = 0;
    
    command *tmp_pc = pc;
    while(pc != NULL)
    {
        check_pipeline = 0;
        redirect* redir = pc->prRedirects;
        while(redir != NULL)
        {
            // checking for pipeline error
            if(redir->fType == PIPE || redir->fType == PIPE_ERROR)
            {
                check_pipeline = 1;
                if(redir->fType == PIPE_ERROR) 
                {
                    is_stderr = 1;
                }

                break;
            }
            redir = redir->prNext;
        }
        if(check_pipeline == 1)
        {
            pipe(fd_pipe);
            fd_out = fd_pipe[1];
        }
        else 
        {
            fd_out = STDOUT_FILENO;
        }

        // performing fork()
        pid_t process_id = fork();
        if(process_id == 0)
        {
            ExecProcess(pc, envs, job->process_group_id, fd_out, fd_in,
                        is_stderr, interactive, tmp_pc->fBackground);
        }
        else if(process_id < 0)
        {
            fprintf(stderr,"Fork failed %s\n", strerror(errno));
            fflush(stderr);
            exit(1);
        }
        else
        {
            struct Process *p = calloc(1, sizeof(struct Process));
            p->process_id = process_id;
            struct Process *tmp = job->process;
            job->process = p;
            job->process->next = tmp;
            if(interactive)
            {
                if(job->process_group_id == 0)
                {
                    job->process_group_id = process_id;
                } 
                setpgid(process_id, job->process_group_id);
            }
        }
        if(fd_in != STDIN_FILENO) 
        {
            close(fd_in);
        }
        if(fd_out != STDOUT_FILENO) 
        {
            close(fd_out);
        }
        pc = NextPipelineCommand(pc);
        fd_in = fd_pipe[0];
    }
    if(interactive == 0)
    {
        int status;
        pid_t process_id;
        do
        {
            process_id = waitpid(-1, &status, WUNTRACED);
        } while(UpdatedProcessStatus(process_id, status) == SUCCESS && UpdatedJobStatus(job) == RUNNING);
    }
    else if(tmp_pc->fBackground == 0)
    {
        job->foreground = 1;
        MakeJobForeGround(job, 0);
    }
    else
    {
        job->notified = 1;
        tcgetattr(terminal, &job->tmodes);
        fprintf(stdout,"[%d]", job->job_num);
        DisplayProcessIDs(job->process);
        fprintf(stdout,"\n");
        fflush(stdout);
        MakeJobBackGround(job, 0);
    }
    for(int i = 0; i < num_env+1; i++)
    {
        if(envs[i] != NULL) 
        {
            free(envs[i]);
        }
    }
    return ret;
}

/* function is used to process command by parsing from file or typing*/
void processCommands(FILE *pf, int interactive)
{
    resetParser(pf);
    int ret;
    while(1)
    {
        if(interactive == 1)
        {
            func_JobNotification();
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

/*main function*/
int main(int argc, char **argv)
{
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
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);        

        pid_terminal = getpid();
        if (setpgid(pid_terminal, pid_terminal) < 0)
        {
            fprintf(stderr, "error setting terminal process group id: %s\n", strerror(errno));
            fflush(stderr);
            exit(1);
        }

        tcsetpgrp(terminal, pid_terminal);

        tcgetattr(terminal, &terminal_tmodes);
    }

    processCommands(stdin, interactive);

    // execution of job    
    while (job_head != NULL)
    {
        struct Job *job = job_head;
        job_head = job_head->next;
        AllocJobMem(job);
    }

    return 0;
}
