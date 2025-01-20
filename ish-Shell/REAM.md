# CS587 - Advanced Operating Systems Project 1: The `ish` shell

## Due Dates
  * Suggested Setup Completion: Thursday, Augs,September 1
  * Phase 1 Due Date: Thursday, September 15th, 12:00pm (66% of assignment grade)
  * Phase 2 Due Date: Thursday, September 29th, 12:00pm (34% of assignment grade)

## Overview
Your task is to implement---on your own---the Unix shell similar to the
one described by Ritchie and Thompson in paper assigned in class, with the specific 
details of the shell you are to implement described in the attached man page. 
A subset of the features are due in phase 1, with a full working job control
shell with UNIX pipes due in phase 2. 

The file RUBRIC.md contains a full grading rubric for the shell, including the features you should try to have working by the setup milestone, the features that will be graded for the phase 1 submission date, and the features that will be graded for the phase 2 submission date.
You will use GitHub classroom to submit a working program on or before the due date. The testcases directory includes scripts and supporting programs that you can use to test some of these features.

Your shell should be written in C (or C++), compile using a Makefile by typing `make`, and produce an executable named `ish`.  Your code must also be "clean" it should compile without warnings or unresolved references using the `gcc` compiler on a Linux machine identical to those in the CS Lab or the UNM CS "trusty" machines (i.e. the set of machines accessible by ssh to trusty.cs.unm.edu).  Your programs are expected to be well organized and easy to read, as well as correct.

## Supporting and Reference Materials

Before starting, you should become familiar with the Unix system calls defined in Section 2. of the UNIX manual There are also several library routines in Section 3 that provide convenient interfaces to some of the more cryptic system calls. However, you may _not_ use the library routine `system` nor any of the routines prohibitied on the `ish` man page.  Several chapters of the Richard Stevens book _Advanced Programming in the UNIX Environment_ contain helpful information; Chapters 7, 8, and 9 are especially relevant, and this book is available online for free through the UNM library.

You may, if you so desire, use the `lex` and `yacc` programs to implement command parsing.  To simplify this somewhat tedious part of the project, I have supplied several files, `lexer.l`, `parser.y`, `command.c`, and `command.h`, along with an associated Makefile and a simple driver program, `ish.c`.  These files implement a simple front-end to the shell that you may use if you desire.  The functions implemented in these files read shell commands from a file and return a data structure describing a command each time `nextCommand()` is invoked.  The `command.h` header file describes this data structure and provides the prototypes of the related functions. The are included in this git repository; please delete them from your repository if you decide not to use them.

Note that I do not guarantee that these files are bug-free, though I have made a reasonable effort to ensure that they work well.  In addition, while the front end should catch syntactic errors as well as some semantic errors, it will not necessarily catch all _semantic_ errors, such as ambiguous redirections (e.g.  `/bin/ls > outfile | pipecommand`).  Be sure to test and debug the provided code and to check for semantic errors when appropriate; if you find any bugs in the code I provided, please let me know and I will distribute a correction to the rest of the class.  In addition, you may find it desireable to change the code I have provided, for example for builtin identification or alias processing. To aid in this, the directory which includes the shell front-end also includes `tutorial.pdf`, a brisk tutorial on lex and yacc, as well as the original papers on lex and yacc.

## Additional Advice

To implement `ish`, you will be creating a process that forks off other processes, which in turn forks off more processes, etc. If you inadvertently fork too many processes, you will cause Unix to run out, making yourself and everyone else on the machine very unhappy. _Be careful about this._ I _strongly_ suggest that you use a desktop Linux machine for testing your program instead of a shared server, and that you limit the number of user processes that you can create using the `sh` shell command `ulimit -u _number_` or the `csh` shell command `limit maxproc _number_` In addition, I suggest you keep an extra shell window open so that you can use it to run `kill -9  -1` if you have a runaway program; thise  will kill all of your processes on the machine (including your login), allowing you to log back in and debug your program.

If you are in doubt about the functionality of `ish` or how it should behave in a particular situation, model the behavior on that of `csh`.  If you have specific questions about the project, post to the prog1 folder on the class Piazza.  A few final bits of advice. First, and most importantly, get started early; you almost certainly have a lot to learn before you can start implementing anything.  Second, once you have a good understanding of what you are being asked to do, I strongly suggest that you develop a detailed design, implementation, and testing plan.  My personal style is to get functionality working one step at a time, for example, processing of simple commands, then environment handling and PATH searching, then I/O redirection, then job control, and finally pipelines. Note, however, that getting, for example, job control, I/O redirection, and pipelines working together will require advance planning.
