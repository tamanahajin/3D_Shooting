# BOM！BOM！BOM！

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

## 操作方法

| 操作           | キー・入力                          |
| ------------ | ------------------------------ |
| 移動           | `WASD`                         |
| カメラ操作・照準     | マウス移動                          |
| 射撃・爆弾投擲      | 左クリック                          |
| ジャンプ         | `Space`                        |
| オプションを開く・閉じる | `Esc`                          |
| メニュー選択       | `W` / `S` または上下キー              |
| メニュー決定       | `Enter` / `Space` またはボタンを左クリック |

## 使用ライブラリ

- DirectX 12
- DirectXTex
- Dear ImGui
- Assimp
- stb

## 使用素材・クレジット

一部の素材として以下を使用しています。

- OtoLogic  
  https://otologic.jp
- Kenny  
  https://www.kenney.nl/assets/mini-dungeon  
  https://www.kenney.nl/assets/blaster-kit  
  https://www.kenney.nl/assets/nature-kit

## ライセンス

本作はMIT Licenseの[BaseCrossDx12](https://github.com/yasyamanoi/BaseCrossDx12)をベースに開発しています。  
BaseCrossDx12、DirectXTex、Assimp、Dear ImGuiなどの第三者ソフトウェアには、それぞれのライセンスが適用されます。著作権表示とライセンス全文は[`THIRD_PARTY_NOTICES.md`](./THIRD_PARTY_NOTICES.md)を参照してください。

第三者ソフトウェアを除く本作独自のソースコード、モデル、画像、音声その他の素材には、現時点で利用ライセンスを設定していません。明示的な許諾がない限り、これらの再利用・再配布はできません。

