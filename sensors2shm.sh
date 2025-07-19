#!/bin/bash

# Скрипт для управления демоном sensors2shm
DAEMON_NAME="sensors2shm"
DAEMON_PATH="./background_ranging"
PID_FILE="/var/run/sensors2shm.pid"

# Функция для проверки статуса демона
check_status() {
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if kill -0 "$PID" 2>/dev/null; then
            echo "Демон $DAEMON_NAME запущен (PID: $PID)"
            return 0
        else
            echo "Демон $DAEMON_NAME не запущен (PID файл устарел)"
            rm -f "$PID_FILE"
            return 1
        fi
    else
        echo "Демон $DAEMON_NAME не запущен"
        return 1
    fi
}

# Функция для запуска демона
start() {
    echo "Запуск демона $DAEMON_NAME..."
    
    if check_status >/dev/null 2>&1; then
        echo "Демон уже запущен"
        return 1
    fi
    
    # Проверяем существование исполняемого файла
    if [ ! -x "$DAEMON_PATH" ]; then
        echo "Ошибка: $DAEMON_PATH не найден или не исполняемый"
        return 1
    fi
    
    # Запускаем демон
    $DAEMON_PATH --daemon
    
    # Ждем немного и проверяем статус
    sleep 2
    if check_status >/dev/null 2>&1; then
        echo "Демон $DAEMON_NAME успешно запущен"
        return 0
    else
        echo "Ошибка: не удалось запустить демон"
        return 1
    fi
}

# Функция для остановки демона
stop() {
    echo "Остановка демона $DAEMON_NAME..."
    
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if kill -0 "$PID" 2>/dev/null; then
            kill "$PID"
            echo "Отправлен сигнал SIGTERM процессу $PID"
            
            # Ждем завершения процесса
            for i in {1..10}; do
                if ! kill -0 "$PID" 2>/dev/null; then
                    echo "Демон $DAEMON_NAME остановлен"
                    rm -f "$PID_FILE"
                    return 0
                fi
                sleep 1
            done
            
            # Если процесс не завершился, принудительно завершаем
            echo "Принудительное завершение процесса $PID"
            kill -9 "$PID"
            rm -f "$PID_FILE"
            return 0
        else
            echo "Процесс $PID не существует"
            rm -f "$PID_FILE"
            return 1
        fi
    else
        echo "PID файл не найден"
        return 1
    fi
}

# Функция для перезапуска демона
restart() {
    echo "Перезапуск демона $DAEMON_NAME..."
    stop
    sleep 2
    start
}

# Основная логика скрипта
case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        restart
        ;;
    status)
        check_status
        ;;
    *)
        echo "Использование: $0 {start|stop|restart|status}"
        echo ""
        echo "Команды:"
        echo "  start   - запустить демон"
        echo "  stop    - остановить демон"
        echo "  restart - перезапустить демон"
        echo "  status  - показать статус демона"
        echo ""
        echo "Примечание: Демон работает без логирования для защиты SD карты Raspberry Pi."
        exit 1
        ;;
esac

exit $? 