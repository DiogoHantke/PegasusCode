# Auditoria e modularização — 7 de novembro de 2026

> A calibração foi posteriormente ampliada . A descrição vigente da aquisição lateral, diagnóstico, recuperação e LED está em [Calibração](calibration.md); os registros abaixo documentam a etapa anterior.

A referência foi o conteúdo local de `src/main.c`, incluindo as alterações não commitadas do autor.

## Alterações

- `include/pegasus_config.h`: constantes originais, sem alteração de valores.
- `include/pegasus_state.h`: estados globais.
- `lib/calibration`: enum e máquina de calibração originais.
- `lib/sensor`: estrutura SENSOR e filtro original, além da normalização já existente.
- `lib/gpio`: sequência original de inicialização dos registradores.
- `src/main.c`: inicialização e máquina de estados principal.
- `lib/flash`: contrato de persistência que retorna indisponibilidade explicitamente.

ADC, PWM, Timer1 e USART já existiam e foram reutilizados. Não foram criados módulos vazios de controle, navegação, diagnóstico ou reposicionamento: não há implementação do autor para mover, nem consumidores que exijam interfaces agora. Os acessos aos motores dentro da calibração foram preservados literalmente. A infraestrutura existente continua disponível; não foram acrescentadas leis de controle ou políticas de recuperação.

## Revisão dos algoritmos

Comentários de revisão assinalam defeitos sem mudar expressões ou transições:

- Timeout do botão sai apenas do `while`: ainda liga PWM e emissores com estado global WAITING. É uma falha crítica preexistente na versão local.
- Espera bloqueante depende da ISR para terminar.
- Subtração em 32 bits não acompanha o retorno a zero do contador de segundos de 16 bits.
- Entrada em EMERGENCY pelo limite de tentativas não desliga os atuadores nesse ramo.
- Etapa lateral não adquire sensores, não incrementa tentativas e não sai. Diagnóstico, reposicionamento e recuperação permanecem pendentes.
- PWM não satura entradas acima de 100%; botão depende de polarização externa; ADC6/7 dependem do encapsulamento real.

O limite de três tentativas adicionais está representado nas constantes/condições do autor, mas ainda não existe fluxo completo que execute essas tentativas. Não se considera esse requisito operacionalmente validado.

## Verificação

PlatformIO `atmelavr@5.1.0`, perfil `uno`, AVR GCC 7.3.0: compilação sem warnings, **1500 bytes de Flash e 107 bytes de SRAM estática**, mesmos valores da referência local antes da separação. O comando funcional nesta máquina é `~/.platformio/penv/bin/pio run`; `/usr/bin/pio` falha por incompatibilidade do PlatformIO 4.3.4 com Click.

Make com a toolchain do sistema também passou sem warnings: **1650 bytes de Flash e 107 de SRAM**, contra 1644/107 antes. As toolchains/opções das duas rotas diferem; não comparar seus tamanhos como regressão.

A comparação lexical confirmou igualdade das funções de calibração e filtro, ignorando apenas espaços e comentários, e da inicialização GPIO extraída. A máquina principal difere apenas pela chamada dessa inicialização. `tests/host/check_preservation.py` guarda hashes da referência local dos algoritmos para detectar alterações posteriores; uma mudança intencional do autor exige atualizar a referência.

`sh tests/host/run.sh` passa: normalização em 16.777.216 combinações, configuração e transmissão USART, médias/transição/aprovação frontal com ADC simulado, timeout de cinco segundos, contrato de Flash desabilitada e preservação lexical. Os cenários rodam em processos separados para reiniciar os estados estáticos.

Os testes antigos falhavam já na compilação por falta de includes. Foram preservados em `tests/host/legacy_diagnostic_test.c`, com includes explícitos. Executar `PEGASUS_TEST_SOURCE=tests/host/legacy_diagnostic_test.c sh tests/host/run.sh` reproduz a expectativa incompatível de relatório serial. O teste corrente verifica a versão local, sem introduzir diagnóstico no firmware para satisfazer o teste antigo.

`-fstack-usage` na rota Make informou frames estáticos de 6 bytes no main, 11 na calibração, 2 no filtro e 8 na ISR Timer1. Isso não é limite total da pilha: chamadas auxiliares de divisão, encadeamento e interrupções precisam entrar na análise ou medição em bancada. A SRAM estática deixa 1941 bytes para pilha e demais usos, sem comprovar o pior caso.

Não houve upload, alteração de fusíveis ou teste físico. Pinos, prescalers e modos dos temporizadores foram mantidos. Consulte [persistência](flash-storage.md) para as restrições específicas da Flash.
