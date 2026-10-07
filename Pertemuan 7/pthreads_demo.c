#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define N_FAKTORIAL 10
#define N_FIBONACCI 15
#define NAMA_FILE "data.txt"
#ifndef ITERASI_UJI
#define ITERASI_UJI 1000000 /* tiap thread menambah counter sebanyak ini */
#endif

/* ---------- Data bersama ---------- */
static long total_langkah = 0; /* shared variable */
static pthread_mutex_t mutex_langkah = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_cetak = PTHREAD_MUTEX_INITIALIZER;

/* ---------- Fungsi sinkronisasi ---------- */
/* Menambah total_langkah. Bagian ini adalah CRITICAL SECTION. */
static void tambah_langkah(void)
{
#ifndef TANPA_MUTEX
    pthread_mutex_lock(&mutex_langkah); /* masuk critical section */
#endif
    total_langkah++; /* read-modify-write (tidak atomik) */
#ifndef TANPA_MUTEX
    pthread_mutex_unlock(&mutex_langkah); /* keluar critical section */
#endif
}

/* Mencetak dengan lock agar output antar-thread tidak bercampur. */
static void cetak(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void cetak(const char *fmt, ...)
{
    va_list args;
    pthread_mutex_lock(&mutex_cetak);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
    pthread_mutex_unlock(&mutex_cetak);
}

/* ---------- Thread 1: Faktorial ---------- */
static void *thread_faktorial(void *arg)
{
    int n = *(int *)arg;
    unsigned long long hasil = 1;

    for (int i = 2; i <= n; i++)
    {
        hasil *= i;
        tambah_langkah();
    }
    cetak("[Thread 1 - Faktorial] %d! = %llu\n", n, hasil);

    for (int i = 0; i < ITERASI_UJI; i++) /* beban uji untuk race condition */
        tambah_langkah();
    return NULL;
}

/* ---------- Thread 2: Fibonacci ---------- */
static void *thread_fibonacci(void *arg)
{
    int n = *(int *)arg;
    unsigned long long a = 0, b = 1;
    char buf[512] = "";
    char tmp[32];

    for (int i = 0; i < n; i++)
    {
        snprintf(tmp, sizeof tmp, "%llu ", a);
        strncat(buf, tmp, sizeof buf - strlen(buf) - 1);
        unsigned long long next = a + b;
        a = b;
        b = next;
        tambah_langkah();
    }
    cetak("[Thread 2 - Fibonacci] %d suku pertama: %s\n", n, buf);

    for (int i = 0; i < ITERASI_UJI; i++)
        tambah_langkah();
    return NULL;
}

/* ---------- Thread 3: Baca file teks ---------- */
static void *thread_baca_file(void *arg)
{
    const char *nama = (const char *)arg;
    FILE *fp = fopen(nama, "r");
    if (!fp)
    {
        cetak("[Thread 3 - Baca File] Gagal membuka '%s'\n", nama);
        return NULL;
    }

    char baris[256];
    int no = 0, kata = 0;
    cetak("[Thread 3 - Baca File] Isi file '%s':\n", nama);
    while (fgets(baris, sizeof baris, fp))
    {
        no++;
        baris[strcspn(baris, "\n")] = '\0';
        cetak("    %d: %s\n", no, baris);

        char salin[256];
        strcpy(salin, baris);
        for (char *t = strtok(salin, " "); t; t = strtok(NULL, " "))
            kata++;
        tambah_langkah();
    }
    fclose(fp);
    cetak("[Thread 3 - Baca File] Total %d baris, %d kata\n", no, kata);

    for (int i = 0; i < ITERASI_UJI; i++)
        tambah_langkah();
    return NULL;
}

/* ---------- main ---------- */
int main(void)
{
    pthread_t t1, t2, t3;
    int n_fakt = N_FAKTORIAL, n_fib = N_FIBONACCI;

    /* Buat data.txt contoh jika belum ada */
    FILE *cek = fopen(NAMA_FILE, "r");
    if (!cek)
    {
        FILE *w = fopen(NAMA_FILE, "w");
        if (w)
        {
            fputs("Pthreads adalah standar POSIX untuk thread.\n"
                  "Race condition terjadi tanpa sinkronisasi.\n",
                  w);
            fclose(w);
        }
    }
    else
    {
        fclose(cek);
    }

    printf("=== Program Pthreads: 3 thread, 3 tugas berbeda ===\n");

    if (pthread_create(&t1, NULL, thread_faktorial, &n_fakt) != 0 ||
        pthread_create(&t2, NULL, thread_fibonacci, &n_fib) != 0 ||
        pthread_create(&t3, NULL, thread_baca_file, (void *)NAMA_FILE) != 0)
    {
        perror("pthread_create");
        return EXIT_FAILURE;
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    long harapan = (N_FAKTORIAL - 1) + N_FIBONACCI + 4 + 3L * ITERASI_UJI;
    /* 4 = jumlah baris data.txt bawaan; berubah jika isi file berbeda */

    printf("\n--- Hasil sinkronisasi ---\n");
    printf("total_langkah (aktual)  : %ld\n", total_langkah);
    printf("total_langkah (harapan) : %ld  (jika data.txt berisi 4 baris)\n", harapan);
#ifdef TANPA_MUTEX
    printf("Mode: TANPA mutex -> hasil bisa salah (race condition)\n");
#else
    printf("Mode: DENGAN mutex -> hasil selalu konsisten\n");
#endif

    pthread_mutex_destroy(&mutex_langkah);
    pthread_mutex_destroy(&mutex_cetak);
    return EXIT_SUCCESS;
}
