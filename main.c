
// *****************************************
//          Carolina Carreira
//               87641
//           Miguel Barros
//               87691
//
//
//                SO
//
// *****************************************
//        2017/2018 1o projeto

// alterar para por o pthread join dentro do ciclo for que esta antes
// possivelmente retirar o lequeue
// devemos por exit na funcao sync?

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "matrix2d.h"
#include "mplib3.h"

// struct que armazena informacao para as threads
typedef struct {

    // ID da tarefa para troca de msgs
    int numTarefa;

    // dimensao da fatia interior
    int dimFatia;

    // limites da fatia total
    int linhaInicial;
    int linhaFinal;

    // dimensoes da matrix total
    int dim;

    int qtdTarefas;

    int n_iter;
} info;


// HEADERS

DoubleMatrix2D* simul(DoubleMatrix2D *matrix, DoubleMatrix2D *matrix_aux,
                      int linhas, int colunas, int numIteracoes);

int parse_integer_or_exit(char const *str, char const *name);
double parse_double_or_exit(char const *str, char const *name) ;

void *workerBee(void *arg);
void *syncTarefas(info *informacao, DoubleMatrix2D *fatia);
DoubleMatrix2D *sliceSimul(DoubleMatrix2D *fatia, info *informacao);

/*--------------------------------------------------------------------
| Function: main
---------------------------------------------------------------------*/

