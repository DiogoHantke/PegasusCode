# Validação do Pegasus Supervisor

Revisão em 21/11/2026, Ubuntu Linux, Python 3.12/Tk 8.6, PlatformIO Core 6.2.0 em ambiente isolado e plataforma Atmel AVR 5.1.0. Não houve upload, pareamento com módulo físico ou teste experimental do robô.

## Resultados executados

| Verificação | Resultado | Limite da evidência |
| --- | --- | --- |
| Pytest | **74 testes aprovados**, incluindo GUI na sessão X11 | Lógica Python, parser C e comandos C com periféricos simulados |
| Compatibilidade CRC/parser | 500 frames válidos gerados e comparados entre C e Python; frames inválidos/truncados rejeitados | Não reproduz ruído elétrico real |
| Firmware de comunicação no host | Treze cenários (incluindo armazenamento de parâmetros, START pendente e congestionamento TX): ESTOP, dead-man, wrap do relógio, intertravamentos, overflow, perda do link, corrida/calibração, duplicatas e calibração física local | GPIO/ADC/relógio simulados |
| PySerial com PTY Linux | Handshake, reconexão e desconexão aprovados usando thread e porta serial de verdade do SO | PTY não contém rádio, HC-05/06 ou USART física |
| Calibração e botão legados | **53 cenários aprovados**: 40 de calibração e 13 de botão/estados/intertravamentos | Periféricos simulados e fluxo atual do botão, sem medição física |
| PlatformIO `atmega328p` | Sucesso, sem warnings: **7284 bytes Flash e 324 bytes SRAM estática** | SRAM da stack não incluída; nenhuma garantia de prazo experimental |
| PlatformIO `test_motors` | Sucesso, sem warnings: 792 bytes Flash e 15 bytes SRAM estática | Firmware de ensaio independente, não é seguro para operação normal |
| Make/AVR-GCC | Compilação e geração de HEX concluídas sem warnings | Toolchain local pode produzir tamanho diferente do PlatformIO |
| Revisão visual | HTML renderizado em Chromium e Tkinter executado, capturados e inspecionados | Não foi medido erro pixel a pixel; fontes de fallback variam |

Logs: [PlatformIO](platformio-build.log), [pytest](pytest.log), [calibração/botão](calibration-host-tests.log).

A instalação global `/usr/bin/pio` desta máquina estava incompatível com a versão do Click (`resultcallback`). A compilação registrada usa PlatformIO 6.2.0 isolado; o README fornece a instalação reproduzível. Não foi alterada a instalação global.

## Comparação visual

O HTML original foi lido integralmente (HTML, CSS e JavaScript) e renderizado em viewport 1280×1024. A caixa principal mede aproximadamente 1024×672, com linha superior de 229 px e inferior de 323 px. Coordenadas verificadas no navegador: painéis superiores com larguras 569,66 e 402,33; inferiores com 653,33 e 318,66 px; gap de 16 px. Os sensores usam quadrados de 64 px. A reprodução preserva essas métricas na janela de Supervisão 1024×751, incluindo abas; a aba PID usa 1024×626, incluindo a faixa de conexão e o rodapé simplificado.

O HTML contém dados estáticos de exemplo e um cronômetro JavaScript fictício. Sua captura abaixo é **referência visual**, não resultado do robô. As capturas Tk marcadas como simulação vêm exclusivamente do robô virtual. A aplicação desconectada não exibe sensores aprovados, porta fixa ou conexão inventada.

- [HTML original renderizado](html-referencia.png)
- [HTML atualizado: aba PID](html-pid-referencia.png)
- [Tkinter: aba PID simulada](tk-pid-simulado.png)
- [Tkinter desconectado](tk-desconectado.png)
- [Tkinter: diagnóstico simulado](tk-diagnostico-simulado.png)
- [Tkinter: corrida simulada](tk-corrida-simulada.png)
- [Tkinter: ESTOP simulado](tk-estop-simulado.png)

