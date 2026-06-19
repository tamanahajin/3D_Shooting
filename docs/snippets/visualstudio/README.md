# Visual Studio 2022 用 Doxygen スニペット

`BOM_Doxygen.snippet` は、Doxygenコメントを短いショートカットで挿入するためのVisual Studio 2022向けスニペットです。

## インポート方法

1. Visual Studio 2022を開く
2. メニューから `ツール` → `コード スニペット マネージャー` を開く
3. `言語` で `Visual C++` を選ぶ
4. `インポート` を押す
5. このファイルを選ぶ

```text
docs/snippets/visualstudio/BOM_Doxygen.snippet
```

インポート先は `My Code Snippets` で問題ありません。

## 使い方

C++のヘッダーやソース上でショートカットを入力し、`Tab` を2回押すと展開されます。

| ショートカット | 用途 |
|---|---|
| `doxfile` | ファイル先頭の `@file` / `@brief` |
| `doxclass` | クラスや構造体の説明 |
| `doxfn` | 引数と戻り値がある関数 |
| `doxvoid` | 戻り値なし関数 |
| `doxbrief` | メンバ変数や短い補足説明 |

## コメントを書く場所

基本はヘッダー側の宣言に書きます。  
実装側には、処理の意図やアルゴリズムが分かりにくい箇所だけ通常コメントを追加します。

例:

```cpp
/*!
@brief 敵の生成位置を検証する
@param[in] candidate 生成候補位置
@return 採用できる位置ならtrue
*/
bool CanSpawnEnemy(const Vec3& candidate) const;
```

実装側では、同じ説明を繰り返さず、処理の理由を補足する程度にします。

```cpp
bool EnemySpawner::CanSpawnEnemy(const Vec3& candidate) const
{
    // 高台や坂の内部に埋まらないよう、地形解決後の位置で判定する。
    const auto resolved = m_spawnResolver->Resolve(candidate);
    return resolved.has_value();
}
```

