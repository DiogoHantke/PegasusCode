# Roteiro de desenvolvimento

O desenvolvimento começa pela relação entre o código e o hardware: é preciso conhecer a leitura da malha QTR, o comportamento dos timers do ATmega328P e a resposta dos motores ao TB6612FNG. Com essa base conferida, a calibração fornece os dados necessários ao estimador, que por sua vez permite integrar o controle de trajetória. As fases abaixo organizam essa sequência e indicam o que deve ser demonstrado antes de avançar.

| Fase | Entrega | Critério de aceite |
| --- | --- | --- |
| 0 — Base confiável | Auditoria de pinos, ADC, PWM, Timer e parada | Canais mapeados, motores testados suspensos, timeout seguro |
| 1 — Calibração | Frontais, laterais, diagnóstico, repetição limitada; EEPROM opcional | Aprovação e falha corretas em dados sintéticos e bancada |
| 2 — Instrumentação | USART, normalização e logs temporizados | Diagnóstico correto sem afetar o controle |
| 3 — Estimador | Posição, confiança e ambiguidades | Casos de centro, bordas, ausência e interseção verificados |
| 4 — Controle | PD periódico inicialmente a 2 ms | Prazo cumprido e seguimento em baixa velocidade |
| 5 — Recuperação | Perda, busca limitada e parada segura | Falhas injetadas não causam avanço ilimitado |
| 6 — Otimização | Velocidade adaptativa, IIR e eventual meta de 1 ms | Ganho medido sem elevar taxa de falha |
| 7 — Robustez | WDT, BOD, EEPROM versionada e fault manager | Matriz de falhas demonstrada |

Para chegar à primeira corrida controlada, a sequência principal passa pela base de hardware, pela calibração mínima, pelo estimador e pelo PD, até incluir a recuperação de linha. A USART ajuda a observar o comportamento durante esse processo e pode evoluir em paralelo, desde que sua integração não atrase as etapas necessárias ao movimento controlado.

## Próximas tarefas

1. Registrar a revisão atual no Git e conservar a configuração `platformio.ini` usada.
2. Auditar em bancada os drivers ADC/PWM/Timer e a semântica de `adc_read()`.
3. Exercitar `sensor_calibration_update()` com sequências sintéticas e documentar transições.
4. Implementar calibração lateral reutilizando o filtro.
5. Implementar diagnóstico sem movimento, aprovando cada sensor individualmente.
6. Implementar repetição limitada, reversão com desaceleração e falha segura.

Cada funcionalidade deve passar por teste em computador, rodas suspensas, pista e atualização desta documentação. Priorize medições antes de otimizações.
