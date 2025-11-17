/*
 * Контрольная работа по дисциплине "Операционные системы"
 * Вариант 29
 *
 * Задание А: Генеалогическое дерево процессов
 *   Оранжевый (root)
 *     ├── Желтый
 *     │     ├── Зеленый
 *     │     └── Голубой
 *     ├── Голубой
 *     └── Голубой
 *
 * Задание Б (вариант 5, 8, 13, 21, 29, 37, 39):
 * Жёлтый получает со стандартного потока ввода список всех активных процессов:
 *   1) Выводит на экран процессы только с четными PID
 *   2) Передаёт при помощи семафоров оранжевому каждый PID
 *      (оранжевый суммирует и выводит каждый раз полученное значение и текущую сумму)
 *   3) Жёлтый при помощи pipe передаёт имена чётных процессов зелёному,
 *      а тот записывает их в файл
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/mman.h>

#define MAX_LINE 256
#define SEM_ORANGE "/sem_orange_29"
#define SEM_YELLOW "/sem_yellow_29"
#define OUTPUT_FILE "even_processes.txt"

/* Цвета процессов для идентификации */
enum ProcessColor {
    ORANGE,  /* Оранжевый - корневой */
    YELLOW,  /* Желтый */
    GREEN,   /* Зеленый */
    BLUE     /* Голубой */
};

const char* color_names[] = {"ОРАНЖЕВЫЙ", "ЖЁЛТЫЙ", "ЗЕЛЁНЫЙ", "ГОЛУБОЙ"};

/* Прототипы функций */
void orange_process(void);
void yellow_process(int pipe_to_green[2]);
void green_process(int pipe_from_yellow[2]);
void blue_process(void);

int main() {
    pid_t pid;
    int pipe_yellow_green[2]; /* pipe от желтого к зеленому */

    printf("[PID=%d, PPID=%d] Процесс %s запущен\n",
           getpid(), getppid(), color_names[ORANGE]);
    fflush(stdout);

    /* Удаляем старые семафоры если они существуют */
    sem_unlink(SEM_ORANGE);
    sem_unlink(SEM_YELLOW);

    /* Создаем pipe ДО fork */
    if (pipe(pipe_yellow_green) < 0) {
        perror("Ошибка создания pipe");
        exit(1);
    }

    /* Создаем ЖЕЛТЫЙ процесс */
    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        perror("Ошибка fork для жёлтого");
        exit(1);
    } else if (pid == 0) {
        /* Желтый процесс */
        close(pipe_yellow_green[0]); /* Закрываем чтение */
        yellow_process(pipe_yellow_green);
        exit(0);
    }

    /* Создаем ГОЛУБЫЕ процессы */
    for (int i = 0; i < 2; i++) {
        fflush(stdout);
        pid = fork();
        if (pid < 0) {
            perror("Ошибка fork для голубого");
            exit(1);
        } else if (pid == 0) {
            /* Голубой процесс */
            close(STDIN_FILENO);
            close(pipe_yellow_green[0]);
            close(pipe_yellow_green[1]);
            blue_process();
            exit(0);
        }
    }

    /* Оранжевый процесс закрывает pipe (он ему не нужен) */
    close(pipe_yellow_green[0]);
    close(pipe_yellow_green[1]);
    close(STDIN_FILENO);

    /* Создаем семафоры в ОРАНЖЕВОМ процессе ПОСЛЕ всех fork'ов */
    sem_t *sem_orange = sem_open(SEM_ORANGE, O_CREAT | O_EXCL, 0666, 0);
    sem_t *sem_yellow = sem_open(SEM_YELLOW, O_CREAT | O_EXCL, 0666, 0);
    if (sem_orange == SEM_FAILED || sem_yellow == SEM_FAILED) {
        perror("Ошибка создания семафоров");
        exit(1);
    }

    /* Выполняем действия оранжевого процесса */
    orange_process();

    /* Ожидаем завершения всех дочерних процессов */
    int status;
    while (wait(&status) > 0 || errno != ECHILD) {
        /* wait until all children are done */
    }

    printf("[PID=%d, PPID=%d] Процесс %s завершён\n",
           getpid(), getppid(), color_names[ORANGE]);

    /* Очистка семафоров */
    sem_close(sem_orange);
    sem_close(sem_yellow);
    sem_unlink(SEM_ORANGE);
    sem_unlink(SEM_YELLOW);

    return 0;
}

