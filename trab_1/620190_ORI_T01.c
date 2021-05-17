/* ==========================================================================
 * Universidade Federal de São Carlos - Campus Sorocaba
 * Disciplina: Organização de Recuperação da Informação
 * Prof. Tiago A. Almeida
 *
 * Trabalho 01 - Indexação
 *
 * RA: 620190 
 * Aluno: Washington Paes Marques da Silva
 * ========================================================================== */
 
/* Bibliotecas */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
 
/* Tamanho dos campos dos registros */
/* Campos de tamanho fixo */
#define TAM_CPF 12
#define TAM_NASCIMENTO 11
#define TAM_CELULAR 12
#define TAM_SALDO 14
#define TAM_TIMESTAMP 15
 
/* Campos de tamanho variável (tamanho máximo) */
// Restaram 204 bytes (48B nome, 47B email, 11B chave CPF, 11B chave número de celular,
// 47B chave email, 37B chave aleatória, 3B demilitadores)
#define TAM_MAX_NOME 48
#define TAM_MAX_EMAIL 47
#define TAM_MAX_CHAVE_PIX 48
 
#define MAX_REGISTROS 1000
#define TAM_REGISTRO_CLIENTE 256
#define TAM_REGISTRO_TRANSACAO 49
#define TAM_ARQUIVO_CLIENTE (TAM_REGISTRO_CLIENTE * MAX_REGISTROS + 1)
#define TAM_ARQUIVO_TRANSACAO (TAM_REGISTRO_TRANSACAO * MAX_REGISTROS + 1)
 
/* Mensagens padrões */
#define SUCESSO                          "OK\n"
#define AVISO_NENHUM_REGISTRO_ENCONTRADO "AVISO: Nenhum registro encontrado\n"
#define ERRO_OPCAO_INVALIDA              "ERRO: Opcao invalida\n"
#define ERRO_MEMORIA_INSUFICIENTE        "ERRO: Memoria insuficiente\n"
#define ERRO_PK_REPETIDA                 "ERRO: Ja existe um registro com a chave primaria %s\n"
#define ERRO_REGISTRO_NAO_ENCONTRADO     "ERRO: Registro nao encontrado\n"
#define ERRO_SALDO_NAO_SUFICIENTE        "ERRO: Saldo insuficiente\n"
#define ERRO_VALOR_INVALIDO              "ERRO: Valor invalido\n"
#define ERRO_CHAVE_PIX_REPETIDA          "ERRO: Ja existe uma chave PIX %s\n"
#define ERRO_CHAVE_PIX_DUPLICADA         "ERRO: Chave PIX tipo %c ja cadastrada para este usuario\n"
#define ERRO_CHAVE_PIX_INVALIDA          "ERRO: Chave PIX invalida\n"
#define ERRO_TIPO_PIX_INVALIDO           "ERRO: Tipo %c invalido para chaves PIX\n"
#define ERRO_ARQUIVO_VAZIO               "ERRO: Arquivo vazio\n"
#define ERRO_NAO_IMPLEMENTADO            "ERRO: Funcao %s nao implementada\n"
 
/* Registro de Cliente */
typedef struct {
    char cpf[TAM_CPF];
    char nome[TAM_MAX_NOME];
    char nascimento[TAM_NASCIMENTO];
    char email[TAM_MAX_EMAIL];
    char celular[TAM_CELULAR];
    double saldo;
    char chaves_pix[4][TAM_MAX_CHAVE_PIX];
} Cliente;
 
/* Registro de transação */
typedef struct {
    char cpf_origem[TAM_CPF];
    char cpf_destino[TAM_CPF];
    double valor;
    char timestamp[TAM_TIMESTAMP];
} Transacao;
 
/*----- Registros dos índices -----*/
 
/* Struct para índice primário de clientes */
typedef struct {
    char cpf[TAM_CPF];
    int rrn;
} clientes_index;
 
/* Struct para índice primário de transações */
typedef struct {
    char cpf_origem[TAM_CPF];
    char timestamp[TAM_TIMESTAMP];
    int rrn;
} transacoes_index;
 
/* Struct para índice secundário de chaves PIX */
typedef struct {
    char chave_pix[TAM_MAX_CHAVE_PIX];
    char pk_cliente[TAM_CPF];
} chaves_pix_index;
 
/* Struct para índice secundário de timestamp e CPF origem */
typedef struct {
    char timestamp[TAM_TIMESTAMP];
    char cpf_origem[TAM_CPF];
} timestamp_cpf_origem_index;
 
/* Variáveis globais */
/* Arquivos de dados */
char ARQUIVO_CLIENTES[TAM_ARQUIVO_CLIENTE];
char ARQUIVO_TRANSACOES[TAM_ARQUIVO_TRANSACAO];
 
/* Índices */
clientes_index *clientes_idx = NULL;
transacoes_index *transacoes_idx = NULL;
chaves_pix_index *chaves_pix_idx = NULL;
timestamp_cpf_origem_index *timestamp_cpf_origem_idx = NULL;
 
/* Contadores */
unsigned qtd_registros_clientes = 0;
unsigned qtd_registros_transacoes = 0;
unsigned qtd_chaves_pix = 0;
 
/* Funções de geração determinística de números pseudo-aleatórios */
uint64_t prng_seed;
 
void prng_srand(uint64_t value) {
    prng_seed = value;
}
 
