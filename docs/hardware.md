# Hardware e mapeamento de pinos

[Início](../README.md) · [Gravação](build-and-flashing.md)

O alvo é ATmega328P a 16 MHz em placa Nano, com 32 KB Flash, 2 KB SRAM e 1 KB EEPROM. O perfil de compilação atual é `uno`; o bootloader instalado determina a configuração correta de gravação. ADC6 e ADC7 são entradas analógicas dedicadas nos encapsulamentos que as disponibilizam.

## Componentes e famílias

O ATmega328P concentra a aquisição dos sensores, a temporização e a geração dos comandos. A malha de reflectância fornece os sinais analógicos usados na calibração, enquanto o TB6612FNG recebe os sinais de PWM e direção para acionar os motores. A tabela identifica as referências disponíveis no projeto e deixa explícito o que ainda depende da conferência da montagem.

| Bloco | Part number, família ou identificação | Papel no sistema |
| --- | --- | --- |
| Microcontrolador AVR | ATmega328P | Executa o firmware a 16 MHz e disponibiliza ADC, timers e USART0 |
| Placa de desenvolvimento | Arduino Nano, variante ATmega328P | Suporte físico do microcontrolador e conexão de programação |
| Ponte H dupla | TB6612FNG | Aciona os dois motores com PWM e sinais de direção |
| Malha frontal de reflectância | Identificação QTR01–QTR06 | Seis canais utilizados pela calibração frontal |
| Sensores laterais | Identificação QTR00 e QTR07 | Dois canais reservados à etapa lateral |
| Comunicação sem fio prevista | HC-05 ou HC-06 | Módulo Bluetooth serial, conforme a montagem |
| Motores | Motores DC; part number não registrado | Produzem o movimento das rodas |
| Botão, LED e buzzer | Componentes discretos; part numbers não registrados | Entrada de operação e sinalização |

QTR01 é o identificador de um canal da malha no firmware. Ele não comprova, por si só, um part number comercial como QTR-1A ou o modelo do elemento óptico instalado. A identificação exata deve ser preenchida a partir da placa ou da lista de materiais, para que a documentação não atribua características de outro sensor ao Pegasus.

## Mapeamento declarado no main atual

| Função | Arduino / AVR | Observação |
| --- | --- | --- |
| PWM chamado esquerdo | D5 / PD5 / OC0B | PWM_B |
| Direção chamada esquerda | D8/PB0 e D9/PB1 | IN1 e IN2 |
| PWM chamado direito | D6 / PD6 / OC0A | PWM_A |
| Direção chamada direita | D4/PD4 e D7/PD7 | IN1 e IN2 |
| Frontais | ADC1–ADC6 | QTR01–QTR06 |
| Laterais | ADC0 e ADC7 | QTR00/QTR07; posição física a confirmar |
| Emissores | D11/PB3 | SENSOR_ON |
| Botão | D2/PD2 | Ativo alto; pull-up interno desabilitado |
| LED | D13/PB5 | Indicador de inicialização/calibração |
| USART | D0/PD0 e D1/PD1 | RX e TX |
| Buzzer histórico | D10/PB2 | Sem acionamento no main atual |

Esses nomes descrevem o software, não comprovam o lado físico. O planejamento informa MOTOR_L em A01/A02 e MOTOR_R em B01/B02 do TB6612FNG, com divergências históricas de nomenclatura. Por isso, a conferência precisa acompanhar cada canal até a roda correspondente e verificar o sentido de rotação antes do ajuste do controlador. Nessa mesma etapa, devem ser medidos a tensão e o estado de STBY, além do comportamento de frenagem e roda livre. PWM zero representa um comando; não comprova parada mecânica nem desligamento elétrico completo.

O botão é descrito no esquema de referência com pull-down externo. Conferir montagem, polaridade, debounce, AVcc/AREF, ruído e ordem física dos seis frontais. Verificar também a polaridade claro/escuro de cada canal.

## Alimentação e comunicação

A energia dos motores vem de VBAT pela chave da placa; conexão USB não confirma alimentação do driver. Confirmar terra comum, reguladores e coexistência USB/bateria. PD0/PD1 podem ser compartilhados pelo conversor USB serial e módulo externo; evitar disputa durante gravação.

HC-05/06 é a opção Bluetooth serial prevista, condicionada ao módulo instalado e aos níveis elétricos. ESP8266 fornece Wi-Fi, não Bluetooth nativo; ESP32 é uma alternativa com Bluetooth em variantes compatíveis. Encoders e IMU estão fora do escopo atual. Sem medição adicional, não se conhece velocidade real, distância de retorno, corrente nem todo defeito mecânico.
