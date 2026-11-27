# Corrida em C

A branch `feature/running-ino` parte de `calibracao-v3` (`bfbc913`). Os arquivos
`src/calibration/` e os gestos do botao foram preservados. No fluxo original,
um toque no estado inicial calibra, segurar por dois segundos pula para espera,
e outro toque em espera inicia. Pular a calibracao nao fornece extremos validos:
a corrida entra em emergencia se os sensores nao estiverem calibrados.

O perfil padrao `nano_running` compila para Nano ATmega328P, usa bootloader a
57600 baud e nao habilita o supervisor/Bluetooth. O perfil anterior `atmega328p`
continua disponivel. Nao existe monitor serial no perfil autonomo.

`line_follower.c` usa `Main (1).ino` como referencia: leitura direta de A6 a A1,
normalizacao 0..100 pelos extremos calibrados, pesos -2.5 a +2.5 e PD com
Kp=1.2 e Kd=4.3. A polaridade vem de `CAL_WHITE_HIGH` da calibracao original;
o ADC do sketch associa preto a valores menores, enquanto o padrao original
do projeto associa preto a valores maiores. A biblioteca mantem a interface
de oito bits da calibracao e acrescenta leitura de dez bits para corrida.

PWM maximo e velocidade de reta: 80/255. Curvas: 46/255. Os comandos usam
base menos correcao na esquerda e base mais correcao na direita. A esquerda
avanca com IN2 alto e a direita com IN1 alto, como a calibracao da montagem.
O PWM percentual da calibracao permanece inalterado.

Uma varredura e consumida a cada 5 ms. A7 >= 30 com A0 <= 30 alterna reta e
curva depois de 120 ms estaveis. Cruzamentos com cinco sensores ativos inibem
marcas. A chegada exige nove segundos desde a partida, saida da marca inicial
e nova marca A0 <= 30 por 120 ms. Estas protecoes corrigem os bloqueios do
sketch: a corrida nao termina apenas por decorrerem nove segundos.

Sem linha frontal, o PWM zera imediatamente; apos 150 ms continua sem linha,
o estado passa a emergencia. ADC sem resposta por 100 ms ou calibracao
invalida tambem causam emergencia. Apertar o botao durante a corrida para os
motores e muda para STOP. A recuperacao usa o fluxo original de estados.

Validacao: `make test-running` cobre os dois sentidos de leitura, comparacao
com mil quadros das expressoes do sketch por polaridade, perda/retorno de linha,
timeout, curvas, cruzamentos, chegada, reinicio e volta do contador de tempo.
`pio run -e nano_running` compila o firmware. A validacao fisica na pista e
a gravacao desta branch devem ser feitas separadamente.

A suite antiga `make test` ja falha na base `bfbc913`, em uma assercao de
sinalizacao de emergencia no teste de calibracao. Essa falha foi preservada;
os novos testes de corrida usam `make test-running` e passam separadamente.