uint64_t prng_rand() {
    // https://en.wikipedia.org/wiki/Xorshift#xorshift*
    uint64_t x = prng_seed; // O estado deve ser iniciado com um valor diferente de 0
    x ^= x >> 12; // a
    x ^= x << 25; // b
    x ^= x >> 27; // c
    prng_seed = x;
    return x * UINT64_C(0x2545F4914F6CDD1D);
}
 
/**
 * Gera um <a href="https://en.wikipedia.org/wiki/Universally_unique_identifier">UUID Version-4 Variant-1</a>
 * (<i>string</i> aleatória) de 36 caracteres utilizando o gerador de números pseudo-aleatórios
 * <a href="https://en.wikipedia.org/wiki/Xorshift#xorshift*">xorshift*</a>. O UUID é
 * escrito na <i>string</i> fornecida como parâmetro.<br />
 * <br />
 * Exemplo de uso:<br />
 * <code>
 * char chave_aleatoria[37];<br />
 * new_uuid(chave_aleatoria);<br />
 * printf("chave aleatória: %s&#92;n", chave_aleatoria);<br />
 * </code>
 *
 * @param buffer String de tamanho 37 no qual será escrito
 * o UUID. É terminado pelo caractere <code>\0</code>.
 */
void new_uuid(char buffer[37]) {
    uint64_t r1 = prng_rand();
    uint64_t r2 = prng_rand();
 
    sprintf(buffer, "%08x-%04x-%04lx-%04lx-%012lx", (uint32_t)(r1 >> 32), (uint16_t)(r1 >> 16), 0x4000 | (r1 & 0x0fff), 0x8000 | (r2 & 0x3fff), r2 >> 16);
}
 
/* Funções de manipulação de data (timestamp) */
int64_t epoch;
 
void set_time(int64_t value) {
    epoch = value;
}
 
void tick_time() {
    epoch += prng_rand() % 864000; // 10 dias
}
 
struct tm gmtime_(const int64_t lcltime) {
    // based on https://sourceware.org/git/?p=newlib-cygwin.git;a=blob;f=newlib/libc/time/gmtime_r.c;
    struct tm res;
    long days = lcltime / 86400 + 719468;
    long rem = lcltime % 86400;
    if (rem < 0) {
        rem += 86400;
        --days;
    }
 
    res.tm_hour = (int) (rem / 3600);
    rem %= 3600;
    res.tm_min = (int) (rem / 60);
    res.tm_sec = (int) (rem % 60);
 
    int weekday = (3 + days) % 7;
    if (weekday < 0) weekday += 7;
    res.tm_wday = weekday;
 
    int era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned long eraday = days - era * 146097;
    unsigned erayear = (eraday - eraday / 1460 + eraday / 36524 - eraday / 146096) / 365;
    unsigned yearday = eraday - (365 * erayear + erayear / 4 - erayear / 100);
    unsigned month = (5 * yearday + 2) / 153;
    unsigned day = yearday - (153 * month + 2) / 5 + 1;
    month += month < 10 ? 2 : -10;
 
    int isleap = ((erayear % 4) == 0 && (erayear % 100) != 0) || (erayear % 400) == 0;
    res.tm_yday = yearday >= 306 ? yearday - 306 : yearday + 59 + isleap;
    res.tm_year = (erayear + era * 400 + (month <= 1)) - 1900;
    res.tm_mon = month;
    res.tm_mday = day;
    res.tm_isdst = 0;
 
    return res;
}
 
/**
 * Escreve a <i>timestamp</i> atual no formato <code>AAAAMMDDHHmmSS</code> em uma <i>string</i>
 * fornecida como parâmetro.<br />
 * <br />
 * Exemplo de uso:<br />
 * <code>
 * char timestamp[TAM_TIMESTAMP];<br />
 * current_timestamp(timestamp);<br />
 * printf("timestamp atual: %s&#92;n", timestamp);<br />
 * </code>
 *
 * @param buffer String de tamanho <code>TAM_TIMESTAMP</code> no qual será escrita
 * a <i>timestamp</i>. É terminado pelo caractere <code>\0</code>.
 */
void current_timestamp(char buffer[TAM_TIMESTAMP]) {
    // http://www.cplusplus.com/reference/ctime/strftime/
    // http://www.cplusplus.com/reference/ctime/gmtime/
    // AAAA MM DD HH mm SS
    // %Y   %m %d %H %M %S
    struct tm tm_ = gmtime_(epoch);
    strftime(buffer, TAM_TIMESTAMP, "%Y%m%d%H%M%S", &tm_);
}
 
/* Remove comentários (--) e caracteres whitespace do começo e fim de uma string */
void clear_input(char *str) {
    char *ptr = str;
    int len = 0;
 
    for (; ptr[len]; ++len) {
        if (strncmp(&ptr[len], "--", 2) == 0) {
            ptr[len] = '\0';
            break;
        }
    }
 
    while(len-1 > 0 && isspace(ptr[len-1]))
        ptr[--len] = '\0';
 
    while(*ptr && isspace(*ptr))
        ++ptr, --len;
 
    memmove(str, ptr, len + 1);
}
 
/* ==========================================================================
 * ========================= PROTÓTIPOS DAS FUNÇÕES =========================
 * ========================================================================== */
 
/* (Re)faz o índice respectivo */
void criar_clientes_index();
void criar_transacoes_index();
void criar_chaves_pix_index();
void criar_timestamp_cpf_origem_index();
 
