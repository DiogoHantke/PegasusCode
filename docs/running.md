# Corrida em C nao bloqueante

O firmware de corrida esta na branch `main`.

## Botao D2 e estados

O botao continua ativo alto com o pull-down externo da montagem. O debounce
e de 25 ms, com eventos distintos de pressionar, toque curto e pressao longa.
Ao ligar, os motores ficam parados e o buzzer emite um bip.

- Primeiro toque de menos de um segundo: inicia a calibracao automaticamente.
- Segurar por dois segundos em qualquer estado: inicia nova calibracao.
- Depois de calibrar, toque de menos de um segundo: inicia a corrida.
- Durante corrida ou calibracao, uma pressao para imediatamente apos debounce.
  Soltar esse mesmo toque nao reinicia os motores.
- Apos STOP, FINISH ou emergencia por perda de linha, um novo toque reinicia
  a corrida com a calibracao disponivel. Sem calibracao valida, tenta calibrar.
- Pressionar entre um e dois segundos nao inicia uma corrida.
- Se ligar com o botao preso, e necessario soltar e pressionar novamente.

O LED pisca durante calibracao, fica aceso quando pronto e durante corrida,
e pisca em emergencia. Dois bips indicam todos os frontais calibrados; tres
indicam calibracao parcial ou emergencia. Nao ha esperas pelo buzzer.

## Leitura e PID

A leitura de corrida e direta, sem media ou filtro temporal. A calibracao
mantem o filtro de medias de dez amostras, threshold 100 e confirmacao 10.
O ADC oferece dez bits na corrida e oito bits na calibracao.

O calculo segue `Main (1).ino`: A6..A1, normalizacao em 0..100, pesos -2.5 a
+2.5 e PD `erro * 1.2 + (erro - erro_anterior) * 4.3`. O limite de correcao
e 250 em reta e 110 em curva. Os comandos sao base menos correcao na
esquerda e base mais correcao na direita, com PWM maximo 200/255. A base e 200
em reta e 46 em curva. O controle atualiza a cada 5 ms.

`CAL_WHITE_HIGH=1` segue o map do sketch: ADC menor corresponde a preto.
Pode ser invertido para sensores de polaridade oposta. O motor esquerdo
avanca com IN2 alto e o direito com IN1 alto, conforme a montagem espelhada.

Canais sem contraste valido sao ignorados. O limiar de presenca de linha e
proporcional ao numero de frontais aproveitados; um canal falho nao cancela
os demais. Sem qualquer frontal utilizavel, nao existe posicao para o PID.
Sem linha detectada, o PWM zera no primeiro quadro. Quando os seis frontais
leem branco logo apos um cruzamento, o ultimo comando se mantem enquanto
a leitura continuar toda branca, por ate 150 ms. Uma perda de linha de
150 ms leva a EMERGENCY. Falta de resposta do ADC por 100 ms tambem para os motores.

As marcas seguem A7 >= 30 e A0 <= 30, com 120 ms de confirmacao. A chegada
requer nove segundos desde a partida, saida da marca inicial e nova marca
A0 <= 30 por 120 ms. Quatro ou mais dos seis frontais ativos caracterizam
um cruzamento, que inibe marcas enquanto e visto. A chegada em A0 permanece bloqueada por dois
segundos apos a ultima leitura de cruzamento ou de branco nos seis
frontais. A confirmacao de 120 ms da chegada comeca apos esse bloqueio.
Um lateral sem calibracao desabilita a funcao correspondente sem impedir
corrida; com A0 falho, a parada final deve ser feita pelo botao.

## Compilacao e diagnostico

`make test` cobre botao, buzzer, UART, calibracao real integrada com a
maquina de estados, falhas parciais/totais, nova tentativa, perda de linha,
parada, passagem do contador de tempo e 2.000 quadros comparados ao sketch.
`pio run -e nano_running` compila para o Nano ATmega328P.

Gravacao: `pio run -e nano_running -t upload --upload-port /dev/ttyUSB0`.
O bootloader usa 57600 baud; o monitor USB usa 115200 baud.
O monitor transmite estado, botao, mascara de frontais validos e ultimas
leituras ADC (quantizadas em passos de quatro). As leituras sao atualizadas
na calibracao e na corrida; em espera permanecem as ultimas amostras.
A UART envia por interrupcao, sem Bluetooth nem comandos de controle.

Timer0 produz PWM, Timer1 fornece o relogio sempre ativo e Timer2 gera o
buzzer de 1 kHz. Nenhum estado desliga o relogio ou espera um botao/sensor.
