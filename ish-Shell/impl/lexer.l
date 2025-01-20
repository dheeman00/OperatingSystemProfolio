%Start PARAM FNAME
%option nounput
%option noinput
%{
#include <string.h>
#include <stdlib.h>

#include "command.h"
#include "parser.tab.h"


extern YYSTYPE yylval;

char *
copyEscapedString(char *zSrc)
{
    int quotechar = 0;
    char *zDest;
    int maxlen = strlen(zSrc)+1;
    int iDest = 0, iSrc = 0;
 
    zDest = (char *)calloc(maxlen, sizeof(char));
    for (iSrc = 0; iSrc < maxlen; iSrc++) 
    {
    	if (zSrc[iSrc] == quotechar) {
	    quotechar = 0;
	    continue;
	} else if (((zSrc[iSrc] == '"') || (zSrc[iSrc] == '\'')) 
		   && !quotechar) {
	    quotechar = zSrc[iSrc];
	    continue;
	} else if (zSrc[iSrc] == '\\' && !quotechar) {
	    zDest[iDest++] = zSrc[++iSrc];
	    continue;
	}
	zDest[iDest++] = zSrc[iSrc];
    }
    return zDest;
}

%}

eoln		\n
sq		\'
dq		\"
backslash	\\
separator 	[&|;]
spaces		[ \t]
quotes		['"]
redirect	[<>]
alpha 		[A-Za-z]
number 		[0-9]
others		[+,%_#@$.*/:!?-]
backspecial	{backslash}({separator}|{redirect}|{eoln}|{spaces}|{number}|{alpha}|{others}|{backslash}|{quotes}|&)
nonquotes	({alpha}|{number}|{backspecial}|{others}|{spaces}|{eoln}|{redirect})
dquoteseq	{dq}(({nonquotes}|{sq})*){dq}
squoteseq	{sq}(({nonquotes}|{dq})*){sq}
word		({alpha}|{number}|{backspecial}|{others}|{dquoteseq}|{squoteseq})*

%%
<INITIAL>{word} { 
                    yylval.string = copyEscapedString(yytext);
		    BEGIN(PARAM);
		    return COMMAND;
		}

<FNAME>{word} { 
                    yylval.string = copyEscapedString(yytext);
		    BEGIN(PARAM);
		    return FILENAME;
		}

{word}		{
                    yylval.string = copyEscapedString(yytext);
		    return WORD;
		}

[>][>][&]	{ 
		    BEGIN(FNAME);
		    return APPEND_ERROR; 
		}


[>][>]		{ 
		    BEGIN(FNAME);
		    return APPEND; 
		}

[|][&]		{ 
		    BEGIN(INITIAL);
		    return PIPE_ERROR; 
		}

[>][&]		{ 
		    BEGIN(FNAME);
		    return REDIRECT_ERROR; 
		}

"<"		{
		    BEGIN(FNAME);
		    return REDIRECT_IN;
		}

">"		{
		    BEGIN(FNAME);
		    return REDIRECT_OUT;
		}

"||"		{	
		    BEGIN(INITIAL);
		    return LOGICAL_OR;
		}

"&&"		{	
		    BEGIN(INITIAL);
		    return LOGICAL_AND;
		}

"&"		{	
		    BEGIN(INITIAL);
		    return BACKGROUND;
		}

"|"		{	
		    BEGIN(INITIAL);
		    return PIPE;
		}

";"		{	
		    BEGIN(INITIAL);
		    return SEMICOLON;
		}

\n		{   
		    BEGIN(INITIAL);
		    return EOLN; 
		}

[ \t]+		;

.		{
		    printf("Invalid %s\n", yytext);
		}
<<EOF>>		{
		    BEGIN(INITIAL);
		    return YEOF;
		}
