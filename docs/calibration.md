# Calibracao automatica

A rotina em `src/calibration/calibration.c` tem quatro etapas: avanco de
2.500 ms, parada de 200 ms, recuo de 2.500 ms e conclusao. Os dois motores
usam 51/255, com o sentido de avanco da montagem. O botao pode cancelar
a qualquer momento. Nao ha espera bloqueante nem exigencia de cruzar um
limiar absoluto de branco/preto para sair de uma etapa.

Os oito canais sao varridos continuamente durante toda a calibracao. O
filtro original em `calibration_filter.c` calcula medias de dez amostras,
aceita variacoes menores que `THRESHOLD_LINE=100` e pede confirmacao de
variacoes maiores com tolerancia 10. Os extremos aceitos ficam em RAM.

Um canal e aproveitado quando apresenta contraste de pelo menos 20 na
escala de oito bits (aproximadamente 80 na escala 0..1023). Os limites sao
individuais. Canais constantes ou sem amostras ficam reprovados.

Se ao menos um frontal tem contraste, a maquina vai para WAITING; laterais
ou outros frontais reprovados nao impedem a partida. Sem frontal utilizavel,
vai para EMERGENCY, com motores e emissores desligados. Um novo toque permite
outra tentativa; nao existe travamento permanente. A rotina nao grava EEPROM.

A pista deve passar por branco e preto durante a varredura automatica para
que os sensores obtenham extremos diferentes. A calibracao termina em cerca
de 5,2 segundos mesmo se um canal falhar; os canais restantes sao utilizados.

Veja os [comandos do botao, PID e diagnostico USB](running.md).
