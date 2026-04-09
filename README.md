# espnow-uart-passthrough

ESP-NOW を使って、2 台の ESP32 系ボード間で UART のテキスト通信を中継するサンプルです。  
UART から受け取った 1 行分の文字列を ESP-NOW フレームに詰めて送信し、受信側ではその内容を UART に改行付きで出力します。  
シリアルモニタ同士を無線でつなぐような用途を、できるだけ小さなオーバーヘッドで実現することを目的にしています。

## 1. このプログラムがどういったものか

このスケッチは、UART で入力されたテキストを 1 行単位で扱う双方向パススルーです。

- 送信側では、UART で `\n` まで受け取った文字列を 1 フレームとして ESP-NOW 送信します
- 受信側では、受け取ったデータを UART にそのまま書き戻し、最後に改行を追加します
- 同じスケッチを 2 台に書き込めば、相互に送受信できます
- `/stat` や `/mac` のようなローカルコマンドも UART 経由で利用できます

通信対象は「1 行のテキスト」を前提にしており、巨大なバイナリ転送ではなく、シンプルなシリアル中継やデバッグ用途向けです。

## 2. 動作確認環境

このプログラムは **XIAO ESP32-C3** で動作確認しています。

- ボード種別: XIAO ESP32-C3
- 通信方式: ESP-NOW
- UART ボーレート: `115200`

`uart_echo_demo.ino` では以下のように UART を設定しています。

- `#define UART_PORT Serial`
- `#define UART_BAUD 115200`
- `#define UART_HAS_PINS 0`

そのため、まずは USB シリアルの `Serial` を使う前提で試すのが簡単です。  
外部 UART を使いたい場合は、`UART_PORT` や `UART_HAS_PINS`、`UART_RX_PIN`、`UART_TX_PIN` を環境に合わせて変更してください。

### `Serial` から `Serial1` へ変更する方法

USB シリアルではなく外部 UART を使いたい場合は、`uart_echo_demo.ino` の UART 設定を変更します。

現在は以下のようになっています。

```cpp
#define UART_PORT Serial
// #define UART_PORT Serial1

#define UART_HAS_PINS 0
// #define UART_HAS_PINS 1
```

`Serial1` を使う場合は、次のように切り替えます。

```cpp
// #define UART_PORT Serial
#define UART_PORT Serial1

// #define UART_HAS_PINS 0
#define UART_HAS_PINS 1
```

このスケッチでは、`UART_HAS_PINS` が `1` のときに以下の初期化が使われます。

```cpp
UART_PORT.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
```

つまり、`Serial1` 利用時は RX/TX ピン定義も合わせて確認してください。

```cpp
#define D7 20
#define D6 21
#define UART_RX_PIN D7
#define UART_TX_PIN D6
```

使い方のポイント:

- `Serial` は主に USB シリアルモニタ用として使います
- `Serial1` は外部機器との UART 通信用として使います
- シリアルモニタでログを見ながら外部 UART も使いたい場合は、ログ用と通信相手用のポートの役割を整理して使う必要があります

注意点:

- 使用するボード定義によって `Serial1` が利用できるピンや挙動が異なることがあります
- XIAO ESP32-C3 で実際に使うピンは、使用しているボードパッケージや配線に合わせて確認してください
- 相手側機器とは GND を共通にしてください

## 3. コマンドの説明

UART から 1 行入力して Enter を送ると、その行が処理されます。  
先頭が `/` の一部コマンドは、無線送信せずローカルで処理されます。

### `/help`

利用できるローカルコマンド一覧を表示します。

出力例:

```text
[INFO] local commands: /stat, /mac, /help
```

### `/stat`

統計情報を表示します。

- `sent`: 自機が送信した行数
- `recv`: 自機が受信した行数
- `bad`: 不正フレームとして破棄した数

出力例:

```text
[STAT] self sent=10, recv=8, bad=0
```

### `/mac`

自機の STA MAC アドレスと、現在設定されている送信先 `peerMac` を表示します。

出力例:

```text
[MAC] self=AA:BB:CC:DD:EE:FF, peer=11:22:33:44:55:66
```

## 4. `peerMac` の設定方法

送信先の MAC アドレスは `uart_echo_demo.ino` の以下で設定します。

```cpp
const uint8_t peerMac[6] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
```

### 特定の 1 台に送る場合

相手ボードの MAC アドレスを 6 バイトで設定します。

```cpp
const uint8_t peerMac[6] = { 0x11,0x22,0x33,0x44,0x55,0x66 };
```

相手の MAC アドレスは、相手側のシリアルモニタで `/mac` を実行すると確認できます。

### ブロードキャストする場合

以下を設定するとブロードキャスト送信になります。

```cpp
const uint8_t peerMac[6] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
```

ブロードキャストの特徴:

- 同じ ESP-NOW チャンネル上の複数端末に送れる可能性があります
- 手軽に疎通確認できます
- 1 対 1 通信を厳密に行いたい場合は、相手の実 MAC アドレスを指定するほうが明確です

このスケッチでは ESP-NOW のチャンネルを `1` に固定しています。  
通信させる機器同士は同じチャンネル設定で使ってください。

## 5. `uart_line_protocol` で実装している内容

`uart_line_protocol` では、UART の 1 行を ESP-NOW で安全に運ぶための簡単なフレーム形式を実装しています。

### フレーム構造

送信データは、ヘッダとペイロードで構成されています。

- `ver`: プロトコルバージョン
- `type`: フレーム種別
- `tx_seq`: 送信シーケンス番号
- `payload_len`: ペイロード長
- `crc16`: CRC-16/CCITT

受信側では、まずこのヘッダを見て妥当性を確認します。

### CRC

誤り検出には **CRC-16/CCITT** を使っています。

- 初期値は `0xFFFF`
- 多項式は `0x1021`
- ヘッダとペイロードを対象に計算します
- 受信時に再計算し、フレーム内の `crc16` と一致しなければ破棄します

このチェックにより、壊れたデータや想定外フォーマットの受信をある程度検出できます。

### 妥当性チェック

`isFrameSane()` では主に以下を確認しています。

- 受信長がヘッダ長以上あるか
- `ver` が対応しているバージョンか
- `type` が想定された種別か
- `len == sizeof(header) + payload_len` を満たすか
- CRC が一致するか

不正なフレームは `bad` カウンタに加算され、UART には出力されません。

## 使い方の流れ

1. 2 台の ESP32 系ボードに同じスケッチを書き込みます
2. 必要に応じて `peerMac` を相手の MAC アドレスに設定します
3. 両方のボードを起動します
4. シリアルモニタから 1 行入力して送信します
5. 相手側の UART に同じ内容が表示されることを確認します

## 補足

- 1 行の最大長は実装上 `220` バイトです
- 改行コードは `\n` を区切りとして扱います
- `\r` は無視します
- 長すぎる行を入力した場合は `[ERR] uart line too long` を表示して破棄します
- 送信失敗時は `[ERR] send failed` を表示します
