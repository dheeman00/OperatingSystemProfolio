#!/bin/sh
if (grep 'getenv(' *)
then
	/bin/false
else
	echo Found potential usage of getenv in source code!!!
fi

if grep 'system(' *
then
	/bin/false
else
	echo Found potential usage of system in source code!!!
fi

if grep 'execve' *
then
	echo Did not find usage of execve in source code!!!
fi
