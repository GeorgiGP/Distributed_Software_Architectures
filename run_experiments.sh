#!/bin/bash
# Скрипт за провеждане на експерименти
# P2P Динамично балансиране - Разпределени Софтуерни Архитектури

cd src

echo "Компилиране на програмите..."
make clean
make all

echo ""
echo "=============================================="
echo "  ЕКСПЕРИМЕНТИ: P2P Динамично Балансиране"
echo "=============================================="
echo ""

# Параметри
FIB_VALUE=38
REPETITIONS=3
PROCS="1 2 4 8"

RESULTS_DIR="../results"
mkdir -p $RESULTS_DIR

OUTPUT_FILE="$RESULTS_DIR/results_$(date +%Y%m%d_%H%M%S).txt"

echo "Резултати записани в: $OUTPUT_FILE"
echo "Fibonacci стойност: $FIB_VALUE"
echo "Повторения: $REPETITIONS"
echo ""

# Функция за измерване на време
run_benchmark() {
    local program=$1
    local np=$2
    local fib=$3
    local name=$4

    echo "  Тестване: $name с $np процес(а)..."

    local best_time=9999.0

    for ((i=1; i<=$REPETITIONS; i++)); do
        # Изпълнение и извличане на времето
        result=$(mpirun --oversubscribe -np $np ./$program $fib 2>&1 | grep "Време за изпълнение" | awk '{print $4}')

        if [ ! -z "$result" ]; then
            # Сравнение за минимално време
            is_better=$(echo "$result < $best_time" | bc -l)
            if [ "$is_better" -eq 1 ]; then
                best_time=$result
            fi
        fi
    done

    echo "$name,$np,$best_time" >> $OUTPUT_FILE
    echo "    Най-добро време: ${best_time}s"
}

echo "Алгоритъм,Процеси,Време(s)" > $OUTPUT_FILE

echo ""
echo "=== СТАТИЧНО БАЛАНСИРАНЕ ==="
for np in $PROCS; do
    run_benchmark "static_balancing" $np $FIB_VALUE "Static"
done

echo ""
echo "=== P2P ВЕРИЖНА ТОПОЛОГИЯ ==="
for np in $PROCS; do
    run_benchmark "p2p_chain" $np $FIB_VALUE "P2P-Chain"
done

echo ""
echo "=== P2P ПЪЛЕН ГРАФ ==="
for np in $PROCS; do
    run_benchmark "p2p_full" $np $FIB_VALUE "P2P-Full"
done

echo ""
echo "=============================================="
echo "  РЕЗУЛТАТИ"
echo "=============================================="
cat $OUTPUT_FILE

echo ""
echo "Експериментите завършиха успешно!"
