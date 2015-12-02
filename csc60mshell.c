#include <stdlib.h>
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

/* ----------------------------------------------------------------- */
/*      Iterate through commands - Handle pipe and redirect          */
/* ----------------------------------------------------------------- */
void process_input(int argc, char **argv) {
  int count = argc;
  int openFlags = O_CREAT | O_WRONLY | O_TRUNC;
  int filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  char inFlag = 0;
  char outFlag = 0; 
  int fd_out = fileno(stdout);
  int fd_in = fileno(stdin);
  /* Loop through arguments, detecting errors and redirecting as necessary */
  while (count > 0) {
    count--;
    
    if (!strcmp(argv[count], "<") || !strcmp(argv[count], ">")) {

      // No redirection symbols as argv[0]
      if(count == 0) {
        perror("ERROR - No command.\n");
        _exit(EXIT_FAILURE);
      }
      
      // All redirections must have a source or target
      if(count+1 >= argc) {
        perror("ERROR - No redirection file specified\n");
        _exit(EXIT_FAILURE);
      }
    }
    if (!strcmp(argv[count], "<")) 
    { 
      // Only one input redirection per command
      if(inFlag) {
        perror("ERROR - Can't have two input redirects on one line.\n");
        _exit(EXIT_FAILURE);
      }

      // Redirect input
      int newIn = open(argv[count+1], O_RDONLY);
      if(newIn == -1) { 
        perror("ERROR - Cannot open input redirection.\n");
        _exit(EXIT_FAILURE);
      }
      else {
        fd_in = dup(newIn);
        close(newIn);
        argv[count] = NULL;
        argc = count;
      }
      inFlag = 1;
    }
    else if (!strcmp(argv[count], ">")) 
    { 

      // Only one output redirection per command
      if(outFlag) {
        perror("ERROR - Can't have two output redirects on one line.\n");
        _exit(EXIT_FAILURE);
      }

      // Redirect output
      int newOut = open(argv[count+1], openFlags, filePerms);
      if(newOut == -1) {
        perror("ERROR - Cannot open redirected output.\n");
        _exit(EXIT_FAILURE);
      }
      else {
        fd_out = dup(newOut);
        close(newOut);
        argv[count] = NULL;
        argc = count;
      }
      outFlag = 1;
    } 
  }

  /* multipipe implementation */
  int pipeCount = 0;
  int pipePos = 0; //Position of previous pipe
  char **commands[MAXARGS];
  int pipefds[MAXARGS-1];
  memset(&commands[0], 0, sizeof(commands));
  /* parse argv into individual commands */
  for (count = 0; count < argc; count++) {
    if (!strcmp(argv[count], "|")) {
      commands[pipeCount] = argv + pipePos;
      commands[pipeCount][count - pipePos] = NULL; 
      pipeCount++;
      pipePos = count + 1;
    }
  }
  commands[pipeCount] = argv + pipePos;

  int i = 0;
  int p[2];
  pid_t pid;
  while (commands[i])
    {
      pipe(p);
      if ((pid = fork()) == -1)
        {
          perror("ERROR - Pipe fork failed");
          exit(EXIT_FAILURE);
        }
      else if (pid == 0)
        {
          dup2(fd_in, 0); //change the input according to the old one 
          if (commands[i+1])
            dup2(p[1], 1);
          else 
            dup2(fd_out, 1);
          close(p[0]);
          execvp(commands[i][0], commands[i]);
          perror("ERROR - Execution failure");
          exit(EXIT_FAILURE); 
        }
      else
        {
          wait(NULL);
          close(p[1]);
          fd_in = p[0]; //save the input for the next command
          i++;
        }
    }
  exit(EXIT_SUCCESS);  
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
    if(argc > 2) perror("ERROR - Too many arguments for cd command\n");
    
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
  if (pid == -1) { 
    perror("Shell Program fork error");
    _exit(EXIT_FAILURE);
  }
  else if (pid == 0) { 
    /* I am child process. I will execute the command, call: execvp */
    sigaction(SIGINT, &child_handler, NULL);
    process_input(argc, argv);
  }
  else { 
    /* I am parent process */
    if (wait(&status) == -1)
      perror("ERROR - Child return error");
    if (WIFEXITED(status))     /* process exited normally */
     printf("child process exited with value %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))      /* child exited on a signal */
     printf("child process exited due to signal %d\n", WTERMSIG(status));
    else if (WIFSTOPPED(status))       /* child was stopped */
     printf("child process was stopped by signal %d\n", WIFSTOPPED(status));
  }
 }
}
