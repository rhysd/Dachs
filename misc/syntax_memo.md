- 引数は immutable な左辺値参照がデフォルト

- 引数の型を書かなければテンプレートになる．テンプレートは明示的に書かせない

```
func plus(lhs, rhs) // テンプレート
func plus(lhs of int, rhs of int) -> int // フリー関数
func int plus(lhs, rhs of typeof(lhs) ) // require considering
```

- 戻り値が無ければ推論

- コピーして値にするときは val を付ける

```
def int plus(lhs of int val, rhs of int val)
```

- 参照を明示したいとき

```
def int plus(lhs of int ref, rhs of int ref)
```

- 変数はデフォルトで immutable な左辺値参照

```
hoge = 'a'
```

- 変数宣言で型を宣言する場合は後置

```
hoge = 42 of int
```

- デフォルト値で初期化（mutable）

```
hoge of int
```

- mutable にするときは mutable を付ける

```
mutable hoge = 42
```

- = は参照を代入，:= はコピーして参照を代入

```
hoge = huga
hoge := huga
```

- tuple をサポート

```
t = (1, 'a', "hoge")
```

- 引数のパターンマッチをサポート // 関数型でどんなパターンマッチができるのかよく調べる．動的にマッチさせるかどうかも．

```
func head(val int[] x:xs)
    return x
end
```

- キーワード引数で名前呼び出しをサポート．変数名を指定してセットできる

```
func hoge(a, b)
end
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
    end
end
```

- __TODO:__ class の定義方法を考える．テンプレートが必要な気がする．

```
class Tmp(T)
    // どうしよう
end
```

- 等値性を示す演算子は ==（値比較）と is（参照値比較）を提供する

- 戻り値に名前を付けられるようにしたい（Go）

- モジュールは python のものをベースに，パッケージ側で hide 指定出来るようにする

- 可変長引数は ... で取れるようにする．tuple に詰め込まれる．引数の数はコンパイル時に決定する必要がある．

```
func f(a, b, c...)
end

f('a', 'b', 'c', 'd') // c は tuple(char, char) 型で値は ('c', 'd')
```
