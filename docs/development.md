# Desenvolvimento e manutenção

[Início](../README.md) · [Roteiro](roadmap.md) · [Validação](testing-and-validation.md)

O desenvolvimento do Pegasus avança uma funcionalidade por vez, mantendo a documentação próxima do que está sendo feito no código. A ideia é começar pelos casos que podem ser reproduzidos em computador e depois observar a mesma função no robô, primeiro com as rodas suspensas e, quando houver condições, na pista. Os resultados dessa sequência ajudam a decidir o próximo ajuste e deixam uma referência para quem continuar o projeto.

## Convenções

Mantenha headers e implementações em `lib/<módulo>/`, com quatro espaços de indentação. Drivers concentram registradores; aplicação concentra estados, autorização de movimento e tempos. Algoritmos recebem dados e estado explícitos para permitir teste sem AVR. Documente unidades, intervalos, propriedade de buffers, validade e comportamento de erro.

Evite alocação dinâmica e dimensione buffers considerando os 2 KB de SRAM. Dados compartilhados com ISR usam volatile quando necessário e acesso atômico para múltiplos bytes. Não confunda volatile com atomicidade. Limite o trabalho das interrupções e das tarefas cooperativas; proíba esperas bloqueantes durante movimento na arquitetura final.

## Integração de uma alteração

1. Defina comportamento, entradas, saídas, falhas e critério de aceite.
2. Identifique pinos, temporizadores, memória e orçamento de execução envolvidos.
3. Implemente a função e conecte suas transições ao supervisor.
4. Compile com os avisos habilitados e revise consumo estático e margem da pilha.
5. Execute os ensaios pertinentes e registre limites ainda não verificados.
6. Atualize o documento do subsistema e a situação no roteiro.

A CI em `.github/workflows/firmware-build.yml` compila o ambiente atmega328p, registra pacotes/tamanho e publica ELF/HEX quando o trabalho conclui com sucesso. O badge representa a execução do workflow no GitHub; não comprova comportamento em bancada nem valida alterações locais não enviadas.

## Registros de engenharia

Cada resultado deve identificar commit, alterações locais, toolchain, hardware, parâmetros, equipamento, procedimento, comportamento esperado e observado. Metas devem permanecer identificadas até haver evidência. Uma mudança nos pinos também precisa aparecer no documento de hardware. Da mesma forma, ajustes de temporização devem atualizar os registros de desempenho, enquanto novas transições precisam ser refletidas na máquina de estados e nas permissões de comunicação.

EEPROM versionada, causa de reset, WDT e BOD exigem procedimentos próprios antes da validação em pista. O diretorio `lib/flash` ainda é uma reserva e não comprova persistência. Nunca documentar valor antigo de Flash/SRAM como medição da versão corrente.
