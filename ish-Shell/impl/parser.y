%{
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>

#include "command.h"

extern int fEndOfFile;
extern command *pcGlobalCommand;
int yylex();
int yyerror(char *s);
%}

%union
{
    command     *cmd;
    redirect    *redir;
    args        *arg;
    char	*string;
    int		integer;
}

%token 	<string>	WORD
%token 	<string>	COMMAND
%token 	<string>	FILENAME
%token	<int>		BACKGROUND
%token	<int>		PIPE
%token	<int>		PIPE_ERROR
%token	<int>		SEMICOLON
%token	<int>		REDIRECT_IN
%token	<int>		REDIRECT_OUT
%token	<int>		REDIRECT_ERROR
%token	<int>		APPEND
%token	<int>		APPEND_ERROR
%token	<string>	STRING
%token	<int>		LOGICAL_AND
%token	<int>		LOGICAL_OR
%token  <int>           EOLN
%token	<int>		YEOF

%type <integer> separator
%type <integer> pipe
%type <string>  parameter

%type <cmd> cmd_line cmd_seq command
%type <redir> redirect redirect_in redirect_out redirect_file redirect_pipe
%type <arg> parameters
%start cmd_line

%left '&' ';'

%%


cmd_line 	: cmd_seq terminator {
                    $$ = $1;
		    pcGlobalCommand = $1;
		    YYACCEPT;
                }
                | terminator { 
                    $$ = NULL; 
		    pcGlobalCommand = NULL;
		    YYACCEPT;
		}
		| error terminator { 
		    $$ = NULL; 
		    pcGlobalCommand = NULL;
		    YYACCEPT; 
		}
                ;

terminator:	YEOF {
		    fEndOfFile = 1;
		}
		| EOLN {
		    fEndOfFile = 0;
		}
		;

cmd_seq         : command separator cmd_seq {
                    appendCommand($1, $3);
		    if ($2 == BACKGROUND) 
			$1->fBackground = 1;
		    $$ = $1;
                } 
                | command separator {
		    if ($2 == BACKGROUND) 
			$1->fBackground = 1;
		    $$ = $1;
		}
                | command { $$ = $1; } 
                ;

command 	: COMMAND parameters redirect {
		    char *c = strrchr($1, '/');
                    setArgument($2, 0, c ? c + 1 : $1);
		    $$ = createCommand(COMMAND, $1, $2, 0, $3, 
				       NULL);
		    free($1); /* allocated by lexer, copied by create */
		}
                ;


redirect        : redirect_file { $$ = $1;}
                | redirect_pipe { $$ = $1; }
                | redirect_file redirect_pipe {
                    appendRedirect($1, $2);
		    $$ = $1;
		}
                | { $$ = NULL; }
                ;
                   
redirect_file   : redirect_in {$$ = $1; }
                | redirect_out {$$ = $1; }
                | redirect_in redirect_out  {
                    appendRedirect($1, $2);
		    $$ = $1;
		}
                | redirect_out redirect_in  {
                    appendRedirect($1, $2);
		    $$ = $1;
		}
                ;

redirect_in     : REDIRECT_IN FILENAME {
		    $$ = createRedirect(REDIRECT_IN, $2, NULL);
		    free($2); /* allocated by lexer, copied by create */
                }
		;

redirect_out    : REDIRECT_OUT FILENAME {
		    $$ = createRedirect(REDIRECT_OUT, $2, NULL);
		    free($2); /* allocated by lexer, copied by create */
		}
                |  REDIRECT_ERROR FILENAME {
		    $$ = createRedirect(REDIRECT_ERROR, $2, NULL);
		    free($2); /* allocated by lexer, copied by create */
		}
		| APPEND FILENAME {
		    $$ = createRedirect(APPEND, $2, NULL);
		    free($2); /* allocated by lexer, copied by create */
		}
		| APPEND_ERROR FILENAME {
		    $$ = createRedirect(APPEND_ERROR, $2, NULL);
		    free($2); /* allocated by lexer, copied by create */
		}
		;

redirect_pipe   : pipe command {
		    /* Check for ambiguous redirect because command has
		     * input redirection */
		    $$ = createRedirect($1, $2, NULL);
		} 
                ;

separator 	: BACKGROUND { $$ = BACKGROUND; }
                | SEMICOLON { $$ = SEMICOLON; }
		;

pipe            : PIPE { $$ = PIPE; }
                | PIPE_ERROR { $$ = PIPE_ERROR; }
                ;

parameters	: parameters parameter {
                    appendArgument($1, $2);
		    free($2); /* allocated by lexer, copied by append */
		    $$ = $1;
                }
                | { $$ = createArguments(NULL); }
		;
parameter	: WORD
		| STRING
		;
%%

int yyerror(char *s)
{
    fprintf(stderr, "syntax error\n");
    return 0;
}