int main(int argc, char **argv) {

    if (argc != 9) {
        fprintf(stderr, "\nNumero invalido de argumentos.\n");
        fprintf(stderr, "Uso: heatSim N tEsq tSup tDir tInf iter trab cassz\n\n");
        return 1;
    }

    // argv[0] = program name
    int N = parse_integer_or_exit(argv[1], "N");
    double tEsq = parse_double_or_exit(argv[2], "tEsq");
    double tSup = parse_double_or_exit(argv[3], "tSup");
    double tDir = parse_double_or_exit(argv[4], "tDir");
    double tInf = parse_double_or_exit(argv[5], "tInf");
    int iter = parse_integer_or_exit(argv[6], "iter");
    int trab = parse_integer_or_exit(argv[7], "trab");
    int csz = parse_integer_or_exit(argv[8], "csz");

    // verificacao de argumentos
    if (N % trab) {
        fprintf(stderr, "\nNo de trab deve ser divisor da dimensao da matriz (N)\n");
        return 1;
    }

    if (N < 1 || tEsq < 0 || tSup < 0 || tDir < 0 || tInf < 0 || iter < 1 || csz < 0 || trab <= 0) {
        fprintf(stderr, "\nErro: Argumentos invalidos.\n Lembrar que N >= 1, temperaturas >= 0, iter >= 1, csz >= "
                "0, trab > 0 \n\n");
        return 1;
    }


    // inicializacao da matriz
    DoubleMatrix2D *matrix;
    matrix = dm2dNew(N + 2, N + 2);
    if (matrix == NULL) {
        fprintf(stderr,"\nErro: Nao foi possivel alocar memoria para as matrizes.\n\n");
        return -1;
    }

    dm2dSetLineTo(matrix, 0, tSup);
    dm2dSetLineTo(matrix, N + 1, tInf);
    dm2dSetColumnTo(matrix, 0, tEsq);
    dm2dSetColumnTo(matrix, N + 1, tDir);


    // inicializacao das structs
    info *informacao[trab];

    // o conjunto de linhas contendo pontos interiores
    int k = N / trab;

    // inicializar structs
    for (int i = 0; i < trab; i++) {

        informacao[i] = (info *)malloc(sizeof(info));

        if (informacao[i] == NULL){
          fprintf(stderr, "\n\nErro ao alocar memoria\n");
          return 1;
        }

        // ID da tarefas para troca de mensagens (comeca no 1)
        // a tarefa mestre e a 0
        informacao[i]->numTarefa = i + 1;

        // dimFatia da fatia interior
        informacao[i]->dimFatia = k;

        // limites da fatia total
        informacao[i]->linhaInicial = (k * i);
        informacao[i]->linhaFinal = k * (i + 1) + 1;

        // dimensoes da matriz total
        informacao[i]->dim = N;

        informacao[i]->qtdTarefas = trab;

        informacao[i]->n_iter = iter;
    }

    // inicializar mplib
    // ncanais = ntrab + 1
    if (inicializarMPlib(csz, trab + 1) == -1) {
        fprintf(stderr, "Erro: inicializarMPlib\n");
        return -1;
    }


    // inicializar tarefas
    pthread_t workers[trab];

    int i, j;
    for (i = 0; i < trab; i++) {

        // criacao das tarefas
        if (pthread_create(&workers[i], NULL, workerBee, informacao[i])) {
            fprintf(stderr, "\nErro ao criar tarefa. \n\n");

            return -1;
        }

        for (j = informacao[i]->linhaInicial; j <= informacao[i]->linhaFinal; j++) {

            // envio do conteudo de cada uma das fatias para as respetivas tarefas
            if (enviarMensagem(0, informacao[i]->numTarefa, dm2dGetLine(matrix, j),
                               sizeof(double) * (informacao[i]->dim + 2)) == -1) {

                fprintf(stderr, "\nNão consegui enviar a mensagem correspondente a linha %d a tarefa %d\n\n",
                        j, informacao[i]->numTarefa);

                return -1;
            }
        }
    }

    // buffer usado para receber as linhas ja calculadas das tarefas
    double bufferRecebe[N + 2];

    for (i = 0; i < trab; i++) {

        // recebe fatia interiores e coloca na matriz principaç
        for (j = informacao[i]->linhaInicial + 1; j < informacao[i]->linhaFinal; j++) {

            if (receberMensagem(informacao[i]->numTarefa, 0, bufferRecebe, sizeof(double) * (informacao[i]->dim + 2)) == -1) {
                fprintf(stderr, "\nErro ao receber a mensagem da tarefa %d correspondente a linha %d\n", i + 1, j);

                return -1;
            }

            dm2dSetLine(matrix, j, bufferRecebe);
        }

        // espera que a tarefa acabe
        if (pthread_join(workers[i], NULL)) {

            fprintf(stderr, "\nErro ao esperar por um trabalhador.\n");
            return -1;
        }
    }

    dm2dPrint(matrix);

    // libertacao de memoria da matriz, das structs informacao e da MPlib
    dm2dFree(matrix);

    for (i = 0; i < trab; i++)
        free(informacao[i]);

    libertarMPlib();

    return 0;
}

/*--------------------------------------------------------------------
| Function: parse_integer_or_exit
| Verificacao de argumentos inteiros
---------------------------------------------------------------------*/

int parse_integer_or_exit(char const *str, char const *name) {
    int value;

    if (sscanf(str, "%d", &value) != 1) {
        fprintf(stderr, "\nErro no argumento \"%s\".\n\n", name);
        exit(1);
    }
    return value;
}

/*--------------------------------------------------------------------
| Function: parse_double_or_exit
| Verificacao de argumentos double
---------------------------------------------------------------------*/

double parse_double_or_exit(char const *str, char const *name) {
    double value;

    if (sscanf(str, "%lf", &value) != 1) {
        fprintf(stderr, "\nErro no argumento \"%s\".\n\n", name);
        exit(1);
    }
    return value;
}


/*--------------------------------------------------------------------
| Function: workerBee
| Executada pelas threads, comunica com a tarefa mestre e envia
| a esta a fatia calculada
---------------------------------------------------------------------*/

