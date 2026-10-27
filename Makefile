CC = gcc
CFLAGS = -Wall -Wextra
TARGET = tetris.exe

BUILD_DIR = build
PDC_DIR = PDCurses
PDC_PLAT = wincon


ifeq ($(OS),Windows_NT)
# 将 shell 设为 Git Bash
GIT_EXEC_PATH := $(strip $(shell git --exec-path 2>NUL))
_git_root     := $(subst /libexec/git-core,,$(GIT_EXEC_PATH))
GIT_USR_BIN   ?= $(subst /ucrt64,,$(subst /clang64,,$(subst /mingw32,,$(subst /mingw64,,$(_git_root)))))/usr/bin

ifeq ($(shell if exist "$(subst /,\,$(GIT_USR_BIN))\sh.exe" echo yes),)
$(error 找不到 Git Bash 的 sh.exe（$(GIT_USR_BIN)）)
endif

SHELL := $(GIT_USR_BIN)/sh.exe
export PATH := $(GIT_USR_BIN):$(PATH)
# 完成将 shell 设为 Git Bash

else
	$(error 暂不支持非 Windows 平台构建)
endif


.PHONY: all clean debug

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm $(TARGET)

pdcurses.a:
	mkdir -p $(abspath $(BUILD_DIR))/$(PDC_DIR)
	make -C $(abspath $(BUILD_DIR))/$(PDC_DIR) -f $(abspath $(PDC_DIR))/$(PDC_PLAT)/Makefile \
		 PDCURSES_SRCDIR=$(abspath $(PDC_DIR)) WIDE=Y DLL=N

$(TARGET): pdcurses.a
	$(CC) main.c $(CFLAGS) $(abspath $(BUILD_DIR))/$(PDC_DIR)/pdcurses.a -o $@