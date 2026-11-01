# Hardware e pinagem

## Configuração atual

- Microcontrolador de compilação: ATmega328P.
- Frequência: 16 MHz.
- Botão declarado em PD2, configurado como entrada com pull-up.
- USART: PD0 (RX) e PD1 (TX), apenas declarados na aplicação.
- PWM: PD6 (OC0A) e PD5 (OC0B), pelo Timer0.
- Timer1: modo CTC, prescaler 64 e OCR1A = 249; período nominal de 1 ms a 16 MHz.
- ADC: referência AVcc, resultado alinhado à esquerda e leitura de 8 bits.

## Pendências para validar na placa

1. O código reserva os canais ADC de 0 a 7 para oito sensores. A associação original PC0 a PC7 foi substituída por identificadores numéricos de canal, pois PC7 não existe no alvo de compilação. A ligação física precisa ser confirmada: os canais ADC6/ADC7, quando disponíveis no encapsulamento, não correspondem a GPIOs PC6/PC7. PC6 também tem função de RESET.
2. O botão é configurado em PD2, mas a calibração consulta `PORTB`. A leitura da entrada deve ser revisada, incluindo o uso do registrador `PINx` correto.
3. O ADC usa prescaler 32, resultando em clock de 500 kHz a 16 MHz. Verifique a precisão necessária e os requisitos do conversor.
4. O Timer1 oferece interrupções, mas a aplicação ainda não as habilita nem consome seus eventos.
5. A calibração inicia e para o temporizador sem coletar amostras. Os extremos dos sensores ainda não são usados.

Esta lista descreve o código atual; não substitui o esquema elétrico ou a validação em bancada.
