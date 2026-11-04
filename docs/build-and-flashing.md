# Compilação e gravação no ATmega328P

[Início](../README.md) · [Hardware](hardware.md) · [Validação](testing-and-validation.md)

Para colocar o firmware no ATmega328P, primeiro é preciso compilar o projeto e gerar a imagem que será gravada na Flash. Depois, a conexão USB da placa permite transferir essa imagem pelo bootloader, desde que o perfil escolhido corresponda à versão instalada. Ao terminar, o microcontrolador reinicia e começa a executar o main. Como o acionamento do botão pode iniciar a calibração e enviar comandos ao TB6612FNG, a gravação deve ocorrer com a alimentação dos motores desabilitada, seguida de uma primeira execução com rodas suspensas.

## 1. Preparar o ambiente

O projeto usa C GNU11, AVR Libc e registradores, sem framework Arduino. `platformio.ini` define ambiente `atmega328p`, plataforma `atmelavr@5.1.0`, frequência de 16 MHz e perfil `uno`. Para reproduzir a versão de PlatformIO indicada na CI em um ambiente Python isolado:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install 'platformio==6.2.0'
pio --version
```

Se o PlatformIO Core já estiver disponível no terminal da extensão, utilize essa instalação. No Linux, Python e o suporte a ambientes virtuais precisam estar instalados. A primeira compilação pode baixar a toolchain AVR.

## 2. Confirmar placa e bootloader

A placa descrita é Nano, mas a configuração corrente seleciona Uno. Confirmar o bootloader instalado antes de gravar. Na seção `[env:atmega328p]`, ajustar `board` ao perfil correspondente, preservando nome do ambiente e frequência:

| Perfil | Referência | Baud de gravação |
| --- | --- | --- |
| `uno` | Perfil atual, bootloader Uno compatível | 115200 |
| `nanoatmega328` | Nano com bootloader antigo | 57600 |
| `nanoatmega328new` | Nano com bootloader novo | 115200 |

A seleção também determina o limite de aplicação reservado pelo bootloader; alterar apenas baud não corrige toda incompatibilidade. Não confundir baud de gravação com `monitor_speed`, que configura o terminal serial da aplicação.

## 3. Compilar e inspecionar

Na raiz do repositório:

```sh
pio run -e atmega328p
pio run -e atmega328p -t size
pio pkg list -e atmega328p
```

Verifique sucesso e avisos antes de utilizar os arquivos `.pio/build/atmega328p/firmware.elf` e `firmware.hex`. Um arquivo remanescente de compilação anterior não comprova que a compilação atual passou. O relatório de RAM cobre dados estáticos; reserve margem para pilha e interrupções.

A rota alternativa usa a toolchain AVR do sistema:

```sh
make
make size
```

Ela requer Make, avr-gcc, AVR Libc e Binutils AVR e produz `build/pegasus.elf` e `build/pegasus.hex`. Gravação manual também requer AVRDUDE.

## 4. Identificar a porta e liberar acesso

Conecte a placa por cabo USB de dados e execute:

```sh
pio device list
ls -l /dev/serial/by-id/
```

A porta pode ser `/dev/ttyUSB0`, `/dev/ttyACM0` ou outra. Use a identificação real. No Linux, confira proprietário e grupo com `ls -l /dev/ttyUSB0` e sua associação com `id`. Se o dispositivo pertencer a `dialout` e seu usuário não participar desse grupo:

```sh
sudo usermod -aG dialout "$USER"
```

Encerre a sessão e entre novamente. Use o grupo efetivo do dispositivo caso seja diferente; não execute toda a toolchain como root. Feche monitores seriais e isole módulos externos que disputem RX/TX com a interface USB.

## 5. Gravar pelo bootloader serial

Com perfil, porta e alimentação conferidos, execute, substituindo a porta do exemplo:

```sh
pio run -e atmega328p -t upload --upload-port /dev/ttyUSB0
```

Aguarde conclusão e verificação. A placa pode reiniciar imediatamente após a gravação. Não ignore erro de assinatura nem utilize imagem que exceda a área de aplicação do bootloader.

Como alternativa manual, para bootloader confirmado como compatível com protocolo Arduino a 115200 baud:

```sh
avrdude -p m328p -c arduino -P /dev/ttyUSB0 -b 115200 \
  -U flash:w:.pio/build/atmega328p/firmware.hex:i
```

Use 57600 somente se corresponder ao bootloader identificado; para a rota Make, substitua a imagem por `build/pegasus.hex`. Mantenha a verificação habilitada. Gravação ISP exige programador, conexões MOSI/MISO/SCK/RESET e alimentação corretas; pode remover o bootloader conforme a operação. Deve seguir um procedimento específico para o equipamento, sem alterações especulativas de fusíveis.

## 6. Primeira execução

Confira desligamento dos comandos, botão, emissores e sensores antes de energizar motores. O programa atual aguarda o botão por dois segundos; sem acionamento entra em WAITING, ainda sem lógica de retomada. Com acionamento, solicita 20% de PWM durante a calibração frontal, com timeout nominal de cinco segundos. Concluir os seis frontais para o PWM e entra na etapa lateral ainda vazia. Isso não inicia uma corrida.

O monitor pode ser aberto com:

```sh
pio device monitor -p /dev/ttyUSB0 -b 115200
```

O main atual não inicializa USART nem transmite relatórios. Ausência de texto é esperada nessa versão. Depois da integração de comunicação, consulte [telemetria](telemetry-and-cli.md) para interpretar os dados.

## Diagnóstico de problemas

| Sintoma | Verificação |
| --- | --- |
| Porta ausente | Cabo de dados, USB e enumeração do sistema |
| Permissão negada | Grupo do dispositivo e nova sessão após alteração |
| Porta ocupada | Monitor, CLI ou outro processo usando a conexão |
| Falha de sincronismo | Bootloader, perfil, baud, porta, reset e disputa RX/TX |
| Assinatura divergente | MCU selecionado, alimentação e conexão; não forçar |
| Motor sem movimento | VBAT, chave, STBY, mapeamento e direção |
| Temporização incorreta | Oscilador real e configuração de 16 MHz |

Registre revisão, perfil, toolchain e procedimento usado a cada validação. A gravação bem-sucedida comprova a transferência da imagem, não o comportamento do robô na pista.
