VECTREC := $(HOME)/retro-tools/vectrec-macos-arm64
export PATH := $(VECTREC):$(PATH)
CMOC    := $(VECTREC)/cmoc
STDLIB  := $(VECTREC)/stdlib
ROM     := build/sphere-link.bin

.PHONY: all run watch clean test-hello test-romdata

all: $(ROM)

test-hello:
	@mkdir -p build
	$(CMOC) --vectrex -I$(STDLIB) -L$(STDLIB) -o build/hello-test.bin $(VECTREC)/examples/hello.c
	@echo "Built build/hello-test.bin"

test-romdata:
	@mkdir -p build
	$(CMOC) --vectrex -I$(STDLIB) -L$(STDLIB) -o build/romdata-test.bin $(VECTREC)/examples/romdata.c
	@echo "Built build/romdata-test.bin"

$(ROM): src/main.c
	@mkdir -p build
	$(CMOC) --vectrex -I$(STDLIB) -L$(STDLIB) -o $@ $<

run: all
	@./run.sh

watch:
	@./watch.sh

clean:
	rm -rf build
