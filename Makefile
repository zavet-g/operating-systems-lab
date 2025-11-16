# Makefile для контрольной работы, вариант 29
# Операционные системы

CC = gcc
CFLAGS = -Wall -Wextra
# На macOS не нужен -lrt, на Linux добавить -lrt
LDFLAGS = -lpthread
TARGET = variant29
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET).o
	rm -f even_processes.txt
	rm -f /tmp/shared_pid_29.txt

run: $(TARGET)
	@echo "Запуск программы с тестовыми данными..."
	@ps -e | head -20 | ./$(TARGET)

test: $(TARGET)
	@echo "=== Тестовый запуск программы ==="
	@echo "Используется список процессов из ps -e"
	@ps -e | ./$(TARGET)
	@echo ""
	@echo "=== Результаты записаны в файл even_processes.txt ==="
	@cat even_processes.txt

.PHONY: all clean run test
