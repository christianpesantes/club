# club
Simulation using C and gcc for a club in which each patron is a fork() using semaphores to get in


Description
This program simulates a club that wants to enforce a female-male patrons ratio; this requires the use of semaphores and shared memory segments.

Input
This programs require a .txt file with the list of patrons coming to the club; it must follow the following format: 'serial_num  gender  time(delay)  time(stay)' The name of the file should be provided as an argument for main!

NOTE!
The provided input must have a solution! A wrong input file with no solution could cause the program to behave erratically. For example: An input file with only males will result in n processes doing a P operation on a semaphore that will never increase due to lack of female patrons coming to the club

Output:
The status of each patron will be printed to the screen

Instructions:
Compile: gcc posix6.c -lrt -o posix6 
Run: ./posix6 input3.txt