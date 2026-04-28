# Makefile за проекта по Разпределени Софтуерни Архитектури
# P2P Динамично балансиране

CC = mpicc
CFLAGS = -O2 -Wall

TARGETS = static_balancing p2p_chain p2p_full

all: $(TARGETS)

static_balancing: static_balancing.c
	$(CC) $(CFLAGS) -o $@ $<

p2p_chain: p2p_chain.c
	$(CC) $(CFLAGS) -o $@ $<

p2p_full: p2p_full.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

# Тестове с различен брой процеси
test: all
	@echo "=== Тест със статично балансиране ==="
	mpirun -np 1 ./static_balancing 35
	mpirun -np 2 ./static_balancing 35
	mpirun -np 4 ./static_balancing 35
	@echo ""
	@echo "=== Тест с P2P верижна топология ==="
	mpirun -np 1 ./p2p_chain 35
	mpirun -np 2 ./p2p_chain 35
	mpirun -np 4 ./p2p_chain 35
	@echo ""
	@echo "=== Тест с P2P пълен граф ==="
	mpirun -np 1 ./p2p_full 35
	mpirun -np 2 ./p2p_full 35
	mpirun -np 4 ./p2p_full 35

.PHONY: all clean test
