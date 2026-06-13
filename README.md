# BOM！BOM！BOM！

DirectX 12とC++で開発している、3Dサバイバルシューティングゲームです。

大量に出現する敵を銃と爆弾で倒し、ウェーブが進むほど激しくなる戦場で生存時間を伸ばします。  
特に、爆発で複数の敵をまとめて吹き飛ばす爽快感と、大量の敵を安定して処理するためのパフォーマンス改善に重点を置いています。

## ゲーム概要

- ジャンル: 3Dサバイバルシューティング
- 対応環境: Windows 64-bit
- グラフィックスAPI: DirectX 12
- 開発言語: C++
- 開発環境: Visual Studio 2022
- ベースフレームワーク: [yasyamanoi/BaseCrossDx12](https://github.com/yasyamanoi/BaseCrossDx12)

本作は`BaseCrossDx12`をベースに、ゲームシステム、敵のバッチ処理、インスタンシング描画、ステージ生成、デバッグツールなどを追加・拡張して開発しています。

ステージクリアはなく、プレイヤーが倒れるまでウェーブが継続します。  
通常射撃に加えて、取得した爆弾を投げることで広範囲の敵へダメージと吹き飛ばしを与えられます。

## パフォーマンス比較

大量の敵を処理した際の、最適化前後の動画と30秒間のベンチマーク結果です。  
動画と対応するCSVを同じ項目内にまとめています。

### 最適化前

- [比較動画を見る（MP4・約79MB）](./docs/performance/before.mp4)
- [ベンチマーク結果を見る（CSV）](./docs/performance/before.csv)

### 最適化後

- [比較動画を見る（MP4・約73MB）](./docs/performance/after.mp4)
- [ベンチマーク結果を見る（CSV）](./docs/performance/after.csv)

主な比較対象:

- 敵の個別描画からインスタンシング描画への変更
- 敵の一括生成から複数フレームへの分散生成
- 敵コリジョン用Proxyのプール化
- 敵データの配列管理と更新処理のバッチ化

### 計測結果

| 指標 | 最適化前 | 最適化後 | 変化 |
|---|---:|---:|---:|
| Build | Release | Release | - |
| 計測時間 | 30.013秒 | 30.001秒 | - |
| 平均FPS | 50.179 | 59.865 | +19.3% |
| 最低FPS | 10.000 | 42.827 | +328.3% |
| 遅かったフレーム群の平均FPS | 19.479 | 47.479 | +143.7% |
| 平均フレーム時間 | 19.929ms | 16.704ms | -16.2% |
| 平均敵更新時間 | 0.779ms | 0.661ms | -15.1% |
| 最大敵更新時間 | 1.778ms | 1.383ms | -22.2% |
| 平均衝突時間 | 5.042ms | 4.090ms | -18.9% |
| 最大衝突時間 | 12.468ms | 10.013ms | -19.7% |
| Raycast回数 | 1,506 | 2,961 | +96.6% |
| 最大衝突回数 | 155 | 235 | +51.6% |
| 計測終了時の敵数 | 899 | 597 | -33.6% |
| 最後のWave | 3 | 3 | - |

最適化後は平均FPSだけでなく、最低FPSと遅かったフレーム群の平均FPSが改善し、瞬間的な処理落ちが軽減されています。

ただし、計測終了時の敵数など一部の条件が一致していないため、この結果だけで最適化効果を厳密に比較することはできません。今後は敵数、解像度、カメラ位置、移動経路を固定した追加計測を行います。

## 主な特徴

### 爆発による集団戦

- 爆弾による範囲ダメージ
- 敵の吹き飛ばしとランダム回転
- 爆発時のカメラシェイク
- ヒットストップ、死亡演出、効果音
- 1回の爆発による最大撃破数の記録

### 大量の敵に対応した処理

- 敵の位置、速度、HP、アニメーション状態を配列で管理
- 敵の更新処理をバッチ化
- インスタンシングによる敵の一括描画
- 敵コリジョン用Proxyのプール化
- 敵生成を複数フレームへ分散し、ウェーブ開始時の負荷を平準化
- 軽量な地形解決処理による移動とスポーン位置の補正

デバッグ設定から敵の描画方式を切り替えられるため、個別描画とインスタンシング描画の性能差を比較できます。

### データ駆動のゲーム設定

- 敵ステータスとウェーブ設定をJSONから読み込み
- ステージの高さ、坂、配置物をCSVから生成
- 木や岩などの配置、回転、セル内位置をゲーム内エディタで編集

主なデータファイル:

- [`EnemyWaveConfig.json`](./3D_Shooting/Assets/Data/EnemyWaveConfig.json)
- [`stage_heights.csv`](./3D_Shooting/Assets/Stage/stage_heights.csv)
- [`stage_objects.csv`](./3D_Shooting/Assets/Stage/stage_objects.csv)
- [`stage_props.csv`](./3D_Shooting/Assets/Stage/stage_props.csv)

### ステージと描画

- 高台、坂、外周壁を含む地形
- 地形表面に沿ったプレイヤーと敵の高さ解決
- 高台や坂の内部、配置物、壁、ステージ外を避ける敵スポーン
- 静的オブジェクトのインスタンシング描画
- フォグ、シャドウ、アニメーション
- HP回復、爆弾補充アイテム

## 操作方法

| 操作 | キー・入力 |
|---|---|
| 移動 | `WASD` または方向キー |
| カメラ操作・照準 | マウス移動 |
| 射撃・爆弾投擲 | 左クリック または `J` |
| ジャンプ | `Space` |
| オプションを開く・閉じる | `Esc` |
| メニュー選択 | `W` / `S` または上下キー |
| メニュー決定 | `Enter` / `Space` / `J` |
| ベンチマーク開始・停止 | `F2` |
| ステージエディタを開く | `F3`（Debugビルドのみ） |

爆弾アイテムを取得すると爆弾が装備され、所持数がなくなるまで射撃入力で投擲します。残数が0になると通常射撃へ戻ります。

## ステージエディタ

Debugビルドでは、ゲーム中に`F3`を押すとImGui製のステージエディタを開けます。

- 地形セルの種類を変更
- 高さと坂を編集
- 木や岩などの配置物を選択
- 配置物の回転を調整
- 1セルを3 x 3に分割した9地点から配置位置を選択
- 編集中は視認性確保のためフォグを無効化

`Esc`でエディタを閉じます。編集結果はステージ用CSVへ保存できます。

## ベンチマーク

インゲーム中に`F2`を押すと30秒間の計測を開始します。もう一度`F2`を押すと途中で終了できます。

CSVは実行時の`BenchmarkResults`ディレクトリへ出力されます。リポジトリ内の保存場所は次のとおりです。

- [`3D_Shooting/BenchmarkResults`](./3D_Shooting/BenchmarkResults/)

記録する主な指標:

- 平均FPS、最低FPS、遅かったフレーム群の平均FPS
- 平均フレーム時間
- 敵更新処理の平均・最大時間
- 衝突処理の平均・最大時間
- Raycast回数
- 最大衝突回数
- 計測終了時の敵数とウェーブ

公開用の比較資料は、README上部の「パフォーマンス比較」にまとめています。

## 使用ライブラリ

- DirectX 12
- DirectXTex
- Dear ImGui
- Assimp
- NVIDIA PhysX
- stb

## ディレクトリ構成

```text
3D_Shooting/
├─ 3D_Shooting/                ゲーム本体
│  ├─ Assets/                  モデル、画像、音声、ステージ・設定データ
│  ├─ BenchmarkResults/        ベンチマークCSV
│  ├─ Common/                  描画、衝突、入力などの共通基盤
│  ├─ EnemyBatchController.*   敵データの管理
│  ├─ EnemyBatchMovement.*     敵の一括移動処理
│  ├─ EnemyInstancedRenderer.* 敵のインスタンシング描画
│  ├─ WaveController.*         ウェーブと分散スポーン
│  └─ GameStage.*              インゲーム進行
├─ docs/performance/            最適化前後の比較動画・CSV
├─ UnitTest/                   単体テスト
└─ 3D_Shooting.sln             Visual Studioソリューション
```

## ビルド方法

### 必要環境

- Windows 10またはWindows 11
- Visual Studio 2022
- MSVC v143
- Windows 10 SDK
- x64ビルド環境

Visual Studioで`3D_Shooting.sln`を開き、`Debug | x64`または`Release | x64`を選択してビルドします。

コマンドラインからDebugビルドする場合:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
  3D_Shooting.sln /m /p:Configuration=Debug /p:Platform=x64
```

Releaseビルド:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
  3D_Shooting.sln /m /p:Configuration=Release /p:Platform=x64
```

現状のプロジェクト設定には一部の開発環境依存パスが含まれています。別のPCでビルドする場合は、`.vcxproj`のインクルードディレクトリとライブラリディレクトリを環境に合わせて調整してください。

## テスト

Debugビルド後、次のコマンドで単体テストを実行できます。

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe" `
  ".\x64\Debug\UnitTest.dll"
```

## 今後の改善

- ベンチマーク条件の固定と改善前後データの公開
- GPU・CPUプロファイリング結果の追加
- ステージエディタの操作性向上
- 敵やアイテムの種類追加
- 爆発エフェクトとサウンド演出の強化

## ライセンスと素材

本作はMIT Licenseの[BaseCrossDx12](https://github.com/yasyamanoi/BaseCrossDx12)をベースに開発しています。  
BaseCrossDx12、DirectXTex、PhysX、Assimp、Dear ImGuiなどの第三者ソフトウェアには、それぞれのライセンスが適用されます。著作権表示とライセンス全文は[`THIRD_PARTY_NOTICES.md`](./THIRD_PARTY_NOTICES.md)を参照してください。

第三者ソフトウェアを除く本作独自のソースコード、モデル、画像、音声その他の素材には、現時点で利用ライセンスを設定していません。明示的な許諾がない限り、これらの再利用・再配布はできません。

外部から取得したモデル、画像、音声、フォントなどを使用する場合は、公開・配布前に各素材の利用条件を個別に確認し、必要なクレジットやライセンス文を追加してください。

実行ファイルを配布する場合は、`THIRD_PARTY_NOTICES.md`も配布物へ同梱してください。

## 謝辞

本作の開発には、[yasyamanoi氏のBaseCrossDx12](https://github.com/yasyamanoi/BaseCrossDx12)をベースフレームワークとして使用しています。

## Author

[tamanahajin](https://github.com/tamanahajin)
