# BOM！BOM！BOM！

DirectX 12とC++で開発している、3Dサバイバルシューティングゲームです。

大量に出現する敵を銃と爆弾で倒し、ウェーブが進むほど激しくなる戦場で生存時間を伸ばします。  
爆発で複数の敵をまとめて吹き飛ばす爽快感と、大量の敵を安定して処理するためのパフォーマンス改善に重点を置いています。

## アピールポイント

- DirectX 12 / C++環境で、3Dゲームのゲームループ、描画、衝突、UI、音、データ読み込みを組み合わせて実装
- 敵を大量に出すゲーム性に合わせ、敵の状態管理、描画、生成、コリジョンを段階的に最適化
- 最適化前後の動画と30秒ベンチマークCSVを残し、改善結果を数値で比較
- ステージをCSV、敵とWaveをJSONへ寄せ、他の人が調整しやすいよう意識した作り
- ImGui製ステージエディタ、ベンチマーク計測、Debug表示など、開発用ツールも実装

## 紹介動画

https://github.com/user-attachments/assets/c555f9e1-e7af-41cf-a216-cf6c2c06bd4a

## ゲーム概要

- ジャンル: 3Dサバイバルシューティング
- 対応環境: Windows 64-bit
- グラフィックスAPI: DirectX 12
- 開発言語: C++
- 開発環境: Visual Studio 2022
- ベースフレームワーク: [yasyamanoi/BaseCrossDx12](https://github.com/yasyamanoi/BaseCrossDx12)

本作は`BaseCrossDx12`をベースに、ゲームシステム、敵の配列管理、インスタンシング描画、ステージ生成、デバッグツールなどを追加・拡張して開発しています。

プレイヤーが倒れるまでウェーブが継続します。
通常射撃に加えて、取得した爆弾を投げることで広範囲の敵へダメージと吹き飛ばしを与えられます。

## 主な実装・改善範囲

BaseCrossDx12を土台として使用し、主に以下の機能を追加・改修しています。

| 分野     | 内容                                                      |
| ------ | ------------------------------------------------------- |
| 敵処理    | `EnemyController`による敵状態の配列管理、移動・戦闘・アニメーションの分離           |
| 描画最適化  | 敵のインスタンシング描画、比較用の個別描画切り替え                               |
| 生成処理   | `EnemySpawner`による分散スポーン、敵スロットと`EnemyCollisionProxy`の再利用 |
| スポーン判定 | 高台、坂、壁、配置物と被らない敵・アイテムの生成位置解決                            |
| 地形処理   | CSVから高台、坂、外周壁、配置物を生成し、軽量な地形高さ解決を実装                      |
| データ駆動  | 敵ステータスとWave設定をJSON化、ステージ高さ・配置物をCSV化                     |
| 開発ツール  | ImGuiステージエディタ、ベンチマーク計測、FPS・elapsedTime表示切り替え            |
| 演出     | 爆発吹き飛ばし、ランダム回転、カメラシェイク、ヒットストップ、ゲームオーバー演出                |

## パフォーマンス比較

大量の敵を処理した際の、最適化前後の動画と30秒間のベンチマーク結果です。  
Releaseビルドで計測し、動画と対応するCSVを同じ項目内にまとめています。

| 比較対象 | 動画                                                  | CSV                                         |
| ---- | --------------------------------------------------- | ------------------------------------------- |
| 最適化前 | [before.mp4（約100MB）](./docs/performance/before.mp4) | [before.csv](./docs/performance/before.csv) |
| 最適化後 | [after.mp4（約92MB）](./docs/performance/after.mp4)    | [after.csv](./docs/performance/after.csv)   |

主な比較対象:

- 敵の個別描画からインスタンシング描画への変更
- 敵の一括生成から複数フレームへの分散生成

### 計測条件

| 条件       | 最適化前    | 最適化後    |
| -------- | -------:| -------:|
| Build    | Release | Release |
| FPS上限    | なし      | なし      |
| 計測時間     | 30.011秒 | 30.007秒 |
| 計測終了時の敵数 | 277体    | 284体    |
| 最後のWave  | 2       | 2       |

敵数の差は7体で、同じWaveまでをほぼ同じ負荷条件で計測しています。

### 主要な結果

| 指標              | 最適化前     | 最適化後        | 改善率         |
| --------------- | --------:| -----------:| -----------:|
| 平均FPS           | 93.065   | **145.767** | **+56.6%**  |
| 最低FPS           | 10.000   | **60.458**  | **+504.6%** |
| 遅かったフレーム群の平均FPS | 41.282   | **81.778**  | **+98.1%**  |
| 平均フレーム時間        | 10.745ms | **6.860ms** | **-36.2%**  |

平均FPSは約1.57倍になりました。特に、遅かったフレーム群の平均FPSが41.282から81.778へ改善しており、平均値だけでなく処理落ちが発生した場面の安定性も向上しています。

### GPU処理の内訳

| 指標                | 最適化前    | 最適化後        | 変化率        |
| ----------------- | -------:| -----------:| ----------:|
| 平均GPUフレーム時間       | 1.806ms | **1.420ms** | **-21.4%** |
| 最大GPUフレーム時間       | 4.751ms | **3.870ms** | **-18.5%** |
| 遅かったGPUフレーム群の平均時間 | 3.494ms | **3.081ms** | **-11.8%** |
| GPU計測フレーム数        | 2,791   | 4,372       | +56.6%     |

GPUフレーム時間も短縮されており、敵のインスタンシング描画など描画負荷の削減が数値にも出ています。GPU計測フレーム数はFPS向上により最適化後の方が多くなっています。

### CPU処理の内訳

| 指標      | 最適化前    | 最適化後        | 変化率        |
| ------- | -------:| -----------:| ----------:|
| 平均敵更新時間 | 0.299ms | 0.320ms     | +7.0%      |
| 最大敵更新時間 | 0.864ms | 1.138ms     | +31.7%     |
| 平均衝突時間  | 1.939ms | **1.854ms** | **-4.4%**  |
| 最大衝突時間  | 8.145ms | **6.635ms** | **-18.5%** |

敵更新時間はやや増えていますが、衝突処理時間は短縮されています。敵数と処理フレーム数が増えた状態でも平均FPSとGPU時間が改善しているため、描画負荷削減の効果が大きい結果になっています。

## 技術的な見どころ

| テーマ      | 代表ファイル                                                                                       | 見どころ                                 |
| -------- | -------------------------------------------------------------------------------------------- | ------------------------------------ |
| 敵の配列管理   | [`EnemyController.h`](./3D_Shooting/Scripts/Enemy/EnemyController.h)                         | 敵をGameObject大量生成ではなく、状態配列として管理       |
| 敵移動と地形追従 | [`EnemyMovement.cpp`](./3D_Shooting/Scripts/Enemy/EnemyMovement.cpp)                         | 追跡、分離力、重力、ノックバック、地形高さ解決を一括更新         |
| 敵描画      | [`EnemyRenderers.cpp`](./3D_Shooting/Scripts/Enemy/EnemyRenderers.cpp)                       | インスタンシング描画                           |
| 敵生成      | [`EnemySpawner.cpp`](./3D_Shooting/Scripts/Enemy/EnemySpawner.cpp)                           | 敵生成を数フレームへ分散し、生成時のFPS低下を抑制           |
| 地形高さ解決   | [`StageGroundResolver.cpp`](./3D_Shooting/Scripts/Stage/StageGroundResolver.cpp)             | プレイヤーと敵が高台・坂へ安定して接地するための共通処理         |
| ステージ生成   | [`GameStageTerrain.cpp`](./3D_Shooting/Scripts/Stage/GameStageTerrain.cpp)                   | CSVから高台・坂を生成し、軽量な地形判定用データを登録         |
| ステージ配置物  | [`GameStagePlacementObjects.cpp`](./3D_Shooting/Scripts/Stage/GameStagePlacementObjects.cpp) | ランダム自然物とエディタ配置物をインスタンシング描画へ統合        |
| ステージエディタ | [`StageEditor.cpp`](./3D_Shooting/Scripts/Stage/StageEditor.cpp)                             | ImGuiで地形、配置物、回転、セル内位置を編集             |
| ベンチマーク   | [`BenchmarkRecorder.cpp`](./3D_Shooting/Common/Library/BasicLib/BenchmarkRecorder.cpp)       | 30秒計測、FPS、敵更新時間、衝突時間、Raycast回数をCSV出力 |

## 操作方法

基本操作は左クリック、WASD、Space、Escに絞っています。Debugビルドでは、検証用のキー入力を追加で使えます。

| 操作           | キー・入力                          |
| ------------ | ------------------------------ |
| 移動           | `WASD`                         |
| カメラ操作・照準     | マウス移動                          |
| 射撃・爆弾投擲      | 左クリック                          |
| ジャンプ         | `Space`                        |
| オプションを開く・閉じる | `Esc`                          |
| メニュー選択       | `W` / `S` または上下キー              |
| メニュー決定       | `Enter` / `Space` またはボタンを左クリック |
| ベンチマーク開始・停止  | `F2`                           |
| ステージエディタを開く  | `F3`（Debugビルドのみ）               |

爆弾アイテムを取得すると爆弾が装備され、所持数がなくなるまで射撃入力で投擲します。残数が0になると通常射撃へ戻ります。

## ステージエディタ

Debugビルドでは、ゲーム中に`F3`を押すとImGui製のステージエディタを開けます。

- 地形セルの種類を変更
- 高さと坂を編集
- 木や岩などの配置物を選択
- 配置物の回転を調整
- 1セルを3 x 3に分割した9地点から配置位置を選択

`Esc`でエディタを閉じます。編集結果はステージ用CSVへ保存できます。

<img width="1274" height="799" alt="スクリーンショット 2026-06-13 161312" src="https://github.com/user-attachments/assets/57873c6a-e2a9-425e-89fc-a5527c9c1198" />

## ベンチマーク

インゲーム中に`F2`を押すと30秒間の計測を開始します。もう一度`F2`を押すと途中で終了できます。

CSVは実行ファイルと同じ階層の`BenchmarkResults`ディレクトリへ出力されます。リポジトリ内で実行した場合の保存場所は次のとおりです。

- Debug: `Debug/BenchmarkResults`
- Release: `Release/BenchmarkResults`

記録する主な指標:

- 平均FPS、最低FPS、遅かったフレーム群の平均FPS
- 平均フレーム時間
- 平均GPUフレーム時間、最大GPUフレーム時間、遅かったGPUフレーム群の平均時間
- 敵更新処理の平均・最大時間
- 衝突処理の平均・最大時間
- Raycast回数
- 最大衝突回数
- 計測終了時の敵数とウェーブ

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
├─ 3D_Shooting/                         ゲーム本体
│  ├─ Assets/                           モデル、画像、音声、ステージ・設定データ
│  ├─ Common/                           描画、衝突、入力などの共通基盤
│  └─ Scripts/
│     ├─ Enemy/                         敵管理、移動、描画、生成、Wave設定
│     │  ├─ EnemyController.*           敵状態の配列管理
│     │  ├─ EnemyMovement.*             敵の移動、重力、地形追従
│     │  ├─ EnemyRenderers.*            敵のインスタンシング描画と比較用個別描画
│     │  ├─ EnemySpawner.*              敵生成と分散スポーン
│     │  └─ EnemyCollisionProxy.*       衝突用Proxyとプール
│     ├─ Stage/                         ステージ生成、地形解決、ステージエディタ
│     ├─ Player/                        プレイヤー、照準、武器、射撃
│     ├─ Item/                          アイテム生成、補充、取得処理
│     └─ Scene/                         タイトル、インゲーム、リザルト、UI
├─ docs/performance/                    最適化前後の比較動画・CSV
├─ docs/snippets/                       Doxygenコメント用スニペット
├─ media/Shaders/                       コンパイル済みシェーダ
├─ UnitTest/                            単体テスト
└─ 3D_Shooting.sln                      Visual Studioソリューション
```

## 使用素材・クレジット

一部の効果音素材として以下を使用しています。

- OtoLogic  
  https://otologic.jp
- Kenny
  https://www.kenney.nl/assets/mini-dungeon
  https://www.kenney.nl/assets/blaster-kit
  https://www.kenney.nl/assets/nature-kit

## ライセンス

本作はMIT Licenseの[BaseCrossDx12](https://github.com/yasyamanoi/BaseCrossDx12)をベースに開発しています。  
BaseCrossDx12、DirectXTex、PhysX、Assimp、Dear ImGuiなどの第三者ソフトウェアには、それぞれのライセンスが適用されます。著作権表示とライセンス全文は[`THIRD_PARTY_NOTICES.md`](./THIRD_PARTY_NOTICES.md)を参照してください。

第三者ソフトウェアを除く本作独自のソースコード、モデル、画像、音声その他の素材には、現時点で利用ライセンスを設定していません。明示的な許諾がない限り、これらの再利用・再配布はできません。

