#ifndef RANDOM_COLOR_H
#define RANDOM_COLOR_H


/*
    Returns a random integer between 0 and range - 1 (inclusive), with uniform distribution 
*/
int random_int(int range);


/*
    Returns a random color from [1, max_color] excluding colors in reserved_colors.
*/
int random_color(int max_color, int * reserved_colors);


#endif