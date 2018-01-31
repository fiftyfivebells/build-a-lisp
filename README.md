## Build Your Own Lisp

This is my follow-along project for the book [Build Your Own Lisp](http://www.buildyourownlisp.com/). The intent is to learn more C and to get basic understand of Lisp-like languages by implementing a simple Lisp in C. I plan to do all of the bonus exercises and try to make this project a bit more my own as I work through it.

To compile the program, make sure you are in the directory with parser.c, and then depending on your OS, use one of the following commands:

On Linux and Max:
```
cc -std=c99 -Wall parser.c mpc.c -ledit -lm -o parsing
```
On Windows:
```
cc -std=c99 -Wall parsing.c mpc.c -o parsing
```

### Extra Features

The following is a list of features that I've added to what the book suggests:

* Modulo operator
* Decimal numbers
* The ^ operator (squaring function)
* The init function (returns whole list minus last element)
* The  len function (returns number of elements in the list)