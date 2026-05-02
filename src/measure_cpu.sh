#!/bin/bash
# Измерва CPU натоварване по време на изпълнение

PROGRAM=$1
PROCS=$2
ARG=$3

echo "=== Тест: $PROGRAM с $PROCS процеса, аргумент $ARG ==="

# Стартираме мониторинг на CPU в background
(while true; do 
    ps -A -o %cpu,command | grep -E "mpirun|$PROGRAM" | grep -v grep
    sleep 0.5
done) &
MONITOR_PID=$!

# Изпълняваме програмата
mpirun -np $PROCS ./$PROGRAM $ARG 2>&1

# Спираме мониторинга
kill $MONITOR_PID 2>/dev/null
wait $MONITOR_PID 2>/dev/null

echo ""
