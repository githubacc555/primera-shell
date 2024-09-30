#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>

#define TAMANO_ARGUMENTOS 10
#define TAMANO_COMANDO 100
#define MAX_COMANDOS 10
#define MAX_FAVORITOS 100
#define ARCHIVO_CONFIG "config_favs.txt" //archivo de configuracion donde se guardara la ubicacion del archivo de la lista de favoritos para facilitar su acceso

struct Favorito{
    int id;
    char comando[100];
};

struct Favorito favoritos[MAX_FAVORITOS];
int num_favs = 0;
char favs_ruta[PATH_MAX] = "";
int aviso = 0;
//funcion que permite guardar un archivo de configuracion que facilita el acceso al archivo donde se encuentra la lista de favoritos
void guardar_configuracion(){
    FILE *archivo_config = fopen(ARCHIVO_CONFIG, "w");
    if(archivo_config != NULL){
        fprintf(archivo_config, "%s\n", favs_ruta);
        fclose(archivo_config);
    }else{
        perror("Error al guardar el archivo de configuracion de comandos favoritos");
    }
}
//funcion que permite crear la lista de favoritos
void favs_crear(char *ruta){
    if (strlen(favs_ruta) > 0){
        printf("El archivo de favoritos ha sido creado en: %s\n", favs_ruta);
        return;
    }
    if(access(ruta, F_OK) != -1){
        printf("El archivo de favoritos ya existe en: %s\n",ruta);
        return;
    }
    printf("Creando archivo de favoritos en: %s\n",ruta);
    int archivo = open(ruta,O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (archivo == -1){
        perror("Error al crear el archivo de favoritos");
        return;
    }
    printf("Archivo de favoritos creado exitosamente en: %s\n",ruta);
    strncpy(favs_ruta,ruta,PATH_MAX-1);
    favs_ruta[PATH_MAX-1] = '\0';
    close(archivo);
    guardar_configuracion();
}
//funcion que comprueba si un comando se encuentra o no en la lista de favoritos
int es_favorito(char *comando){
    for(int i=0; i<num_favs; i++){
        if(strcmp(favoritos[i].comando, comando) == 0){
            return 1;
        }
    }
    return 0;
}
//funcion que carga los favoritos que se encuentran en la lista
void cargar_favoritos(char *ruta){
    FILE *archivo = fopen(ruta, "r");
    if (archivo == NULL){
        return;
    }
    char line[256];
    int id;
    char comando[100];
    num_favs = 0;
    while(fgets(line, sizeof(line), archivo)){
        if(sscanf(line, "%d: %[^\n]", &id, comando) == 2){
            favoritos[num_favs].id = id;
            strncpy(favoritos[num_favs].comando, comando, sizeof(favoritos[num_favs].comando)-1);
            favoritos[num_favs].comando[sizeof(favoritos[num_favs].comando)-1] = '\0';
            num_favs = id;
        }
    }
    fclose(archivo);
}
//funcion que permite cargar el archivo de configuracion que facilita el acceso al archivo donde se encuentra la lista de favoritos
void cargar_configuracion(){
    FILE *archivo_config = fopen(ARCHIVO_CONFIG, "r");
    if(archivo_config != NULL){
        fgets(favs_ruta, sizeof(favs_ruta), archivo_config);
        favs_ruta[strcspn(favs_ruta, "\n")] = '\0';
        fclose(archivo_config);
    }else{
        printf("No se encuentra el archivo de configuracion de comandos favoritos. Use favs crear 'ruta' para crearlo\n");
    }
}
//funcion que permite agregar un comando a favoritos cuando este sea utilizado
void agregar_a_favoritos(char *comando){
    if(strlen(favs_ruta) == 0){
        if(!aviso){
            printf("No se ha especificado una ruta de favoritos. Si desea empezar a añadir sus comandos a favoritos cree una usando favs crear 'ruta'\n");
            aviso = 1;
        }
        return;
    }
    if(num_favs < MAX_FAVORITOS && !es_favorito(comando)){
        favoritos[num_favs].id = num_favs + 1;
        strncpy(favoritos[num_favs].comando, comando, sizeof(favoritos[num_favs].comando)-1);
        favoritos[num_favs].comando[sizeof(favoritos[num_favs].comando)-1] = '\0';
        num_favs++;
        printf("El comando (%s) ha sido agregado con exito a favoritos y su ID es %d\n",comando,favoritos[num_favs-1].id);
        if(strlen(favs_ruta) > 0){
            FILE *archivo = fopen(favs_ruta, "a");
            if(archivo == NULL){
                perror("Error al abrir el archivo de favoritos");
                return;
            }
            fprintf(archivo,"%d: %s\n", favoritos[num_favs-1].id, comando);
            fclose(archivo);
        }else{
            printf("No se especifico un archivo de favoritos, utilize favs crear 'ruta' para crearlo\n");
        }
    }
}
//funcion que se encarga de ejecutar comandos estandar
void comando_simple(char *argumentos[], char *comando){
    int pid = fork();
    int estado_hijo;
    
    if(pid < 0){
        perror("Error en fork()");
    }else if(pid == 0){
        if(execvp(argumentos[0], argumentos) < 0){
            perror("Error al ejecutar el comando");
            exit(1);
        }
    }else{
        wait(&estado_hijo);
        if(WIFEXITED(estado_hijo) && WEXITSTATUS(estado_hijo) == 0){
            agregar_a_favoritos(comando);
        }
    }
}

//funcion que se encarga de gestionar el uso de pipes entre multiples comandos
void comando_con_pipes(char *comandos[], int num_com){
    int pipe_arr[2*(num_com-1)];
    int pid;
    int estado_hijo;

    for(int i=0; i<num_com-1; i++){
        if (pipe(pipe_arr+i*2) == -1){
            perror("Error al crear el pipe");
            exit(1);
        }
    }

    for(int i=0; i<num_com; i++){
        pid = fork();
        if(pid == -1){
            perror("Error en fork()");
            exit(1);
        }else if(pid == 0){
            //redirigimos la entrada si no es el primer comando ejecutado
            if(i > 0){
                if(dup2(pipe_arr[(i-1)*2],0) == -1){
                    perror("Error en la entrada");
                    exit(1);
                }
            }
            //redirigimos la salida si no es el ultimo comando ejecutado
            if(i < num_com-1){
                if(dup2(pipe_arr[i*2+1], 1) == -1){
                    perror("Error en la salida");
                    exit(1);
                }
            }
            //cerrar todos los pipes en el proceso hijo
            for(int j=0; j<2*(num_com-1); j++){
                close(pipe_arr[j]);
            }
            //parsear y ejecutar el comando
            char *argumentos[TAMANO_ARGUMENTOS];
            int num_arg = 0;
            char *aux = strtok(comandos[i], " ");
            while(aux != NULL && num_arg < TAMANO_ARGUMENTOS-1){
                argumentos[num_arg++] = aux;
                aux = strtok(NULL, " ");
            }
            argumentos[num_arg] = NULL;
            if(execvp(argumentos[0], argumentos) == -1){
                perror("Error al ejecutar el comando");
                exit(1);
            }
        }
    }
    //cerrar todos los pipes en el proceso padre
    for(int i=0; i<2*(num_com-1); i++){
        close(pipe_arr[i]);
    }
    //esperar al proceso hijo que termine
    for(int i=0; i<num_com;i++){
        wait(&estado_hijo);
    }
}
//funcion que muestra en pantalla todos los comandos en la lista de favoritos
void favs_mostrar(){
    if(num_favs == 0){
        printf("No hay comandos en la lista de favoritos.\n");
        return;
    }
    printf("Lista de comandos favoritos:\n");
    for(int i=0; i<num_favs; i++){
        printf("%d: %s\n",favoritos[i].id,favoritos[i].comando);
    }
}
//funcion que se encarga de borrar todos los comandos que se encuentran en la lista de favoritos
void favs_borrar(){
    if(num_favs == 0){
        printf("La lista de favoritos esta vacia\n");
        return;
    }
    num_favs = 0;
    printf("Todos los comandos de la lista han sido borrados.\n");
    FILE *archivo = fopen(favs_ruta, "w");
    if(archivo == NULL){
        perror("Error al abrir el archivo de favoritos");
        return;
    }
    fclose(archivo);
}

int main(){
    char comando[TAMANO_COMANDO];
    char *comandos[MAX_COMANDOS];
    char *aux;
    int num_com;
    char cwd[PATH_MAX];
    cargar_configuracion();
    if(strlen(favs_ruta) > 0){
        cargar_favoritos(favs_ruta);
    }
    while(1){
        if(getcwd(cwd, sizeof(cwd)) == NULL){
            perror("Error al obtener el directorio");
            continue;
        }
        printf("shellLM2024:~%s$ ",cwd);
        fgets(comando,sizeof(comando), stdin);
        comando[strcspn(comando,"\n")] = 0;

        //si el tamano del comando ingresado es 0 quiere decir que el usuario el dio a enter por lo tanto imprimira denuevo el prompt
        if(strlen(comando) == 0){
            continue;
        }
        //separar el comando por pipes
        num_com = 0;
        aux = strtok(comando,"|");
        while(aux != NULL && num_com < MAX_COMANDOS){
            comandos[num_com] = aux;
            num_com++;
            aux = strtok(NULL,"|");
        }
        comandos[num_com] = NULL;
        //parsear el comando
        char *argumentos[TAMANO_ARGUMENTOS];
        int num_arg = 0;
        aux = strtok(comandos[0], " ");
        while(aux != NULL && num_arg < TAMANO_ARGUMENTOS-1){
           argumentos[num_arg++] = aux;
            aux = strtok(NULL," ");
        }
        argumentos[num_arg] = NULL;

        //Si el usuario ingreso el comando exit termina el while por lo tanto sale de la shell
        if(strcmp(argumentos[0],"exit") == 0){ 
            break;
        }

        //comando cat
        if(strcmp(argumentos[0],"cat") == 0 && argumentos[1] == NULL){
            printf("Error, ingrese un argumento para el comando cat pulse CTRL+D para reintentar\n");
        }

        //comando favs
        if(strcmp(argumentos[0],"favs") == 0){
            if(argumentos[1] == NULL){
                printf("Error proporcione un argumento valido para el comando favs\n");
            }else if(strcmp(argumentos[1],"crear") == 0){ //subcomando crear
                if(argumentos[2] == NULL){
                    printf("Error proporcione una ruta valida para el comando favs\n");
                }else{
                    strncpy(favs_ruta, argumentos[2], PATH_MAX-1);
                    favs_ruta[PATH_MAX-1] = '\0';
                    favs_crear(favs_ruta);
                    guardar_configuracion();
                    cargar_favoritos(favs_ruta);
                }
            }else if(strcmp(argumentos[1],"mostrar") == 0){ //subcomando mostrar
                favs_mostrar();
                continue;

            }else if(strcmp(argumentos[1],"borrar") == 0){ //subcomando borrar
                favs_borrar();
                continue;
            }else{
                printf("Argumento invalido para 'favs'.\n");
            }
            continue;
        }
        //comando cd
        if(strcmp(argumentos[0],"cd") == 0){
            if(argumentos[1] == NULL){
                printf("Ingrese una ruta valida\n");
            }else{
                if(chdir(argumentos[1]) != 0){
                    perror("Error al cambiar de directorio");
                }else{
                    agregar_a_favoritos(comando);
                }
            }
            continue;
        }
        if(num_com == 1){
            comando_simple(argumentos, comando);
        }else{
            comando_con_pipes(comandos,num_com);
        }
    }
    return 0;
}
