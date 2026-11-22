# Comunicação básica PEGASUS v1

O firmware fornece apenas comunicação, diagnóstico e parâmetros configuráveis. **Não implementa corrida, PID de atuação, voltas ou cronometragem.** A calibração existente é preservada. O simulador do PC pode exercitar corrida fictícia, sempre identificada como SIMULADO.

## Frames

```text
@SEQ,TIPO[,CAMPO...]*CC<LF>
```

ASCII sem espaços nem CR. Tamanho máximo: **47 bytes**, incluindo delimitadores, CRC e LF; buffer C de 48 bytes inclui o NUL. Sequência decimal 0–255. Tipos `[A-Z][A-Z_0-9]*`; campos não vazios `[A-Za-z0-9_.-]+`. Valores decimais; os campos PWM de telemetria admitem sinal negativo.

CRC-8, polinômio 0x07, valor inicial 0, sem reflexão ou XOR final. Calculado do primeiro dígito da sequência ao último byte antes de `*`, sem `@`, `*`, CC e LF. CC usa dois dígitos hexadecimais maiúsculos. Vetor ASCII `123456789` → F4.

`@` reinicia montagem. Frames truncados, extensos ou corrompidos não executam comandos; retomada ocorre no próximo `@`. Sem CRC válido não se envia ACK. Overflow/erro UART descarta RX e registra falha; interrompe apenas movimento manual ativo, sem encerrar tarefas autônomas do firmware do usuário.

## Comandos

| Comando | Campos | Função |
| --- | --- | --- |
| PING | nenhum | Identificação e ativação da telemetria do monitor |
| GET_STATUS | nenhum | Solicita T |
| GET_SENSORS | nenhum | Solicita S |
| GET_PID | nenhum | Consulta parâmetros G |
| SET_PID | kp100,ki100,kd100 | Guarda ganhos em RAM para integração futura; não executa PID |
| CALIBRATE | nenhum | Aciona a calibração embarcada existente |
| STOP | nenhum | Parada solicitada explicitamente pelo operador |
| ESTOP | nenhum | Parada com trava até rearme |
| REARM | nenhum | Em STOP, limpa trava e mantém motores parados em WAITING |
| MANUAL_ENABLE | nenhum | Habilita ensaio manual em estado seguro |
| MANUAL_DISABLE | nenhum | Encerra manual; rejeitado em corrida/calibração |
| MANUAL_FORWARD | pwm | L/R positivos |
| MANUAL_BACKWARD | pwm | L/R negativos |
| MANUAL_LEFT | pwm | L negativo, R positivo |
| MANUAL_RIGHT | pwm | L positivo, R negativo |
| MANUAL_STOP | nenhum | PWM zero no manual |
| START | nenhum | **NACK 6: corrida ainda não implementada** |

PWM manual: 0–160, escala 0–255; driver existente converte para porcentagem/OCR. Padrão PC: 80. Ganhos em centésimos: Kp 0–1000, Ki/Kd 0–500. Padrão armazenado 145/8/32 corresponde a 1,45/0,08/0,32, extraído da referência visual e não ajustado fisicamente. SET_PID exige estado seguro, sem manual/ESTOP, e valida os três campos antes de alterar qualquer parâmetro.

Não há comando TELEM nesta versão básica; a frequência é fixa. Não há EEPROM nem parâmetros aplicados por um controlador provisório. O firmware de corrida futuro pode consultar `supervisor_get_gains()`, declarado em `include/supervisor.h`, no contexto cooperativo do programa.

## Respostas

| Tipo | Campos |
| --- | --- |
| ACK | 0 |
| NACK | código |
| ID | PEGASUS,1,0,0 |
| T | estado,flags,falha,pwm_L,pwm_R,0,etapa_cal,white_high |
| S | adc1,adc2,adc3,adc4,adc5,adc6,valid_mask |
| D | canal,min,max,aprovado |
| G | kp100,ki100,kd100 |

