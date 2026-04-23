CC        = gcc
CFLAGS    = -Wall -Wextra -I./include -I./test
SRC_DIR   = src
TEST_DIR  = test
BUILD_DIR = build

# 테스트 데이터 레코드 수 (기본값 10,000, 8의 배수)
# 사용법: make testdata N=1000000
N ?=

# ── Encoder (enc_hello): compress + decompress + encoder/decoder 로직 ──────
ENC_SRCS = \
    $(SRC_DIR)/main.c          \
    $(SRC_DIR)/encoder.c       \
    $(SRC_DIR)/fm_record.c     \
    $(SRC_DIR)/addr_compress.c \
    $(SRC_DIR)/addr_decompress.c \
    $(SRC_DIR)/io_compress.c   \
    $(SRC_DIR)/io_decompress.c

# ── Decoder (decoder): decompress 전용, encoder 링크 불필요 ─────────────────
DEC_SRCS = \
    $(SRC_DIR)/main_decoder.c  \
    $(SRC_DIR)/decoder.c       \
    $(SRC_DIR)/fm_record.c     \
    $(SRC_DIR)/addr_decompress.c \
    $(SRC_DIR)/io_decompress.c

ENC_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(ENC_SRCS))
DEC_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(DEC_SRCS))

ENC_TARGET      = $(BUILD_DIR)/enc_hello
DEC_TARGET      = $(BUILD_DIR)/decoder
GEN_TYPE0       = $(BUILD_DIR)/gen_type0
GEN_TYPE1       = $(BUILD_DIR)/gen_type1
GEN_TYPE2       = $(BUILD_DIR)/gen_type2
GEN_TYPE3       = $(BUILD_DIR)/gen_type3
GEN_TYPE4       = $(BUILD_DIR)/gen_type4
GEN_TYPE5       = $(BUILD_DIR)/gen_type5
GEN_TYPE6       = $(BUILD_DIR)/gen_type6
GEN_TYPE7       = $(BUILD_DIR)/gen_type7
GEN_TYPE8       = $(BUILD_DIR)/gen_type8
GEN_TESTDATA    = $(BUILD_DIR)/gen_testdata
EXTRACT_RECORDS = $(BUILD_DIR)/extract_records
DUMP_RECORDS    = $(BUILD_DIR)/dump_records

all: $(ENC_TARGET) $(DEC_TARGET) $(GEN_TESTDATA)

