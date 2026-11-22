# Integração da comunicação básica

A revisão atual reduz o módulo ao necessário para usar o app como ferramenta de diagnóstico e configuração, reservando SRAM e Flash para o firmware de corrida que será desenvolvido pelo usuário.

## Mantido

- USART de hardware 8N1, 9600 baud, RX/TX por interrupções curtas e laço cooperativo.
- CRC-8, sequência e confirmação ACK/NACK.
- Identificação, estado, seis sensores, diagnóstico da calibração e parâmetros PID em RAM.
- Acionamento da calibração existente, STOP/ESTOP explícitos e ensaio manual com dead-man local.
- Interface Tkinter com duas abas, histórico/gráfico no PC e simulador explicitamente identificado.
- Autonomia em relação ao Bluetooth: falta de PING só suspende telemetria; erro RX não encerra operações autônomas.

## Retirado do AVR

- Controlador experimental de corrida, execução PID, integração/derivada e regras experimentais de perda de linha/watchdog de controle.
- Arquivos `src/communication/pid.c` e `include/pegasus_pid.h`; os ganhos agora são apenas três parâmetros de 16 bits.
- Cronômetro experimental e contadores de tempo de corrida; START retorna NACK 6, sem mover motores.
- printf/snprintf e buffers intermediários de mensagens.
- Cache de oito comandos, substituído pela última atuação, sem repetição automática de movimento pelo PC.
- Telemetria continuamente transmitida sem monitor e configuração TELEM que não era usada pela interface.

RX32 + TX64 + parser48 totalizam 144 bytes, antes 512. Mensagens são montadas diretamente no ring TX, com reserva para ACK e pedidos coalescidos em bits. Textos fixos ficam na Flash. O parser limita frames a 47 bytes no C e no Python. O protocolo documenta capacidades e o app não apresenta corrida ou tempo físico como implementados.

## Pontos para seu firmware de corrida

O main atual preserva a máquina de estados e a lógica de botão do usuário. A integração usa `supervisor_step(&state)` antes do switch e `supervisor_after(&state)` depois. Inicialização fica em `src/communication/system_init.c` e é declarada em `include/pegasus_system.h`.

`supervisor_get_gains()` devolve um ponteiro somente de leitura para `SUPERVISOR_GAINS` (Kp, Ki, Kd em centésimos). A futura lógica de controle pode ler/copiar esses parâmetros no contexto cooperativo antes da corrida. O módulo não calcula a saída de controle nem modifica PWM em RUNNING, exceto por um STOP/ESTOP explícito. Novos eventos/tempos e autorização do START deverão ser integrados quando a rotina existir.

Os canais QTR1–6 são A1–A6, HR é A0 e HL é A7 no esquema. Os laterais continuam na calibração, sem inventar detectores de volta. O cache ADC existente é somente uma cópia das conversões concluídas; o supervisório não compete pelo ADC durante calibração/corrida. Drivers de PWM, ADC, Timer1 e rotina de calibração foram preservados.

O main usa debounce de 500 ms e leitura global por bordas. Foram corrigidos o reaproveitamento da mesma pressão entre estados, o cálculo de tempo perto da virada do contador de 16 bits e a falta de calibration_reset() ao iniciar uma nova calibração local. O botão é bloqueado no modo manual/ESTOP e exige liberação antes de aceitar uma ação após desbloqueio. STOP, FINISH e EMERGENCY garantem PWM zero; EMERGENCY desliga emissores e atualiza o LED. Os testes do main foram atualizados para esses contratos e agora passam junto dos 40 testes de calibração.

## Validação

Consulte `docs/validation/README.md`: uso medido por PlatformIO/avr-size, testes do parser e emissor real C contra Python, congestionamento, limites, duplicatas, manual e ausência de dependência do enlace. Compilação/host/simulação não são validação de ruído, latência ou temporização experimental. Nenhum upload foi feito.

## Padrao de escrita

Os arquivos C acrescentados seguem as referencias do usuario: indentacao de quatro espacos, chaves junto da declaracao (`void funcao(void){`), blocos separados por linhas em branco e declaracoes de variaveis individuais. O codigo de corrida continua pendente; esta revisao nao acrescenta controlador ou dependencia do Bluetooth.
