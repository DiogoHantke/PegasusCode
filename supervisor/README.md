# Pegasus Supervisor

Aplicação desktop Python 3/Tkinter para o PegasusCode, com conexão Bluetooth Classic SPP por PySerial, seis sensores frontais, calibração embarcada, corrida, controle manual e parada remota. O modo real inicia **desconectado**. Somente identificação PEGASUS v1, ACK de PING e telemetria válida confirmam a conexão.

A interface reproduz o [HTML original](</home/diogohantke/Área de trabalho/supervis_rio_rob_seguidor_de_linha_tkinter.html>): grade superior 7/5, inferior 8/4, painéis, cores, bordas, cronômetro e indicadores de 64 px. O HTML não é necessário em tempo de execução e não é escrito pela implementação. A conexão ocupa uma faixa discreta adicional. Os comandos de manual, STOP, rearme e PWM aproveitam o espaço do painel WASD. Veja [comparação visual e validação](../docs/validation/README.md).

## Instalação no Ubuntu

Requer Python 3.10 ou superior, Tk 8.6, acesso a uma sessão gráfica e `cc` para os testes do C no computador:

```sh
sudo apt install python3 python3-venv python3-tk bluez gcc
cd /home/diogohantke/Documentos/PlatformIO/Projects/PegasusCode/supervisor
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
./run.sh
```

Para apenas executar, `requirements.txt` instala o PySerial. Pytest é dependência de desenvolvimento. Existe um ambiente `.venv` local preparado nesta entrega. Nenhuma dependência do navegador é exigida pela aplicação. As fontes Reddit Sans e Source Code Pro são usadas se instaladas; há fallback para DejaVu Sans/Mono. A ausência dessas fontes pode produzir pequenas diferenças tipográficas em relação ao HTML.

```sh
./run.sh --simulate
./run.sh --debug
```

`--simulate` conecta explicitamente ao robô virtual. Também é possível selecionar **SIMULAÇÃO** na lista de portas. O cabeçalho e rodapé identificam esse modo. O menu Simulação aparece somente nesse modo e permite perder a comunicação e falhar a próxima calibração. Após perda, reconecte explicitamente. O simulador gera voltas de cinco segundos, termina após três voltas e usa leituras sintéticas; esses dados nunca são apresentados como medições físicas.

## Bluetooth e porta serial

O aplicativo usa SPP/RFCOMM, não BLE. Faça o pareamento no gerenciador Bluetooth do Ubuntu ou em `bluetoothctl`:

```text
power on
agent on
default-agent
scan on
pair AA:BB:CC:DD:EE:FF
trust AA:BB:CC:DD:EE:FF
scan off
quit
```

Substitua o endereço pelo módulo encontrado e informe o PIN configurado no HC-05/06. O PIN, nome e canal SPP dependem da configuração do módulo; não são descobertos nem alterados pelo aplicativo. Consulte o canal do serviço serial do seu dispositivo. Exemplo para canal **1 somente quando confirmado**:

```sh
sudo rfcomm bind 0 AA:BB:CC:DD:EE:FF 1
ls -l /dev/rfcomm*
sudo usermod -aG dialout "$USER"
# Entre novamente na sessão para aplicar o grupo.
# Ao encerrar a utilização:
sudo rfcomm release 0
```

Algumas instalações precisam do pacote BlueZ que fornece `rfcomm`/`sdptool`. O supervisório lista portas serial e `/dev/rfcomm*`; isso não é busca de dispositivos Bluetooth pelo rádio. Use ↻ para atualizar ou digite o caminho da porta manualmente. `/dev/ttyUSB0` também pode ser usado em bancada, com apenas um transmissor ligado ao RX do AVR.

Escolha o baud correspondente **aos dois lados**. O firmware entregue usa `BT_BAUD=9600UL`, 8N1. Alterar o seletor Python não reconfigura o módulo HC-05/06 nem a USART do firmware. Configure o modo AT do módulo conforme seu modelo e recompile o firmware para outro baud. Não pressuponha 115200. Abertura da porta pode reiniciar o Nano via conversor USB; aguarde a identificação. Nenhuma reconexão é automática.

PD0/PD1 compartilham a USART com o conversor USB serial do Nano. Desconecte ou isole o módulo durante programação quando houver disputa; não conecte dois TX ao RX do AVR. ATmega alimentado em 5 V pode exigir divisor ou conversor de nível entre **TX do AVR e RX do Bluetooth de 3,3 V**. Confira o módulo real: regulador no breakout não comprova tolerância de 5 V no RX. Use terra comum e confirme alimentação/níveis na bancada.

