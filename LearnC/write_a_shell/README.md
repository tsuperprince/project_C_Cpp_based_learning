# Tutorial - Write a Shell in C

## Basic lifetime of a shell

A shell does three main things in its lifetime.

- Initialize: In this step, a typical shell would read and execute its configuration files. These change aspects of the shell’s behavior.
- Interpret: Next, the shell reads commands from stdin (which could be interactive, or a file) and executes them.
- Terminate: After its commands are executed, the shell executes any shutdown commands, frees up any memory, and terminates.

In this shell, just call the looping function and then terrminate, not use any configuration files and not use any shutdown command. 

## Basic loop of a shell
a simple way to handle commands is with three steps:

- Read: Read the command from standard input.
- Parse: Separate the command string into a program and arguments.
- Execute: Run the parsed command.

```

void shell_loop(void)
{
  char *line;
  char **args;
  int status;

  do {
    printf("> ");
    line = shell_read_line();
    args = shell_split_line(line);
    status = shell_execute(args);

    free(line);
    free(args);
  } while (status);
}

```
Within the loop, we print a prompt, call a function to read a line, call a function to split the line into args, and execute the args. Finally, we free the line and arguments that we created earlier. Note that we’re using a status variable returned by shell_execute() to determine when to exit.

## Reading a line

```
char *shell_read_line(void)
{
  char *line = NULL;
  ssize_t bufsize = 0; // have getline allocate a buffer for us
  getline(&line, &bufsize, stdin);
  return line;
}
```

## Parsing the line

we need to parse that line into a list of arguments. I’m going to make a glaring simplification here, and say that we won’t allow quoting or backslash escaping in our command line arguments. Instead, we will simply use whitespace to separate arguments from each other. So the command echo "this message" would not call echo with a single argument this message, but rather it would call echo with two arguments: "this and message".

With those simplifications, all we need to do is “tokenize” the string using whitespace as delimiters. That means we can break out the classic library function strtok to do some of the dirty work for us.

```

#define shell_TOK_BUFSIZE 64
#define shell_TOK_DELIM " \t\r\n\a"
char **shell_split_line(char *line)
{
  int bufsize = shell_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "shell: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, shell_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += shell_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "shell: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, shell_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}
```

At the start of the function, we begin tokenizing by calling strtok. It returns a pointer to the first token. What strtok() actually does is return pointers to within the string you give it, and place \0 bytes at the end of each token. We store each pointer in an array (buffer) of character pointers.

Finally, we reallocate the array of pointers if necessary. The process repeats until no token is returned by strtok, at which point we null-terminate the list of tokens.

## How shells start processes
Now, we’re really at the heart of what a shell does. Starting processes is the main function of shells. So writing a shell means that you need to know exactly what’s going on with processes and how they start. That’s why I’m going to take us on a short diversion to discuss processes in Unix

There are only two ways of starting processes on Unix. The first one (which almost doesn’t count) is by being Init. You see, when a Unix computer boots, its kernel is loaded. Once it is loaded and initialized, the kernel starts only one process, which is called Init. This process runs for the entire length of time that the computer is on, and it manages loading up the rest of the processes that you need for your computer to be useful.

Since most programs aren’t Init, that leaves only one practical way for processes to get started: the fork() system call. When this function is called, the operating system makes a duplicate of the process and starts them both running. The original process is called the “parent”, and the new one is called the “child”. fork() returns 0 to the child process, and it returns to the parent the process ID number (PID) of its child. In essence, this means that the only way for new processes is to start is by an existing one duplicating itself.

This might sound like a problem. Typically, when you want to run a new process, you don’t just want another copy of the same program – you want to run a different program. That’s what the exec() system call is all about. It replaces the current running program with an entirely new one. This means that when you call exec, the operating system stops your process, loads up the new program, and starts that one in its place. A process never returns from an exec() call (unless there’s an error).

With these two system calls, we have the building blocks for how most programs are run on Unix. First, an existing process forks itself into two separate ones. Then, the child uses exec() to replace itself with a new program. The parent process can continue doing other things, and it can even keep tabs on its children, using the system call wait().

In the child process, we want to run the command given by the user. So, we use one of the many variants of the exec system call, execvp. The different variants of exec do slightly different things. Some take a variable number of string arguments. Others take a list of strings. Still others let you specify the environment that the process runs with. This particular variant expects a program name and an array (also called a vector, hence the ‘v’) of string arguments (the first one has to be the program name). The ‘p’ means that instead of providing the full file path of the program to run, we’re going to give its name, and let the operating system search for the program in the path.

If the exec command returns -1 (or actually, if it returns at all), we know there was an error. So, we use perror to print the system’s error message, along with our program name, so users know where the error came from. Then, we exit so that the shell can keep running.

The second condition (pid < 0) checks whether fork() had an error. If so, we print it and keep going – there’s no handling that error beyond telling the user and letting them decide if they need to quit.

The third condition means that fork() executed successfully. The parent process will land here. We know that the child is going to execute the process, so the parent needs to wait for the command to finish running. We use waitpid() to wait for the process’s state to change. Unfortunately, waitpid() has a lot of options (like exec()). Processes can change state in lots of ways, and not all of them mean that the process has ended. A process can either exit (normally, or with an error code), or it can be killed by a signal. So, we use the macros provided with waitpid() to wait until either the processes are exited or killed. Then, the function finally returns a 1, as a signal to the calling function that we should prompt for input again.


## Shell Builtins 
There are three parts to this code. The first part contains forward declarations of my functions. A forward declaration is when you declare (but don’t define) something, so that you can use its name before you define it. The reason I do this is because shell_help() uses the array of builtins, and the arrays contain shell_help(). The cleanest way to break this dependency cycle is by forward declaration.

The next part is an array of builtin command names, followed by an array of their corresponding functions. This is so that, in the future, builtin commands can be added simply by modifying these arrays, rather than editing a large “switch” statement somewhere in the code. If you’re confused by the declaration of builtin_func, that’s OK! I am too. It’s an array of function pointers (that take array of strings and return an int). Any declaration involving function pointers in C can get really complicated. I still look up how function pointers are declared myself!

Finally, I implement each function. The shell_cd() function first checks that its second argument exists, and prints an error message if it doesn’t. Then, it calls chdir(), checks for errors, and returns. The help function prints a nice message and the names of all the builtins. And the exit function returns 0, as a signal for the command loop to terminate.


## Putting it all together

hat’s all the code that goes into the shell. If you’ve read along, you should understand completely how the shell works. To try it out (on a Linux machine), you would need to copy these code segments into a file (main.c), and compile it. Make sure to only include one implementation of lsh_read_line(). You’ll need to include the following headers at the top. I’ve added notes so that you know where each function comes from.
```
#include <sys/wait.h>
  waitpid() and associated macros
#include <unistd.h>
  chdir()
  fork()
  exec()
  pid_t
#include <stdlib.h>
  malloc()
  realloc()
  free()
  exit()
  execvp()
  EXIT_SUCCESS, EXIT_FAILURE
#include <stdio.h>
  fprintf()
  printf()
  stderr
  getchar()
  perror()
#include <string.h>
  strcmp()
  strtok()
```
Once you have the code and headers, it should be as simple as running gcc -o main main.c to compile it, and then ./main to run it.
