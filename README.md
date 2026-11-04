![Pegasus — High-Performance Line Follower Robot](docs/assets/pegasus-banner.png)

# PegasusCode

[![Firmware build](https://github.com/DiogoHantke/PegasusCode/actions/workflows/firmware-build.yml/badge.svg?branch=main)](https://github.com/DiogoHantke/PegasusCode/actions/workflows/firmware-build.yml)
![Target: ATmega328P](https://img.shields.io/badge/target-ATmega328P-168bff)
![Language: Embedded C](https://img.shields.io/badge/language-Embedded_C-168bff)
![PlatformIO](https://img.shields.io/badge/build-PlatformIO-34465e)

**PegasusCode** é o firmware embarcado do **Pegasus**, um robô diferencial seguidor de linha desenvolvido para competições de velocidade. O projeto utiliza **C embarcado**, com acesso direto aos registradores do **ATmega328P**, para integrar aquisição de sensores, calibração automática, processamento da linha, controle de trajetória e acionamento dos motores.

A proposta é construir um sistema de controle com **baixa latência, temporização previsível e comportamento robusto**, capaz de responder às mudanças da pista sem depender de um computador externo. O desenvolvimento considera não apenas a execução dos algoritmos, mas também as limitações físicas do robô, a qualidade das medições e o tratamento de condições anormais.

> **Sobre o escopo:** este README apresenta o Pegasus como sistema completo de referência. Parte das funcionalidades descritas integra a arquitetura projetada e ainda está em desenvolvimento. A implementação disponível e os critérios de validação estão detalhados na [documentação técnica](#documentação-técnica).

## Visão geral

O Pegasus utiliza **oito canais analógicos de refletância**, organizados em seis sensores frontais para acompanhar a linha e dois sensores laterais destinados à identificação de marcações da pista. O ATmega328P adquire os sinais pelo ADC interno, processa as leituras e produz comandos diferenciais para dois motores DC, acionados por uma ponte H **TB6612FNG**.

O fluxo de processamento compreende:

1. **Aquisição:** leitura sequencial dos sensores, com controle dos emissores e tratamento das condições de conversão.
2. **Calibração:** determinação de referências individuais de refletância, filtragem de amostras e avaliação da qualidade de cada canal.
3. **Estimativa da linha:** normalização das leituras, determinação da posição relativa da faixa e identificação de medições ambíguas ou ausentes.
4. **Controle de trajetória:** cálculo do erro de seguimento e correção diferencial dos comandos dos motores, respeitando limites de atuação.
5. **Supervisão:** gerenciamento dos estados operacionais, autorização do movimento, recuperação de referência e resposta a falhas.
6. **Diagnóstico:** disponibilização de dados operacionais e parâmetros por comunicação serial para análise e configuração.

A realimentação do controle ocorre pela **posição observada da linha**. Como a configuração de referência não possui encoders, o firmware não mede diretamente a velocidade das rodas nem realiza odometria; os valores de PWM representam comandos de atuação, não medições de RPM.

## Hardware

| Componente | Especificação e função |
| --- | --- |
| **Microcontrolador** | ATmega328P, arquitetura AVR de 8 bits, operando a 16 MHz |
| **Plataforma** | Arduino Nano compatível, programado sem o framework Arduino |
| **Memória** | 32 KB de Flash, 2 KB de SRAM e 1 KB de EEPROM |
| **Sensor frontal** | Sensor de linha e refletância IR QRE1113, 8 canais analógicos, similar ao QTR-8A |
| **Sensores laterais** | Sensores de refletância IR QRE1113 |
| **Aquisição** | ADC interno com leitura de 8 bits no driver de referência |
| **Motores** | Micro motor DC com caixa de redução, 6 V, 3000 RPM |
| **Driver de motores** | TB6612FNG |
| **PWM** | Timer0, com saídas de hardware OC0A e OC0B |
| **Base temporal** | Timer1 em modo CTC, com interrupções periódicas |
| **Comunicação** | USART0; interface serial USB ou módulo Bluetooth compatível |
| **Interface local** | Botão de operação, LED e buzzer previsto no circuito |

O esquemático de referência contempla uma interface para **HC-05/HC-06**. Uma eventual comunicação por Wi-Fi com ESP8266 constitui uma alternativa de integração, não um recurso Bluetooth nativo desse módulo.

O mapeamento de pinos, as conexões elétricas e as verificações de montagem estão em [Hardware e mapeamento](docs/hardware.md).

## Funcionamento do firmware

### Calibração automática

A calibração fornece as referências usadas para interpretar a refletância de cada sensor. Em vez de assumir que todos os canais respondem da mesma maneira, o firmware mantém dados individuais de mínimo, máximo, médias e qualidade de contraste.

O procedimento considera a geometria da malha frontal e dos sensores laterais, empregando amostragem sequencial, confirmação de mudanças de leitura e critérios de aprovação. A arquitetura também contempla diagnóstico de canais inconsistentes, novas tentativas limitadas e bloqueio da corrida quando a calibração não satisfaz as condições necessárias.

O filtro de calibração foi concebido para evitar que pequenas oscilações ou ruído sejam interpretados imediatamente como uma mudança efetiva de superfície. A [documentação de calibração](docs/calibration.md) apresenta os critérios, parâmetros e limites; o [funcionamento do sistema](docs/system-operation.md) descreve a sequência operacional.

### Percepção e controle de trajetória

Após a calibração, os sinais dos sensores são normalizados para permitir comparação entre canais. O estimador utiliza a distribuição espacial das leituras frontais para determinar a posição da linha em relação ao eixo de referência do robô.

A arquitetura de controle utiliza uma correção **PD (proporcional-derivativa)** aplicada diferencialmente aos motores, com saturação dos comandos e um perfil de velocidade condicionado ao erro e à confiança na detecção. A estratégia inclui tratamento de perda de linha, busca limitada e parada segura quando a referência não pode ser recuperada.

O objetivo não é simplesmente utilizar o maior duty cycle possível, mas **manter a maior velocidade compatível com a capacidade de perceber, corrigir e percorrer a pista de forma estável**. Sem sensores de velocidade, a ação sobre os motores permanece em malha aberta quanto à rotação das rodas.

Detalhes de processamento, estimativa e atuação estão em [Arquitetura do firmware](docs/architecture.md).

### Estados operacionais e segurança

O sistema é organizado por máquinas de estados, separando preparação, calibração, espera, execução, finalização e tratamento de falhas. Essa estrutura estabelece quais operações são permitidas em cada etapa e impede que a lógica de corrida seja executada sem as condições de autorização previstas.

Os estados globais do projeto são `INIT`, `CALIBRATION`, `WAITING`, `RUNNING`, `FINISH`, `STOP` e `EMERGENCY`. A calibração possui subestados próprios. Na operação completa, as transições são condicionadas à validade dos sensores, aos eventos da pista e aos limites temporais e operacionais.

A supervisão contempla falhas de calibração, perda persistente da linha, atrasos de execução, parâmetros inválidos e condições que exijam interrupção do movimento. A descrição de estados, transições e prioridades está em [Máquina de estados](docs/firmware-state-machine.md).

### Temporização e desempenho

O firmware utiliza periféricos de hardware para gerar PWM e organizar a base de tempo, preservando o processamento principal para aquisição e controle. A arquitetura adota um **superloop cooperativo**, com interrupções curtas, sem exigir um RTOS para o escopo atual.

O projeto estabelece **500 Hz (período de 2 ms)** como meta inicial para o controle e **1 kHz (1 ms)** como objetivo evolutivo, sujeitos à verificação do tempo de aquisição, processamento, atualização de PWM e interferência da comunicação. Essas frequências são **metas de engenharia**, não resultados já medidos.

As métricas e os procedimentos de medição estão em [Desempenho e temporização](docs/performance.md).

### Diagnóstico e telemetria

A arquitetura prevê comunicação pela **USART0** e uma ferramenta de diagnóstico em Python, denominada **Pegasus CLI**. A interface permite acompanhar leituras dos sensores, resultados de calibração, estados do sistema, comandos dos motores, parâmetros de controle e eventos de corrida.

A comunicação é auxiliar à operação: **o seguimento da linha é executado localmente pelo ATmega328P**. O envio de dados deve respeitar limites de buffer e frequência para não comprometer o tempo de resposta do controlador. O protocolo, os comandos e as permissões estão descritos em [Telemetria e CLI](docs/telemetry-and-cli.md).

## Organização do projeto

O código separa as responsabilidades de aplicação, processamento e acesso aos periféricos. A estrutura inclui o programa principal em `src/`, bibliotecas de ADC, PWM, temporização e outros módulos em `lib/`, testes em `tests/` e documentação especializada em `docs/`.

A organização favorece a implementação incremental, a reutilização de drivers e a execução de testes dos algoritmos em computador, mesmo sem acesso imediato ao robô físico. A estrutura atual e a modularização proposta são explicadas em [Arquitetura](docs/architecture.md) e [Desenvolvimento](docs/development.md).

## Compilação

O projeto utiliza **PlatformIO**, toolchain **AVR-GCC** e **AVR Libc**, com o ambiente de compilação `atmega328p`.

```bash
pio run -e atmega328p
pio run -e atmega328p -t size
```

A compilação também pode ser realizada pela configuração Makefile do repositório. Antes da gravação, é necessário verificar a versão do bootloader e o perfil de placa, pois a plataforma física é baseada no Nano, enquanto o ambiente de referência utiliza o perfil `uno`.

As instruções de instalação, compilação e gravação estão em [Compilação e gravação](docs/build-and-flashing.md).

## Documentação técnica

O README apresenta o sistema de forma integrada. Os detalhes de implementação, equações, parâmetros, procedimentos e critérios de aceitação estão organizados nos documentos abaixo.

| Documento | Conteúdo |
| --- | --- |
| [Funcionamento do sistema](docs/system-operation.md) | Sequência operacional e funcionamento da calibração no firmware de referência |
| [Arquitetura](docs/architecture.md) | Organização dos módulos, aquisição, estimativa e controle |
| [Hardware](docs/hardware.md) | Componentes, interfaces elétricas e mapeamento de pinos |
| [Calibração](docs/calibration.md) | Filtro, contraste, aprovação e estratégias de recalibração |
| [Máquina de estados](docs/firmware-state-machine.md) | Estados operacionais, transições, falhas e recuperação |
| [Telemetria e CLI](docs/telemetry-and-cli.md) | Comunicação serial, diagnóstico e interface Python |
| [Desempenho](docs/performance.md) | Períodos, latência, jitter e metas de processamento |
| [Compilação e gravação](docs/build-and-flashing.md) | Ambiente de desenvolvimento, geração e upload do firmware |
| [Testes e validação](docs/testing-and-validation.md) | Ensaios em computador, bancada e pista |
| [Desenvolvimento](docs/development.md) | Convenções, integração, documentação e manutenção |
| [Roteiro de desenvolvimento](docs/roadmap.md) | Etapas de implementação e critérios de conclusão |

## Desenvolvimento e validação

O Pegasus é desenvolvido de maneira incremental: primeiro são verificados aquisição, calibração e atuação; depois são integrados estimativa de posição, controle, recuperação e instrumentação. Cada funcionalidade deve ser exercitada com entradas conhecidas, testada em bancada e posteriormente avaliada em pista.

A validação considera tanto o **comportamento lógico do firmware** quanto os **limites físicos do robô**, incluindo aderência, frenagem, resposta dos motores, ruído dos sensores e atrasos de execução. Compilar com sucesso não equivale a validar o funcionamento real do sistema.

O planejamento de implementação está em [Roadmap](docs/roadmap.md), e os procedimentos de avaliação estão em [Testes e validação](docs/testing-and-validation.md).

## Autoria

**[Diogo Hantke](https://github.com/DiogoHantke)** — desenvolvimento do firmware PegasusCode.
