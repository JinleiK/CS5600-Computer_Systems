/*
 * file:        homework.c
 * description: Skeleton for homework 1
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, Jan. 2012
 * $Id: homework.c 500 2012-01-15 16:15:23Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uprog.h"

/***********************************/
/* Declarations for code in misc.c */
/***********************************/

typedef int *stack_ptr_t;
extern void init_memory(void);
extern void do_switch(stack_ptr_t *location_for_old_sp, stack_ptr_t new_value);
extern stack_ptr_t setup_stack(int *stack, void *func);
extern int get_term_input(char *buf, size_t len);
extern void init_terms(void);

extern void  *proc1;
extern void  *proc1_stack;
extern void  *proc2;
extern void  *proc2_stack;
extern void **vector;


/***********************************************/
/********* Your code starts here ***************/
/***********************************************/

/* Global Variables */
/*q2*/
#define INPUT_MAX_LEN 128
char argv[10][INPUT_MAX_LEN];   //to store the arguments of the command
int argv_num = 0;               //number of arguments 
char cmd[INPUT_MAX_LEN];        //to store the input command
/*q3*/
stack_ptr_t pro1_sp		= NULL;	//Store proc1 context
stack_ptr_t pro2_sp		= NULL;	//Store proc2 context
stack_ptr_t stack_sp	= NULL;	//Used when return to main
stack_ptr_t main_sp		= NULL;	//Store main context
/*
 * Question 1.
 *
 * The micro-program q1prog.c has already been written, and uses the
 * 'print' micro-system-call (index 0 in the vector table) to print
 * out "Hello world".
 *
 * You'll need to write the (very simple) print() function below, and
 * then put a pointer to it in vector[0].
 *
 * Then you read the micro-program 'q1prog' into memory starting at
 * address 'proc1', and execute it, printing "Hello world".
 *
 */
void print(char *line)
{
    /*
     * Your code goes here. 
     */
     printf("%s", line);
}

int readfile(char *file_name, char *buf)
{
    FILE *fp = fopen(file_name, "r");
    if(fp == NULL)
    {
        perror("Cannot open file!");
        return 0;
    }
    int i = 0;
    int c = getc(fp);
    while(c != EOF)
    {
        buf[i ++] = c;
        c = getc(fp);
    }
    fclose(fp);
    return 1;
}

void q1(void)
{
    /*
     * Your code goes here. Initialize the vector table, load the
     * code, and go.
     */
    vector[0] = &print;
    int (*printStr)(void) = NULL;
    int suc = readfile("q1prog", proc1);    //read file("q1prog") into memory
    //if reading file succeeded, let printStr point to proc1, and invoke printStr
    if(suc != 0)        
    {
        printStr = proc1;
        printStr();
    }
    return;
}


/*
 * Question 2.
 *
 * Add two more functions to the vector table:
 *   void readline(char *buf, int len) - read a line of input into 'buf'
 *   char *getarg(int i) - gets the i'th argument (see below)

 * Write a simple command line which prints a prompt and reads command
 * lines of the form 'cmd arg1 arg2 ...'. For each command line:
 *   - save arg1, arg2, ... in a location where they can be retrieved
 *     by 'getarg'
 *   - load and run the micro-program named by 'cmd'
 *   - if the command is "quit", then exit rather than running anything
 *
 * Note that this should be a general command line, allowing you to
 * execute arbitrary commands that you may not have written yet. You
 * are provided with a command that will work with this - 'q2prog',
 * which is a simple version of the 'grep' command.
 *
 * NOTE - your vector assignments have to mirror the ones in vector.s:
 *   0 = print
 *   1 = readline
 *   2 = getarg
 */
void readline(char *buf, int len) /* vector index = 1 */
{
    /*
     * Your code here.
     */
    fgets(buf, len, stdin);
    int buflen = strlen(buf);
    buf[buflen - 1] = '\0';
}

