#ifndef TEST_ATOMIC_H
#define TEST_ATOMIC_H
/* Execucao sequencial no host; nao simula concorrencia de interrupcoes. */
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(type) for (uint8_t test_atomic = 1; test_atomic; test_atomic = 0)
#endif
