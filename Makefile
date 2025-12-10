# Makefile for bootleg3D
# Builds the library and all example programs

CC = cc
CFLAGS = -O3 -Wall -Wextra

# SDL2 configuration (auto-detect via sdl2-config)
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs)

# Directory structure
INCLUDE_DIR = include
SRC_DIR = src
EXAMPLES_DIR = examples

# Library source and object
LIB_SRC = $(SRC_DIR)/b3d.c
LIB_OBJ = $(SRC_DIR)/b3d.o

# Example source files
IMAGE_SRC = $(EXAMPLES_DIR)/image.c
CUBES_SRC = $(EXAMPLES_DIR)/cubes.c
OBJ_SRC = $(EXAMPLES_DIR)/obj.c
FPS_SRC = $(EXAMPLES_DIR)/fps.c

# Executable names
IMAGE_EXE = image
CUBES_EXE = cubes
OBJ_EXE = obj
FPS_EXE = fps

# All targets
ALL_TARGETS = $(IMAGE_EXE) $(CUBES_EXE) $(OBJ_EXE) $(FPS_EXE)

# Default target - build all examples
all: $(ALL_TARGETS)

# Build library object file
$(LIB_OBJ): $(LIB_SRC) $(SRC_DIR)/math.h $(INCLUDE_DIR)/b3d.h
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Image example - no SDL2 dependency
$(IMAGE_EXE): $(IMAGE_SRC) $(LIB_OBJ)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $^ -o $@ -lm

# Examples that require SDL2
$(CUBES_EXE): $(CUBES_SRC) $(LIB_OBJ)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -I$(INCLUDE_DIR) $^ -o $@ $(SDL_LIBS) -lm

$(OBJ_EXE): $(OBJ_SRC) $(LIB_OBJ)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -I$(INCLUDE_DIR) $^ -o $@ $(SDL_LIBS) -lm

$(FPS_EXE): $(FPS_SRC) $(LIB_OBJ)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -I$(INCLUDE_DIR) $^ -o $@ $(SDL_LIBS) -lm

# Aliases for building individual examples
build-image: $(IMAGE_EXE)
build-cubes: $(CUBES_EXE)
build-obj: $(OBJ_EXE)
build-fps: $(FPS_EXE)

# Image generation
image_gen: $(IMAGE_EXE)
	./$(IMAGE_EXE)

# Clean build artifacts
clean:
	rm -f $(ALL_TARGETS) $(LIB_OBJ)

# Clean everything including generated assets
cleanall: clean
	rm -f assets/cube.tga

# Rebuild everything
rebuild: clean all

# Run examples from top-level directory
run-image: $(IMAGE_EXE)
	./$(IMAGE_EXE)

run-cubes: $(CUBES_EXE)
	./$(CUBES_EXE)

run-obj: $(OBJ_EXE)
	./$(OBJ_EXE)

run-fps: $(FPS_EXE)
	./$(FPS_EXE)

# List all targets
list:
	@echo "Available targets:"
	@echo "  all         - Build all examples"
	@echo "  build-image - Build image example (no SDL2 dependency)"
	@echo "  build-cubes - Build cubes example (requires SDL2)"
	@echo "  build-obj   - Build obj example (requires SDL2)"
	@echo "  build-fps   - Build fps example (requires SDL2)"
	@echo "  image_gen  - Build and run image example to generate assets/cube.tga"
	@echo "  run-image  - Run image example (generates assets/cube.tga)"
	@echo "  run-cubes  - Run cubes example"
	@echo "  run-obj    - Run obj example"
	@echo "  run-fps    - Run fps example"
	@echo "  clean      - Remove all built executables and object files"
	@echo "  cleanall   - Remove executables, object files, and generated assets"
	@echo "  rebuild    - Clean and rebuild all examples"

.PHONY: all clean cleanall rebuild list build-image build-cubes build-obj build-fps image_gen run-image run-cubes run-obj run-fps
