#!/bin/bash

# --- Configuration ---

# Ensure the paths below are correct for your environment
WAV_DIR=""
BIN_DIR="./bin"
WAV_FILE_IN="sample01Mono.wav"
ENCODED_FILE="temp.enc"
DECODED_FILE="temp_out.wav"

# Check if the input file exists
if [ ! -f "$WAV_FILE_IN" ]; then
    echo "Error: Input file ${WAV_FILE_IN} not found."
    echo "Please place a WAV file named 'input.wav' in the current directory."
    exit 1
fi

# Get the size of the original file for ratio calculation
WAV_SIZE=$(stat -c%s "$WAV_FILE_IN")
# Calculate initial Bits Per Sample (BPS) for 16-bit PCM (assuming 2 channels, but we only encode 1)
# BPS_IN should be 16, but calculating it based on file size/samples is tricky due to WAV header.
# We'll use the file size ratio as the primary metric.

# --- Parameters to Test (Modify these arrays for your final report) ---

# 1. Varying the Kept Fraction (-frac)
FRAC_TESTS=(0.5 0.2 0.1 0.05)
FRAC_TEST_QBITS=16 # Keep QBits constant for this test
FRAC_TEST_BS=1024

# 2. Varying the Quantization Bits (-qbits)
QBITS_TESTS=(32 16 8 4)
QBITS_TEST_FRAC=0.2 # Keep Frac constant for this test
QBITS_TEST_BS=1024

# 3. Varying the Block Size (-bs) - Optional, affects time more than ratio/error
BS_TESTS=(512 1024 2048 4096)
BS_TEST_FRAC=0.2
BS_TEST_QBITS=16

# --- Header for the CSV Output ---
echo "Test_Type,Block_Size,Coeff_Fraction,Quant_Bits,Compression_Ratio,Encoded_Size_B,Time_Enc_s,Time_Dec_s,MSE,MAE,SNR_dB"

# --- Function to Run a Single Test ---
run_test() {
    local TYPE=$1
    local BS=$2
    local FRAC=$3
    local QBITS=$4

    # --- 1. Encoding (Capture Time and Encoded Size) ---
    ENC_CMD="${BIN_DIR}/wav_dct_enc -bs $BS -frac $FRAC -qbits $QBITS $WAV_FILE_IN $ENCODED_FILE"
    
    # Use 'time' to capture execution time (outputting to stderr)
    TIME_OUTPUT=$( { time -p $ENC_CMD; } 2>&1 )
    
    # Extract real time from 'time -p' output
    TIME_ENC=$(echo "$TIME_OUTPUT" | grep real | awk '{print $2}')
    
    # Check if encoding failed
    if [ $? -ne 0 ]; then
        echo "$TYPE,$BS,$FRAC,$QBITS,ERROR,,,,,,"
        return
    fi
    
    ENCODED_SIZE=$(stat -c%s "$ENCODED_FILE")
    
    # Compression Ratio calculation: WAV_SIZE / ENCODED_SIZE using 'bc'
    COMPRESSION_RATIO=$(echo "scale=2; $WAV_SIZE / $ENCODED_SIZE" | bc)

    # --- 2. Decoding (Capture Time) ---
    DEC_CMD="${BIN_DIR}/wav_dct_dec $ENCODED_FILE $DECODED_FILE"
    TIME_OUTPUT=$( { time -p $DEC_CMD; } 2>&1 )
    TIME_DEC=$(echo "$TIME_OUTPUT" | grep real | awk '{print $2}')

    # --- 3. Comparison (Get Distortion Metrics) ---
    # The wav_cmp is critical for MSE, MAE, and SNR
    CMP_CMD="${BIN_DIR}/wav_cmp $WAV_FILE_IN $DECODED_FILE"
    CMP_OUTPUT=$($CMP_CMD | grep -E "MSE|MAE|SNR")
    
    # Parse metrics from wav_cmp output
    MSE=$(echo "$CMP_OUTPUT" | grep "MSE" | awk '{print $NF}')
    MAE=$(echo "$CMP_OUTPUT" | grep "MAE" | awk '{print $NF}')
    SNR=$(echo "$CMP_OUTPUT" | grep "SNR" | awk '{print $NF}' | tr -d 'dB')
    
    # --- 4. Output Results ---
    printf "%s,%s,%s,%s,%.2f,%s,%.3f,%.3f,%s,%s,%s\n" \
           "$TYPE" "$BS" "$FRAC" "$QBITS" "$COMPRESSION_RATIO" \
           "$ENCODED_SIZE" "$TIME_ENC" "$TIME_DEC" "$MSE" "$MAE" "$SNR"
           
    # Clean up temporary files
    rm -f "$ENCODED_FILE" "$DECODED_FILE"
}

# --- Execution Loops ---

# Test Set 1: Fraction Tests (Fraction vs. Error/Ratio)
for FRAC in "${FRAC_TESTS[@]}"; do
    run_test "Frac_Test" "$FRAC_TEST_BS" "$FRAC" "$FRAC_TEST_QBITS"
done

# Test Set 2: Quantization Bits Tests (QBits vs. Error/Ratio)
for QBITS in "${QBITS_TESTS[@]}"; do
    run_test "QBits_Test" "$QBITS_TEST_BS" "$QBITS_TEST_FRAC" "$QBITS"
done

# Test Set 3: Block Size Tests (BS vs. Time)
for BS in "${BS_TESTS[@]}"; do
    run_test "BS_Test" "$BS" "$BS_TEST_FRAC" "$BS_TEST_QBITS"
done