/* Оранжевый процесс - получает PID'ы через семафоры и суммирует */
void orange_process(void) {
    sem_t *sem_orange, *sem_yellow;
    int total_sum = 0;
    int pid_value;
    int count = 0;

    /* Открываем уже созданные семафоры (созданы в main) */
    sem_orange = sem_open(SEM_ORANGE, 0);
    sem_yellow = sem_open(SEM_YELLOW, 0);

    if (sem_orange == SEM_FAILED || sem_yellow == SEM_FAILED) {
        perror("Ошибка открытия семафоров в оранжевом");
        return;
    }

    printf("[PID=%d, PPID=%d] %s: Ожидание PID от жёлтого через семафоры\n",
           getpid(), getppid(), color_names[ORANGE]);
    fflush(stdout);

    /* Получаем PID'ы через семафорную синхронизацию */
    while (1) {
        /* Ждем сигнала от желтого */
        sem_wait(sem_orange);

        /* Используем временный файл с уникальным именем */
        char filename[64];
        snprintf(filename, sizeof(filename), "/tmp/shared_pid_29_%d.txt", getpid());

        FILE *f = fopen(filename, "r");
        if (f) {
            if (fscanf(f, "%d", &pid_value) == 1) {
                if (pid_value == -1) {
                    fclose(f);
                    remove(filename); /* Удаляем временный файл */
                    sem_post(sem_yellow);
                    break; /* Конец передачи */
                }
                total_sum += pid_value;
                count++;
                printf("[PID=%d, PPID=%d] %s: Получен PID=%d, текущая сумма=%d\n",
                       getpid(), getppid(), color_names[ORANGE], pid_value, total_sum);
                fflush(stdout);
            }
            fclose(f);
            remove(filename); /* Удаляем временный файл после чтения */
        }

        /* Сигнализируем желтому, что приняли */
        sem_post(sem_yellow);
    }

    printf("[PID=%d, PPID=%d] %s: Итоговая сумма чётных PID = %d (обработано=%d)\n",
           getpid(), getppid(), color_names[ORANGE], total_sum, count);

    sem_close(sem_orange);
    sem_close(sem_yellow);
}

