#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>

// Estructuras auxiliares para almacenar la matrix
typedef struct {
    int width, height;  //Tamaño de la matriz
    short **matrix;     //Puntero a la matriz
} image_data;

typedef struct {
    int min_w, min_h;   //"Minimos" de la sub-sección
    int max_w, max_h;   //"Maximos" de la sub-sección
    long sum;           //Suma parcial de la sub-sección
    image_data *data;   //Puntero a los datos
} portion_data;

// Metodos a usar
void generate_matrix(image_data *d);
void free_matrix(image_data *d);
void save_matrix(image_data *d);
double do_threads(int size, image_data *d);
double do_process(int size, image_data *d);
double do_secuential(image_data *d);

int main(int argc, const char* argv[]) {
    clock_t begin, end;
    double threads_time, process_time, secuential_time;
    double threads_prom, process_prom, secuential_prom;
    srand(time(NULL));
    
    //Generar las dimensiones de la matrix
    image_data *image = (image_data*)malloc(sizeof(image_data));
    printf("Ingrese el numero de filas: ");
    scanf("%d", &(image->height));
    printf("Ingrese el numero de columnas: ");
    scanf("%d", &(image->width));

    //En base a pruebas, una separación de 1000 * 1000 es lo suficientemente grande
    //para ameritar otro proceso/hilo
    int size = ((image->height * image->width) / 1000000) + 1;

    //Generar matrix y guardar en archivo
    generate_matrix(image);
    save_matrix(image);

    //Calculo Secuencial
    begin = clock();
    secuential_prom = do_secuential(image);
    end = clock();
    secuential_time = (double) (end-begin) / CLOCKS_PER_SEC;

    //Calculo por Procesos
    begin = clock();
    process_prom = do_process(size, image);
    end = clock();
    process_time = (double) (end-begin) / CLOCKS_PER_SEC;

    //Calculo por Hilos
    begin = clock();
    threads_prom = do_threads(size, image);
    end = clock();
    threads_time = (double) (end-begin) / CLOCKS_PER_SEC;

    //Imprimir resultados
    printf("Calculo por Hilos: %f | Tiempo demorado: %f\n", threads_prom, threads_time);
    printf("Calculo por Procesos: %f | Tiempo demorado: %f\n", process_prom, process_time);
    printf("Calculo Secuencial: %f | Tiempo demorado: %f\n", secuential_prom, secuential_time);

    if (secuential_prom < 128) {
        printf("\n\rLa matriz es OSCURA\n");
    } else {
        printf("\n\rLa matriz es CLARA\n");
    }

    //Liberar memoria y salir
    free_matrix(image);
    free(image);
    return 0;
}

//  --------------------------------
//  ** Generación de la Matrix **
//  --------------------------------
//Imprimir la matrix
void print_matrix(image_data *d) {
    int i, j;
    for (i = 0; i < d->height; i++){
        for (j = 0; j < d->width; j++){
            printf("%d ", d->matrix[i][j]);
        }
        printf("\n");
    }
}