/* Exibe um registro */
int exibir_cliente(int rrn);
int exibir_transacao(int rrn);
 
/* Recupera do arquivo o registro com o RRN informado
 * e retorna os dados nas struct Cliente/Transacao */
Cliente recuperar_registro_cliente(int rrn);
Transacao recuperar_registro_transacao(int rrn);
 
/* Funções principais */
void cadastrar_cliente(char *cpf, char *nome, char *nascimento, char *email, char *celular);
void remover_cliente(char *cpf);
void alterar_saldo(char *cpf, double valor);
void cadastrar_chave_pix(char *cpf, char tipo);
void transferir(char *chave_pix_origem, char *chave_pix_destino, double valor);
 
/* Busca */
void buscar_cliente_cpf(char *cpf);
void buscar_cliente_chave_pix(char *chave_pix);
void buscar_transacao_cpf_origem_timestamp(char *cpf, char *timestamp);
 
/* Listagem */
void listar_clientes_cpf();
void listar_transacoes_periodo(char *data_inicio, char *data_fim);
void listar_transacoes_cpf_origem_periodo(char *cpf, char *data_inicio, char *data_fim);
 
/* Liberar espaço */
void liberar_espaco();
 
/* Imprimir arquivos de dados */
void imprimir_arquivo_clientes();
void imprimir_arquivo_transacoes();
 
/* Imprimir índices secundários */
void imprimir_chaves_pix_index();
void imprimir_timestamp_cpf_origem_index();
 
/* Liberar memória e encerrar programa */
void liberar_memoria();
 
/* <<< COLOQUE AQUI OS DEMAIS PROTÓTIPOS DE FUNÇÕES, SE NECESSÁRIO >>> */
void inserir_registro_cliente(Cliente c);
void inserir_registro_transacao(Transacao t);
 
clientes_index *  search_client_index(char *cpf);
transacoes_index * search_transacoes_index(char * cpf, char * timestamp);
chaves_pix_index * search_chaves_pix_index(char * chave_pix);
transacoes_index * search_t_timestamp_cpf_index(char * cpf, char *timestamp);
 
 
void inserir_index_clientes( clientes_index *c);
void inserir_index_transacoes(transacoes_index *t);
void inserir_index_chave_pix(chaves_pix_index *p);
void inserir_index_timestamp_cpf_origem_index( timestamp_cpf_origem_index *tc);
 
/* Funções auxiliares para o qsort.
 * Com uma pequena alteração, é possível utilizá-las no bsearch, assim, evitando código duplicado.
 * */
int qsort_clientes_index(const void *a, const void *b);
int qsort_transacoes_index(const void *a, const void *b);
int qsort_chaves_pix_index(const void *a, const void *b);
int qsort_timestamp_cpf_origem_index(const void *a, const void *b);
 
/* ==========================================================================
 * ============================ FUNÇÃO PRINCIPAL ============================
 * =============================== NÃO ALTERAR ============================== */
 
