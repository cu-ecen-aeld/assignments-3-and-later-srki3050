# aesd-assignments
This repo contains public starter source code, scripts, and documentation for Advanced Embedded Software Development (ECEN-5713) and Advanced Embedded Linux Development assignments University of Colorado, Boulder.

## Setting Up Git

Use the instructions at [Setup Git](https://help.github.com/en/articles/set-up-git) to perform initial git setup steps. For AESD you will want to perform these steps inside your Linux host virtual or physical machine, since this is where you will be doing your development work.

## Setting up SSH keys

See instructions in [Setting-up-SSH-Access-To-your-Repo](https://github.com/cu-ecen-aeld/aesd-assignments/wiki/Setting-up-SSH-Access-To-your-Repo) for details.

## Specific Assignment Instructions

Some assignments require further setup to pull in example code or make other changes to your repository before starting.  In this case, see the github classroom assignment start instructions linked from the assignment document for details about how to use this repository.

## Testing

The basis of the automated test implementation for this repository comes from [https://github.com/cu-ecen-aeld/assignment-autotest/](https://github.com/cu-ecen-aeld/assignment-autotest/)

The assignment-autotest directory contains scripts useful for automated testing  Use
```
git submodule init update --recursive
```
to synchronize after cloning and before starting each assignment, as discussed in the assignment instructions.

As a part of the assignment instructions, you will setup your assignment repo to perform automated testing using github actions.  See [this page](https://github.com/cu-ecen-aeld/aesd-assignments/wiki/Setting-up-Github-Actions) for details.

Note that the unit tests will fail on this repository, since assignments are not yet implemented.  That's your job :) 

## Additional Notes (By Sricharan - In assignment 2)

Commands entered to get the file details in files located in
assignment-2-srki3050/assignments/assignment-2

cross-compiler.txt

1 Print the sysroot command of the installed ARM compiler
	aarch-64-none-linux-gnu-gcc -print-sysroot &> //home/sricharan/Documents/AESD/Assignments/assignment-1-srki3050/assignments/assignment2/cross-compiler.txt
2 Print the Version of the ARM compiler
	aarch-64-none-linux-gnu-gcc -v &> //home/sricharan/Documents/AESD/Assignments/assignment-1-srki3050/assignments/assignment2/cross-compiler.txt

fileresult.txt
Print the ARM compiled output to
	file writer &> //home/sricharan/Documents/AESD/Assignments/assignment-1-srki3050/assignments/assignment2/fileresults.txt
	
# Notes for Assignment 7

Perform user space implementation of a circular buffer

Without Designing Circular buffer in user space in case you directly implement it on the kernel, kernel debugging is a difficult task.

The main structure has
another structure as a member which by itself contains
	a pointer to the current element in circular buffer
	the size of the current circular buffer
the current write location
the current read location
A checker to check if the buffer is full

Functions required
aesd_circular_buffer_init - Initialize the circular buffer to empty in the beginning
aesd_circular_buffer_add_entry - Add an entry to the circular buffer
aesd_circular_buffer_find_entry_offset_for_fpos - find the current offset location to write. 
