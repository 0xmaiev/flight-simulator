SHELL = /bin/sh
CXX := g++ -std=c++11
# -MMD -MP: write a .d file per object listing the headers it includes, so
# editing a header rebuilds every .cpp that uses it (see -include at the bottom)
CXXFLAGS := -O2 -Wall -MMD -MP
# helps with tracing undefined behavior
DEBUGFLAGS := -O0 -Wall -MMD -MP -g -D_GLIBCXX_DEBUG -D_GLIBCXX_ASSERTIONS -fno-omit-frame-pointer -fsanitize=address,undefined
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
INC_DIR := .
OUTPUTS_DIR := outputs
RESULTS_DIR := results
ARGS := ?

EXE := $(BIN_DIR)/flight-simulator
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
# Overrides CXXFLAGS and appends sanitizer flags to LDLIBS for linking
debug: CXXFLAGS := $(DEBUGFLAGS)
debug: LDLIBS += -fsanitize=address,undefined

# Builds with debug settings first, then runs the executable
debug_run: debug
	./$(EXE) $(ARGS)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR) 
	$(CXX) $(CXXFLAGS) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR) $(OUTPUTS_DIR)/*.* $(RESULTS_DIR)/*.*

-include $(OBJ:.o=.d)

