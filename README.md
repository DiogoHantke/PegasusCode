![Pegasus](docs/assets/pegasus-banner.png)

# PegasusCode

Seguidor de linha para Nano ATmega328P, em C puro, sem framework Arduino.
Calibracao, botao D2, buzzer, controle PD e diagnostico USB funcionam por
etapas curtas e interrupcoes, sem delays ou lacos de espera por hardware.

## Operacao

Ao ligar, um bip confirma a inicializacao e os motores ficam parados.
O primeiro toque curto calibra; depois de calibrar, outro toque curto inicia.
Segurar por dois segundos recalibra. Durante calibracao ou corrida, apertar
para os motores; soltar nao reinicia. O botao mantem o D2 da montagem.

A calibracao automatica dura cerca de 5,2 segundos e aproveita os frontais
validos mesmo com outros canais falhos. O PID usa Kp=1.2, Kd=4.3 e a formula
de `Main (1).ino`. A corrida usa leitura direta e PWM maximo de 70% (179/255).

## Compilar, testar e gravar

```sh
make test
pio run -e nano_running
pio run -e nano_running -t upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

O bootloader grava a 57600 baud. O monitor e somente TX e mostra botao,
estados e sensores a 115200 baud. Bluetooth nao participa do firmware.
Os testes de hardware devem confirmar leituras, sentidos e resposta na pista.

## Fontes principais

- `src/app.c`: maquina de estados e sinalizacao.
- `src/button.c`: debounce, toque curto e pressao longa.
- `src/buzzer.c`: tom de 1 kHz por Timer2.
- `src/calibration/`: varredura automatica e filtro.
- `src/line_follower.c`: posicao, PD, marcas e parada.
- `src/status_monitor.c`: diagnostico USB sem bloquear.

[Calibracao](docs/calibration.md) · [Corrida e operacao](docs/running.md) ·
[Pinos e hardware](docs/hardware.md)

O firmware de corrida esta na branch `main`.
