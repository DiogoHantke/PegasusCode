MCU ?= atmega328p
F_CPU ?= 16000000UL
CC = avr-gcc
OBJCOPY = avr-objcopy
SIZE = avr-size
BUILD_DIR = build
TARGET = $(BUILD_DIR)/pegasus
SOURCES = $(wildcard src/*.c src/calibration/*.c src/communication/*.c lib/*/*.c)
OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
CPPFLAGS += -DPEGASUS_SUPERVISOR -DBT_BAUD=9600UL -DF_CPU=$(F_CPU) -Iinclude $(addprefix -I,$(wildcard lib/*))
CFLAGS += -mmcu=$(MCU) -std=gnu11 -Os -Wall -Wextra -ffunction-sections -fdata-sections
LDFLAGS += -mmcu=$(MCU) -Wl,--gc-sections

.PHONY: all clean size test
all: $(TARGET).hex

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET).elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

size: $(TARGET).elf
	$(SIZE) --mcu=$(MCU) --format=avr $<

test:
	sh tests/host/run.sh

clean:
	rm -rf $(BUILD_DIR)

-include $(OBJECTS:.o=.d)
