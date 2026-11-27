# Compilação, gravação e testes

[Início](../README.md) · [Hardware](hardware.md) · [Calibração](calibration.md)

Execute os comandos na raiz do projeto. O PlatformIO usa a plataforma `atmelavr@5.1.0`, alvo ATmega328P a 16 MHz e C11. Não há dependências de bibliotecas externas.

## Firmware principal

```sh
pio run -e atmega328p
pio run -e atmega328p -t size
pio run -e atmega328p -t upload --upload-port /dev/ttyUSB0
```

Ajuste a porta e o perfil de gravação ao bootloader da placa. Compilar não grava o dispositivo. Os arquivos gerados ficam em `.pio/` e não devem ser versionados.

Se `pio` do sistema falhar por incompatibilidade entre Python e Click, use o ambiente isolado instalado pelo PlatformIO, quando disponível:

```sh
~/.platformio/penv/bin/pio run -e atmega328p
```

Com `avr-gcc`, `avr-objcopy` e `avr-size` instalados, há uma alternativa sem PlatformIO:

```sh
make
make size
make clean
```

O Makefile gera `build/pegasus.elf` e `build/pegasus.hex`. `make clean` remove essa pasta.

## Ensaio de motores

O ambiente `test_motors` compila somente `src/diagnostics/motors.c` e os drivers necessários, sem o main da aplicação. Inicia movimento ao ligar, com PWM de 30%; use rodas suspensas. Alterna os sentidos a partir da leitura dos sensores, com pausa de dois segundos antes da inversão. Esse ensaio não possui os timeouts da calibração principal.

```sh
pio run -e test_motors
pio run -e test_motors -t upload --upload-port /dev/ttyUSB0
```

## Onde acompanhar o código

Comece por `src/main.c`: ele configura GPIO, ADC, PWM e Timer1, mantém o estado principal e, no laço contínuo, consulta o botão antes de executar o `switch(state)`. A leitura do botão e o debounce também ficam no `main.c`.

Na pasta `src/calibration/`, `calibration.c` mostra a sequência de estados. `calibration_initial.c` contém a preparação e a aquisição frontal/lateral, `calibration_diagnostic.c` conduz as tentativas de recuperação, `calibration_filter.c` trata as amostras e `calibration_detection.c` confirma as referências de branco/preto. O contexto fica privado nesses módulos, declarado em `calibration_internal.h`; a interface pública continua em `include/calibration.h`.

## Testes no computador

```sh
make test
```

Equivale a `sh tests/host/run.sh` e requer um compilador C disponível como `cc` ou definido em `CC`. O script cria um executável temporário fora do projeto e o remove ao terminar.

São 20 cenários executados nas duas polaridades claro/escuro: aprovação direta, contraste insuficiente, recuperação frontal e lateral, recuperação tardia, ruído, ausência de referências, ADC sem progresso, timeouts e reinício da calibração após sucesso ou falha. São conferidos ordem dos movimentos, pausa de inversão, limites de tentativas, motores parados e sinalização do resultado. Os testes usam ADC assíncrono e GPIO simulados; não medem os sinais físicos da placa.

Além dos 40 casos de calibração, 13 cenários exercitam o `main.c` atual: toque válido com debounce de 500 ms, toque longo, repique, virada de contador, nova calibração após retorno por FINISH/EMERGENCY, pressão durante calibração, modo manual, ESTOP, rearme com botão segurado e partida com botão pressionado ao ligar. Os stubs representam somente periféricos/eventos externos e não implementam corrida. O teste específico do LED em `supervisor/tests/test_firmware.py` valida limites, repetição e reentrada no padrão de emergência.

A CI executa esses testes e compila os dois ambientes do PlatformIO. Para validar na bancada, confira os oito canais e a polaridade dos sensores, os sentidos dos motores com rodas suspensas, os tempos e a indicação do LED.
