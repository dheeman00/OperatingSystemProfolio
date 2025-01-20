#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "command.h"
#include "parser.tab.h"

/* TODO
 * double-linking of lists
 */
command *pcGlobalCommand;
int fEndOfFile = 0;
void yyrestart(FILE *pf);
int yyparse(void);

command *
createCommand(int fType, char *zCommand, args *pa, 
	      int fBackground, redirect *prRedirects, command *pcNext)
{
    command *pc = (command *)malloc(sizeof(command));
    if (!pc) return NULL;
    pc->zCmd = strdup(zCommand);
    pc->fType = fType;
    pc->pa = pa;
    pc->fBackground = fBackground;
    pc->prRedirects = prRedirects;

    pc->pcNext = pcNext;
    return pc;
}

void
destroyCommand(command *pc)
{
    assert(pc);

    assert(pc->zCmd);
    free(pc->zCmd);
    pc->zCmd = NULL;

    if (pc->pcNext) {
	destroyCommand(pc->pcNext);
	pc->pcNext = NULL;
    }

    if (pc->prRedirects) {
	destroyRedirect(pc->prRedirects);
	pc->prRedirects = NULL;
    }

    assert(pc->pa);
    destroyArguments(pc->pa);
    pc->pa = NULL;

    free(pc);
    return;
}

void
appendCommand(command *pc1, command *pc2)
{
    assert(pc1 && pc2);
    while (pc1->pcNext)
	pc1 = pc1->pcNext;
    pc1->pcNext = pc2;
}

void 
printCommand(command *pc, FILE *pf)
{
    assert(pc);

    fprintf(pf, "%s ", pc->zCmd);
    printArguments(pc->pa, pf);

    if (pc->prRedirects)
	printRedirect(pc->prRedirects, pf);

    if (pc->fBackground)
	fprintf(pf, "& ");
    else if (pc->pcNext) 
	fprintf(pf, "; ");
	
    if (pc->pcNext)
	printCommand(pc->pcNext, pf);

    return;
}

void
resetParser(FILE *pf)
{
    yyrestart(pf);
    fEndOfFile = 0;
}

command * 
nextCommand(int *eof)
{
    yyparse();
    if (eof)
	*eof = fEndOfFile;
    return pcGlobalCommand;
}

redirect * 
createRedirect(int fType, void *p, redirect *prNext)
{
    redirect *pr = (redirect *)malloc(sizeof(redirect));

    if (!pr) 
	return NULL;

    pr->fType = fType;
    switch (fType)
    {
      case REDIRECT_IN: case REDIRECT_OUT:
      case REDIRECT_ERROR: case APPEND: case APPEND_ERROR:
	  pr->u.pzFile = strdup((char *)p);
	  break;
      case PIPE: case PIPE_ERROR:
	  pr->u.pcPipe = (command *)p;
	  break;
      default:
	  assert("Invalid redirect type" && 0);
    }

    pr->prNext = prNext;
    return pr;
}

void
appendRedirect(redirect *pr1, redirect *pr2)
{
    while (pr1->prNext)
	pr1 = pr1->prNext;
    pr1->prNext = pr2;
}

void
destroyRedirect(redirect *pr)
{
    assert(pr);

    if (pr->prNext) {
	destroyRedirect(pr->prNext);
	pr->prNext = NULL;
    }

    switch (pr->fType)
    {
      case REDIRECT_IN: case REDIRECT_OUT:
      case REDIRECT_ERROR: case APPEND: case APPEND_ERROR:
	  free(pr->u.pzFile);
	  break;
      case PIPE: case PIPE_ERROR:
	  destroyCommand(pr->u.pcPipe);
	  break;
      default:
	  assert("Invalid redirect type" && 0);
    }

    free(pr);
}

void 
printRedirect(redirect *pr, FILE *pf)
{
    char *zRedirect;
    int fPipe = 0;

    assert(pr);

    switch (pr->fType) 
    {
      case REDIRECT_IN:
	  zRedirect = "<";
	  break;
      case REDIRECT_OUT:
	  zRedirect = ">";
	  break;
      case REDIRECT_ERROR:
	  zRedirect = ">&";
	  break;
      case APPEND:
	  zRedirect = ">>";
	  break;
      case APPEND_ERROR:
	  zRedirect = ">>&";
	  break;
      case PIPE:
	  zRedirect = "|";
	  fPipe = 1;
	  break;
      case PIPE_ERROR:
	  zRedirect = "|&";
	  fPipe = 1;
	  break;
    }
    fprintf(pf, "%s ", zRedirect);
    if (fPipe)
	printCommand(pr->u.pcPipe, pf);
    else
	fprintf(pf, "%s ", pr->u.pzFile);

    if (pr->prNext)
	printRedirect(pr->prNext, pf);

    return;
}

/* The arguments code is blatantly inefficient - it makes copies of
 * the strings appended to it, even though these strings themselves
 * could perhaps be allocated. This makes this code slower, but easier
 * to write, understand, and debug. For a simple class assignment like
 * this where you're looking at my code, that's appropriate. */

#define DEFAULT_MAXARGS 8

args * 
createArguments(char *zArg) 
{
    args *pa = (args *)malloc(sizeof(args));
    if (!pa) return NULL;
    
    pa->azArgs = (char **)calloc(DEFAULT_MAXARGS + 1, sizeof(char *));
    if (!pa->azArgs) {
	free(pa->azArgs);
	return NULL;
    }
    pa->nMaxArgs = DEFAULT_MAXARGS;
    pa->nArgs = 1;
    if (pa->azArgs[0]) {
	pa->azArgs[0] = strdup(zArg);
    }
    pa->azArgs[1] = NULL;
    return pa;
}

void 
setArgument(args *pa, int n, char *zArg)
{
    assert(pa);
    assert(n <= pa->nMaxArgs);

    if (pa->azArgs[n]) free(pa->azArgs[n]);
    pa->azArgs[n] = zArg ? strdup(zArg) : NULL;
    return;
}

char *
getArgument(args *pa, int n)
{
    assert(pa);
    assert(n <= pa->nMaxArgs);
    return pa->azArgs[n];
}

void
appendArgument(args *pa, char *zArg)
{
    assert(pa);
    if (pa->nArgs == pa->nMaxArgs) {
	pa->nMaxArgs *= 2;
	pa->azArgs = (char **)realloc(pa->azArgs, sizeof(char *) * pa->nMaxArgs + 1);
    } else {
	assert(pa->nArgs < pa->nMaxArgs);
    }
    pa->azArgs[pa->nArgs++] = strdup(zArg);
    pa->azArgs[pa->nArgs] = NULL;
}

void
printArguments(args *pa, FILE *pf)
{
    int i;
    for (i = 1; i < pa->nArgs; i++) {
	fprintf(pf, "%s ", pa->azArgs[i]);
    }
    assert(pa->azArgs[pa->nArgs] == NULL);
}

void
destroyArguments(args *pa)
{
    int i;
    for (i = 0; i < pa->nArgs; i++)
	if (pa->azArgs[i]) {
	    free(pa->azArgs[i]);
	    pa->azArgs[i] = NULL;
	}
    free(pa->azArgs);
    free(pa);
}