/* Желтый процесс */
void yellow_process(int pipe_to_green[2]) {
    pid_t pid;
    sem_t *sem_orange, *sem_yellow;
    char line[MAX_LINE];
    int even_count = 0;

    printf("[PID=%d, PPID=%d] Процесс %s запущен\n",
           getpid(), getppid(), color_names[YELLOW]);
    fflush(stdout);

    /* Даем время оранжевому создать семафоры и инициализироваться */
    usleep(500000); /* 500ms */

    /* Открываем семафоры */
    sem_orange = sem_open(SEM_ORANGE, 0);
    sem_yellow = sem_open(SEM_YELLOW, 0);

    if (sem_orange == SEM_FAILED || sem_yellow == SEM_FAILED) {
        perror("Ошибка открытия семафоров в жёлтом");
        close(pipe_to_green[1]);
        return;
    }

    /* Создаем ЗЕЛЕНЫЙ процесс как потомок желтого */
    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        perror("Ошибка fork для зелёного от жёлтого");
        close(pipe_to_green[1]);
        sem_close(sem_orange);
        sem_close(sem_yellow);
        return;
    } else if (pid == 0) {
        /* Зеленый процесс */
        close(STDIN_FILENO);
        close(pipe_to_green[1]); /* Закрываем запись */
        sem_close(sem_orange);
        sem_close(sem_yellow);
        green_process(pipe_to_green);
        exit(0);
    }

    printf("[PID=%d, PPID=%d] %s: Чтение списка процессов из stdin...\n",
           getpid(), getppid(), color_names[YELLOW]);
    fflush(stdout);

    /* Читаем со стандартного потока ввода список процессов */
    while (fgets(line, sizeof(line), stdin) != NULL) {
        int pid_val;
        char name[MAX_LINE];

        /* Пытаемся распарсить строку формата "PID NAME" или вывод ps */
        if (sscanf(line, "%d %255s", &pid_val, name) >= 1) {
            /* Проверяем, четный ли PID */
            if (pid_val > 0 && pid_val % 2 == 0) {
                printf("[PID=%d, PPID=%d] %s: Найден чётный PID=%d (%s)\n",
                       getpid(), getppid(), color_names[YELLOW], pid_val, name);
                fflush(stdout);
                even_count++;

                /* Используем уникальное имя файла для каждой передачи */
                char filename[64];
                snprintf(filename, sizeof(filename), "/tmp/shared_pid_29_%d.txt", getppid());

                FILE *f = fopen(filename, "w");
                if (f) {
                    fprintf(f, "%d\n", pid_val);
                    fclose(f);

                    sem_post(sem_orange);
                    sem_wait(sem_yellow);

                    /* Передаем имя зеленому */
                    dprintf(pipe_to_green[1], "%s\n", name);
                }
            }
        }
    }

    printf("[PID=%d, PPID=%d] %s: Чтение завершено, отправка сигнала завершения\n",
           getpid(), getppid(), color_names[YELLOW]);

    /* Сигнализируем конец передачи оранжевому */
    char filename[64];
    snprintf(filename, sizeof(filename), "/tmp/shared_pid_29_%d.txt", getppid());

    FILE *f = fopen(filename, "w");
    if (f) {
        fprintf(f, "-1\n");
        fclose(f);

        sem_post(sem_orange);
        sem_wait(sem_yellow);
    }

    /* Закрываем pipe для записи (сигнал EOF для зеленого) */
    close(pipe_to_green[1]);

    /* Создаем ГОЛУБОЙ процесс как потомок желтого */
    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        perror("Ошибка fork для голубого от жёлтого");
    } else if (pid == 0) {
        /* Голубой процесс */
        close(STDIN_FILENO);
        sem_close(sem_orange);
        sem_close(sem_yellow);
        blue_process();
        exit(0);
    }

    /* Закрываем семафоры */
    sem_close(sem_orange);
    sem_close(sem_yellow);

    /* Ожидаем завершения голубого */
    wait(NULL);

    printf("[PID=%d, PPID=%d] Процесс %s завершён (обработано %d чётных PID)\n",
           getpid(), getppid(), color_names[YELLOW], even_count);
}

/* Зеленый процесс */
void green_process(int pipe_from_yellow[2]) {
    int fd;
    char buffer[MAX_LINE];
    ssize_t bytes_read;
    int total_bytes = 0;

    printf("[PID=%d, PPID=%d] Процесс %s запущен\n",
           getpid(), getppid(), color_names[GREEN]);
    fflush(stdout);

    /* Открываем файл для записи */
    fd = open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("Ошибка открытия выходного файла");
        close(pipe_from_yellow[0]);
        return;
    }

    printf("[PID=%d, PPID=%d] %s: Запись имён процессов в файл '%s'\n",
           getpid(), getppid(), color_names[GREEN], OUTPUT_FILE);

    /* Читаем имена процессов из pipe и записываем в файл */
    while ((bytes_read = read(pipe_from_yellow[0], buffer, sizeof(buffer)-1)) > 0) {
        write(fd, buffer, bytes_read);
        total_bytes += bytes_read;
    }

    close(fd);
    close(pipe_from_yellow[0]);

    printf("[PID=%d, PPID=%d] Процесс %s завершён (записано %d байт в файл)\n",
           getpid(), getppid(), color_names[GREEN], total_bytes);
}

/* Голубой процесс */
void blue_process(void) {
    printf("[PID=%d, PPID=%d] Процесс %s запущен\n",
           getpid(), getppid(), color_names[BLUE]);
    fflush(stdout);

    /* Голубые процессы только выводят информацию о себе */
    usleep(500000); /* 0.5 секунды для наглядности */

    printf("[PID=%d, PPID=%d] Процесс %s завершён\n",
           getpid(), getppid(), color_names[BLUE]);
}