ACK/NACK repetem SEQ do comando; dados usam SEQ 0. O último campo de ID anuncia capacidade de corrida: **0** no firmware básico. O anterior é capacidade de voltas, também 0. O PC aceita o formato ID de três campos de versões anteriores/simulador. Ao receber ID básico, o app bloqueia START e mostra o cronômetro indisponível. O campo temporal de T permanece 0 por compatibilidade estrutural, não é um tempo medido de corrida.

Estados: INIT=0, CALIBRATION=1, WAITING=2, RUNNING=3, FINISH=4, STOP=5, EMERGENCY=6. O módulo apenas observa RUNNING se a futura lógica do usuário entrar nesse estado; não gera esse estado por START. Flags: bit0 manual, bit1 ESTOP, bit2 calibração concluída. Falhas emitidas pela comunicação: 0 nenhuma, 1 UART/overflow, 2 dead-man manual; outros códigos permanecem reservados para compatibilidade do modelo Python.

Os sensores são ADC1–ADC6, 8 bits, 0–255. Máscara S usa o número do ADC como bit. D usa canais 1–6 e aprovação por contraste da rotina existente, não defeito inferido de leitura zero. Etapas: 0 preparação, 1 início frontal, 2 frontal, 3 início lateral, 4 lateral, 5 diagnóstico/recuperação, 6 falha. Polaridade white_high=0: branco baixo; 1: branco alto.

NACK: 1 desconhecido, 2 argumentos inválidos, 3 estado incompatível, 4 ESTOP travado, 5 sequência reutilizada com conteúdo diferente na janela de duplicata, **6 função ainda não implementada**.

## Recursos e prioridade

RX circular: **32 bytes** (31 úteis); TX circular: **64 bytes** (63 úteis); parser: **48 bytes**. Total dos três buffers: **144 bytes**, antes 512. Não existem buffers intermediários de formatação, printf, heap ou cálculo PID. Textos/comandos ficam na Flash. Conversão decimal usa apenas cinco bytes temporários na stack.

O emissor escreve diretamente na área livre do ring TX e publica o frame completo somente após incluir CRC. O ISR UDRE envia um byte; o ISR RX apenas enfileira. O laço processa no máximo 32 bytes RX por passagem e uma mensagem completa por vez. Um ACK pendente ocupa sequência/código, sem fila de frames; pedidos de dados são coalescidos em uma máscara de bits. ACK/NACK têm prioridade e os dados deixam 16 bytes de reserva no TX. Sob congestionamento, dados aguardam/coalescem; não bloqueiam o laço esperando o UART.

A proteção de duplicatas guarda somente o **último comando de atuação**: sequência, CRC do payload e resultado. Duplicata imediata idêntica repete ACK sem atuar ou renovar movimento. Conteúdo diferente na mesma sequência recente recebe NACK 5. Consultas e STOP/ESTOP válidos são idempotentes e não ficam barrados por esse cache. Não há deduplicação eterna ou autenticação.

## Frequências e autonomia

A cada 200 ms, enquanto o monitor estiver ativo, são solicitadas T/S e um D, alternando os seis canais. Após 1500 ms sem PING, apenas a telemetria periódica é desativada; **nenhum estado operacional ou motor é alterado**. Consultas explícitas continuam possíveis. O PC envia PING a cada 400 ms. Taxa padrão: 9600 baud, 8N1.

Corrida futura e calibração não exigem Bluetooth. Só o ensaio manual depende de comandos novos: após 300 ms, PWM zero, manual desabilitado e STOP. PING não renova esse prazo. Desconectar o app não envia STOP a uma tarefa autônoma. MANUAL_DISABLE tardio não interfere em corrida/calibração.

Durante calibração e RUNNING o módulo não inicia conversões ADC, evitando disputar o periférico com o firmware de controle. Nesses estados, S usa o cache das leituras concluídas pelo driver; fora deles, faz a varredura frontal para diagnóstico. O Timer1 existente é mantido ativo para os prazos; nenhum timer ou ISR de controle novo foi adicionado.