int main() {
    // variáveis utilizadas pelo interpretador de comandos
    char input[500];
    uint64_t seed = 2;
    uint64_t time = 1616077800; // UTC 18/03/2021 14:30:00
    char cpf[TAM_CPF];
    char nome[TAM_MAX_NOME];
    char nascimento[TAM_NASCIMENTO];
    char email[TAM_MAX_EMAIL];
    char celular[TAM_CELULAR];
    double valor;
    char tipo_chave_pix;
    char chave_pix_origem[TAM_MAX_CHAVE_PIX];
    char chave_pix_destino[TAM_MAX_CHAVE_PIX];
    char chave_pix[TAM_MAX_CHAVE_PIX];
    char timestamp[TAM_TIMESTAMP];
    char data_inicio[TAM_TIMESTAMP];
    char data_fim[TAM_TIMESTAMP];
 
    scanf("SET ARQUIVO_CLIENTES '%[^\n]\n", ARQUIVO_CLIENTES);
    int temp_len = strlen(ARQUIVO_CLIENTES);
    if (temp_len < 2) temp_len = 2; // corrige o tamanho caso a entrada seja omitida
    qtd_registros_clientes = (temp_len - 2) / TAM_REGISTRO_CLIENTE;
    ARQUIVO_CLIENTES[temp_len - 2] = '\0';
 
    scanf("SET ARQUIVO_TRANSACOES '%[^\n]\n", ARQUIVO_TRANSACOES);
    temp_len = strlen(ARQUIVO_TRANSACOES);
    if (temp_len < 2) temp_len = 2; // corrige o tamanho caso a entrada seja omitida
    qtd_registros_transacoes = (temp_len - 2) / TAM_REGISTRO_TRANSACAO;
    ARQUIVO_TRANSACOES[temp_len - 2] = '\0';
 
    // inicialização do gerador de números aleatórios e função de datas
    prng_srand(seed);
    set_time(time);
 
    criar_clientes_index();
    criar_transacoes_index();
    criar_chaves_pix_index();
    criar_timestamp_cpf_origem_index();
 
    while (1) {
        fgets(input, 500, stdin);
        clear_input(input);
 
        /* Funções principais */
        if (sscanf(input, "INSERT INTO clientes VALUES ('%[^']', '%[^']', '%[^']', '%[^']', '%[^']');", cpf, nome, nascimento, email, celular) == 5)
            cadastrar_cliente(cpf, nome, nascimento, email, celular);
        else if (sscanf(input, "DELETE FROM clientes WHERE cpf = '%[^']';", cpf) == 1)
            remover_cliente(cpf);
        else if (sscanf(input, "UPDATE clientes SET saldo = saldo + %lf WHERE cpf = '%[^']';", &valor, cpf) == 2)
            alterar_saldo(cpf, valor);
        else if (sscanf(input, "UPDATE clientes SET chaves_pix = array_append(chaves_pix, '%c') WHERE cpf = '%[^']';", &tipo_chave_pix, cpf) == 2)
            cadastrar_chave_pix(cpf, tipo_chave_pix);
        else if (sscanf(input, "INSERT INTO transacoes VALUES ('%[^']', '%[^']', %lf);", chave_pix_origem, chave_pix_destino, &valor) == 3)
            transferir(chave_pix_origem, chave_pix_destino, valor);
 
        /* Busca */
        else if (sscanf(input, "SELECT * FROM clientes WHERE cpf = '%[^']';", cpf) == 1)
            buscar_cliente_cpf(cpf);
        else if (sscanf(input, "SELECT * FROM clientes WHERE '%[^']' = ANY (chaves_pix);", chave_pix) == 1)
            buscar_cliente_chave_pix(chave_pix);
        else if (sscanf(input, "SELECT * FROM transacoes WHERE cpf_origem = '%[^']' AND timestamp = '%[^']';", cpf, timestamp) == 2)
            buscar_transacao_cpf_origem_timestamp(cpf, timestamp);
 
        /* Listagem */
        else if (strcmp("SELECT * FROM clientes ORDER BY cpf ASC;", input) == 0)
            listar_clientes_cpf();
        else if (sscanf(input, "SELECT * FROM transacoes WHERE timestamp BETWEEN '%[^']' AND '%[^']' ORDER BY timestamp ASC;", data_inicio, data_fim) == 2)
            listar_transacoes_periodo(data_inicio, data_fim);
        else if (sscanf(input, "SELECT * FROM transacoes WHERE cpf_origem = '%[^']' AND timestamp BETWEEN '%[^']' AND '%[^']' ORDER BY timestamp ASC;", cpf, data_inicio, data_fim) == 3)
            listar_transacoes_cpf_origem_periodo(cpf, data_inicio, data_fim);
 
        /* Liberar espaço */
        else if (strcmp("VACUUM clientes;", input) == 0)
            liberar_espaco();
 
        /* Imprimir arquivos de dados */
        else if (strcmp("\\echo file ARQUIVO_CLIENTES", input) == 0)
            imprimir_arquivo_clientes();
        else if (strcmp("\\echo file ARQUIVO_TRANSACOES", input) == 0)
            imprimir_arquivo_transacoes();
 
        /* Imprimir índices secundários */
        else if (strcmp("\\echo index chaves_pix_index", input) == 0)
            imprimir_chaves_pix_index();
        else if (strcmp("\\echo index timestamp_cpf_origem_index", input) == 0)
            imprimir_timestamp_cpf_origem_index();
 
        /* Liberar memória e encerrar programa */
        else if (strcmp("\\q", input) == 0)
            { liberar_memoria(); return 0; }
        else if (sscanf(input, "SET SRAND %lu;", &seed) == 1)
            { prng_srand(seed); printf(SUCESSO); continue; }
        else if (sscanf(input, "SET TIME %lu;", &time) == 1)
            { set_time(time); printf(SUCESSO); continue; }
        else if (strcmp("", input) == 0)
            continue; // não avança o tempo caso seja um comando em branco
        else
            printf(ERRO_OPCAO_INVALIDA);
 
        tick_time();
    }
}
 
/* (Re)faz o índice primário clientes_index */
void criar_clientes_index() {
    if (clientes_idx==NULL)
        clientes_idx = malloc(MAX_REGISTROS * sizeof(clientes_index));
 
    if (clientes_idx==NULL) {
        printf(ERRO_MEMORIA_INSUFICIENTE);
        exit(1);
    }
 
    for (unsigned i = 0; i < qtd_registros_clientes; ++i) {
        Cliente c = recuperar_registro_cliente(i);
 
        if (strncmp(c.cpf, "*|", 2) == 0)
            clientes_idx[i].rrn = -1; // registro excluído
        else
            clientes_idx[i].rrn = i;
 
        strcpy(clientes_idx[i].cpf, c.cpf);
    }
 
    qsort(clientes_idx, qtd_registros_clientes, sizeof(clientes_index), qsort_clientes_index);
}
 
/* (Re)faz o índice primário transacoes_index */
void criar_transacoes_index() {
    if (!transacoes_idx)
        transacoes_idx = malloc(MAX_REGISTROS * sizeof(transacoes_index));
    
    if(!transacoes_idx) {
        printf(ERRO_MEMORIA_INSUFICIENTE);
        exit(1);
    }
 
    for (unsigned i=0; i < qtd_registros_transacoes; i++){
        Transacao t = recuperar_registro_transacao(i);
 
        transacoes_idx[i].rrn = i;
 
        strcpy(transacoes_idx[i].cpf_origem, t.cpf_origem);
        strcpy(transacoes_idx[i].timestamp, t.timestamp); 
    }
 
    qsort(transacoes_idx, qtd_registros_transacoes, sizeof(transacoes_index), qsort_transacoes_index);
}
 
