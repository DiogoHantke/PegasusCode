# Persistência em Flash do ATmega328P

## Situação implementada

`flash_calibration_read`, `flash_calibration_write` e `flash_calibration_validate` retornam `FLASH_NOT_CONFIGURED`. Ponteiro nulo ou tamanho zero nas operações com buffer retorna `FLASH_INVALID_ARGUMENT`. `FLASH_OK` vale zero; o chamador deve comparar explicitamente o enum. Nenhuma operação acessa memória de programa, altera o buffer de destino ou usa EEPROM/SRAM como persistência. Não existe registro persistente válido nesta versão.

A leitura também está desabilitada porque não existe região reservada e identificada. Ler bytes arbitrários do firmware como parâmetros não constitui uma implementação segura do contrato. O módulo não é chamado pelo algoritmo de calibração.

## Evidência técnica

Foi consultado o [datasheet Microchip DS40002061B, ATmega48A/PA/88A/PA/168A/PA/328/P](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega48A-PA-88A-PA-168A-PA-328-P-DS-DS40002061B.pdf), especificamente as seções para **ATmega328P**: memória de programa, capítulo 27 e tabela 28-11. Flash de 32 KiB, 256 páginas de 64 palavras/128 bytes, resistência mínima de 10.000 ciclos. SPM precisa executar na seção de boot, cujo tamanho depende de BOOTSZ. Lock bits controlam acesso; execução na região RWW precisa respeitar ocupação/reabilitação durante programação. Apagar e escrever são operações distintas de milissegundos. Baixa tensão pode corromper Flash; BOD/reset e alimentação precisam de validação. A seção do datasheet sobre ATmega48 não se aplica a este alvo.

## Configuração encontrada e impedimentos

`platformio.ini` seleciona `uno`, sem framework Arduino, script de linker customizado ou reserva de páginas. O manifesto Uno instalado declara aplicação de até 32256 bytes, bootloader `optiboot/optiboot_atmega328.hex`, LFUSE=0xFF, HFUSE=0xDE, EFUSE=0xFD e lock=0x0F. Esses são padrões do perfil, **não leitura do dispositivo**. A documentação do hardware fala em Nano. Não há dump do bootloader instalado, fusíveis ou lock bits, nem ABI de chamada SPM comprovada.

O linker padrão não reserva páginas de parâmetros. O limite do perfil cobre somente a região de aplicação pressuposta; Make nem aplica esse limite específico do bootloader. Não há garantia de isolamento de dados em nenhuma rota.

O builder Atmel AVR instalado usa AVRDUDE, protocolo `arduino`, `-D` e `-U flash:w:...:i` no upload serial. `-D` sozinho não comprova preservação: o bootloader determina as páginas apagadas e gravadas, e uma imagem futura pode ocupar qualquer endereço não reservado. Um chip erase por ISP pode apagar tanto registros quanto bootloader; EESAVE não preserva dados armazenados em Flash. Nenhum procedimento de atualização foi validado para preservar registros nesta configuração.

## Requisitos para habilitar posteriormente

1. Identificar placa/encapsulamento, ler e registrar fusíveis/locks e identificar o bootloader real sem modificá-los automaticamente.
2. Validar uma rotina de gravação na seção de boot, sua ABI, restrições de endereço, tratamento de interrupções e restauração da região RWW. Não assumir que o protocolo serial do Optiboot oferece uma API chamável pela aplicação.
3. Reservar pelo menos duas páginas inteiras fora do código e do bootloader em ambas as rotas de compilação. Aplicar limites e verificações de ELF/HEX que falhem ao invadir a reserva. Os endereços dependem da configuração real e não foram escolhidos arbitrariamente.
4. Definir procedimento de atualização com preservação explícita da reserva e do bootloader; verificar antes/depois em dispositivo. Tratar ISP/chip erase separadamente.
5. Validar alimentação/BOD e interrupções de energia em cada etapa. Não gravar na malha crítica de controle.

Formato proposto, ainda não implementado: serialização explícita little-endian com identificador de quatro bytes, versão u16, tamanho u16, sequência u32, payload dos oito sensores (canal, mínimo, máximo e aprovação), CRC-16/CCITT-FALSE sobre cabeçalho e payload, marcador de validade e preenchimento até 128 bytes. Não serializar diretamente a struct SENSOR com seus acumuladores e possível padding.

Estratégia a validar: alternar páginas, preservando a última válida enquanto apaga e grava a outra; reler e conferir CRC, versão, tamanho e marcador. Uma página parcialmente gravada deve ser rejeitada no boot. CRC detecta corrupção, mas não torna gravação atômica nem garante integridade sob toda falha elétrica. Sequência e tratamento de wrap precisam ser especificados. Não presumir que escrever um marcador separadamente é seguro sem validar as restrições de programação da página.

Reduzir desgaste gravando apenas calibrações completas alteradas, fora do controle, com orçamento de ciclos e alternância. A política de quando aprovar e persistir continua sendo do autor. A escrita permanece desabilitada até essas dependências e testes serem atendidos.
