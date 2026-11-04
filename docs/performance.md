# Desempenho e temporização

O desempenho do Pegasus depende de quanto tempo o ATmega328P leva para transformar uma leitura da malha QTR em um novo comando para o TB6612FNG. Por isso, além da frequência do controle, o projeto acompanha o atraso de aquisição e a variação entre ciclos. As metas abaixo orientam esse trabalho e serão confrontadas com medições conforme o controle for integrado.

## Metas iniciais

| Métrica | Meta inicial | Meta evolutiva | Medição |
| --- | --- | --- | --- |
| Período do controle | 2 ms / 500 Hz | 1 ms / 1 kHz | GPIO e analisador lógico |
| Violações de prazo | zero no ensaio definido | igual sob telemetria | contador e WCET medido |
| Jitter | ≤100 µs | ≤50 µs | osciloscópio/analisador lógico |
| Leitura até PWM | ≤5 ms | ≤2 ms | estímulo e captura simultâneos |
| SRAM estática | alvo ≤1 KB | preservar margem de pilha | `avr-size` e análise de pilha |

Não há medições publicadas que comprovem essas metas. Timer0 deve gerar PWM por hardware; Timer1 fornece base de tempo. A proposta é usar ISR curta para ticks e superloop cooperativo, sem RTOS até que medições justifiquem. ADC deve operar de forma não bloqueante, sequenciar canais explicitamente e publicar varredura consistente com timestamp e sequência.

A 16 MHz, prescaler ADC 64 produz clock de 250 kHz e conversão nominal de 13 ciclos, cerca de 52 µs; oito conversões levam cerca de 416 µs antes do overhead. Para precisão de 10 bits, a faixa usual é 50–200 kHz. O firmware atual usa ADCH com alinhamento à esquerda, reduzindo a amostra a 8 bits. Avalie ruído, impedância e aquisição após troca de canal antes de fixar prescaler ou descartar conversões.

Como o ADC lê um canal por vez, a primeira amostra de uma varredura é mais antiga que a última. A medição precisa acompanhar esse percurso completo, incluindo processamento, cálculo do controle e atualização do PWM. Ao repetir o ensaio com telemetria ativa, será possível verificar quanto a comunicação interfere no tempo de resposta e ajustar sua prioridade para preservar a atuação.

No AVR, leituras e escritas de contadores compartilhados com ISR acima de 8 bits precisam de seção atômica ou snapshot consistente. Evite `delay()` e espera ocupada durante movimento, inclusive USART, calibração e reversão. WDT deve ser alimentado pelo supervisor somente quando houver progresso real. Um reset do watchdog não garante por si só saída elétrica segura durante reinicialização.
