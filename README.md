# MyShell (mysh)

## Overview
MyShell (mysh) is a simple shell implemented in C. It supports basic shell functionalities, including command execution, input/output redirection, piping, and process control. The shell allows users to interact with the operating system through a command-line interface.

## Features
- **Command Execution:** Execute standard Unix commands.
- **Input Redirection:** Redirect the input of a command from a file using `<`.
- **Output Redirection:** Redirect the output of a command to a file using `>` and `>>` for appending.
- **Piping:** Pass the output of one command as the input to another using `|`.
- **Background Execution:** Run commands in the background using `&`.
- **Error Handling:** Comprehensive error messages for various command syntax errors.

## Usage
- **Start the Shell:**
  ```bash
  ./mysh [prompt]
