SRCS := \
	prelude/error.go \
	prelude/log.go \
	prelude/position.go \
	prelude/source.go \
	syntax/token.go \
	syntax/lexer.go \

TESTS := \
	driver/dachs/main.go \
	prelude/log_test.go \
	prelude/source_test.go \
	prelude/error_test.go \
	syntax/token_test.go \

all: build test

build: dachs

dachs: $(SRCS)
	go get -t -d ./...
	if which time > /dev/null; then time go build ./driver/dachs; else go build ./driver/dachs; fi

test: $(TESTS)
	go test ./...

clean:
	rm -f dachs

.PHONY: all build clean test
