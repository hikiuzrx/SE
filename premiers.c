/*
 * TP1 - Systemes d'exploitation (M1 IV) - USTHB 2026/2027
 * Crible parallele des nombres premiers entre 2 et n.
 *
 * Synchronisation sans signaux :
 *   fork()    : P cree P1, P1 cree P2
 *   waitpid() : P1 attend P2, P attend P1
 *   exit()    : P2 renvoie le prochain premier a P1 par son code de retour,
 *               P1 le renvoie a son tour a P (0 = plus de premier)
 *
 * Un code de retour tient sur 8 bits (0..255), d'ou la limite n <= 65535 :
 * un premier utile verifie p*p <= n, donc p <= 255.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <math.h>

#define F  "f.txt"
#define F1 "f1.txt"
#define F2 "f2.txt"
#define N_MAX 65535

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

/* Une ligne de log : etiquette coloree, pid, message */
#define LOG(color, tag, fmt, ...) \
    printf(color BOLD tag RESET DIM " %6d │ " RESET fmt "\n", getpid(), ##__VA_ARGS__)

#define LOG_P(fmt, ...)  LOG(BLUE,    "P     ", fmt, ##__VA_ARGS__)
#define LOG_P1(fmt, ...) LOG(GREEN,   "  P1  ", fmt, ##__VA_ARGS__)
#define LOG_P2(fmt, ...) LOG(MAGENTA, "    P2", fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) LOG(RED,    "ERREUR", RED fmt RESET, ##__VA_ARGS__)

static FILE *open_file(const char *name, const char *mode)
{
    FILE *fp = fopen(name, mode);
    if (!fp)
        LOG_ERR("impossible d'ouvrir %s", name);
    return fp;
}

/* Affiche le contenu d'un fichier sur une ligne */
static void show_file(const char *name)
{
    FILE *fp = fopen(name, "r");
    if (!fp)
        return;

    int x, count = 0;
    printf(CYAN "         └─ %-6s" RESET " : ", name);
    while (fscanf(fp, "%d", &x) == 1) {
        printf("%d ", x);
        count++;
    }
    printf(DIM "(%d nombres)" RESET "\n", count);
    fclose(fp);
}

/*
 * P2 : ecrit dans f2 les multiples de q, et cherche le prochain premier
 * (premier nombre > q divisible ni par p ni par q) qu'il renvoie a P1.
 */
static void run_p2(int p, int q)
{
    LOG_P2("démarre avec q = " BOLD "%d" RESET, q);

    FILE *f = open_file(F, "r");
    FILE *f2 = open_file(F2, "w");
    int x, next = 0, count = 0;

    while (f && f2 && fscanf(f, "%d", &x) == 1) {
        if (x <= q)
            continue;
        if (x % q == 0) {
            fprintf(f2, "%d\n", x);
            count++;
        } else if (!next && x % p != 0) {
            next = x;
        }
    }
    if (f)
        fclose(f);
    if (f2)
        fclose(f2);

    if (next > 255)
        next = 0;   /* next*next > N_MAX >= n : inutile */

    LOG_P2("%d multiples de %d écrits dans %s", count, q, F2);
    LOG_P2("prochain premier = " BOLD "%d" RESET " → renvoyé à P1", next);

    fflush(stdout);
    _exit(next);   /* _exit : ne pas vider f1, herite de P1 */
}

/*
 * P1 : ecrit dans f1 les multiples de p. Au premier nombre non divisible
 * par p, il cree P2 avec ce nombre, puis continue en parallele.
 * Il attend P2 et renvoie a P le premier que P2 lui a renvoye.
 */
static void run_p1(int p)
{
    LOG_P1("démarre avec p = " BOLD "%d" RESET, p);

    FILE *f = open_file(F, "r");
    FILE *f1 = open_file(F1, "w");
    if (!f || !f1)
        exit(0);   /* 0 : P s'arrete */

    pid_t p2 = 0;
    int x, count = 0;

    while (fscanf(f, "%d", &x) == 1) {
        if (x <= p)
            continue;
        if (x % p == 0) {
            fprintf(f1, "%d\n", x);
            count++;
        } else if (!p2) {
            LOG_P1("%d non divisible par %d → création de P2", x, p);
            fflush(stdout);   /* sinon P2 herite des logs non affiches */
            p2 = fork();
            if (p2 == 0)
                run_p2(p, x);
            if (p2 < 0)
                LOG_ERR("échec de la création de P2");
        }
    }
    fclose(f);
    fclose(f1);

    LOG_P1("%d multiples de %d écrits dans %s", count, p, F1);
    int next = 0;   /* reste 0 si P2 n'a pas ete cree */
    if (p2 > 0) {
        LOG_P1("attend la fin de P2 (pid %d)", p2);
        int status;
        waitpid(p2, &status, 0);
        next = WEXITSTATUS(status);
        LOG_P1("P2 terminé, a renvoyé " BOLD "%d" RESET " → renvoyé à P", next);
    }
    exit(next);
}

/* P : retire de f les nombres presents dans f1 et f2 */
static void update_f(int n)
{
    char *present = calloc(n + 1, 1);
    const char *names[] = { F, F1, F2 };
    int x;

    for (int i = 0; i < 3; i++) {
        FILE *fp = fopen(names[i], "r");
        if (!fp)
            continue;
        while (fscanf(fp, "%d", &x) == 1)
            present[x] = (i == 0);   /* f ajoute, f1 et f2 retirent */
        fclose(fp);
    }

    FILE *f = open_file(F, "w");
    for (x = 2; f && x <= n; x++)
        if (present[x])
            fprintf(f, "%d\n", x);
    if (f)
        fclose(f);
    free(present);
}

int main(int argc, char *argv[])
{
    int n = argc > 1 ? atoi(argv[1]) : 100;
    if (n < 2 || n > N_MAX) {
        LOG_ERR("n doit être entre 2 et %d (usage : %s n)", N_MAX, argv[0]);
        return 1;
    }

    LOG_P("nombres premiers entre 2 et " BOLD "%d" RESET, n);
    FILE *f = open_file(F, "w");
    if (!f)
        return 1;
    for (int i = 2; i <= n; i++)
        fprintf(f, "%d\n", i);
    fclose(f);
    LOG_P("%s initialisé avec 2..%d", F, n);

    int p = 2;
    int limit = (int)sqrt(n);
    for (int it = 1; p && p <= limit; it++) {
        printf(YELLOW BOLD "\n━━━━━━━━ Itération %d · p = %d ━━━━━━━━\n" RESET, it, p);
        remove(F2);   /* eviter de reutiliser le f2 de l'iteration precedente */

        fflush(stdout);   /* sinon P1 herite des logs non affiches */
        pid_t p1 = fork();
        if (p1 < 0) {
            LOG_ERR("échec de la création de P1");
            return 1;
        }
        if (p1 == 0)
            run_p1(p);
        LOG_P("P1 créé (pid %d), en attente de sa fin…", p1);

        int status;
        waitpid(p1, &status, 0);
        int next = WEXITSTATUS(status);
        LOG_P("P1 et P2 terminés, P1 a renvoyé " BOLD "%d" RESET, next);

        show_file(F1);
        show_file(F2);
        update_f(n);
        LOG_P("%s mis à jour", F);
        show_file(F);

        p = next;
    }

    printf(YELLOW BOLD "\n━━━━━━━━ Résultat ━━━━━━━━\n" RESET);
    if (p)
        LOG_P("p = %d et p² = %d > %d → arrêt", p, p * p, n);
    LOG_P("nombres premiers entre 2 et %d :", n);
    show_file(F);
    return 0;
}