char *getarg(int i)		/* vector index = 2 */
{
    /*
     * Your code here. 
     */
    if(i < argv_num && *argv[i] != '\0')
        return argv[i];
    return NULL;
}

int splitWords(char *line)
{
    char *words = strtok(line, " ");
    int count = 0;
    while(words != NULL){
        if(count == 0)
            strcpy(cmd, words);
        else{
            strcpy(argv[count - 1], words);
            argv_num ++;
        }
        count ++;

        words = strtok(NULL, " ");
    }
    return count;
}

/*
 * Note - see c-programming.pdf for sample code to split a line into
 * separate tokens. 
 */
void q2(void)
{
    /* Your code goes here */
    vector[0] = &print;
    vector[1] = &readline;
    vector[2] = &getarg;

    char *line;
    line = malloc(sizeof(*line) * INPUT_MAX_LEN);
    int word_num = 0;
    int suc = 0;
    void (*runcmd)(void);
    while (1) {
	/* get a line */
        printf("> ");
        readline(line, INPUT_MAX_LEN);
	/* split it into words */
        word_num = splitWords(line);
	/* if zero words, continue */
        if(word_num == 0)
            continue;
	/* if first word is "quit", break */
	if(strcmp(cmd, "quit") == 0)
            break;
	/* make sure 'getarg' can find the remaining words */
	/* load and run the command */
	else 
	{
        suc = readfile(cmd, proc2);
        if(suc != 0)
	    {
           	runcmd = proc2;
           	runcmd();
        }
        else
	    {
            perror("can't find and load the command!");
        }
	}
      
    }
    /*
     * Note that you should allow the user to load an arbitrary command,
     * and print an error if you can't find and load the command binary.
     */
     free(line);
     return ;
}

/*
 * Question 3.
 *
 * Create two processes which switch back and forth.
 *
 * You will need to add another 3 functions to the table:
 *   void yield12(void) - save process 1, switch to process 2
 *   void yield21(void) - save process 2, switch to process 1
 *   void uexit(void) - return to original homework.c stack
 *
 * The code for this question will load 2 micro-programs, q3prog1 and
 * q3prog2, which are provided and merely consists of interleaved
 * calls to yield12() or yield21() and print(), finishing with uexit().
 *
 * Hints:
 * - Use setup_stack() to set up the stack for each process. It returns
 *   a stack pointer value which you can switch to.
 * - you need a global variable for each process to store its context
 *   (i.e. stack pointer)
 * - To start you use do_switch() to switch to the stack pointer for 
 *   process 1
 */

void yield12(void)		/* vector index = 3 */
{
    /* Your code here */
	do_switch(&pro1_sp, pro2_sp);
}

void yield21(void)		/* vector index = 4 */
{
    /* Your code here */
	do_switch(&pro2_sp, pro1_sp);
}

void uexit(void)		/* vector index = 5 */
{
    /* Your code here */
	do_switch(NULL, main_sp);
}

void q3(void)
{
    /* Your code here */ 
	// declear the temp var
	int result = 0;

	/* load q3prog1 into process 1 and q3prog2 into process 2 */
	// load file
	result = readfile("q3prog1", proc1);
	if (result == 0)
	{
		perror("Faild load the file: q3prog1");
		return;
	}
	result = readfile("q3prog2", proc2);
	if (result == 0)
	{
		perror("Faild load the file: q3prog2");
		return;
	}

	//re-define syscall table
	vector[0] = &print;
	vector[1] = &readline;
	vector[2] = &getarg;
	vector[3] = &yield12;
	vector[4] = &yield21;
	vector[5] = &uexit;

	//Setup stack for proc1 and proc2
	pro1_sp = setup_stack(proc1_stack, proc1);
	pro2_sp = setup_stack(proc2_stack, proc2);

	// Store current stack into main_sp and call proc1
	do_switch(&main_sp, pro1_sp);

	//exit
	uexit();
	return;
}


/***********************************************/
/*********** Your code ends here ***************/
/***********************************************/
