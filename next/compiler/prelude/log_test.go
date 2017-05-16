package prelude

import (
	"bytes"
	"strings"
	"testing"
)

func TestOutputAllLogs(t *testing.T) {
	var buf bytes.Buffer

	DisableLog(All)
	EnableLog(All)
	SetLogWriter(&buf)

	NowLogging(Parsing)

	Log("parsing", 42)
	Log("this", "is", "test", 3.14)

	NowLogging(Sema)
	Logf("semantics %d", 42)
	Log("this", "is", "test", 1.14)

	got := strings.Split(buf.String(), "\n")
	want := []string{
		"[Parsing] log_test.go:18: parsing 42",
		"[Parsing] log_test.go:19: this is test 3.14",
		"[Sema] log_test.go:22: semantics 42",
		"[Sema] log_test.go:23: this is test 1.14",
		"",
	}

	if len(got) != len(want) {
		t.Fatalf("Length mismatch between want and got. want: %d, got: %d", len(want), len(got))
	}

	for i, expected := range want {
		actual := got[i]
		if expected != actual {
			t.Errorf("Output mismatch at line %d: want: '%s', got: '%s'", i+1, expected, actual)
		}
	}
}

func TestOutputSpecific(t *testing.T) {
	var buf bytes.Buffer

	DisableLog(All)
	EnableLog(Parsing)
	SetLogWriter(&buf)

	NowLogging(Parsing)
	Log("parsing", 42)
	Log("this", "is", "test", 3.14)

	NowLogging(Sema)
	Logf("semantics %d", 42)
	Log("this", "is", "test", 1.14)

	got := strings.Split(buf.String(), "\n")
	want := []string{
		"[Parsing] log_test.go:54: parsing 42",
		"[Parsing] log_test.go:55: this is test 3.14",
		"",
	}

	if len(got) != len(want) {
		t.Fatalf("Length mismatch between want and got. want: %d, got: %d", len(want), len(got))
	}

	for i, expected := range want {
		actual := got[i]
		if expected != actual {
			t.Errorf("Output mismatch at line %d: want: '%s', got: '%s'", i+1, expected, actual)
		}
	}
}

func TestOutputDisableSpecific(t *testing.T) {
	var buf bytes.Buffer

	EnableLog(All)
	DisableLog(Parsing)
	SetLogWriter(&buf)

	NowLogging(Parsing)
	Log("parsing", 42)
	Log("this", "is", "test", 3.14)

	NowLogging(Sema)
	Logf("semantics %d", 42)
	Log("this", "is", "test", 1.14)

	got := strings.Split(buf.String(), "\n")
	want := []string{
		"[Sema] log_test.go:92: semantics 42",
		"[Sema] log_test.go:93: this is test 1.14",
		"",
	}

	if len(got) != len(want) {
		t.Fatalf("Length mismatch between want and got. want: %d, got: %d", len(want), len(got))
	}

	for i, expected := range want {
		actual := got[i]
		if expected != actual {
			t.Errorf("Output mismatch at line %d: want: '%s', got: '%s'", i+1, expected, actual)
		}
	}
}

func TestOutputDisableAll(t *testing.T) {
	var buf bytes.Buffer

	DisableLog(All)
	SetLogWriter(&buf)

	NowLogging(Parsing)
	Log("parsing", 42)
	Log("this", "is", "test", 3.14)

	NowLogging(Sema)
	Logf("semantics %d", 42)
	Log("this", "is", "test", 1.14)

	out := buf.String()
	if out != "" {
		t.Fatalf("Expected empty string but actually '%s'", out)
	}
}

func TestIsLogEnabled(t *testing.T) {
	DisableLog(All)
	EnableLog(Sema)
	NowLogging(Sema)
	if !IsLogEnabled(Sema) {
		t.Fatalf("IsLogEnabled() returns false although log is enabled")
	}
	if IsLogEnabled(Parsing) {
		t.Fatalf("IsLogEnabled() returns true although log is disabled")
	}
}
