#include "random_color.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


int random_int(int range){

    int limit;
    int r;
    limit = RAND_MAX - (RAND_MAX % range);

    while((r = rand()) >= limit);
    return r % range;

}



int random_color(int max_color, int * reserved_colors){

    int * valid_colors = (int *)malloc((max_color + 1) * sizeof(int));
    int i = 1;
    int valid_colors_number = 0;
    for (i = 1; i < max_color + 1; i++){
        if(reserved_colors[i] > 0){
            continue;
        }
        else{
            valid_colors[valid_colors_number] = i;
            valid_colors_number++;
        }
    }

    int color_index = random_int(valid_colors_number);
    int color = valid_colors[color_index];

    free(valid_colors);
    return color;

}
