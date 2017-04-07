package main

import (
	"flag"
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/driver"
	"os"
	"strings"
)

var (
	help       = flag.Bool("help", false, "Show this help")
	showTokens = flag.Bool("tokens", false, "Lexing code only and show tokens")
	log        = flag.String("log", "", "Logging components (comma-separated). Component: 'Lexing', 'Parsing' or 'All'")
)

const usageHeader = `Usage: dachs [flags] [file]

  Hello, world! This is a compiler for Dachs programming language.
  When a file is given as argument, compiler will target it. Otherwise,
  compiler will attempt to read source code from STDIN to compile.

  Project: https://github.com/rhysd/Dachs/tree/master/next

Flags:`

func usage() {
	fmt.Fprintln(os.Stderr, usageHeader)
	flag.PrintDefaults()
}

func main() {
	flag.Usage = usage
	flag.Parse()

	if *help {
		usage()
		os.Exit(0)
	}

	file := ""
	if flag.NArg() > 0 {
		file = flag.Arg(0)
	}

	d, err := driver.NewDriver(file, strings.Split(*log, ","))
	if err != nil {
		fmt.Fprintln(os.Stderr, err.Error())
		os.Exit(4)
	}

	switch {
	case *showTokens:
		tokens, err := d.Lex()
		for _, tok := range tokens {
			fmt.Println(tok.String())
		}
		if err != nil {
			fmt.Fprintf(os.Stderr, "\n%s\n", err.Error())
			os.Exit(4)
		}
	default:
		panic("unimplemented")
	}

	os.Exit(0)
}