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
e 250 em reta e 110 em curva. Os comandos sao base mais correcao na
esquerda e base menos correção na direita. O PWM máximo é 180 na reta e 153 no modo de curva.
A base é 180 na reta e 46 no modo de curva. O controle atualiza a cada 5 ms.

O Pegasus segue a faixa branca. Com `CAL_WHITE_HIGH=0`, leituras
proximas do minimo calibrado representam o branco. Use 1 para sensores
de polaridade oposta. O motor esquerdo
avanca com IN2 alto e o direito com IN1 alto, conforme a montagem espelhada.

Canais sem contraste valido sao ignorados. Um frontal valido com leitura
de branco normalizada de pelo menos 25 basta para detectar linha. Somente
frontais que atendem a esse limiar entram no calculo da posicao.
Sem linha detectada, o PWM zera no primeiro quadro; apos 150 ms o estado
e EMERGENCY. Falta de resposta do ADC por 100 ms tambem para os motores.

A7 arma a chegada ao ler 80 ou mais fora da marca. Depois, uma leitura
de 60 ou menos durante 20 ms confirma a chegada. Os limites usam o ADC
bruto de A7, na escala de 0 a 1023.
Quatro ou mais frontais ativos caracterizam cruzamento. A7 fica
bloqueado durante o cruzamento e por 2 s apos a ultima leitura dele.
Uma marca lida durante o bloqueio exige sair dela antes de confirmar
outra chegada. A0 indica curva quando le 30 ou menos e A7 esta fora da
marca de chegada, com 120 ms de confirmacao.

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