void *workerBee(void *arg) {

    info *informacao = (info *)(arg);
    int i;

    DoubleMatrix2D *fatia = dm2dNew(informacao->dimFatia + 2, informacao->dim + 2);

    double bufferRecebe[informacao->dim + 2];

    // incializacao da fatia feita por sincronizacao por mensagens com a tarefa mestre
    for (i = 0; i <= informacao->dimFatia + 1; i++) {
        if (receberMensagem(0, informacao->numTarefa, bufferRecebe, sizeof(double) * (informacao->dim + 2)) != -1)
            dm2dSetLine(fatia, i, bufferRecebe);

        else {
            fprintf(stderr, "\nA tarefa %d nao conseguiu receber a mensagem %d da tarefa mestre\n\n",
                    informacao->numTarefa, i);
            exit(1);
        }
    }

    // calculo da propragacao da temperatura na fatia
    fatia = sliceSimul(fatia, informacao);

    // enviar para a tarefa mestre a fatia calculada
    for (i = 1; i <= informacao->dimFatia; i++)
        if (enviarMensagem(informacao->numTarefa, 0, dm2dGetLine(fatia, i),
                           sizeof(double) * (informacao->dim + 2)) == -1){
          fprintf(stderr, "Erro ao enviar as fatias completas para a main\n");

          exit(1);
        }


    dm2dFree(fatia);

    return NULL;
}

/*--------------------------------------------------------------------
| Function: syncTarefas
| Sincroniza as linhas de fronteira das fatias das respetivas
| tarefas entre si
---------------------------------------------------------------------*/

void *syncTarefas(info *informacao, DoubleMatrix2D *fatia) {

    double bufferRecebe[informacao->dim + 2];

    // caso estejamos na primeira fatia
    if (informacao->numTarefa == 1) {

        // envia  ultima linha do interior para a fatia de baixo
        if (enviarMensagem(informacao->numTarefa, informacao->numTarefa + 1,
                           dm2dGetLine(fatia, informacao->dimFatia), sizeof(double) * (informacao->dim + 2)) == -1){

            fprintf(stderr, "\nerro ao enviar a linha %d da tarefa %d para a tarefa %d\n",
                    informacao->dimFatia + 1, informacao->numTarefa, informacao->numTarefa + 1);
            exit(1);
          }

        // recebe linha da fatia de baixo para a ultima linha (limite da fatia)
        if (receberMensagem(informacao->numTarefa + 1, informacao->numTarefa, bufferRecebe,
                            sizeof(double) * (informacao->dim + 2)) == -1){

            fprintf(stderr, "\nerro ao receber a linha %d da tarefa %d para a tarefa %d\n",
                    informacao->dimFatia + 1, informacao->numTarefa + 1, informacao->numTarefa);
            exit(1);
          }

        // caso nao haja erro na recepcao
        else
            dm2dSetLine(fatia, informacao->dimFatia + 1, bufferRecebe);
    }

    // caso estejamos na ultima fatia da matriz
    else if (informacao->numTarefa == informacao->qtdTarefas) {

        // recebe linha da fatia de cima para a primeira (0, limite da fatia)
        if (receberMensagem(informacao->numTarefa - 1, informacao->numTarefa,
                            bufferRecebe, sizeof(double) * (informacao->dim + 2)) == -1){

            fprintf(stderr, "\nerro ao receber a linha %d da tarefa %d para a tarefa %d\n",
                    informacao->dimFatia + 1, informacao->numTarefa - 1, informacao->numTarefa);
            exit(1);
          }

        else
            dm2dSetLine(fatia, 0, bufferRecebe);

        // enviar primeira linha do interior para a fatia de cima
        if (enviarMensagem(informacao->numTarefa, informacao->numTarefa - 1, dm2dGetLine(fatia, 1),
                           sizeof(double) * (informacao->dim + 2)) == -1){

            fprintf(stderr, "\nerro ao enviar a linha %d da tarefa %d para a tarefa %d\n", 1,
                    informacao->numTarefa, informacao->numTarefa - 1);
            exit(1);
          }
    }

    // caso estejamos numa das fatias interiores da matriz
    else {

        // recebe linha da fatia de cima para a primeira linha (0, limite da fatia)
        if (receberMensagem(informacao->numTarefa - 1, informacao->numTarefa,
                            bufferRecebe, sizeof(double) * (informacao->dim + 2)) == -1){

            fprintf(stderr, "\nerro ao receber a linha %d da tarefa %d para a tarefa %d\n",
                    informacao->dimFatia + 1, informacao->numTarefa - 1, informacao->numTarefa);
            exit(1);
          }

        else
            dm2dSetLine(fatia, 0, bufferRecebe);


        // envia primeira linha do interior para a fatia de cima
        if (enviarMensagem(informacao->numTarefa, informacao->numTarefa - 1,
                           dm2dGetLine(fatia, 1), sizeof(double) * (informacao->dim + 2)) == -1){

            fprintf(stderr,
                    "\nerro ao enviar a linha %d da tarefa %d para a tarefa %d\n", 1,
                    informacao->numTarefa, informacao->numTarefa - 1);
            exit(1);
          }


        // envia ultima linha do interior para a fatia de baixo
        if (enviarMensagem(informacao->numTarefa, informacao->numTarefa + 1,
                           dm2dGetLine(fatia, informacao->dimFatia),
                            sizeof(double) * (informacao->dim + 2)) == -1){

            fprintf(stderr, "\nerro ao enviar a linha %d da tarefa %d para a tarefa %d\n",
                    informacao->dimFatia + 1, informacao->numTarefa, informacao->numTarefa + 1);
            exit(1);
          }


        // recebe linha da fatia de baixo para a ultima linha da fatia (limite da fatia)
        if (receberMensagem(informacao->numTarefa + 1, informacao->numTarefa,
                            bufferRecebe, sizeof(double) * (informacao->dim + 2)) == -1) {

            fprintf(stderr, "\nerro ao receber a linha %d da tarefa %d para a tarefa %d\n",
                    informacao->dimFatia + 1, informacao->numTarefa + 1, informacao->numTarefa);
            exit(1);
          }

        else
            dm2dSetLine(fatia, informacao->dimFatia + 1, bufferRecebe);
    }

    return NULL;
}


