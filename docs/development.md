# Desenvolvimento

## Módulos

| Módulo | Responsabilidade | Situação |
| --- | --- | --- |
| `src/main.c` | Inicialização e máquina de estados | Parcial |
| `lib/adc` | Leitura assíncrona de 8 bits | Implementada; validar em hardware |
| `lib/pwm` | Duty cycle nas saídas do Timer0 | Implementado; validar em hardware |
| `lib/timer` | Eventos de 1 ms, 10 ms, 100 ms e 1 s | Implementado; integrar à aplicação |
| `lib/usart` | Comunicação serial | Arquivos vazios reservados |

## Próximas etapas

- Confirmar placa, encapsulamento e mapeamento dos oito sensores.
- Corrigir a leitura do botão e definir seu tratamento de acionamento.
- Implementar coleta de mínimos e máximos durante a calibração.
- Implementar as ações e transições dos demais estados.
- Integrar as interrupções e os eventos do Timer1.
- Implementar a USART, se necessária.
- Validar ADC, PWM e temporização em bancada.

## Organização aplicada

Os módulos foram movidos de `src/libs/` para `lib/`, separados por periférico. Os includes foram ajustados à nova estrutura.

Foram corrigidos os bloqueios de compilação encontrados: proteção e tipos do cabeçalho ADC, include de um arquivo inexistente, retorno ausente em `adc_read` e passagem/atribuição do ponteiro de estado na calibração. Os identificadores dos sensores agora representam números de canais ADC (0 a 7), sem depender do símbolo inexistente PC7.

As configurações antigas do editor foram preservadas em `.local/editor-backup/vscode/`, ignorada pelo Git. Elas continham caminhos locais de AVR e configurações de ROS/Python sem relação com este firmware.

Não há testes automatizados de comportamento. Compilar com sucesso não significa que as funções do robô estejam concluídas.

## Verificação da organização

A compilação foi validada com `make all size` (todos os módulos C) e `pio run` (aplicação e dependências utilizadas). Permanecem avisos de variáveis não utilizadas na calibração incompleta. Não foi realizada gravação ou validação em hardware.