Pequenas diferenças esperadas: fontes DejaVu quando Reddit Sans/Source Code Pro não estiverem instaladas, ícones Unicode no lugar da fonte Material Symbols e seletores reais ttk. A organização, proporções, paleta, bordas, botões, sensores, WASD e cronômetro foram mantidos. Os novos controles funcionais ficam na faixa de conexão e no espaço do painel manual; o rodapé preserva um aviso curto da parada remota. Por solicitação posterior, informações técnicas repetidas foram removidas da tela, as legendas encurtadas e os controles de PWM/rearme exibidos conforme o estado.

Para refazer as capturas Tk, instale opcionalmente Pillow no ambiente de desenvolvimento e execute `python supervisor/tools/capture_visuals.py` da raiz, numa sessão X11. O script usa apenas simulação. SHA-256 da versão auditada inicialmente: `8eee51a8f938b9896567f5480e367f207d848928c9262d1466106c58abe3ec9d`.

## Procedimento de bancada pendente

1. Confira continuidade, pinos e níveis elétricos contra `hardware-reference.pdf` e a montagem real. Confirme STBY, terra, alimentação, avarias e interferência USB/HC-05/06. Faça o primeiro ensaio com rodas suspensas e parada física acessível.
2. Grave o ambiente operacional, com bootloader adequado ao Nano; confirme PWM zero desde energização, durante reset, abertura da porta e reconexão.
3. Configure o mesmo baud no HC-05/06, AVR e PC. Observe RX/TX com analisador lógico: 8N1, CRC, descarte de truncados, taxa efetiva e ausência de overflow. Meça o atraso entre recepção válida do ESTOP e PWM zero; a latência total do rádio deve ser medida separadamente.
4. Teste W/A/S/D com PWM baixo: sentidos físicos, identificação das rodas, inversões e resposta com carga. Solte teclas, retire foco, congele/encerre a GUI, interrompa a conexão e confirme comando zero após timeout. Meça tempo e distância mecânica de frenagem; o software só confirma a solicitação PWM.
5. Acione ESTOP durante manual, calibração e corrida. Tente comandos antigos, START e MANUAL_ENABLE; nenhum movimento deve ocorrer até REARM seguido de solicitação explícita. Reconectar não deve iniciar/reiniciar tarefas ou interromper uma corrida já em andamento.
6. Passe branco/preto nos oito canais e confira extremos, polaridade e critérios de diagnóstico. Uma leitura zero pode ser válida. Valide calibração, falha, cancelamento e rotina do botão sem Bluetooth. Os laterais HR/HL não são detectores de volta nesta versão.
7. Confirme a ordem S1–S6 e a polaridade. O módulo básico não possui controle de corrida nem saída PID; esses ensaios ficam para sua futura implementação. Quando existir, avalie ruído, jitter e frequência de controle com telemetria ligada/desligada.
8. Meça stack máxima com instrumentação, uso efetivo de CPU/USART e queda de telemetria sob congestionamento antes de aumentar taxa ou adicionar campos. Interrompa o Bluetooth e feche o app durante corrida e calibração: as tarefas devem continuar sob as regras locais, sem reiniciar ao reconectar. No manual, confira a parada pelo dead-man de 300 ms, independentemente de PING.
9. Implemente e valide um mecanismo físico de chegada/voltas antes de habilitar `lap_support=1` no firmware. Compare tempos com uma referência independente. Até lá, última/melhor volta reais permanecem indisponíveis.

## Critérios atendidos e pendentes

Implementados e exercitados no computador: interface Tkinter, comunicação bidirecional pelo protocolo único, handshake, portas configuráveis, seis sensores, diagnóstico por calibração, modos visuais, rotina de calibração embarcada acionável, START, manual WASD, dead-man, STOP/ESTOP, thread sem widgets, simulador, testes e compilação.