/* (Re)faz o índice secundário chaves_pix_index */
void criar_chaves_pix_index() {
    if (!chaves_pix_idx)
        chaves_pix_idx = malloc(MAX_REGISTROS * 4 * sizeof(chaves_pix_index));
    
    if(!chaves_pix_idx) {
        printf(ERRO_MEMORIA_INSUFICIENTE);
        exit(1);
    }
 
    for (unsigned i=0, k=0; i < qtd_registros_clientes, k < qtd_chaves_pix; i++){
        unsigned j=0;
        
        Cliente c =  recuperar_registro_cliente(i);
 
        while (j < 4){
            if (c.chaves_pix[j][0] != '\0'){
                strcpy(chaves_pix_idx[k].chave_pix,c.chaves_pix[j]);
                strcpy(chaves_pix_idx[k].pk_cliente,c.cpf);
 
                k++;
            }
            j++;
        }
    }
    qsort(chaves_pix_idx, qtd_chaves_pix, sizeof(chaves_pix_index), qsort_chaves_pix_index);
}
 
/* (Re)faz o índice secundário timestamp_cpf_origem_index */
void criar_timestamp_cpf_origem_index() {
    if (!timestamp_cpf_origem_idx)
        timestamp_cpf_origem_idx = malloc(MAX_REGISTROS * sizeof(timestamp_cpf_origem_index));
    
    if(!timestamp_cpf_origem_idx) {
        printf(ERRO_MEMORIA_INSUFICIENTE);
        exit(1);
    }
 
    for (unsigned i=0; i < qtd_registros_transacoes; i++){
        Transacao t = recuperar_registro_transacao(i);
 
        strcpy(timestamp_cpf_origem_idx[i].timestamp, t.timestamp); 
        strcpy(timestamp_cpf_origem_idx[i].cpf_origem, t.cpf_origem);
    }
 
    qsort(timestamp_cpf_origem_idx, qtd_registros_transacoes, sizeof(timestamp_cpf_origem_index), qsort_timestamp_cpf_origem_index);
}
 
/* Exibe um cliente dado seu RRN */
int exibir_cliente(int rrn) {
    if (rrn < 0)
        return 0;
 
    Cliente c = recuperar_registro_cliente(rrn);
 
    printf("%s, %s, %s, %s, %s, %.2lf, {", c.cpf, c.nome, c.nascimento, c.email, c.celular, c.saldo);
 
    int imprimiu = 0;
    for (int i = 0; i < 4; ++i) {
        if (c.chaves_pix[i][0] != '\0') {
            if (imprimiu)
                printf(", ");
            printf("%s", c.chaves_pix[i]);
            imprimiu = 1;
        }
    }
    printf("}\n");
 
    return 1;
}
 
/* Exibe uma transação dada seu RRN */
int exibir_transacao(int rrn) {
    if (rrn < 0)
        return 0;
 
    Transacao t = recuperar_registro_transacao(rrn);
 
    printf("%s, %s, %.2lf, %s\n", t.cpf_origem, t.cpf_destino, t.valor, t.timestamp);
 
    return 1;
}
 
/* Recupera do arquivo de clientes o registro com o RRN
 * informado e retorna os dados na struct Cliente */
Cliente recuperar_registro_cliente(int rrn) {
    Cliente c;
    char temp[TAM_REGISTRO_CLIENTE + 1], *p;
    strncpy(temp, ARQUIVO_CLIENTES + (rrn * TAM_REGISTRO_CLIENTE), TAM_REGISTRO_CLIENTE);
    temp[TAM_REGISTRO_CLIENTE] = '\0';
 
    p = strtok(temp, ";");
    strcpy(c.cpf, p);
    p = strtok(NULL, ";");
    strcpy(c.nome, p);
    p = strtok(NULL, ";");
    strcpy(c.nascimento, p);
    p = strtok(NULL, ";");
    strcpy(c.email, p);
    p = strtok(NULL, ";");
    strcpy(c.celular, p);
    p = strtok(NULL, ";");
    c.saldo = atof(p);
    p = strtok(NULL, ";");
 
    for(int i = 0; i < 4; ++i)
        c.chaves_pix[i][0] = '\0';
 
    if(p[0] != '#') {
        p = strtok(p, "&");
 
        for(int pos = 0; p; p = strtok(NULL, "&"), ++pos)
            strcpy(c.chaves_pix[pos], p);
    }
 
    return c;
}
 
/* Recupera do arquivo de transações o registro com o RRN
 * informado e retorna os dados na struct Transacao */
Transacao recuperar_registro_transacao(int rrn) {
    Transacao t;
    char temp[TAM_REGISTRO_TRANSACAO + 1], *p;
    strncpy(temp, ARQUIVO_TRANSACOES + (rrn * TAM_REGISTRO_TRANSACAO), TAM_REGISTRO_TRANSACAO);
    temp[TAM_ARQUIVO_TRANSACAO] = '\0';
    unsigned i = 0;
    char* aux;
 
    strncpy(t.cpf_origem, temp + i, 11);
    i += 11;
    strncpy(t.cpf_destino, temp + i, 11);
    i += 11;
    strncpy(aux, temp + i, 13);
    i += 13;
    t.valor = atof(aux);
    strncpy(t.timestamp, temp + i, 14);
    i+= 14;
 
    return t;
}
 
