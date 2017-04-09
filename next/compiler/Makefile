SRCS := \
	ast/node.go \
	ast/visitor.go \
	ast/printer.go \
	driver/dachs/main.go \
	driver/mediator.go \
	prelude/error.go \
	prelude/log.go \
	prelude/position.go \
	prelude/source.go \
	syntax/token.go \
	syntax/lexer.go \

	# syntax/grammar.go \

TESTS := \
	prelude/log_test.go \
	prelude/source_test.go \
	prelude/error_test.go \
	syntax/token_test.go \
	syntax/lexer_test.go \

all: build test

build: dachs

dachs: $(SRCS)
	go get -t -d ./...
	if which time > /dev/null; then time go build ./driver/dachs; else go build ./driver/dachs; fi

syntax/grammar.go: syntax/grammar.go.y
	go get golang.org/x/tools/cmd/goyacc
	goyacc -o syntax/grammar.go syntax/grammar.go.y

test: $(TESTS)
	go test ./...

clean:
	rm -f dachs

.PHONY: all build clean test