$(ENC_TARGET): $(ENC_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(DEC_TARGET): $(DEC_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# ── extract_records ───────────────────────────────────────────────────────────
extract_records: $(EXTRACT_RECORDS)

$(EXTRACT_RECORDS): $(SRC_DIR)/extract_records.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# ── dump_records ──────────────────────────────────────────────────────────────
dump_records: $(DUMP_RECORDS)

$(DUMP_RECORDS): $(SRC_DIR)/dump_records.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# ── gen_testdata (통합 바이너리) ───────────────────────────────────────────────
GEN_COMMON   = $(TEST_DIR)/gen_common.c

GEN_ALL_SRCS = $(TEST_DIR)/gen_testdata.c $(GEN_COMMON) \
    $(TEST_DIR)/gen_type0.c $(TEST_DIR)/gen_type1.c $(TEST_DIR)/gen_type2.c \
    $(TEST_DIR)/gen_type3.c $(TEST_DIR)/gen_type4.c $(TEST_DIR)/gen_type5.c \
    $(TEST_DIR)/gen_type6.c $(TEST_DIR)/gen_type7.c $(TEST_DIR)/gen_type8.c

gen_testdata: $(GEN_TESTDATA)

$(GEN_TESTDATA): $(GEN_ALL_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DGEN_TESTDATA_COMBINED -o $@ $^

# ── 테스트 데이터 생성 (개별) ─────────────────────────────────────────────────
$(GEN_TYPE0): $(TEST_DIR)/gen_type0.c $(GEN_COMMON)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(GEN_TYPE1): $(TEST_DIR)/gen_type1.c $(GEN_COMMON)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(GEN_TYPE2): $(TEST_DIR)/gen_type2.c $(GEN_COMMON)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(GEN_TYPE3): $(TEST_DIR)/gen_type3.c $(GEN_COMMON)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(GEN_TYPE4): $(TEST_DIR)/gen_type4.c $(GEN_COMMON)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(GEN_TYPE5): $(TEST_DIR)/gen_type5.c $(GEN_COMMON)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(GEN_TYPE6): $(TEST_DIR)/gen_type6.c $(GEN_COMMON)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(GEN_TYPE7): $(TEST_DIR)/gen_type7.c $(GEN_COMMON)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(GEN_TYPE8): $(TEST_DIR)/gen_type8.c $(GEN_COMMON)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

testdata_type0: $(GEN_TYPE0)
	@mkdir -p $(TEST_DIR)/data/type0
	$(GEN_TYPE0) $(N)

testdata_type1: $(GEN_TYPE1)
	@mkdir -p $(TEST_DIR)/data/type1
	$(GEN_TYPE1) $(N)

testdata_type2: $(GEN_TYPE2)
	@mkdir -p $(TEST_DIR)/data/type2
	$(GEN_TYPE2) $(N)

testdata_type3: $(GEN_TYPE3)
	@mkdir -p $(TEST_DIR)/data/type3
	$(GEN_TYPE3) $(N)

testdata_type4: $(GEN_TYPE4)
	@mkdir -p $(TEST_DIR)/data/type4
	$(GEN_TYPE4) $(N)

testdata_type5: $(GEN_TYPE5)
	@mkdir -p $(TEST_DIR)/data/type5
	$(GEN_TYPE5) $(N)

testdata_type6: $(GEN_TYPE6)
	@mkdir -p $(TEST_DIR)/data/type6
	$(GEN_TYPE6) $(N)

testdata_type7: $(GEN_TYPE7)
	@mkdir -p $(TEST_DIR)/data/type7
	$(GEN_TYPE7) $(N)

testdata_type8: $(GEN_TYPE8)
	@mkdir -p $(TEST_DIR)/data/type8
	$(GEN_TYPE8) $(N)

testdata: testdata_type0 testdata_type1 testdata_type2 testdata_type3 testdata_type4 testdata_type5 testdata_type6 testdata_type7 testdata_type8

# ── 테스트 ────────────────────────────────────────────────────────────────────
benchmark: all testdata
	@echo "=== TYPE0 (순차+리텐션) ==="
	DATA_DIR=$(TEST_DIR)/data/type0 bash $(TEST_DIR)/benchmark.sh
	@echo ""
	@echo "=== TYPE1 (랜덤+리텐션) ==="
	DATA_DIR=$(TEST_DIR)/data/type1 bash $(TEST_DIR)/benchmark.sh
	@echo ""
	@echo "=== TYPE2 (고엔트로피 IO) ==="
	DATA_DIR=$(TEST_DIR)/data/type2 bash $(TEST_DIR)/benchmark.sh
	@echo ""
	@echo "=== TYPE3 (뱅크 인터리브) ==="
	DATA_DIR=$(TEST_DIR)/data/type3 bash $(TEST_DIR)/benchmark.sh
	@echo ""
	@echo "=== TYPE4 (컬럼 결함) ==="
	DATA_DIR=$(TEST_DIR)/data/type4 bash $(TEST_DIR)/benchmark.sh
	@echo ""
	@echo "=== TYPE5 (블록 결함) ==="
	DATA_DIR=$(TEST_DIR)/data/type5 bash $(TEST_DIR)/benchmark.sh
	@echo ""
	@echo "=== TYPE6 (역방향 스캔) ==="
	DATA_DIR=$(TEST_DIR)/data/type6 bash $(TEST_DIR)/benchmark.sh
	@echo ""
	@echo "=== TYPE7 (혼합 결함) ==="
	DATA_DIR=$(TEST_DIR)/data/type7 bash $(TEST_DIR)/benchmark.sh
	@echo ""
	@echo "=== TYPE8 (클러스터 결함) ==="
	DATA_DIR=$(TEST_DIR)/data/type8 bash $(TEST_DIR)/benchmark.sh

dump_patterns: dump_records testdata
	DUMP=$(DUMP_RECORDS) DATA_DIR=$(TEST_DIR)/data bash $(TEST_DIR)/dump_patterns.sh

dump_patterns_md: dump_records testdata
	DUMP=$(DUMP_RECORDS) DATA_DIR=$(TEST_DIR)/data bash $(TEST_DIR)/dump_patterns.sh --md > docs/test_data_patterns.md
	@echo "Generated docs/test_data_patterns.md"

roundtrip: all testdata
	@echo "=== TYPE0 ==="
	DATA_DIR=$(TEST_DIR)/data/type0 bash $(TEST_DIR)/roundtrip.sh
	@echo ""
	@echo "=== TYPE1 ==="
	DATA_DIR=$(TEST_DIR)/data/type1 bash $(TEST_DIR)/roundtrip.sh
	@echo ""
	@echo "=== TYPE2 ==="
	DATA_DIR=$(TEST_DIR)/data/type2 bash $(TEST_DIR)/roundtrip.sh
	@echo ""
	@echo "=== TYPE3 ==="
	DATA_DIR=$(TEST_DIR)/data/type3 bash $(TEST_DIR)/roundtrip.sh
	@echo ""
	@echo "=== TYPE4 ==="
	DATA_DIR=$(TEST_DIR)/data/type4 bash $(TEST_DIR)/roundtrip.sh
	@echo ""
	@echo "=== TYPE5 ==="
	DATA_DIR=$(TEST_DIR)/data/type5 bash $(TEST_DIR)/roundtrip.sh
	@echo ""
	@echo "=== TYPE6 ==="
	DATA_DIR=$(TEST_DIR)/data/type6 bash $(TEST_DIR)/roundtrip.sh
	@echo ""
	@echo "=== TYPE7 ==="
	DATA_DIR=$(TEST_DIR)/data/type7 bash $(TEST_DIR)/roundtrip.sh
	@echo ""
	@echo "=== TYPE8 ==="
	DATA_DIR=$(TEST_DIR)/data/type8 bash $(TEST_DIR)/roundtrip.sh

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all gen_testdata extract_records dump_records dump_patterns dump_patterns_md testdata_type0 testdata_type1 testdata_type2 testdata_type3 testdata_type4 testdata_type5 testdata_type6 testdata_type7 testdata_type8 testdata benchmark roundtrip clean
