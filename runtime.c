/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
	#define __RUNTIME_IMPL__

  /************System include***********************************************/
	#include <assert.h>
	#include <errno.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <sys/wait.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <signal.h>

  /************Private include**********************************************/
	#include "runtime.h"
	#include "io.h"

  /************Defines and Typedefs*****************************************/
    /*  #defines and typedefs should have their names in all caps.
     *  Global variables begin with g. Global constants with k. Local
     *  variables should be in all lower case. When initializing
     *  structures and arrays, line everything up in neat columns.
     */
        #define MAXJOBS 64
        #define RUNNING 1
        #define STOPPED 2
        #define Done 3
 
        pid_t gfg;
//        pid_t gfproc;
        int gi = 1;
  /************Global Variables*********************************************/

	#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

	typedef struct bgjob_l {
		pid_t pid;
	        char* status;
		struct bgjob_l* next;
	        struct command_t* cmd;
	} bgjobL;

	/* the pids of the background processes */
	//bgjobL *bgjobs = NULL;
        bgjobL bgjobs[MAXJOBS];
        
  /************Function Prototypes******************************************/
	/* run command */
	static void RunCmdFork(commandT*, bool);
	/* runs an external program command after some checks */
	static void RunExternalCmd(commandT*, bool);
	/* resolves the path and checks for exutable flag */
	static bool ResolveExternalCmd(commandT*);
	/* forks and runs an external program */
	static void Exec(commandT*, bool);
	/* runs a builtin command */
	static void RunBuiltInCmd(commandT*);
	/* checks whether a command is a builtin command */
	static bool IsBuiltIn(char*);
  /************External Declaration*****************************************/

/**************Implementation***********************************************/
    int total_task;
	void RunCmd(commandT** cmd, int n)
	{
      int i;
      total_task = n;
      if(n == 1)
          RunCmdFork(cmd[0], TRUE);
      else{
        RunCmdPipe(cmd[0], cmd[1]);
        for(i = 0; i < n; i++)
          ReleaseCmdT(&cmd[i]);      
	   }
	}

	void RunCmdFork(commandT* cmd, bool fork)
	{
		if (cmd->argc<=0)
			return;
		if (IsBuiltIn(cmd->argv[0]))
		{
			RunBuiltInCmd(cmd);
		}
		else
		{
			RunExternalCmd(cmd, fork);
		}
	}

	void RunCmdBg(commandT* cmd)
	{
		// TODO
	  pid_t procbg = getpid();
	  //	  printf("getpid in RunCmdBg %d\n", procbg);
	  kill(procbg, SIGCONT);
	  printf("[%d] %d\n", gi, procbg);
	  exit(1);
	}

	void RunCmdPipe(commandT* cmd1, commandT* cmd2)
	{
	}

	void RunCmdRedirOut(commandT* cmd, char* file)
	{
	}

	void RunCmdRedirIn(commandT* cmd, char* file)
	{
	}


