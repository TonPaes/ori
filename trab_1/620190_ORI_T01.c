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

    sprintf(buffer, "%08x-%04x-%04llx-%04llx-%012llx", (uint32_t)(r1 >> 32), (uint16_t)(r1 >> 16), 0x4000 | (r1 & 0x0fff), 0x8000 | (r2 & 0x3fff), r2 >> 16);
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
    if (!clientes_idx)
        clientes_idx = malloc(MAX_REGISTROS * sizeof(clientes_index));

    if (!clientes_idx) {
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
    
    if(!clientes_idx) {
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
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "criar_chaves_pix_index");
}

/* (Re)faz o índice secundário timestamp_cpf_origem_index */
void criar_timestamp_cpf_origem_index() {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "criar_timestamp_cpf_origem_index");
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
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "recuperar_registro_transacao");
}

/* Funções principais */
void cadastrar_cliente(char *cpf, char *nome, char *nascimento, char *email, char *celular) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "cadastrar_cliente");
}

void remover_cliente(char *cpf) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "remover_cliente");
}

void alterar_saldo(char *cpf, double valor) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "alterar_saldo");
}

void cadastrar_chave_pix(char *cpf, char tipo) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "cadastrar_chave_pix");
}

void transferir(char *chave_pix_origem, char *chave_pix_destino, double valor) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "transferir");
}

/* Busca */
void buscar_cliente_cpf(char *cpf) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "buscar_cliente_cpf");
}

void buscar_cliente_chave_pix(char *chave_pix) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "buscar_cliente_chave_pix");
}

void buscar_transacao_cpf_origem_timestamp(char *cpf, char *timestamp) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "buscar_transacao_cpf_origem_timestamp");
}

/* Listagem */
void listar_clientes_cpf() {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "listar_clientes_cpf");
}

void listar_transacoes_periodo(char *data_inicio, char *data_fim) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "listar_transacoes_periodo");
}

void listar_transacoes_cpf_origem_periodo(char *cpf, char *data_inicio, char *data_fim) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "listar_transacoes_cpf_origem_periodo");
}

/* Liberar espaço */
void liberar_espaco() {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "liberar_espaco");
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
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "liberar_memoria");
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
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "qsort_chaves_pix_index");
}

/* Função de comparação entre timestamp_cpf_origem_index */
int qsort_timestamp_cpf_origem_index(const void *a, const void *b) {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "qsort_timestamp_cpf_origem_index");
}