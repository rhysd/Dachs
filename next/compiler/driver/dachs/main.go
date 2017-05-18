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
	log        = flag.String("log", "", "Logging components (comma-separated). Component: 'Parsing', 'Sema' or 'All'")
	showTokens = flag.Bool("tokens", false, "Lexing code only and show tokens")
	showAST    = flag.Bool("ast", false, "Parse code only and show AST as output")
	analyze    = flag.Bool("analyze", false, "Analyze source and report the result")
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

	logs := []string{}
	if len(*log) > 0 {
		logs = strings.Split(*log, ",")
	}

	d, err := driver.NewDriver(file, logs)
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
	case *showAST:
		if err := d.FprintAST(os.Stdout); err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(4)
		}
	case *analyze:
		if err := d.Analyze(); err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(4)
		}
	default:
		fmt.Fprintln(os.Stderr, "Default action is not implemented yet. Please try implemented flags. -help is available to see all flags.")
		panic("unimplemented")
	}

	os.Exit(0)
}
