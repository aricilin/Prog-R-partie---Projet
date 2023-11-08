#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>




int main(int argc, char *argv[])
{

    if (argc != 3){
        printf("Utilisation : %s fichier_graphe fichier_coloration\n", argv[0]);
        exit(1);
    }

    char * line = NULL;
    size_t line_length = 0;
    ssize_t read;

    int node_number = 0;
    int color_number = 0;
    int i;

    FILE * coloration_file = fopen(argv[2], "r");

    /*
        Count lines in the coloring file, to get the number of vertices
    */
    while ((read = getline(&line, &line_length, coloration_file)) != -1) {
        node_number++;
    }

    /*
        Back to the start of the file
    */
    fseek(coloration_file, 0, SEEK_SET);



    int * node_colors = (int*)malloc((node_number + 1)* sizeof(int));
    for(i = 0; i < node_number + 1; i++){
        node_colors[i] = -1;
    }
    int node_i;
    int color;
    int rounds;
    float node_time;
    int incomplete_coloring = 0;
    int new_color = 1;
    int max_rounds = 0;
    float max_time = 0;

    /*
        For each vertex, read its color, execution time and round number
    */
    while ((read = getline(&line, &line_length, coloration_file)) != -1) {
        sscanf(line, "%d %d %f %d\n", &node_i, &color, &node_time, &rounds);
        new_color = 1;
        
        if(color == 0){
            printf("WARNING : le noeud %d n'est pas colorié\n", node_i);
            incomplete_coloring = 1;
        }
        else{
            for(i = 1; i < node_number + 1; i++){
                if(node_colors[i] == color){
                    new_color = 0;
                    break;
                }
            }
            if(new_color > 0){
                color_number++;
            }
        }

        if(max_rounds < rounds){
            max_rounds = rounds;
        }
        if (max_time < node_time){
            max_time = node_time;
        }
        
        node_colors[node_i] = color;
    }

    fclose(coloration_file);


    /*
        Open graph file
    */
    FILE * graph_file = fopen(argv[1], "r");
    int node_1;
    int node_2;


    /*
        For each edge, check if both ends have different colors
    */
    while ((read = getline(&line, &line_length, graph_file)) != -1) {
        if(line[0] == 'e'){
            sscanf(line, "e %d %d\n", &node_1, &node_2);
            if(node_colors[node_1] == node_colors[node_2]){
                printf("ERREUR : l'arête (%d, %d) a des sommets incidents de même couleur : %d\n", node_1, node_2, node_colors[node_1]);
                exit(1);
            }
        }
    }

    printf("Pas d'erreur détectée, la coloration est propre\n");
    if(incomplete_coloring > 0){
        printf("Certains noeuds n'ont pas de couleur, coloration partielle\n");
    }
    printf("Nombre de couleurs : %d\n", color_number);
    printf("Maximum d'itérations par noeud : %d\n", max_rounds);
    printf("Maximum de temps par noeud : %.5f secondes\n", max_time);

    fclose(graph_file);

    return 0;

}