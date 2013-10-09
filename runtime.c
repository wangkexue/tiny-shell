/***************************************************************************
 *  Title:  
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
//#include <stdbool.h>

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
//      #define RUNNING 1
//      #define STOPPED 2
//      #define Done 3
 
        pid_t gfg; // foreground job
        commandT* gfgcmd;
        int gi = 1;
        int BGtoFG;
  /************Global Variables*********************************************/

	#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

	typedef struct bgjob_l {
		pid_t pid;
	        int jid;
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
        /* get job from bgjobs by pid */
        static bgjobL* Getjob(pid_t pid);
        /*Add a job to bgjobs*/
        static void Addjob(pid_t pid, commandT* cmd, char* status);
        /* Delete a Background job when it is finished or back to foreground*/
        static void Deletejob(pid_t pid, char* status);
        /* find whether certain job is in background*/
        static bool IsBgjob(int i);
        /* find the latest stopped or running job*/
        static int FindLatestjob();
        /* bring Background job bakc to Foreground*/
        static void Bg2Fg(int pnum);
  /************External Declaration*****************************************/

/**************Implementation***********************************************/
    int total_task;
    void RunCmd(commandT** cmd, int n)
    {
      int i;
      total_task = n;
      if(n == 1)
	{
	  if(cmd[0]->is_redirect_in)
	    RunCmdRedirIn(cmd[0], cmd[0]->redirect_in);
	  else if(cmd[0]->is_redirect_out)
	    RunCmdRedirOut(cmd[0], cmd[0]->redirect_out);
	  else
	    RunCmdFork(cmd[0], TRUE);
	}
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
	  // exit(NULL);
	}

	void RunCmdPipe(commandT* cmd1, commandT* cmd2)
	{
	}

	void RunCmdRedirOut(commandT* cmd, char* file)
	{
	  char* cmdRedir = strrchr(cmd->cmdline,'>');
	  *cmdRedir = '\0';
	  // printf("%s\n", cmd->cmdline);
	  mode_t filemode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	  int stdout = dup(STDOUT_FILENO);
	  int fd = open(file, O_WRONLY | O_CREAT, filemode);
	  if(fd == -1)
	    perror("open() error");
	  dup2(fd, 1);
	  close(fd);
	  RunExternalCmd(cmd, TRUE);
	  dup2(stdout, 1);
	}

	void RunCmdRedirIn(commandT* cmd, char* file)
	{
	  char* cmdredir = strrchr(cmd->cmdline, '<');
	  *cmdredir = '\0';
	  int stdin = dup(STDIN_FILENO);
	  int fd = open(file, O_RDONLY);
	  if(fd == -1)
	    perror("open() error");
	  dup2(fd, 0);
	  close(fd);
	  RunExternalCmd(cmd, TRUE);
	  dup2(stdin, 0);
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
	  sigset_t sigset;
	  sigemptyset(&sigset);
	  sigaddset(&sigset, SIGCHLD);
	  sigprocmask(SIG_BLOCK, &sigset, NULL);

	  pid_t proc;
	  int status;
	  //	  printf("child pid in Exec %d", proc);
      	  // int status;
	  if((proc = fork()) > 0)
	    {
	      //gfg = proc;
	      //printf("parent's GID is %d\n", getpgid(getpid()));
	      //setpgid(getpid(), 1);
	      if(!cmd->bg)
		{
		  gfg = proc;
		  /*
		  if(gfg>0)
		    printf("%d now running\n", gfg);
		  */
		  gfgcmd = cmd;
		  waitpid(gfg, &status, WNOHANG | WUNTRACED);
		  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
		  // printf("status:%d\tWIFSTOPPED:%d\tgfg:%d\n", status, WIFSTOPPED(status), gfg);
		  while(gfg > 0)
		    {
		      sleep(1);
		    }
		  // printf("%d", status);
		  /*
		  if(WIFSTOPPED(status))
		   {
		   }
		  */
		}
	        else
		{
		  waitpid(proc, &status, WNOHANG | WUNTRACED);
		  Addjob(proc, cmd, "Running");
		  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
      		 }
	     }
	  else if(proc == 0)
	    {
	         if(cmd->bg) // background
		 {
		   RunCmdBg(cmd);
		 }
		 else  // foreground
		 {
		   setpgid(0, 0);
		 }
		 const char* name = cmd->name;
		 char* const* argv = cmd->argv;
		 sigprocmask(SIG_UNBLOCK, &sigset, NULL);
		 execv(name, argv);
	    }
	  else
	    {
	      printf("fork error");
	      //exit(1);
	    }
	}

        static bool IsBuiltIn(char* cmd)
        {
	  if(strcmp(cmd, "bg")==0 || strcmp(cmd, "cd")==0 || strcmp(cmd, "jobs")==0 || strcmp(cmd, "fg")==0 || strcmp(cmd,"alias")==0)
	        return TRUE;
	  else
        	return FALSE;     
        }

void RunBgJobs(int pnum)
{
     kill(bgjobs[pnum].pid, SIGCONT);
     bgjobs[pnum].status = "Running";
     int len = strlen((bgjobs[pnum].cmd)->cmdline);
     int l = len;
     while(((bgjobs[pnum].cmd->cmdline)[--l])==' ')
       {
       }
     ((bgjobs[pnum].cmd)->cmdline)[l+1] = ' ';
     ((bgjobs[pnum].cmd)->cmdline)[l+2] = '&';
     ((bgjobs[pnum].cmd)->cmdline)[l+3] = '\0';

     //printf("[%d] %s\n", pnum, (bgjobs[pnum].cmd)->cmdline);
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
		    if(pnum >= gi || strcmp(bgjobs[pnum].status, "Inactive")==0)
		      printf("bg: %d: no such job\n", pnum);
		    else if(strcmp(bgjobs[pnum].status, "Done")==0)
		      {
			printf("bg: job has terminated\n");
			CheckJobs(pnum);
		      }
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
		    printf("path not found\n");
		}
	      else
		{
		  int errd = chdir(cmd->argv[1]);
		  if(errd == -1)
		    printf("cd: %s: No such file or directory\n", cmd->argv[1]);
	     	}
	    }
	  else if(strcmp(cmd->argv[0], "jobs")==0)
	    {
	      //printf("jobs\n");
	      int i;
	      for(i = 1;i < gi;i++)
		{ 
		  if(IsBgjob(i))
		    {
		      printf("[%d]   %s                 %s\n", i, bgjobs[i].status, (bgjobs[i].cmd)->cmdline);
		      continue;
		    }
		  if(strcmp(bgjobs[i].status, "Done")==0)
		    CheckJobs(i);
		 }
	    }
	  else if(strcmp(cmd->argv[0], "fg")==0)
	    {
	      
	      sigset_t sigset;
	      sigemptyset(&sigset);
	      sigaddset(&sigset, SIGCHLD);
	      sigprocmask(SIG_BLOCK, &sigset, NULL);
	      
	      if(cmd->argc == 1)
		{
		  int latestjob = FindLatestjob();
		  //		  printf("latestjob is %d\n", latestjob);
		  if(!latestjob)
		    printf("fg: current: no such job\n");
		  else
		    {
		      Bg2Fg(latestjob);
		      //printf("success\n");
		      sigprocmask(SIG_UNBLOCK, &sigset, NULL);
		      while(gfg>0)
			sleep(1);
		    }
		}
	      else
		{
		  int pnum = atoi(cmd->argv[1]);
		  if(pnum >= gi || strcmp(bgjobs[pnum].status, "Inactive")==0)
		    printf("fg: %d: no such job\n", pnum);
		  else if(strcmp(bgjobs[pnum].status, "Done")==0)
		    {
		      printf("fg: job has terminated\n");
		      //		      CheckJobs(pnum);
		    }
		  else
		    {
		      Bg2Fg(pnum);
		      sigprocmask(SIG_UNBLOCK, &sigset, NULL);
		      while(gfg > 0)
			sleep(1);
		    }
		}
	    }
	  else if(strcmp(cmd->argv[0], "alias") == 0)
	    {
	    }
	}
