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
func plus(val lhs : int, val rhs : int) : int
```

- 変数はデフォルトで immutable な左辺値参照

```
hoge = 'a'
```

- 変数宣言で型を宣言する場合は後置

```
hoge : int = 42
```

- デフォルト値で初期化（mutable）

```
hoge : int
```

- mutable にするときは val を付ける

```
val hoge = 42
```

- = は参照を代入，:= はコピーして参照を代入

```
hoge = huga // alias and it's immutable
hoge := huga // copy and it's mutable
```

- tuple をサポート

```
t = (1, 'a', "hoge")
```

- 引数のパターンマッチをサポート // 関数型でどんなパターンマッチができるのかよく調べる．動的にマッチさせるかどうかも．

```
func head(val x:xs : int[])
    return x
end
```

- キーワード引数で名前呼び出しをサポート．変数名を指定してセットできる

```
func hoge(a, b)
end

hoge(a: 1, b: 2)
```

- maybe 指定子をサポート

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

- >>= 演算子 を使えるようにする？

```
arg >>= may_fail1 >>= may_fail2
```

- __TODO:__ class の定義方法を考える．ジェネリクスの必要性

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

f('a', 'b', 'c', 'd') // c は tuple(char, char) 型で値は ('c', 'd')
```

- ラムダ式

```
\a,b do
    return a < b
end

\a, b -> a < b
```

- Ruby と同様に do-end が使える

```
sort(array) do |a, b|
    return a < b
end
```

- 複数のブロックを取りたいときは where で名前渡しする

```
func sort(array, predicate)
    ...
end

sort(array, \a, b -> a < b)

sort(array) where
    predicate do
        return a < b
    end
end

connect(addr) where
    on_success do
        ...
    end
    on_failure do
        ...
    end
end
```

- 事前条件を書けるようにする．事前条件はコンパイル時に評価できるものもできないものも OK．実行時評価のものはリリースビルド時はチェックしない．

```
func do_something(a, b, c)
    # ここに事前条件
begin
    # 関数本体
end
```

- 引数を変更する場合は関数ではなく手続きを使う

```
proc do_procedure(a, b, c) # 戻り値なし
    # 引数を変更する処理
end
```
