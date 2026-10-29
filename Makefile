# Makefile for nepali_tray_calendar.c
#
# Default toolchain: clang-cl (uses the Windows SDK/CRT from your VS Build
# Tools install, so it's fully ABI-compatible with plain cl.exe).
#
# Run from a "Developer Command Prompt for VS" (or after calling
# vcvarsall.bat) so INCLUDE/LIB env vars point at the Windows SDK.
#
# Usage:
#   make            build nepalical.exe
#   make clean      remove build artifacts
#   make CC=cl      use MSVC's cl.exe instead
#   make CC=zig-cc  use zig cc instead (see notes at bottom)

TARGET  = nepalical.exe
SRCS    = main.c date.c

CC      = clang-cl

CFLAGS  = /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /W3 /O2 /nologo
LIBS    = shell32.lib gdi32.lib user32.lib advapi32.lib
LDFLAGS = /link /subsystem:windows /entry:wWinMainCRTStartup $(LIBS)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) /Fe:$(TARGET) $(LDFLAGS)

clean:
	del /Q $(TARGET) *.obj 2>nul || true

# -----------------------------------------------------------------------
# Alternate toolchains (same flags work for cl.exe unchanged, since
# clang-cl mirrors cl.exe's command-line dialect):
#
#   make CC=cl
#
# For zig, zig's C frontend uses gcc/clang-style flags, not cl-style, and
# it targets mingw (GNU) ABI by default rather than MSVC. It needs a
# different flags set entirely -- not a drop-in CC swap. If you want to
# try it:
#
#   zig cc -municode -DUNICODE -D_UNICODE -O2 nepali_tray_calendar.c \
#       -o nepalical.exe -lshell32 -lgdi32 -luser32 -mwindows
#
# (untested against this file's UTF-8 Devanagari literals under mingw's
# wide-char handling -- clang-cl is the safer default for that reason.)
# -----------------------------------------------------------------------
