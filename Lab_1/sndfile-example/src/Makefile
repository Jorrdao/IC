#Default target
all:
	@(if [ ! -d build ]; then mkdir build; fi)
	@(cd build && cmake .. && cmake --build .)

#Clean build artifacts
clean:
	rm -rf build ../bin

# Rebuild everything from scratch
rebuild:	clean all