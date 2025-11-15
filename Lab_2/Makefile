# Compiler
CXX = g++

# Compiler flags: -Wall (all warnings), -g (debug symbols)
# Added -std=c++17, as the new Golomb code requires it
CXXFLAGS = -Wall -g -std=c++17

# Preprocessor flags: -I (Include paths)
CPPFLAGS = -I/usr/include/opencv4

# Linker flags: -L (Library paths)
LDFLAGS = -L/usr/lib/x86_64-linux-gnu

# Libraries to link: -l (library names)
# The Golomb test doesn't need OpenCV, but linking it is harmless
# and simplifies the Makefile.
LDLIBS = -lopencv_core -lopencv_imgcodecs -lopencv_highgui

# --- Targets ---
TARGET_1_1 = 1.1_extract_channel
TARGET_1_2 = 1.2_image_ops
TARGET_2_1 = 2.1_golomb_test
TARGET_5 = image_codec

# --- Source Files ---
SRCS_1_1 = extract_channel.cpp
SRCS_1_2 = image_operations.cpp
SRCS_2_1 = main_part2.cpp golomb.cpp bitstream.cpp
SRCS_5 = image_codec.cpp golomb.cpp bitstream.cpp

# --- Object Files ---
# This converts the .cpp list from SRCS_2_1 into a .o list (e.g., main_part2.o)
# This is the standard way to handle multi-file projects in 'make'
OBJS_2_1 = $(SRCS_2_1:.cpp=.o) 
OBJS_5 = $(SRCS_5:.cpp=.o)


# Default rule: build all targets
all: $(TARGET_1_1) $(TARGET_1_2) $(TARGET_2_1) $(TARGET_5)

# --- Rules for Part 1 (OpenCV) ---
$(TARGET_1_1): $(SRCS_1_1)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $(TARGET_1_1) $(SRCS_1_1) $(LDLIBS)

$(TARGET_1_2): $(SRCS_1_2)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $(TARGET_1_2) $(SRCS_1_2) $(LDLIBS)


# --- Rules for Part 2 (Golomb) ---

# This is a generic "pattern rule". It tells 'make' how to
# create any .o file from any .cpp file.
# $< means "the first prerequisite" (the .cpp file)
# $@ means "the target" (the .o file)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@ # <-- NEW: "compile only" rule

# This rule links the final 2.1 executable from the new .o files
$(TARGET_2_1): $(OBJS_2_1)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET_2_1) $(OBJS_2_1) $(LDLIBS) # <-- NEW: "link" rule

$(TARGET_5): $(OBJS_5)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET_5) $(OBJS_5) $(LDLIBS)

# Rule to clean up build files and result images
clean:
	# Updated to clean all targets and the new .o files
	rm -f $(TARGET_1_1) $(TARGET_1_2) $(TARGET_2_1) $(TARGET_5)
	rm -f *.o
	rm -f Result_Images/*