## Operação

1. Ligue o robô com rodas suspensas na primeira validação e confirme que os motores ficam parados.
2. Selecione a porta e baud, conecte e aguarde o estado confirmado. START não é liberado por abrir a porta.
3. Calibração solicita confirmação porque a rotina existente movimenta os motores. São usados os oito canais embarcados; o painel mostra somente os seis frontais. A interface indica a calibração em andamento, sem inventar percentual; a etapa detalhada está na telemetria e no log de depuração. STOP cancela. Contraste mínimo de aprovação: 40/255, de acordo com o firmware.
4. No firmware básico, START permanece pendente e não movimenta os motores. O cronômetro físico fica indisponível. No simulador, START solicita corrida. A interface aguarda o estado RUNNING recebido para trocar o modo e exibir a contagem embarcada. A corrida e o cálculo PID reais serão integrados pelo usuário; os ganhos enviados ficam apenas armazenados em RAM.
5. Manual ON solicita modo manual. Segure uma tecla ou botão W/A/S/D. Mais de uma direção resulta em parada determinística. Soltar, perder foco ou sair do modo manual interrompe o comando. O limite PWM é configurável em 0–160/255 pelo botão PWM, exibido somente no manual, padrão 80. Teclas digitadas no seletor de porta não movimentam o robô. Esc envia STOP.
6. STOP encerra operação e modo manual. ESTOP trava movimento no firmware. Rearmar envia REARM, mantém motores parados e exige nova solicitação explícita de movimento. START, calibração e manual são intertravados no firmware.

A parada remota depende do enlace e **não substitui circuito físico de emergência**. PWM zero é comando de parada; não comprova parada mecânica, corte de energia ou atuação no STBY do TB6612FNG. O esquema não oferece um STBY controlado pelo firmware. As verificações de tempo real e da frenagem exigem a montagem física.

Sem comandos manuais novos por 300 ms, o firmware zera PWM, sai de manual e entra em STOP. Corrida e calibração, inclusive quando solicitadas pelo app, continuam sem conexão. PING é somente monitoramento do enlace e não é condição para operar. PING não renova o prazo de movimento. Ao congelar o aplicativo, o firmware mantém seus próprios prazos. A calibração pelo botão físico continua disponível sem Bluetooth; o botão fica inibido no manual e durante ESTOP. O botão é lido com debounce de 500 ms em todos os estados; cada ação exige a borda apropriada, e modo manual/ESTOP bloqueiam ações locais. Voltar a INIT exige uma nova pressão posterior para iniciar calibração. Reconexão nunca envia START, CALIBRATE ou MANUAL_ENABLE.

## Sensores e cronometragem

S1–S6 correspondem a ADC1–ADC6. O ADC existente é de 8 bits. HR é ADC0 e HL é ADC7 conforme o esquema; os dois canais laterais continuam na calibração e não foram promovidos a sensores de volta.

Diagnóstico usa mínimos, máximos e aprovação informados pelo firmware. Verde indica contraste aprovado, vermelho indica reprovação ao final de uma calibração com falha. Zero sozinho não é defeito. Sem extremos, dados ou calibração conclusiva, os indicadores mostram o motivo. Valores recentes do ADC são exibidos nos dois modos. Durante corrida, a classificação é normalizada pelos extremos e pela polaridade `CAL_WHITE_HIGH` (padrão 0: branco corresponde a valor baixo); cinza representa preto e verde representa branco, com intensidade contínua.

No firmware básico, o cronômetro físico fica indisponível: ainda não há rotina de corrida medindo tempo. No simulador e numa futura integração, o modelo aceita tempos recebidos, sem extrapolar pelo relógio do PC. Não há detector físico de chegada ou voltas implementado: contador físico permanece zero e última/melhor volta permanecem indisponíveis. O protocolo e o modelo já aceitam eventos LAP para uma integração futura que anuncie essa capacidade. Não há RPM, encoders ou distância estimada.

## Arquitetura

```text
Tkinter (main thread) → RobotController → encoder / validação CRC
                            ↓                     ↓
                       RobotState         SerialTransport (worker)
                            ↑                     ↓
                     Queue + after()    RFCOMM → HC-05/06 → USART
                                                  ↓
                                          parser cooperativo C
                                                  ↓
                                       calibração / manual / parâmetros
```

