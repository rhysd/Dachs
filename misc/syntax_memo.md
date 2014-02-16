- 引数は immutable な左辺値参照がデフォルト

- 引数の型を書かなければテンプレートになる．テンプレートは明示的に書かせない

```
func plus(lhs, rhs) // テンプレート
func plus(lhs : int, rhs : int) -> int // フリー関数
func plus(lhs, rhs : typeof(lhs)) -> int // require considering
```

- 戻り値が無ければ推論

- コピーして値にするときは val を付ける

```
def int plus(val lhs : int, val rhs : int)
```

- 参照を明示したいとき

```
def int plus(ref lhs : int, ref rhs : int)
```

- 変数はデフォルトで immutable な左辺値参照

```
hoge = 'a'
var hoge = 'a' // こっちじゃないと代入と初期化を区別できないのでは？
```

- 変数宣言で型を宣言する場合は後置

```
hoge : int = 42
```

- デフォルト値で初期化（mutable）

```
hoge : int
```

- mutable にするときは mutable を付ける

```
mutable hoge = 42
```

- = は参照を代入，:= はコピーして参照を代入

```
hoge = huga // immutable
hoge := huga // mutable
```

- tuple をサポート

```
t = (1, 'a', "hoge")
```

- 引数のパターンマッチをサポート // 関数型でどんなパターンマッチができるのかよく調べる．動的にマッチさせるかどうかも．

```
func head(val x:xs : int[])
    return x
```

- キーワード引数で名前呼び出しをサポート．変数名を指定してセットできる

```
func hoge(a, b)

hoge(a: 1, b: 2)
```

- maybe 型をサポート

```
func huga(maybe int a)
    hoge = a // コンパイルエラー
    ensure a
        hoge = a // a の中身を確かめないとアクセスできない
    else
        // a が nothing のとき
```

- __TODO:__ class の定義方法を考える．ジェネリクスの必要性

```
class Tmp(T)
    // どうしよう
```

- 等値性を示す演算子は ==（値比較）と is（参照値比較）を提供する

- 戻り値に名前を付けられるようにしたい（Go）

- モジュールは python のものをベースに，パッケージ側で hide 指定出来るようにする

- 可変長引数は ... で取れるようにする．tuple に詰め込まれる．引数の数はコンパイル時に決定する必要がある．

```
func f(a, b, c...)

f('a', 'b', 'c', 'd') // c は tuple(char, char) 型で値は ('c', 'd')
```

- ラムダ式を最後の引数に取る関数には with 構文が使える

```
func sort(array, predicate)
    ...

sort(array, \(a, b) => a < b)

sort(array) with(a, b)
    return a < b
```