/*--------------------------------------------------------------------
| Function: sliceSimul
| Calcula a propagacao da temperatura na matriz numa determinada fatia
| num numero dado de interacoes (e sincroniza esta fatia com as vizinhas,
| chama a syncTarefas)
---------------------------------------------------------------------*/

DoubleMatrix2D *sliceSimul(DoubleMatrix2D *fatia, info *informacao) {

    DoubleMatrix2D *fatia_aux = dm2dNew(informacao->dimFatia + 2, informacao->dim + 2);

    DoubleMatrix2D *tmp;

    // copia fatia dada para a auxiliar
    dm2dCopy(fatia_aux, fatia);

    int i, j, iter;
    double value;

    // executar as iteracoes
    for (iter = 0; iter < informacao->n_iter; iter++) {

        // percorre as linhas interiores
        for (i = 1; i <= informacao->dimFatia; i++) {

            // percorre as colunas interiores
            for (j = 1; j <= informacao->dim; j++) {

                // calculo da temperatura no ponto (i, j)
                value = (dm2dGetEntry(fatia_aux, i - 1, j) +
                         dm2dGetEntry(fatia_aux, i + 1, j) +
                         dm2dGetEntry(fatia_aux, i, j - 1) +
                         dm2dGetEntry(fatia_aux, i, j + 1)) / 4.0;
                dm2dSetEntry(fatia, i, j, value);
            }
        }

        //nao e preciso sincronizar se so ouver uma thread
        if (informacao->qtdTarefas != 1)
          // sincroniza as fronteiras das fatias entre as tarefas
          syncTarefas(informacao, fatia);

        // faz a troca entre a matriz a alterar
        tmp = fatia;
        fatia = fatia_aux;
        fatia_aux = tmp;
    }

    dm2dFree(fatia);

    return fatia_aux;
}