`app/ui/main_window.py` mantém a reprodução Canvas e os seletores ttk. `theme.py` centraliza a paleta do HTML. `core/config.py` centraliza parâmetros do PC; `core/robot_state.py` valida telemetria antes de alterar o modelo. `robot_controller.py` centraliza conexão, ACKs, timeout e teclas. `communication/protocol.py` é independente da UI; `serial_transport.py` faz E/S numa thread e usa filas limitadas. Nenhum widget é alterado pela worker. O simulador usa exatamente os frames do protocolo e a interface do transporte.

O fechamento libera a porta/thread sem enviar STOP à corrida ou à calibração. Se o último estado conhecido for manual, tenta MANUAL_DISABLE; se a mensagem não chegar, o dead-man local de 300 ms interrompe apenas o movimento manual. MANUAL_DISABLE é rejeitado durante corrida/calibração, protegendo inclusive contra um estado manual desatualizado no PC. Não há repetição automática de comandos de movimento, START ou calibração. Um ACK ausente deixa a confirmação incerta e encerra apenas a conexão de monitoramento; não cancela uma operação autônoma já aceita pelo firmware. O protocolo, limites, estados e códigos estão em [protocolo v1](../docs/supervisor-protocol.md); decisões e arquivos alterados estão em [integração](../docs/supervisor-integration.md).

## Compilação e upload

Na raiz do repositório:

```sh
python3 -m venv .local/pio-venv
.local/pio-venv/bin/pip install 'platformio==6.2.0'
.local/pio-venv/bin/pio run -e atmega328p
.local/pio-venv/bin/pio run -e atmega328p -t upload --upload-port /dev/ttyUSB0
```

O ambiente ainda usa `board=uno`, preservando a configuração existente. Ajuste o perfil/velocidade de upload ao bootloader realmente gravado no Nano, incluindo bootloaders antigos. Não há Arduino Framework. `make`/`make size` continuam disponíveis com AVR-GCC. O ambiente `test_motors` é um ensaio legado separado que **inicia movimento ao ligar e não contém os intertravamentos do supervisório**; não o use como firmware operacional.

## Testes e diagnóstico

```sh
# Dentro de supervisor/
PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 .venv/bin/python -m pytest -q
# Na raiz
sh tests/host/run.sh
```

`PYTEST_DISABLE_PLUGIN_AUTOLOAD=1` evita plugins externos, como os do ROS presentes nesta máquina. Os testes de GUI executam se DISPLAY estiver disponível e são ignorados num ambiente sem servidor gráfico; a suíte restante funciona sem tela. Os testes compilam o parser e o motor de comandos C com `cc`, verificam compatibilidade com Python e usam uma PTY Linux com PySerial. Veja [resultados e procedimentos de bancada](../docs/validation/README.md).

| Sintoma | Verificação |
| --- | --- |
| Lista sem dispositivo | Pareamento e bind RFCOMM; use ↻ ou digite a porta |
| Permissão negada | Grupo dialout e permissões da porta; nova sessão após inclusão no grupo |
| Conectando, depois erro | Firmware v1 carregado, baud/8N1, TX/RX cruzados, terra, alimentação e disputa USB |
| CRC/falhas crescentes | Baud, níveis elétricos, ruído de motores e congestionamento |
| START recusado | Calibração completa de oito canais, manual desligado e ausência de ESTOP |
| Manual não atua | Estado WAITING, Manual ON confirmado, foco da janela, timeout e alimentação da ponte |
| Calibração falha | Pista com transições, emissores, oito canais e sentido dos motores; consulte docs/calibration.md |
| Código NACK | Consulte a tabela do protocolo; nenhum NACK implica início da operação |
| PlatformIO do sistema quebra em resultcallback | Use o venv isolado acima; a instalação global antiga é incompatível com Click nesta máquina |

Não houve upload nem ensaio do robô físico nesta entrega. Compilação, testes de periféricos simulados e E/S via PTY não comprovam latência real, distância de frenagem ou estabilidade na pista.

## Interface simplificada

Conforme a revisão solicitada, foram retirados do painel os endereços ADC, textos repetidos em cada sensor, PWM dos dois motores e contadores técnicos do rodapé. Conexão, estado e último comando usam mensagens curtas em português. O rodapé mantém apenas o evento relevante e o aviso da parada remota. Rearmar aparece quando necessário; PWM aparece no manual; ferramentas de simulação aparecem somente no modo simulado. Valores sem diagnóstico usam travessão, com legenda única, sem classificar a ausência de dados como falha.

Os detalhes de frames, parâmetros de sensores e PWM recebido permanecem acessíveis pelo modo `--debug`. Essa simplificação não modifica o protocolo, os intertravamentos ou o firmware.

