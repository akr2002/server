CXX = clang
CXX_MODULE_FLAGS = -fimplicit-modules -fimplicit-module-maps -fprebuilt-module-path=.
CXX_SANITIZE_FLAGS = -fsanitize=address -fsanitize=leak -fsanitize=undefined -fsanitize=bounds -fsanitize=float-divide-by-zero -fsanitize=integer-divide-by-zero -fsanitize=nonnull-attribute -fsanitize=null -fsanitize=pointer-overflow -fsanitize=integer -fno-omit-frame-pointer
CXX_WARNING_FLAGS = -Weverything -Wno-c++98-compat
CXX_FLAGS = -std=c23 -O1 $(CXX_SANITIZE_FLAGS) $(CXX_WARNING_FLAGS) -lc -g

SRC_DIR = src
BUILD_DIR = build
TARGET = $(BUILD_DIR)/server

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCES))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXX_FLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXX_FLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