/* Funções principais */
void cadastrar_cliente(char *cpf, char *nome, char *nascimento, char *email, char *celular) {
    Cliente c;
    clientes_index c_idx;
 
    if (search_client_index(cpf) != NULL){
        printf(ERRO_PK_REPETIDA, cpf);
        return;
    }
 
    strcpy(c.cpf, cpf);
    strcpy(c.nome, nome);
    strcpy(c.nascimento, nascimento);
    strcpy(c.email, email);
    strcpy(c.celular, celular);
 
    c.saldo = 0.0;
 
    for(unsigned i; i< 4; i++)
        c.chaves_pix[i][0] = '\0';
 
    inserir_registro_cliente(c);
 
    strcpy(c_idx.cpf, c.cpf);
    c_idx.rrn = qtd_registros_clientes - 1;
 
    inserir_index_clientes( &c_idx);
    printf(SUCESSO);
 
}
 
void remover_cliente(char *cpf) {
    clientes_index * c;
    clientes_index * c_idx;
 
 
    c =  (clientes_index *)search_client_index(cpf);
    if ( c == NULL){
        printf(ERRO_REGISTRO_NAO_ENCONTRADO);
        return;
    }
 
    ARQUIVO_CLIENTES[c->rrn * TAM_REGISTRO_CLIENTE] = '*'; 
    ARQUIVO_CLIENTES[(c->rrn * TAM_REGISTRO_CLIENTE) + 1] = '|'; 
 
    c_idx->rrn = -1;
 
}
 
void alterar_saldo(char *cpf, double valor) {
    clientes_index * c_idx = malloc(sizeof(clientes_index));
    Cliente c;
    c_idx = (clientes_index *)search_client_index(cpf);
 
    if (c_idx == NULL){
        printf(ERRO_REGISTRO_NAO_ENCONTRADO);
        return;
    }
    c = recuperar_registro_cliente(c_idx->rrn);
    
    char temp[TAM_REGISTRO_CLIENTE + 1], *p;
    p = ARQUIVO_CLIENTES + (c_idx->rrn * TAM_REGISTRO_CLIENTE);
 
    int aux = 0;
    while (aux < 5){
        if(*p == ';')
            aux++;
        p++;
    }
    if (valor == 0.0000000){
        printf(ERRO_VALOR_INVALIDO);
        return;
    }
    if (c.saldo + valor < 0){
        printf(ERRO_SALDO_NAO_SUFICIENTE);
        return;
    }
    char buffer[14];
    sprintf(buffer, "%013.2f", c.saldo + valor);
 
    strncpy(p, buffer,13);
 
    printf(SUCESSO);
}
 
void cadastrar_chave_pix(char *cpf, char tipo) {
    clientes_index * c_idx;
    Cliente  c;
    c_idx = (clientes_index *)search_client_index(cpf);
 
    if (c_idx == NULL)
        printf(ERRO_REGISTRO_NAO_ENCONTRADO);
    
    c = recuperar_registro_cliente(c_idx->rrn);
    int i = 0;
 
    switch(tipo)
    {
    case 'N':
        for (; c.chaves_pix[i][0] != '\0';i++)
            if (c.chaves_pix[i][0] == 'N')
                printf(ERRO_CHAVE_PIX_DUPLICADA, c.chaves_pix[i][0]);
        strcpy(c.chaves_pix[i], c.celular);
        break;
    case 'C':
        for (; c.chaves_pix[i][0] != '\0';i++)
            if (c.chaves_pix[i][0] == 'C')
                printf(ERRO_CHAVE_PIX_DUPLICADA, c.chaves_pix[i][0]);
        strcpy(c.chaves_pix[i], c.cpf);
        break;
    case 'A':
        for (; c.chaves_pix[i][0] != '\0';i++)
            if (c.chaves_pix[i][0] == 'A')
                printf(ERRO_CHAVE_PIX_DUPLICADA, c.chaves_pix[i][0]);
        new_uuid(c.chaves_pix[i]);
        break;
    case 'E':
        for (; c.chaves_pix[i][0] != '\0';i++)
            if (c.chaves_pix[i][0] == 'E')
                printf(ERRO_CHAVE_PIX_DUPLICADA, c.chaves_pix[i][0]);
        strcpy(c.chaves_pix[i], c.email);
        break;
    default:
        printf(ERRO_CHAVE_PIX_INVALIDA);
        break;
    }
 
    chaves_pix_index cp;
 
    strcpy(cp.chave_pix, c.chaves_pix[i]);
    strcpy(cp.pk_cliente, c.cpf);
    inserir_index_chave_pix(&cp);
}
 
void transferir(char *chave_pix_origem, char *chave_pix_destino, double valor) {
    
}
 
void buscar_cliente_cpf(char *cpf) { 
    clientes_index * c_idx;
 
    c_idx = search_client_index(cpf);
 
    exibir_cliente(c_idx->rrn);
}
 
void buscar_cliente_chave_pix(char *chave_pix) {
    chaves_pix_index * cp_idx;
    clientes_index * c_idx;
 
    cp_idx = (search_chaves_pix_index(chave_pix));
 
    c_idx = search_client_index(cp_idx->pk_cliente);
    
    exibir_cliente(c_idx->rrn);
}
 
void buscar_transacao_cpf_origem_timestamp(char *cpf, char *timestamp) {
    transacoes_index * t_idx;
 
    t_idx = search_transacoes_index(cpf, timestamp);
 
    exibir_transacao(t_idx->rrn);
}
 
