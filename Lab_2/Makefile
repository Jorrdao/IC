# Compiler
CXX = g++

# Compiler flags: -Wall (all warnings), -g (debug symbols)
CXXFLAGS = -Wall -g

# Preprocessor flags: -I (Include paths)
CPPFLAGS = -I/usr/include/opencv4

# Linker flags: -L (Library paths)
LDFLAGS = -L/usr/lib/x86_64-linux-gnu

# Libraries to link: -l (library names)
LDLIBS = -lopencv_core -lopencv_imgcodecs -lopencv_highgui

# Source file
SRCS = extract_channel.cpp

# Target executable name
TARGET = 1.1

# Default rule: build the target
all: $(TARGET)

# Rule to link the executable
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $(TARGET) $(SRCS) $(LDLIBS)

# Rule to clean up build files and result images
clean:
	rm -f $(TARGET)
	rm -f Result_Images/*
