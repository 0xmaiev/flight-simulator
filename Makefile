SHELL = /bin/sh
CXX := g++ -std=c++11
# -MMD -MP: write a .d file per object listing the headers it includes, so
# editing a header rebuilds every .cpp that uses it (see -include at the bottom)
CXX_FLAGS := -O2 -Wall -MMD -MP
# helps with tracing undefined behavior
DEBUG_FLAGS := -O0 -Wall -MMD -MP -g -D_GLIBCXX_DEBUG -D_GLIBCXX_ASSERTIONS -fno-omit-frame-pointer
SANITIZER_FLAGS := -fsanitize=address,undefined
SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := obj
DEBUG_DIR := debug
RELEASE_DIR := release
BIN_DIR := bin
INC_DIR := .
OUTPUTS_DIR := outputs
RESULTS_DIR := results
RELEASE_EXE := flight-simulator
DEBUG_EXE := flight-simulator-debug
ARGS ?= 

# `make` builds the release binary, `make debug` an instrumented one; each has
# its own object directory, so switching between them never mixes their objects
BUILD ?= release
ifeq ($(BUILD),debug)
	CXX_FLAGS := $(DEBUG_FLAGS)
	LDFLAGS += $(SANITIZER_FLAGS)
	OBJ_DIR := $(OBJ_DIR)/$(DEBUG_DIR)
	EXE := $(BIN_DIR)/$(DEBUG_EXE)
else
	OBJ_DIR := $(OBJ_DIR)/$(RELEASE_DIR)
	EXE := $(BIN_DIR)/$(RELEASE_EXE)
endif

EXE := $(BIN_DIR)/$(RELEASE_EXE)
SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# let us check the operating system (Linux, Mac-Darwinuname) to define libs for linker
SYSTEM_UNAME := $(shell uname -s)
ifeq ($(SYSTEM_UNAME),Linux)
	LDLIBS := -lm -lGL -lGLEW -lglfw 
endif
ifeq ($(SYSTEM_UNAME),Darwin)
	LDLIBS := -lm -framework OpenGL -L/opt/local/lib/ -lglm -lGLEW -lglfw
endif

.PHONY: all clean run debug debug_run

all: $(EXE)

run: $(EXE)
	./$(EXE) $(ARGS)

# --- DEBUG TARGETS ---

debug:
	$(MAKE) BUILD=debug

debug_run: debug
	./$(BIN_DIR)/$(DEBUG_EXE) $(ARGS)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR) 
	$(CXX) $(CXX_FLAGS) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR) $(OUTPUTS_DIR)/*.* $(RESULTS_DIR)/*.*

-include $(OBJ:.o=.d)