Pendentes de validação experimental: rádio Bluetooth real, compatibilidade elétrica, sentidos e frenagem dos motores, estabilidade e prazos de controle na pista, repetibilidade da calibração e margem de stack. Pendente de implementação física: detecção de voltas/chegada. Nenhum desses itens foi substituído por medições fictícias.

## Auditoria do fluxo do botão

Corrigidas quatro falhas reproduzidas em testes: cálculo da duração com largura diferente do contador de 16 bits; ausência de reset ao iniciar nova calibração; ações por nível que reutilizavam a mesma pressão entre estados; ausência de intertravamento do botão com manual/ESTOP. O debounce de 500 ms foi preservado e centralizado em pegasus_config.h. Uma pressão iniciada durante bloqueio não é reaproveitada após REARM. O relógio local é mantido pelo main e não pela presença do monitor.

Os testes anteriores do botão esperavam outro fluxo de 30 ms; foram substituídos por 13 cenários que verificam o contrato atual, incluindo bordas, repique, wrap, retomada após falha/sucesso, pressão durante calibração, ESTOP, manual e botão pressionado no boot. Todos passaram. A suíte total do host agora executa 53 cenários. O caso RUNNING continua vazio: testar entrada no estado não comprova um controle de corrida existente.

## Revisão de autonomia

Os testes atuais conferem que desconexão, perda do link e erros UART não encerram a corrida, que calibração remota conclui sem monitor e que a reconexão consulta a mesma tarefa sem zerar o cronômetro. Também verificam que o manual continua parando por falta de comandos novos, mesmo recebendo PING, e que um MANUAL_DISABLE tardio não interfere em corrida/calibração. Essa evidência continua sendo de host/simulação, sem ensaio do robô físico.

## Redução para comunicação básica

| Medida | Antes (com protótipo de corrida) | Comunicação básica atual |
| --- | ---: | ---: |
| Flash | 10.196 bytes | **7.034 bytes (21,8%)** |
| SRAM estática | 1.045 bytes | **324 bytes (15,8%)** |
| Buffers RX/TX/parser | 512 bytes | **144 bytes** |

A redução é de 721 bytes de SRAM (aproximadamente 69%) e 3.162 bytes de Flash (31%). Restam 1.724 bytes de SRAM para futura implementação e pilha, cujo pico ainda não foi medido no robô. O ambiente `test_motors` permanece com 792 bytes Flash e 15 bytes SRAM estática.

O emissor C foi comparado com o decoder Python usando ACK, identificação com capacidades, estado, sensores nos extremos e ganhos máximos. Também foram exercitados congestionamento TX, reserva de ACK, frames corrompidos, duplicatas, limites e ausência de dependência do Bluetooth. START físico agora recebe NACK 6 e não movimenta motores. As capturas de corrida/curva anteriores continuam sendo apenas de simulação; não demonstram uma corrida implementada no AVR.

A rotina PID experimental e as regras experimentais de linha/tempo de controle foram retiradas. Os testes que simulam um estado RUNNING definem estado e PWM no harness para confirmar que a comunicação não interfere numa futura rotina do usuário; eles não executam um firmware de corrida real.

## LED em EMERGENCY

Implementado o padrão solicitado: aceso em 250–500 ms e 750–1000 ms; apagado nos demais intervalos; ciclo de 2000 ms. O teste host verifica limites, repetição, reentrada e a virada do contador de 16 bits, preservando outros bits de PORTB. A sinalização usa três bytes de estado. Após a revisão de botão e formatação, o total estático medido é 324 bytes de SRAM e 7284 bytes de Flash. Não houve medição física do LED.

## Revisao visual e de estilo do codigo

O C acrescentado foi ajustado ao padrao de escrita do usuario. Apos a formatacao, foram repetidos os testes de CRC/parser/emissor, comandos e LED, a suite de calibracao/botao e as duas compilacoes PlatformIO. Nao houve upload nem teste experimental de motores, sensores ou Bluetooth.