## Aba PID & Gráfico de Controle

A nova aba reproduz a referência HTML: ganhos à esquerda, gráfico à direita, três indicadores abaixo e botões **Aplicar Ganhos PID** / **Restaurar Padrão**. A primeira aba mantém Supervisão & Controle. Os dados técnicos continuam fora da tela principal.

- Ganhos: Kp 0–10, Ki/Kd 0–5, com até duas casas decimais; vírgula ou ponto são aceitos. Padrão da referência: **1,45 / 0,08 / 0,32**. Os pequenos valores no topo de cada campo representam a leitura do firmware; o texto editável representa a próxima configuração.
- Aplicar envia SET_PID e aguarda leitura G do firmware, sem tratar edição local como confirmação. A aplicação e o firmware bloqueiam alterações durante corrida, calibração, manual e ESTOP. Ao entrar na aba PID, eventual controle manual é encerrado; a corrida autônoma pode continuar para visualização, com aplicação de ganhos bloqueada.
- Restaurar Padrão preenche os campos, sem transmitir ou mover o robô. Pressione Aplicar para efetivar. Os ganhos ficam apenas na RAM e reiniciam no padrão ao desligar o AVR. Firmware antigo sem GET_PID/SET_PID precisa ser atualizado.
- O gráfico utiliza os seis sensores calibrados e o tempo de corrida recebido do AVR, com janela de dez segundos. **Desvio (%)** substitui graus porque não há geometria física suficiente para medir ângulo. Não é orientação angular, velocidade ou posição métrica.
- Erro Médio é a média dos módulos das amostras; Desvio Máximo é o maior módulo; Tempo em Faixa é o intervalo observado nas amostras recentes dentro de ±5%. Nenhum desses campos é o overshoot ou o tempo de acomodação de um ensaio de degrau identificado. Sem amostras, os campos ficam em `—`; não se desenha uma curva fictícia no modo físico.
- O simulador emite leituras sintéticas por uma resposta amortecida ilustrativa influenciada pelos ganhos, sempre identificada como SIMULADO. Essa curva não prevê o comportamento do robô real.

O firmware básico apenas guarda os ganhos PID em RAM. Não executa PID, seguimento de linha ou cronometragem de corrida. Essas funções serão integradas pelo usuário usando `supervisor_get_gains()`; alterar ganhos não produz movimento.

Veja [captura da nova aba](../docs/validation/tk-pid-simulado.png) e [referência HTML renderizada](../docs/validation/html-pid-referencia.png).

## Autonomia em relação ao Bluetooth

O supervisório é uma ferramenta de observação, diagnóstico e configuração. A futura rotina de controle embarcado não é condicionada a PING, telemetria entregue, app aberto ou conexão ativa pela comunicação básica. A calibração em andamento também não exige que o app permaneça conectado. Erros elétricos ou overflow da recepção UART descartam dados e registram falha de comunicação, sem parar tarefas autônomas. No manual, continuam levando à parada, pois o movimento é comandado pelo operador remoto.

Desconectar, fechar o app ou reconectar não envia STOP/ESTOP nem reinicia uma corrida. Somente um comando explícito de parada ou uma condição local do firmware pode encerrá-la. A reconexão consulta o estado atual e os ganhos, preservando no robô o cronômetro e os parâmetros já aplicados. No simulador, o dispositivo virtual também é preservado entre conexões da mesma janela.

Esta revisão não altera as demais políticas locais de sensores, PID, partida ou ESTOP. Novos ajustes de ganhos continuam sendo enviados pelo protocolo sem recompilar a cada alteração, com o robô parado. Para remover a antiga política de perda do link no robô físico, é necessário gravar esta versão do firmware uma vez. Não houve upload nesta entrega.

## Versão reduzida para reservar memória

A comunicação atual usa RX32, TX64 e parser48 (144 bytes). Frames têm no máximo 47 bytes, com o mesmo formato ASCII/CRC nos dois lados. O firmware não usa printf nem buffers temporários de frames, e mantém os textos na Flash. Telemetria é de 5 Hz somente enquanto houver PING do monitor; parar de receber PING suspende a telemetria, sem parar calibração ou outra rotina autônoma.

START e os tempos físicos estão identificados como pendentes. Os ganhos da aba PID são armazenados e confirmados, mas somente terão efeito no robô quando sua futura rotina os utilizar. O simulador preserva a corrida virtual para testar a interface, claramente marcada como SIMULADO. O protocolo completo atualizado está em `docs/supervisor-protocol.md`.
