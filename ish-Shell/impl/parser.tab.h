/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     WORD = 258,
     COMMAND = 259,
     FILENAME = 260,
     BACKGROUND = 261,
     PIPE = 262,
     PIPE_ERROR = 263,
     SEMICOLON = 264,
     REDIRECT_IN = 265,
     REDIRECT_OUT = 266,
     REDIRECT_ERROR = 267,
     APPEND = 268,
     APPEND_ERROR = 269,
     STRING = 270,
     LOGICAL_AND = 271,
     LOGICAL_OR = 272,
     EOLN = 273,
     YEOF = 274
   };
#endif
/* Tokens.  */
#define WORD 258
#define COMMAND 259
#define FILENAME 260
#define BACKGROUND 261
#define PIPE 262
#define PIPE_ERROR 263
#define SEMICOLON 264
#define REDIRECT_IN 265
#define REDIRECT_OUT 266
#define REDIRECT_ERROR 267
#define APPEND 268
#define APPEND_ERROR 269
#define STRING 270
#define LOGICAL_AND 271
#define LOGICAL_OR 272
#define EOLN 273
#define YEOF 274




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 22 "parser.y"
{
    command     *cmd;
    redirect    *redir;
    args        *arg;
    char	*string;
    int		integer;
}
/* Line 1529 of yacc.c.  */
#line 95 "parser.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