static void Bg2Fg(int pnum)
{
  //printf("%d\n", bgjobs[pnum].pid);
  if(strcmp(bgjobs[pnum].status, "Stopped")==0)
    kill(bgjobs[pnum].pid, SIGCONT);
  
  if(strrchr((bgjobs[pnum].cmd)->cmdline, '&'))
    {
      int len = strlen((bgjobs[pnum].cmd)->cmdline);
      ((bgjobs[pnum].cmd)->cmdline)[len-2]='\0';
    }
  gfg = bgjobs[pnum].pid;
  gfgcmd = bgjobs[pnum].cmd;
  bgjobs[pnum].status = "FG";
  BGtoFG = bgjobs[pnum].jid;
}

        void CheckJobs()
	{
	  int i;
	  for(i=1;i<gi;i++)
	    {
	      if(strcmp(bgjobs[i].status, "Done")==0)
		{
		  printf("[%d]   %s                 %s\n", i, bgjobs[i].status, (bgjobs[i].cmd)->cmdline);
		 }
	    }
	  //printf("%d", gi);
	  int j = gi;
	  if(gi == 1)
	    return;
	  while(strcmp(bgjobs[--j].status, "Inactive")==0)
	    {
	      if(j==1)
		gi = 1;
		break;
	    }
	  bgjobs[i].status = "Inactive";
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

static int FindLatestjob()
{
  int fi = gi;
  while(bgjobs[--fi].pid)
    {
      if(strcmp(bgjobs[fi].status, "Stopped")==0)
	break;
    }
  // printf("fi for Stopped job is %d\n", fi);
  if(!fi)
    {
      fi = gi;
      while(bgjobs[--fi].pid)
	{
	  if(strcmp(bgjobs[fi].status, "Running")==0)
	    break;
	}
    }
  return fi;
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
  if(!gfg)
    return;
  kill(gfg, SIGTSTP);
  printf("\n");
  if(BGtoFG)
    {
      printf("[%d]   Stopped                 %s\n", BGtoFG, gfgcmd->cmdline);
      bgjobs[BGtoFG].status = "Stopped";
      BGtoFG = 0;
    }
  else
    Addjob(gfg, gfgcmd, "Stopped");
  
  gfg = 0;
 }

void IntFgProc()
{
  if(!gfg)
    return;
  printf("\n");
  kill(-gfg, SIGINT);
  if(BGtoFG)
    {
      bgjobs[BGtoFG].status = "Inactive";
      BGtoFG = 0;
    }
  gfg = 0;
}

void SigchldHandler()
{
  pid_t child;
  int status;
  child = waitpid(-1, &status, WUNTRACED | WNOHANG);
  if(!WIFEXITED(status))    // ignore SIGTSTP & SIGINT
    return;
  if(WIFCONTINUED(status))
    return;
  //printf("I'm stopped\n");
  if(child == gfg)
    {
      // printf("%d finished\n", gfg);
      gfg = 0;
      return;
    }
  //bgjobL *bgjob = Getjob(child);
  //bgjob->status = "Done";
  //
  Deletejob(child, "Done");
}

static void Addjob(pid_t pid, commandT* cmd, char* status)
{
  //printf("%d", gi);
  /*
  if(gi>1)
    {
      bgjobL* job = NULL;
      if(Getjob(pid))
	{
	if(strcmp(job->status, "FG")==0)
	  {
	    job->status = status;
	    printf("[%d] Stopped                %s\n", job->jid, job->cmd->cmdline);
	    return;
	  }
	}
    }
  */
  bgjobs[gi].pid = pid;
  bgjobs[gi].cmd = cmd;
  bgjobs[gi].status = status;
  bgjobs[gi].jid = gi;
  if(strcmp(status, "Running")==0)
    strcat((bgjobs[gi].cmd)->cmdline, "&");
  int fi = gi;  
  while(bgjobs[--fi].pid)
    {
      if(strcmp(bgjobs[fi].status, "Running")||strcmp(bgjobs[fi].status, "Running"))
	{
	  bgjobs[fi].next = bgjobs + gi;
	  break;
	}
    }
  if(strcmp(bgjobs[gi].status, "Stopped")==0)
    printf("[%d] Stopped                %s\n", gi, gfgcmd->cmdline);
  else if(strcmp(bgjobs[gi].status, "Running")==0)
    printf("[%d] %d\n", gi, bgjobs[gi].pid);
  gi++;
}

static void Deletejob(pid_t pid, char* status)
{
  bgjobL* job;
  //printf("Done\n");
  
  if((job = Getjob(pid)))
    {
      //if(strcmp(job->status,"FG")!=0)
      //job->status = "Inactive";
      //else
	job->status = status;
    }
  //printf("%d\n", Getjob(pid));
  
}

static bgjobL* Getjob(pid_t pid)
{
  int i;
  /*
  if(gi==2)
    if(bgjobs[gi].pid == pid)
      {
	bgjobL* job;
	job = bgjobs + gi;
	return job;
	}*/
  for(i=1;i<gi;i++)
    {
      if(bgjobs[i].pid == pid)
	{
	  bgjobL *job;
	  job = bgjobs + i;
	  return job;
	}
    }
  return NULL;
}

static bool IsBgjob(int i)
{
  if(strcmp(bgjobs[i].status, "Running")==0 || strcmp(bgjobs[i].status, "Stopped")==0)
    return TRUE;
  return FALSE;
}

void KillBG()
{
  int i;
  for(i=1;i<gi;i++)
    {
      if(IsBgjob(i))
	kill(bgjobs[i].pid, SIGINT);
    }
}
