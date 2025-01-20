/* Data structure describing a parsed command */

#ifndef _COMMAND_H_
#define _COMMAND_H_

/* This header file describes an interface for front-end processing in
 * a UNIX command shell. Note that this interface was designes for ease
 * of use and understanding, *not* speed. As such, there are a number 
 * of gratuitous string copies in place to ease tracking of memory 
 * allocation, for example.
 *
 * The three data structures defined in this file are "args", "redirect",
 * and "command". "args" is a data structure containing a list of argument
 * strings that should be passed to an executed command. "redirect" describes
 * a list of shell command redirections. Finally, "command" describes a 
 * sequence of shell commands, including their arguments and redirections.
 *
 * The main orgainzation of a shell will normally be a loop that,
 * after initially calling resetParser(filename), issues a command line
 * if running interactively and then call nextCommand(&eofFlag) until 
 * a quit command is found or end-of-file is reached. Erroneous command
 * lines return a NULL command; NULL commands should be ignored.
 *
 * It is the job of the rest of the shell to perform any necessary additional 
 * semantic checking on the returned command structure, to execute 
 * the described command, and to free the command data structure using 
 * destroyCommand() at the appropriate time.
 */

typedef struct _args args;
typedef struct _redirect redirect;
typedef struct _command command;

struct _args {
    int nArgs, nMaxArgs;
    char **azArgs; /* argv[0] is always the short name of the command */
};

struct _redirect {
    int fType; /* 'REDIRECT_IN', 'REDIRECT_OUT', from y.tab.h, etc. */
    union {
	char *pzFile;
	command *pcPipe;
    } u;
    redirect *prNext; /* List of redirects */
};

struct _command {
    char *zCmd;
    int fType;    /* Currently always COMMAND - if you want the 
		  * parser to detect builtins, you could have it
		  * insert a different type here. */
    int fBackground; /* Should the *entire command, including the pipeline*
		      * be executed in the background? */
    args *pa;
    redirect *prRedirects; /* Head of list of redirections */
    command *pcNext; /* Circular list of next commands */
};

/* These functions constitute the primary interface to the front-end 
 * from the rest of the shell. */
void 	   resetParser(FILE *pf);
command *  nextCommand(int *fEof);
void       destroyCommand(command *pc);

/* Functions below this point are primarily for the use of the shell 
 * frontend itself */
command *  createCommand(int fType, char *zCmd, args *a, int fBackground, 
			 redirect *prRedirects, command *pcNext);
void       appendCommand(command *pc1, command *pc2);
void       printCommand(command *pc, FILE *pf);

redirect * createRedirect(int fType, void *p, redirect *prNext);
void       appendRedirect(redirect *pc1, redirect *pc2);
void       printRedirect(redirect *pc, FILE *pf);
void       destroyRedirect(redirect *pr);

args * createArguments(char *zArg);
void   setArgument(args *pa, int n, char *zArg);
char * getArgument(args *pa, int n);
void   appendArgument(args *pa, char *zArg);
void   printArguments(args *pa, FILE *pf);
void   destroyArguments(args *pa);

#endif /* _COMMAND_H_ */
