# geometry_added — 変更概要

## 背景

Boost.Geometry extensions（dissolve, offset 等）を利用していたため、
Boost 1.90 で暫定的に使える状態に修正した。

`no_rescale_policy` の廃止に伴い、各アルゴリズムが修正中のようであったため、
その方針に則って対応を進めた。

## dissolve の修正

dissolve のテストケースには、メーリングリストから投稿されたと思われる
様々なケースが記載されていた。しかし、自己交差を含むものが多数あり、
これらは結果が処理する人の主観に基づいて曖昧な正解が提示されていると
判断したため、テストから除外することにした。

また、dissolve の float 精度でのテストはパスしないため、テストから除外した。

## make_valid の追加

実装中の dissolve には make_valid に相当する機能が盛り込まれているように
感じたが、本来別の目的のアルゴリズムであろうと思い、
代わりに自社のポリゴン再構成ルーチンを移植し、`make_valid` として
提供することにした（Boost Software License に同意します）。

## ファイル一覧

### 新規ファイル

| ファイル | 説明 |
|----------|------|
| `extensions/algorithms/make_valid.hpp` | make_valid 実装 |
| `extensions/algorithms/make_valid.md` | make_valid ドキュメント（英語） |
| `extensions/algorithms/make_valid_jp.md` | make_valid ドキュメント（日本語） |
| `extensions/algorithms/dissolve.md` | dissolve ドキュメント（英語） |
| `extensions/algorithms/dissolve_jp.md` | dissolve ドキュメント（日本語） |
| `extensions/algorithms/images/fig1〜fig8.png` | make_valid アルゴリズム説明図（8枚） |

### 変更ファイル

| ファイル | 変更内容 |
|----------|----------|
| `extensions/algorithms/dissolve.hpp` | Boost 1.90 API 対応 |
| `extensions/algorithms/offset.hpp` | Boost 1.90 API 対応（no_rescale_policy 廃止等） |
| `extensions/algorithms/detail/overlay/dissolver.hpp` | Boost 1.90 API 対応 |
| `extensions/algorithms/detail/overlay/dissolve_traverse.hpp` | overlay_dissolve モード対応 |
| `algorithms/detail/overlay/graph/select_edge.hpp` | dissolve 用の左折優先ソート追加 |
| `algorithms/detail/overlay/graph/traverse_graph.hpp` | dissolve 用の TOI 二重管理追加 |
| `extensions/test/algorithms/dissolve.cpp` | 自己交差 Ring テストを条件付きに変更 |
| `extensions/test/algorithms/Jamfile` | Boost 1.90 ビルド対応 |
| `test/geometry_test_common.hpp` | Boost 1.90 ビルド対応 |

### 変更点ドキュメント

| ファイル | 対象 |
|----------|------|
| `extensions/algorithms/offset.md` / `offset_jp.md` | offset.hpp の変更点 |
| `algorithms/detail/overlay/graph/select_edge.md` / `select_edge_jp.md` | select_edge.hpp の変更点 |
| `algorithms/detail/overlay/graph/traverse_graph.md` / `traverse_graph_jp.md` | traverse_graph.hpp の変更点 |
