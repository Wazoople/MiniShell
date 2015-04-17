#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#define MAXLINE 80
#define MAXARGS 20

pid_t pid = -1;

static void sig_Handler(int signum)
{
  if(pid == 0) exit(0);
}

void process_input(int argc, char **argv) {
  /* Problem 1: perform system call execvp to execute command     */
  /*            No special operator(s) detected                   */
  /* Hint: Please be sure to review execvp.c sample program       */
  /* if (........ == -1) {                                        */
  /*  perror("Shell Program");                                    */
  /*  _exit(-1);                                                  */
  /* }                                                            */
  /* Problem 2: Handle redirection operators: < , or  >, or both  */
  int count = argc;
  int openFlags = O_CREAT | O_WRONLY | O_TRUNC;
  int filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |
                 S_IROTH | S_IWOTH;
  char inFlag = 0;
  char outFlag = 0;

  /* Loop through arguments, detecting errors and redirecting as necessary */
  while (count > 0) {
    count--;

    if (!strcmp(argv[count], "<") || !strcmp(argv[count], ">")) {

      // No redirection symbols as argv[0]
      if(count == 0) {
        fprintf(stderr, "ERROR - No command.\n");
        exit(-1);
      }

      // All redirections must have a source or target
      if(count+1 >= argc) {
        fprintf(stderr, "ERROR - No redirection file specified\n");
        exit(-1);
      }
    }
    if (!strcmp(argv[count], "<"))
    {
      // Only one input redirection per command
      if(inFlag) {
        fprintf(stderr, "ERROR - Can't have two input redirects on one line.\n");
        exit(-1);
      }

      // Redirect input
      int newIn = open(argv[count+1], O_RDONLY);
      if(newIn == -1) {
        fprintf(stderr, "ERROR - Cannot open redirected input.\n");
        exit(-1);
      }
      else {
        dup2(newIn, 0);
        close(newIn);
        argv[count] = NULL;
      }
      inFlag = 1;
    }
    else if (!strcmp(argv[count], ">"))
    {

      // Only one output redirection per command
      if(outFlag) {
        fprintf(stderr, "ERROR - Can't have two output redirects on one line.\n");
        exit(-1);
      }

      // Redirect output
      int newOut = open(argv[count+1], openFlags, filePerms);
      if(newOut == -1) {
        fprintf(stderr, "ERROR - Cannot open redirected output.\n");
        exit(-1);
      }
      else {
        dup2(newOut, 1);
        close(newOut);
        argv[count] = NULL;
      }
      outFlag = 1;
    }
  }

  /* multipipe implementation */
  int pipeCount = 0;
  int pipePos = 0; //Position of previous pipe
  char **commands[MAXARGS];
  int pipefds[MAXARGS-1];

  /* parse argv into individual commands */
  for (count = 0; count < argc; count++) {
    if (!strcmp(argv[count], "|")) {
      commands[pipeCount] = argv + pipePos;
      commands[pipeCount][count - pipePos] = NULL;
      if( pipe(pipefds + pipeCount*2) < 0 ) {
        fprintf(stderr, "ERROR - Cannot open pipe %d", pipeCount);
        exit(-1);
      }
      pipeCount++;
      pipePos = count + 1;
    }
  }
  commands[pipeCount] = argv + pipePos;

  /* link a bunch of pipes together */
  int commandCount;
  for(commandCount = 0; commands[commandCount] != NULL; commandCount++) {

    int pipe_pid = fork();
      //printf("|");
      if( pipe_pid == 0 ) {
        write(2, "aaaaa", 5);
        // Open read feed so long as it is not the first command
        //printf("%s", commands[commandCount][0]);
        if( commandCount != 0 ) {
          if( dup2(pipefds[(commandCount-1)*2], 0) < 0 ) {
            fprintf(stderr, "ERROR - Pipe Redirection input");
            exit(-1);
          }
        }

        // Open write feed so long as it is no the final command
        if( commands[commandCount+1] != NULL ) {
          if(commandCount != 1) {
            printf("not last");
            if( dup2(pipefds[commandCount*2+1], 1) < 0 ) {
              fprintf(stderr, "ERROR - Pipe Redirection output");
              exit(-1);
            }
          }
        }
        // Close child's file descriptors 
        close(pipefds[(commandCount-1)*2]);
        close(pipefds[commandCount*2+1]);

        // Execute. I'm guessing this needs to go elsewhere?      
        if(execvp(commands[commandCount][0], commands[commandCount]) == -1) {
          fprintf(stderr, "ERROR - execvp failed (piped)");
          exit(-1);
        }
        exit(0);
      } else if (pid < 0) {
        fprintf(stderr, "ERROR - forking for pipe");
        exit(-1);
      }
      else {
        commandCount++;
      }
  }
  // Close parent's file descriptors
  for(count  = 0; count < 2*pipeCount; count++ ) {
    close( pipefds[count] );
  }
//  if (execvp(argv[0], argv) == -1) {
//    perror("Shell Program");
//      exit(-1);
//  } 
}

