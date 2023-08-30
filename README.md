# Wordle as seen on www.nytimes.com/games/wordle


High Level Overview: 

This application is a single-process multi-threaded TCP server for the Wordle word game. It uses POSIX threads to implement a TCP server
that handles multiple client connections in parallel. Specifically, my top-level main thread blocks on the accept() system call, listening 
on the port number specified as a command-line argument. For each connection request received by my server, a child thread will be created
to handle that specific connection. Once a connection is accepted, the client sends a five-byte packet containing a guess, e.g., "ready";
the guessed word can be a mix of uppercase and lowercase letters, as case does not matter. The server replies with an eight-byte packet that 
is formatted as follows:
```
                  +-----+-----+-----+-----+-----+-----+-----+-----+
SERVER REPLY:     |valid| guesses   |           result            |
                  |guess| remaining |                             |
                  +-----+-----+-----+-----+-----+-----+-----+-----+
```              
The valid guess field is a one-byte char value that is either 'Y' (yes) or 'N' (no).
The guesses remaining field is a two-byte short value that indicates how many guesses the client has left, which starts at 6.
The result field is a five-byte character string that corresponds to the client’s guess. If a guess is not valid word, simply send "?????"; 
for a valid guess, an uppercase letter indicates a matching letter in the correct position while a lowercase letter to means that the letter
is in the word but not in the correct position. Lastly, a '-' shows that the letter is not found in the hidden word at all.

Game play stops when the player guesses the word correctly or runs out of guesses. In either case, the server thread closes the TCP connection, 
then that corresponding thread terminates. Note that the server can also be shut down through a signal through the following command: 

```
kill -USR1 PIDID        Note: Subsitute PIDID with the actual process ID found by: pgrep nameofexec (e.g. pgrep a.out)
```

Command to Run Wordle Server locally:
1. Clone the repo: git clone https://github.com/williamlin6803/Wordle.git
2. Navigate to the cloned repo on your local machine
3. Open up two terminals
4. Compile and run the server in the first terminal by doing:
```
   clang -Wall -Werror wordle.c wordle-main.c
   ./a.out 8192 wordle-words.txt 5757
```
5. Compile and run the client in the second terminal by doing:
```
   clang -Wall -Werror wordle-client.c
   ./a.out
```
6. Repeat step 5 up to 20 times to play the Wordle game with friends
