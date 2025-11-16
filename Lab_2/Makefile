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

# --- Part 4: Audio Codec ---
# libsndfile flags
SNDFILE_CFLAGS = $(shell pkg-config --cflags sndfile 2>/dev/null || echo "")
SNDFILE_LIBS = $(shell pkg-config --libs sndfile 2>/dev/null || echo "-lsndfile")

# --- Targets ---
TARGET_1_1 = 1.1_extract_channel
TARGET_1_2 = 1.2_image_ops
TARGET_2_1 = 2.1_golomb_test
TARGET_4_1 = audio_encoder  
TARGET_5 = image_codec      

# --- Source Files ---
SRCS_1_1 = extract_channel.cpp
SRCS_1_2 = image_operations.cpp
SRCS_2_1 = main_part2.cpp golomb.cpp bitstream.cpp
SRCS_4_1 = audio_encoder.cpp AudioCodec.cpp golomb.cpp bitstream.cpp  
SRCS_4_2 = benchmark_audio.cpp AudioCodec.cpp golomb.cpp bitstream.cpp  
SRCS_5 = image_codec.cpp golomb.cpp bitstream.cpp

# --- Object Files ---
OBJS_2_1 = $(SRCS_2_1:.cpp=.o)
OBJS_4_1 = $(SRCS_4_1:.cpp=.o)  
OBJS_4_2 = $(SRCS_4_2:.cpp=.o)  

# Default rule: build all targets
# Note: TARGET_4_2 (benchmark) is optional, not included by default
all: $(TARGET_1_1) $(TARGET_1_2) $(TARGET_2_1) $(TARGET_4_1)  

# Convenience targets for specific parts
part1: $(TARGET_1_1) $(TARGET_1_2)
part2: $(TARGET_2_1)
part4: $(TARGET_4_1)  

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
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(SNDFILE_CFLAGS) -c $< -o $@

# This rule links the final 2.1 executable from the new .o files
$(TARGET_2_1): $(OBJS_2_1)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET_2_1) $(OBJS_2_1) $(LDLIBS)

# --- Rules for Part 4 (Audio Codec) ---  <-- NEW SECTION
# Audio encoder/decoder
$(TARGET_4_1): $(OBJS_4_1)
	$(CXX) $(CXXFLAGS) -o $(TARGET_4_1) $(OBJS_4_1) $(SNDFILE_LIBS)

# Audio benchmark (optional)
$(TARGET_4_2): $(OBJS_4_2)
	$(CXX) $(CXXFLAGS) -o $(TARGET_4_2) $(OBJS_4_2) $(SNDFILE_LIBS)

# Special dependencies for Audio Codec files
AudioCodec.o: AudioCodec.cpp AudioCodec.h golomb.h bitstream.h
	$(CXX) $(CXXFLAGS) $(SNDFILE_CFLAGS) -c AudioCodec.cpp -o AudioCodec.o

audio_encoder.o: audio_encoder.cpp AudioCodec.h
	$(CXX) $(CXXFLAGS) -c audio_encoder.cpp -o audio_encoder.o

benchmark_audio.o: benchmark_audio.cpp AudioCodec.h
	$(CXX) $(CXXFLAGS) -c benchmark_audio.cpp -o benchmark_audio.o

# --- Test Targets ---  
# Quick test of audio codec
test-audio: $(TARGET_4_1)
	@echo "Testing audio codec..."
	@if [ ! -f test.wav ]; then \
		echo "Error: test.wav not found"; \
		exit 1; \
	fi
	./$(TARGET_4_1) -c test.wav test.glmb
	./$(TARGET_4_1) -d test.glmb test_decoded.wav
	@echo "Comparing files..."
	@if cmp -s test.wav test_decoded.wav; then \
		echo "✓ Test passed! Files are identical."; \
		rm -f test.glmb test_decoded.wav; \
	else \
		echo "✗ Test failed! Files differ."; \
	fi

# Rule to clean up build files and result images
clean:
	# Updated to clean all targets and the new .o files
	rm -f $(TARGET_1_1) $(TARGET_1_2) $(TARGET_2_1) $(TARGET_4_1) $(TARGET_4_2)
	rm -f *.o
	rm -f *.glmb test_decoded.wav temp_*
	rm -f Result_Images/*

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build Part 1, 2, and 4 (audio encoder)"
	@echo "  part1        - Build only Part 1 (OpenCV exercises)"
	@echo "  part2        - Build only Part 2 (Golomb test)"
	@echo "  part4        - Build only Part 4 (audio encoder)"
	@echo "  part4-full   - Build Part 4 with benchmark"
	@echo "  test-audio   - Test audio codec (requires test.wav)"
	@echo "  clean        - Remove all build files"
	@echo ""
	@echo "Audio codec usage:"
	@echo "  ./audio_encoder -c input.wav output.glmb [options]"
	@echo "  ./audio_encoder -d input.glmb output.wav"

.PHONY: all part1 part2 part4 part4-full test-audio clean help