/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  if (ResolveExternalCmd(cmd)){
    Exec(cmd, fork);
  }
  else {
    printf("%s: command not found\n", cmd->argv[0]);
    fflush(stdout);
    ReleaseCmdT(&cmd);
  }
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd)
{
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;

  if(strchr(cmd->argv[0],'/') != NULL){
    if(stat(cmd->argv[0], &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
      }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)){
    c = strchr(&(pathlist[i]),':');
    if(c != NULL){
      for(j = 0; c != &(pathlist[i]); i++, j++)
	buf[j] = pathlist[i];
      i++;
    }
    else{
      for(j = 0; i < strlen(pathlist); i++, j++)
	buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    strcat(buf, "/");
    strcat(buf,cmd->argv[0]);
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
	if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
	  cmd->name = strdup(buf); 
	  return TRUE;
	}
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

	static void Exec(commandT* cmd, bool forceFork)
	{
	  pid_t proc = fork();
	  //	  printf("child pid in Exec %d", proc);
      	  int status;
	  if(proc > 0)
	    {
	      gfg = proc;
	      //printf("parent's GID is %d\n", getpgid(getpid()));
	      //setpgid(getpid(), 1);
	      waitpid(proc, &status, WUNTRACED);
	      //wait(&status);
	       if(WIFSTOPPED(status))
		{
		  cmd->bg = 1;
		  bgjobs[gi].status = "Stopped";
		  printf("[%d] Stopped                %s\n", gi, cmd->cmdline);
		}
	        if(cmd->bg)
		{
		  bgjobs[gi].pid = proc;
		  bgjobs[gi].cmd = cmd;
		  if(!bgjobs[gi].status)
		    {
		      bgjobs[gi].status = "Running";
		      strcat((bgjobs[gi].cmd)->cmdline, "&");
		    }
		  //printf("[%d]\t%d\t%s\n", gi, bgjobs[gi].pid, (bgjobs[gi].cmd)->cmdline);
		  if(bgjobs[gi-1].pid)
		    {
		      bgjobs[gi-1].next = bgjobs+gi;
		    }
		  gi++;
      		 }
	     }
	  else if(proc == 0)
	    {
	         if(cmd->bg) // background
		 {
		   RunCmdBg(cmd);
		   //exit(1);
		 }
		 else  // foreground
		 {
		   setpgid(0, 0);
		   //printf("child GID is %d\n", getpgid(getpid()));
		   //signal(SIGTTOU, SIG_IGN);
		   //tcsetpgrp(STDIN_FILENO, getpid());
		   const char* name = cmd->name;
		   char* const* argv = cmd->argv;
		   execv(name, argv);
		 }
	    }
	  else
	    {
	      printf("fork error");
	      exit(1);
	    }
	}

        static bool IsBuiltIn(char* cmd)
        {
	  if(strcmp(cmd, "bg")==0 || strcmp(cmd, "cd")==0 || strcmp(cmd, "jobs")==0 || strcmp(cmd, "fg")==0)
	        return TRUE;
	  else
        	return FALSE;     
        }

void RunBgJobs(int pnum)
{
     kill(bgjobs[pnum].pid, SIGCONT);
     bgjobs[pnum].status = "Running";
     strcat((bgjobs[pnum].cmd)->cmdline, " &");
     printf("[%d] %s\n", pnum, (bgjobs[pnum].cmd)->cmdline);
}
   
	static void RunBuiltInCmd(commandT* cmd)
	{
	  if(strcmp(cmd->argv[0], "bg")==0)
	    {
	      	if(cmd->argc == 1)
		{
		  int i;
		  int max = 0;
		  for(i=1;i<gi;i++)
		    {
		      if(strcmp(bgjobs[i].status,"Stopped")==0)
			max = i;
		    }
		  if(!max)
		    printf("bg: current: no such job\n");
		  else
		    {
		      RunBgJobs(max);
		    }
		}
		else
		  {
		    int pnum = atoi(cmd->argv[1]);
		    //printf("argc is %d\n", cmd->argc);
		    if(pnum >= gi)
		      printf("bg: %d: no such job\n", pnum);
		    else if(strcmp(bgjobs[pnum].status, "Running")==0)
		      printf("bg: job %d already in background\n", pnum);
		    else
		      {
			RunBgJobs(pnum);
		      }
		  }

	    }
	  else if(strcmp(cmd->argv[0], "cd")==0)
	    {
	      if(cmd->argc == 1)
		{
		  int errd = chdir(getenv("HOME"));
		  if(errd == -1)
		    printf("path not found");
		}
	      else
		{
		  int errd = chdir(cmd->argv[1]);
		  if(errd == -1)
		      printf("path not found");
	     	}
	    }
	  else if(strcmp(cmd->argv[0], "jobs")==0)
	    {
	      //printf("jobs\n");
	      int i;
	      for(i = 1;i < gi;i++)
		{
		  printf("[%d] %s               %s\n", i, bgjobs[i].status, (bgjobs[i].cmd)->cmdline);
		 }
	    }
	  else if(strcmp(cmd->argv[0], "fg")==0)
	    {
	      if(cmd->argc == 1)
		{
		}
	      else
		{
		  int pnum = atoi(cmd->argv[1]);
		  if(pnum >= gi)
		    printf("fg: %d: no such job\n", pnum);
		  else
		    {
		      int status;
		      gfg = bgjobs[pnum].pid;
		      waitpid(gfg, &status, WUNTRACED);
		    }
		}
	    }
	}


        void CheckJobs()
	{
 	}


commandT* CreateCmdT(int n)
{
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd){
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}

void StopFgProc()
{ 
  //pid_t child = getpid() + 2;
  kill(gfg, SIGTSTP);
  printf("\n");
 }

void IntFgProc()
{
  printf("\n");
  kill(-gfg, SIGINT);
}

void KillBG()
{
}
