# PegasusCode

Firmware em C para AVR, com módulos de ADC, PWM, temporização e uma máquina de estados para leitura e calibração de sensores QTR.

**Status:** em desenvolvimento. A calibração ainda não realiza a coleta dos sensores, e os estados de espera, execução, finalização e parada ainda não têm implementação.

## Hardware

A configuração de compilação usa **ATmega328P a 16 MHz**, conforme a configuração original do editor. No PlatformIO, o perfil `uno` fornece os parâmetros desse microcontrolador; o firmware usa AVR diretamente, sem o framework Arduino.

Confirme o microcontrolador, o encapsulamento e as conexões antes de gravar. Veja as pendências de pinagem em [docs/hardware.md](docs/hardware.md).

## Estrutura

```text
PegasusCode/
├── docs/                 # Notas de hardware e desenvolvimento
├── lib/
│   ├── adc/              # Conversão analógico-digital
│   ├── pwm/              # PWM pelo Timer0
│   ├── timer/            # Eventos de tempo pelo Timer1
│   └── usart/            # Arquivos reservados para USART
├── src/
│   └── main.c            # Inicialização e máquina de estados
├── Makefile              # Compilação com a ferramenta AVR local
└── platformio.ini        # Configuração do PlatformIO
```

## Compilação

Com PlatformIO Core instalado, execute na raiz:

```sh
pio run
```

Alternativamente, com `avr-gcc`, `avr-libc`, `avr-binutils` e GNU Make instalados:

```sh
make
make size
```

O Makefile gera `build/pegasus.elf` e `build/pegasus.hex`. O PlatformIO gera seus artefatos em `.pio/`. Essas pastas estão no `.gitignore`.

A compilação não valida o circuito nem o comportamento no hardware. A gravação depende da placa e do programador utilizados.

## Desenvolvimento

Consulte [docs/development.md](docs/development.md) para conhecer os módulos e as pendências. Use quatro espaços e mantenha cabeçalhos e implementações no diretório de cada módulo.

## Publicação

O `.gitignore` exclui artefatos de compilação, configurações pessoais do editor e arquivos de ambiente. Revise os arquivos antes do primeiro commit.

Ainda não foi definida uma licença de distribuição. Escolha uma licença e adicione um arquivo `LICENSE` antes de conceder permissões de uso e redistribuição.
