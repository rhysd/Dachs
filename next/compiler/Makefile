SRCS := \
	prelude/error.go \
	prelude/log.go \
	prelude/position.go \
	prelude/source.go \

TESTS := \
	prelude/log_test.go \

all: build test

build: dachs

dachs: $(SRCS)
	go get -t -d ./...
	if which time > /dev/null; then time go build ./cmd/dachs; else go build ./cmd/dachs; fi

test: $(TESTS)
	go test ./...

clean:
	rm -f dachs

.PHONY: all build clean test