/* Listagem */
void listar_clientes_cpf() {
    int count = 0;
    for (unsigned i=0; i < qtd_registros_clientes; i++){
        exibir_cliente(clientes_idx[i].rrn);
        count++;
    }
 
    if (count == 0)
        printf(AVISO_NENHUM_REGISTRO_ENCONTRADO);
}
 
void listar_transacoes_periodo(char *data_inicio, char *data_fim) {
    int count = 0;
 
    for (unsigned i=0; i < qtd_registros_transacoes; i++){
        if (strcmp(transacoes_idx[i].timestamp, data_inicio) <= 0 && strcmp(transacoes_idx[i].timestamp, data_fim) >= 0){
            exibir_transacao(transacoes_idx[i].rrn);
            count++;
        }
    }
 
    if (count == 0)
        printf(AVISO_NENHUM_REGISTRO_ENCONTRADO);
}
 
void listar_transacoes_cpf_origem_periodo(char *cpf, char *data_inicio, char *data_fim) {
    int count = 0;
 
    for (unsigned i=0; i < qtd_registros_transacoes; i++){
        if (strcmp(transacoes_idx[i].timestamp, data_inicio) <= 0 && strcmp(transacoes_idx[i].timestamp, data_fim) >= 0)
            if (strcmp(cpf, transacoes_idx[i].cpf_origem) == 0){
                exibir_transacao(transacoes_idx[i].rrn);
                count++;
            }
    }
 
    if(count == 0)
        printf(AVISO_NENHUM_REGISTRO_ENCONTRADO);
 
}
 
/* Liberar espaço */
void liberar_espaco() {
    char *p = ARQUIVO_CLIENTES;
    int apagados = 0;
    char *aux;
    for (unsigned i = 0; i < qtd_registros_clientes; i++){
        aux = (p  + (i* TAM_REGISTRO_CLIENTE));
        unsigned j = 0;
        while(aux[j + TAM_REGISTRO_CLIENTE] == '*'){
            apagados++;
            j++;
        }
        strncpy(aux, aux + (apagados * TAM_REGISTRO_CLIENTE),TAM_REGISTRO_CLIENTE);
    }
 
    criar_clientes_index();
    criar_chaves_pix_index();
 
    p = ARQUIVO_TRANSACOES;
    apagados = 0;
    for (unsigned i = 0; i < qtd_registros_transacoes; i++){
        aux = (p  + (i* TAM_REGISTRO_TRANSACAO));
        unsigned j = 0;
        while(aux[j + TAM_REGISTRO_TRANSACAO] == '*'){
            apagados++;
            j++;
        }
        strncpy(aux, aux + (apagados * TAM_REGISTRO_TRANSACAO),TAM_REGISTRO_TRANSACAO);
    }
 
    criar_transacoes_index();
    criar_timestamp_cpf_origem_index();
 
    printf(SUCESSO);
}
 
/* Imprimir arquivos de dados */
void imprimir_arquivo_clientes() {
    if (qtd_registros_clientes == 0)
        printf(ERRO_ARQUIVO_VAZIO);
    else
        printf("%s\n", ARQUIVO_CLIENTES);
}
 
void imprimir_arquivo_transacoes() {
    if (qtd_registros_transacoes == 0)
        printf(ERRO_ARQUIVO_VAZIO);
    else
        printf("%s\n", ARQUIVO_TRANSACOES);
}
 
/* Imprimir índices secundários */
void imprimir_chaves_pix_index() {
    if (qtd_chaves_pix == 0)
        printf(ERRO_ARQUIVO_VAZIO);
 
    for (unsigned i = 0; i < qtd_chaves_pix; ++i)
        printf("%s, %s\n", chaves_pix_idx[i].chave_pix, chaves_pix_idx[i].pk_cliente);
}
 
void imprimir_timestamp_cpf_origem_index() {
    if (qtd_registros_transacoes == 0)
        printf(ERRO_ARQUIVO_VAZIO);
 
    for (unsigned i = 0; i < qtd_registros_transacoes; ++i)
        printf("%s, %s\n", timestamp_cpf_origem_idx[i].timestamp, timestamp_cpf_origem_idx[i].cpf_origem);
}
 
/* Liberar memória e encerrar programa */
void liberar_memoria() {
    free(chaves_pix_idx);
    free(clientes_idx);
    free(transacoes_idx);
    free(timestamp_cpf_origem_idx);
}
 
/* Função de comparação entre clientes_index */
int qsort_clientes_index(const void *a, const void *b) {
    return strcmp(( (clientes_index *)a )->cpf, ( (clientes_index*)b )->cpf);
}
 
/* Função de comparação entre transacoes_index */
int qsort_transacoes_index(const void *a, const void *b) {
    int cmp;
    cmp = strcmp(( (transacoes_index *)a )->cpf_origem, ( (transacoes_index*)b )->cpf_origem);
    
    if (0 !=  cmp)
        return cmp;
    
    return strcmp(( (transacoes_index *)a )->timestamp, ( (transacoes_index*)b )->timestamp);
}
 
/* Função de comparação entre chaves_pix_index */
int qsort_chaves_pix_index(const void *a, const void *b) {
    return strcmp(( (chaves_pix_index *)a )->chave_pix, ( (chaves_pix_index*)b )->chave_pix);
}
 
