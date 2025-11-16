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

    /* Создаем pipe для связи желтого и зеленого */
    if (pipe(pipe_yellow_green) < 0) {
        perror("Ошибка создания pipe");
        exit(1);
    }

    /* Порождаем первого потомка - ЖЕЛТЫЙ процесс */
    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        perror("Ошибка fork для жёлтого");
        exit(1);
    } else if (pid == 0) {
        /* Желтый процесс */
        close(pipe_yellow_green[0]); /* Закрываем чтение из pipe */
        yellow_process(pipe_yellow_green);
        exit(0);
    }

    /* Порождаем второго потомка - ГОЛУБОЙ процесс */
    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        perror("Ошибка fork для голубого #1");
        exit(1);
    } else if (pid == 0) {
        /* Голубой процесс */
        close(pipe_yellow_green[0]);
        close(pipe_yellow_green[1]);
        fclose(stdin); /* Голубому stdin не нужен */
        blue_process();
        exit(0);
    }

    /* Порождаем третьего потомка - ГОЛУБОЙ процесс */
    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        perror("Ошибка fork для голубого #2");
        exit(1);
    } else if (pid == 0) {
        /* Голубой процесс */
        close(pipe_yellow_green[0]);
        close(pipe_yellow_green[1]);
        fclose(stdin); /* Голубому stdin не нужен */
        blue_process();
        exit(0);
    }

    /* Оранжевый процесс закрывает pipe (он ему не нужен) */
    close(pipe_yellow_green[0]);
    close(pipe_yellow_green[1]);
    fclose(stdin); /* Оранжевому stdin не нужен */

    /* Выполняем действия оранжевого процесса */
    orange_process();

    /* Ожидаем завершения всех дочерних процессов */
    while (wait(NULL) > 0);

    printf("[PID=%d, PPID=%d] Процесс %s завершён\n",
           getpid(), getppid(), color_names[ORANGE]);

    /* Очистка семафоров */
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

    /* Создаем семафоры */
    sem_orange = sem_open(SEM_ORANGE, O_CREAT, 0666, 0);
    sem_yellow = sem_open(SEM_YELLOW, O_CREAT, 0666, 0);

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

        /* Читаем значение из разделяемой памяти (упрощенно через файл) */
        FILE *f = fopen("/tmp/shared_pid_29.txt", "r");
        if (f) {
            if (fscanf(f, "%d", &pid_value) == 1) {
                if (pid_value == -1) {
                    fclose(f);
                    sem_post(sem_yellow); /* Подтверждаем получение сигнала завершения */
                    break; /* Конец передачи */
                }
                total_sum += pid_value;
                count++;
                printf("[PID=%d, PPID=%d] %s: Получен PID=%d, текущая сумма=%d\n",
                       getpid(), getppid(), color_names[ORANGE], pid_value, total_sum);
            }
            fclose(f);
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

    /* Открываем семафоры с повторными попытками */
    int retry_count = 0;
    while (retry_count < 10) {
        sem_orange = sem_open(SEM_ORANGE, 0);
        sem_yellow = sem_open(SEM_YELLOW, 0);

        if (sem_orange != SEM_FAILED && sem_yellow != SEM_FAILED) {
            break; /* Успешно открыли */
        }

        /* Если не удалось, ждем и пробуем снова */
        usleep(100000); /* 100ms */
        retry_count++;
    }

    if (sem_orange == SEM_FAILED || sem_yellow == SEM_FAILED) {
        perror("Ошибка открытия семафоров в жёлтом после повторных попыток");
        close(pipe_to_green[1]);
        return;
    }

    /* Порождаем ЗЕЛЕНЫЙ процесс */
    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        perror("Ошибка fork для зелёного");
        sem_close(sem_orange);
        sem_close(sem_yellow);
        close(pipe_to_green[1]);
        return;
    } else if (pid == 0) {
        /* Зеленый процесс */
        sem_close(sem_orange);
        sem_close(sem_yellow);
        close(pipe_to_green[1]); /* Закрываем запись */
        fclose(stdin); /* Зелёному stdin не нужен */
        green_process((int[]){pipe_to_green[0], -1});
        exit(0);
    }

    /* Порождаем ГОЛУБОЙ процесс */
    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        perror("Ошибка fork для голубого от жёлтого");
    } else if (pid == 0) {
        /* Голубой процесс */
        sem_close(sem_orange);
        sem_close(sem_yellow);
        close(pipe_to_green[1]);
        fclose(stdin); /* Голубому stdin не нужен */
        blue_process();
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
        if (sscanf(line, "%d %s", &pid_val, name) == 2) {
            /* Проверяем, четный ли PID */
            if (pid_val % 2 == 0) {
                printf("[PID=%d, PPID=%d] %s: Найден чётный PID=%d (%s)\n",
                       getpid(), getppid(), color_names[YELLOW], pid_val, name);
                even_count++;

                /* Передаем PID оранжевому через семафоры */
                FILE *f = fopen("/tmp/shared_pid_29.txt", "w");
                if (f) {
                    fprintf(f, "%d", pid_val);
                    fclose(f);
                }
                sem_post(sem_orange); /* Сигнализируем оранжевому */
                sem_wait(sem_yellow); /* Ждем подтверждения */

                /* Передаем имя зеленому через pipe */
                write(pipe_to_green[1], name, strlen(name));
                write(pipe_to_green[1], "\n", 1);
            }
        }
    }

    printf("[PID=%d, PPID=%d] %s: Чтение завершено, отправка сигнала завершения\n",
           getpid(), getppid(), color_names[YELLOW]);

    /* Сигнализируем конец передачи оранжевому */
    FILE *f = fopen("/tmp/shared_pid_29.txt", "w");
    if (f) {
        fprintf(f, "-1");
        fclose(f);
    }
    sem_post(sem_orange);
    sem_wait(sem_yellow); /* Ждём подтверждения получения сигнала завершения */

    /* Закрываем pipe - это даст сигнал зелёному, что данных больше не будет */
    close(pipe_to_green[1]);

    /* Ожидаем завершения дочерних процессов */
    while (wait(NULL) > 0);

    printf("[PID=%d, PPID=%d] Процесс %s завершён (обработано %d чётных PID)\n",
           getpid(), getppid(), color_names[YELLOW], even_count);

    sem_close(sem_orange);
    sem_close(sem_yellow);
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
    while ((bytes_read = read(pipe_from_yellow[0], buffer, sizeof(buffer))) > 0) {
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
