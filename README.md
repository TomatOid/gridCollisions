# gridCollisions
A slightly overcomplicated but fast collisions system in C

You will need to have the SDL2 library installed to build this code

Usage: 
```
$ make

$ ./gridCollisions particles gravity radius [flags]
```
Flag | Description
-----|------------
-d       |Super secret debug mode
-u       |Spawn particles unbalenced with more velocity on one side
-c[int]  |Physics cycles per frame, must be nonzero
-e[float]|Coefficient of restitution / efficiency, must be on [0, 1]
-o[int]  |Clearing opacity, lower values create trails, must be between 0 and 255
 
 Example:
 ```
 $ ./gridCollisions 1000 2000 5 -c30 -e0.95
 ```
 Try moving the window around