//Guardar la matrix en un archivo para revisión manual futura
void save_matrix(image_data *d){
    int i, j;
    FILE *file;
    char filename[] = "matrix.data";

    file = fopen(filename, "w");

    if (file == NULL){
        return;
    }

    fprintf(file, "Filas: %d, Columnas: %d\n\n", d->height, d->width);

    for (i = 0; i < d->height; i++){
        for (j = 0; j < d->width; j++){
            fprintf(file, "%d ", d->matrix[i][j]);
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    printf("\n** Matrix guardada en %s para revisión manual **\n\n", filename);
}

//Generar la matrix aleatoriamente del tamaño designado
void generate_matrix(image_data *d) {
    int i, j;
    d->matrix = malloc(sizeof *d->matrix * d->height);
    for (i = 0; i < d->height; i++) {
        d->matrix[i] = malloc(sizeof *d->matrix[i] * d->width);
    }

    for (i = 0; i < d->height; i++){
        for (j = 0; j < d->width; j++){
            d->matrix[i][j] = rand() % 256;
        }
    }

    //print_matrix(d);
}

//Liberar la memoria de la matrix
void free_matrix(image_data *d) {
    int i;

    for (i = (d->height)-1; i >= 0; i--){
        free(d->matrix[i]);
    }

    free(d->matrix);
}

//  --------------------------------
//  ** Calculo mediante Hilos **
//  --------------------------------
//Funcion que calcula la suma parcial
void *partial_sumt(void *i){
    int k, j;
    long sum = 0;
    portion_data *pd = (portion_data *)i;

    for (k = pd->min_h; k < pd->max_h; k++){
        for (j = pd->min_w; j < pd->max_w; j++) {
            sum += pd->data->matrix[k][j];
        }
    }

    pd->sum = sum;
    return 0;
}

//"Main" para hacer el calculo mediante hilos
double do_threads(int size, image_data *d) {
    int i;
    long sum = 0;
    
    //Generar los threads
    pthread_t threads[size];

    //Generar las estructuras auxiliares para seccionar la matrix y crear el Hilo
    portion_data *pd = (portion_data*)calloc(size, sizeof(portion_data));

    //Separar la matrix en base al lado más largo
    if (d->height > d->width) {
        for (i = 0; i < size; i++){
            (pd + i)->min_w = 0;
            (pd + i)->max_w = d->width;

            (pd + i)->min_h = i * (d->height / size);
            (pd + i)->max_h = (i + 1) * (d->height / size);

            //Si estoy en el ultimo numero, el tope serán las filas restantes,
            if (i == size - 1){
                (pd + i)->max_h = d->height;
            }
        }
    } else {
        for (i = 0; i < size; i++){
            (pd + i)->min_h = 0;
            (pd + i)->max_h = d->height;

            (pd + i)->min_w = i * (d->width / size);
            (pd + i)->max_w = (i + 1) * (d->width / size);

            //Si estoy en el ultimo numero, el tope serán las columnas restantes,
            if (i == size - 1){
                (pd + i)->max_w = d->width;
            }
        }
    }

    for (i = 0; i < size; i++){
        (pd + i)->data = d;
        (pd + i)->sum = 0;
        //Crear hilo
        pthread_create(&threads[i], NULL, &partial_sumt, (void *)(pd + i));
    }
    
    //Esperar a que los hilos terminen
    for (i = 0; i < size; i++) {
        pthread_join(threads[i], NULL);
    }

    //Con los hilos terminados, juntar todas las sumas parciales
    for (i = 0; i < size; i++) {
        sum += (pd + i)->sum;
    }

    //Liberar memoria y Retornar el promedio
    free(pd);
    return (double)(sum * 1.0d / (d->height * d->width));
}

//  --------------------------------
//  ** Calculo mediante Procesos **
//  --------------------------------
//Calcular la sección de la matrix
long partial_sump(image_data *d, int min_h, int max_h, int min_w, int max_w){
    int i, j;
    long sum = 0;

    for (i = min_h; i < max_h; i++) {
        for (j = min_w; j < max_w; j++) {
            sum += d->matrix[i][j];
        }
    }

    return sum;
}

//Hacer el calculo de la matrix mediante Procesos
double do_process(int size, image_data *d) {
    int i;
    long sum = 0;

    //Crear la cola FIFO para la comunicación
    int fifo_pipe[2];
    pipe(fifo_pipe);

    //Preparar los pids
    pid_t pids[size];

    for (i = 0; i < size; i++) {
        //Generar el hijo
        pids[i] = fork();

        //En el hijo
        if (pids[i] == 0) {
            //Cerrar el lector de la tuberia
            close(fifo_pipe[0]);

            //Separar la matrix en base al lado más largo
            int min_h, max_h, min_w, max_w;
            if (d->height > d->width){
                min_w = 0;
                max_w = d->width;
                min_h = i * (d->height / size);
                max_h = (i + 1) * (d->height / size);

                //Si estoy en el ultimo numero, el tope serán las filas restantes,
                if (i == size - 1){
                    max_h = d->height;
                }
            } else {
                min_h = 0;
                max_h = d->height;
                min_w = i * (d->width / size);
                max_w = (i + 1) * (d->width / size);

                //Si estoy en el ultimo numero, el tope serán las columnas restantes,
                if (i == size - 1){
                    max_w = d->width;
                }
            }

            //Calcular la suma parcial
            long temp_sum = partial_sump(d, min_h, max_h, min_w, max_w);

            //Escribir el resultado en la tuberia.
            write(fifo_pipe[1], &temp_sum, sizeof(long));

            //Cerrar Tuberia, liberar memoria y Finalizar ejecución
            close(fifo_pipe[1]);
            free_matrix(d);
            free(d);
            exit(0);
        }
    }

    //Padre cierra tuberia y espera a que todos los hijos terminen
    close(fifo_pipe[1]);
    for (i = 0; i < size; i++) {
        waitpid(pids[i], NULL, 0);
    }

    //Sumar la suma parciales de los hijos
    for (i = 0; i < size; i++) {
        long temp_sum;
        read(fifo_pipe[0], &temp_sum, sizeof(long));
        sum += temp_sum;
    }

    //Cerrar la tuberia y devolver el promedio
    close(fifo_pipe[1]);
    return (double)(sum * 1.0d / (d->height * d->width));
}

//  --------------------------------
//  ** Calculo Secuencial **
//  --------------------------------
//Hacer el calculo de la matrix secuencialmente
double do_secuential(image_data *d) {
    int i, j;
    long sum = 0;

    //Recorrer la matrix casilla por casilla y almacenar la suma
    for (i = 0; i < d->height; i++){
        for (j = 0; j < d->width; j++){
            sum += d->matrix[i][j];
        }
    }

    // printf("**SECUENTIAL SUM: %ld\n", sum);

    //Retornar el promedio
    return (double)(sum * 1.0d / (d->height * d->width));
}