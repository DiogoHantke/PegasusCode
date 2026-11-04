# Testes e validação

[Início](../README.md) · [Desempenho](performance.md) · [Roteiro](roadmap.md)

A validação acompanha o mesmo caminho do desenvolvimento. Primeiro, os algoritmos recebem sequências conhecidas em computador, para que seus resultados possam ser conferidos. Depois, a bancada permite verificar como o ADC do ATmega328P lê a malha QTR e como o TB6612FNG responde aos comandos. Com essas partes compreendidas, os ensaios avançam para o movimento e a pista. Este documento organiza os procedimentos que devem produzir essas evidências.

## Testes disponíveis

`tests/host/run.sh` compila um executável temporário com GCC e registradores AVR simulados, exercitando normalização e diagnóstico serial. O teste de integração existente pressupõe uma sequência com relatório frontal que não está presente no main atual; precisa ser reconciliado antes de servir como critério de aceite. Não considerar a existência do teste como prova de aprovação.

```sh
sh tests/host/run.sh
```

## Algoritmos em computador

| Área | Casos necessários |
| --- | --- |
| Média/filtro | Dez amostras, independência entre canais, startup sem referência, deriva, igualdade aos limiares e salto repetido |
| Transições | Médias 220,218,35,33; 220,218,35,220; definir dez amostras por média ao montar fixtures |
| Normalização | Min/max válidos, igualdade, saturação, fora de faixa e overflow |
| Centroide | Centro, extremos, soma zero, faixa larga e cruzamento |
| Controlador | Saturação, sinal, derivada ruidosa e reentrada após perda |
| Estados | Guardas, timeouts, limite de tentativas e parada |
| Persistência | Versão/CRC inválidos e escrita interrompida |

## Bancada

Confirmar os oito canais contra superfícies claras/escuras, polaridade e ruído. Com rodas suspensas, testar cada motor isoladamente, relação A/B com lados, direção, PWM mínimo, STBY e tempo de desaceleração. Medir Timer1, portadora PWM, período de controle e jitter com GPIO e analisador lógico.

Aplicar telemetria no pior cenário permitido e observar violações de prazo. Simular sensor inválido, referência ausente e aquisição pendente. Após implementar WDT, forçar travamento e verificar o comportamento elétrico durante reset e boot. Não concluir segurança a partir apenas de variáveis de software.

## Pista e evidências

Repetir retas, curvas suaves e fechadas, diferentes luminosidades e sujeira. Comparar filtro e leitura direta, velocidade fixa e adaptativa, períodos de amostragem. Medir tempo de percurso, perdas e recuperações por configuração. Velocidade exige método físico adequado; duty não é medição.

Cada ensaio registra revisão, parâmetros, montagem, bateria, pista, instrumentos, quantidade de repetições, esperado, observado e arquivos brutos. Fixar critérios antes do teste e registrar falhas também. Robustez exige repetição e falhas injetadas; um percurso bem-sucedido não demonstra toda a matriz de falhas.
