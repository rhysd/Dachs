package prelude

type Position struct {
	Offset int
	Line   int
	Column int
	File   *Source
}