/* Função de comparação entre timestamp_cpf_origem_index */
int qsort_timestamp_cpf_origem_index(const void *a, const void *b) {
    int cmp;
    cmp = strcmp(( (timestamp_cpf_origem_index *)a )->timestamp, ( (timestamp_cpf_origem_index*)b )->timestamp);
    
    if (0 !=  cmp)
        return cmp;
    
    return strcmp(( (timestamp_cpf_origem_index *)a )->cpf_origem, ( (timestamp_cpf_origem_index*)b )->cpf_origem);
}
 
void inserir_registro_cliente(Cliente c){
    char * p = ARQUIVO_CLIENTES;
    char chaves[TAM_MAX_CHAVE_PIX * 4];
    char * aux_chaves_pix = chaves; 
    unsigned i=0;
    int filers = 0;
 
    while(c.chaves_pix[i][0] != '\0' && i < 4 ){
        if (i > 0)
            aux_chaves_pix[strlen(c.chaves_pix[i-1])] = '&';
 
        aux_chaves_pix = strcat(aux_chaves_pix, c.chaves_pix[i]); 
    }
    
    sprintf(p + (TAM_REGISTRO_CLIENTE * qtd_registros_clientes), 
            "%s;%s;%s;%s;%s;%013.2f;%s;", 
            c.cpf, 
            c.nome, 
            c.nascimento, 
            c.email, 
            c.celular, 
            c.saldo, 
            aux_chaves_pix
            );
 
    filers =  45 + strlen(c.nome) + strlen(c.email) + strlen(aux_chaves_pix)+4+2+1;
 
    while(filers < 256){
        p[(TAM_REGISTRO_CLIENTE * qtd_registros_clientes) + filers] = '#';
        filers++;
    }
    qtd_registros_clientes++;
}
 
void inserir_registro_transacao(Transacao t){
 
}
 
clientes_index * search_client_index(char *cpf){
    clientes_index  c_idx;
 
    strcpy(c_idx.cpf, cpf);
 
    return bsearch(&c_idx, clientes_idx, qtd_registros_clientes, sizeof(clientes_index), qsort_clientes_index);
}
 
transacoes_index * search_transacoes_index(char * cpf, char * timestamp){
    transacoes_index * t_idx;
 
    strcpy(t_idx->cpf_origem, cpf);
    strcpy(t_idx->timestamp, timestamp);
 
    return bsearch(t_idx, transacoes_idx, qtd_registros_transacoes, sizeof(transacoes_idx), qsort_transacoes_index);
}
 
chaves_pix_index * search_chaves_pix_index(char * chave){
    chaves_pix_index * c_idx;
    strcpy(c_idx->chave_pix, chave);
 
     return bsearch(c_idx, chaves_pix_idx, qtd_chaves_pix, sizeof(chaves_pix_idx), qsort_chaves_pix_index);
}
 
 
 
void inserir_index_clientes(clientes_index *c){
    int i = 0;
    if (qtd_registros_clientes == 1){
        strcpy(clientes_idx[0].cpf, c->cpf);
        clientes_idx[0].rrn = c->rrn;
        return;
    }
 
    for (i = qtd_registros_clientes - 2; i >= 0 && qsort_clientes_index(clientes_idx + i, c) > 0; i--){
        clientes_idx[i + 1].rrn = clientes_idx[i].rrn;
        strcpy(clientes_idx[i + 1].cpf, clientes_idx[i].cpf);
    }
    strcpy(clientes_idx[i+1].cpf, c->cpf);
    clientes_idx[i+1].rrn = c->rrn;
}
 
void inserir_index_transacoes(transacoes_index *t){
    int i = 0;
    if (qtd_registros_transacoes ==1){
        strcpy(transacoes_idx[0].cpf_origem, t->cpf_origem);
        strcpy(transacoes_idx[0].timestamp, t->timestamp);
        transacoes_idx[0].rrn = t->rrn;
        return;
    }
 
    for (i = qtd_registros_clientes - 2; i >= 0 && qsort_transacoes_index(transacoes_idx + i, t) > 0; i--){
        strcpy(transacoes_idx[i+1].cpf_origem, transacoes_idx[i].cpf_origem);
        strcpy(transacoes_idx[i+1].timestamp, transacoes_idx[i].timestamp);
        transacoes_idx[i].rrn = t->rrn;
    }
    strcpy(transacoes_idx[i].cpf_origem, t->cpf_origem);
    strcpy(transacoes_idx[i].timestamp, t->timestamp);
    transacoes_idx[i].rrn = t->rrn;
 
}
 
void inserir_index_chave_pix(chaves_pix_index *p){
    int i = 0;
    for (i = qtd_chaves_pix -1;  i >= 0 && qsort_chaves_pix_index(chaves_pix_idx + 1, p) > 0; i--)
        chaves_pix_idx[i+1] = chaves_pix_idx[i];
    
    chaves_pix_idx[i+1] = *p;
}
 
void inserir_index_timestamp_cpf_origem_index( timestamp_cpf_origem_index *tc){
    int i=0;
    for (i = qtd_chaves_pix -1; i >= 0 && qsort_timestamp_cpf_origem_index(timestamp_cpf_origem_idx +1, tc) > 0; i++)
        timestamp_cpf_origem_idx[i] = timestamp_cpf_origem_idx[i];
    
    timestamp_cpf_origem_idx[i] = *tc;
}