/* ----------------------------------------------------------------- */
/*                  parse input line into argc/argv format           */
/* ----------------------------------------------------------------- */
int parseline(char *cmdline, char **argv)
{
  int count = 0;
  char *separator = " \n\t";
  argv[count] = strtok(cmdline, separator);
  while ((argv[count] != NULL) && (count+1 < MAXARGS)) {
   argv[++count] = strtok((char *) 0, separator);
  }
  return count;
}
/* ----------------------------------------------------------------- */
/*                  The main program starts here                     */
/* ----------------------------------------------------------------- */
int main(void)
{
 struct sigaction parent_handler;
 parent_handler.sa_handler = SIG_IGN;
 sigemptyset(&parent_handler.sa_mask);
 parent_handler.sa_flags = 0;
 sigaction(SIGINT, &parent_handler, NULL);

 struct sigaction child_handler;
 child_handler.sa_handler = SIG_DFL;
 sigemptyset(&child_handler.sa_mask);
 child_handler.sa_flags = 0;

 char cmdline[MAXLINE];
 char *argv[MAXARGS];
 int argc;
 int status;

 /* Loop forever to wait and process commands */
 while (1) {
  // Display prompt and wait for a command 
  printf("csc60mshell> ");
  fgets(cmdline, MAXLINE, stdin);

  // Restart loop on carriage return
  argc = parseline(cmdline, argv);
  if (argc == 0) continue;

  // Handle exit command
  if (argc == 1 && !strcmp(argv[0], "exit")) { exit(0); }

  // Handle pwd command
  if (argc == 1 && !strcmp(argv[0], "pwd")) {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);
    continue;
  }

  // Handle cd command
  if (!strcmp(argv[0], "cd")) {

    // Check for acceptable argc
    if(argc > 2) fprintf(stderr, "ERROR - Too many arguments for cd command\n");

    // If there are no additional arguments, change to the home directory.
    else if(!argv[1]) chdir(getenv("HOME"));

    // Change to the argument directory, using a relative or absolute path
    else {
      char dir[1024] = { 0 };
      // Enable the relative path
      if(argv[1][0] != '/') {
        getcwd(dir, sizeof(dir));
        strcat(dir, "/");
      }
      strcat(dir, argv[1]);
      chdir(dir);
    }

    // Update the PWD environment variable
    char dir[1024];
    getcwd(dir, sizeof(dir));
    setenv("PWD", dir, 1);
    continue;
  }
  /* Step 1: Else, fork off a process */
  pid = fork();
  if (pid == -1)
    perror("Shell Program fork error");
  else if (pid == 0) {
    /* I am child process. I will execute the command, call: execvp */
    sigaction(SIGINT, &child_handler, NULL);
    process_input(argc, argv);
  }
  else
    /* I am parent process */
    if (wait(&status) == -1)
      perror("Shell Program error");
    if (WIFEXITED(status))     /* process exited normally */
     printf("child process exited with value %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))      /* child exited on a signal */
     printf("child process exited due to signal %d\n", WTERMSIG(status));
    else if (WIFSTOPPED(status))       /* child was stopped */
     printf("child process was stopped by signal %d\n", WIFSTOPPED(status));
 }
}
~                         