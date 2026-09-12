# Компіляція програм лабораторної роботи.
#
# Завдання, реалізовані мовою C, мають розширення .c і компілюються gcc;
# завдання, у яких використано засоби мови C++ (потоки введення-виведення,
# посилання, класи файлових потоків), мають розширення .cpp і компілюються g++.
#
# Використання:  make            - скомпілювати всі завдання у каталог bin/
#                make run1       - скомпілювати й запустити завдання 1
#                make clean      - прибрати результати компіляції

CC       := gcc
CXX      := g++
CFLAGS   := -std=c17 -Wall -Wextra -pedantic -O2
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2
LDLIBS   := -lm
BINDIR   := bin

CSRC := $(wildcard task*.c)
CXXSRC := $(wildcard task*.cpp)
BIN  := $(patsubst %.c,$(BINDIR)/%,$(CSRC)) $(patsubst %.cpp,$(BINDIR)/%,$(CXXSRC))

.PHONY: all clean
all: $(BIN)

$(BINDIR)/%: %.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

$(BINDIR)/%: %.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(BINDIR)

# Цілі швидкого запуску: make run1, make run2, make run3
run%: $(BINDIR)/task%
	./$<
