# xfOpenCV-PostProcess
Implement PostProcess with xfOpenCV

作成者 : 稲益秀成

## 概要

Xilinx UltraScale+ ZCU104ボードのFPGAで動作する回路をxfOpenCVライブラリとXilinx SDxを用いて実装する。

ポストプロセスとして処理されるBloomエフェクト処理を実行する。

## 実行方法

### 1. ビルド済みファイルをダウンロード

[200117_bloom_sd.zip](https://github.com/Rinadehi/xfOpenCV-PostProcess/releases/download/v2020.1.17/200117_bloom_sd.zip) をダウンロードし、展開する。

### 2. SDカードにファイルを配置

microSDカードをFAT32にフォーマットする。

展開したzipファイルの中のファイルをmicroSDカードにコピーする。

### 3. ZCU104でのsdカードブート

ZCU104ボードのDIPスイッチ(SW6)を
```
DIP switch SW6:
Mode0 (#1) OFF
Mode1 (#2) ON
Mode2 (#3) OFF
Mode3 (#4) ON
```

と設定する。SDカードをZCU104に挿入し、電源をオンにする。

### 4. 実行

microUSBでPCとシリアル接続し、ターミナルを開く。ボーレートは115200に設定する。
Linuxが起動するので、
```
root@xilinx-zcu104-2019_1:~# cd /mnt/
root@xilinx-zcu104-2019_1:/mnt# export LD_LIBRARY_PATH=/mnt/lib/
root@xilinx-zcu104-2019_1:/mnt# ./bloom.elf Torus.png 500
```
とすることで実⾏できる。

ちなみに、第２引数に画像ファイル、第３引数に実行回数を指定する。

## 開発環境

### ビルド環境

* CentOS 7.7.1908 Core
* Xilinx SDx v2019.1 (64-bit)
* xfOpenCV 2019.1

### 実行環境

* Xilinx Zynq UltraScale+ MPSoC ZCU104 評価キット 

## ビルド方法

### 0. 準備

[reVISION 入門ガイド 3. ソフトウェア ツールおよびシステム要件](https://github.com/Xilinx/reVISION-Getting-Started-Guide/blob/master/docs-jp/Docs/software-tools-system-requirements.md) 内の[zcu104-rv-ss-2018-3.zip](https://japan.xilinx.com/member/forms/download/design-license-xef.html?filename=zcu104-rv-ss-2018-3.zip)
をダウンロードし、展開する。

### 1. xfOpenCVのインストール

Xilinx SDx v2019.1 (64-bit)を起動する。

適当なworkspaceを指定し、ウインドウが表示されたら、メニューのXilinx -> SDx Libraries... を選択

ここで、xfOpenCVのインストールを行う。

### 2. 新しいプロジェクトの作成

メニューから File -> New -> SDx Application Project を選択し、適当なプロジェクト名をつける。

Platformは**ZCU104**を指定する。

System Configurationでは、
 * System configuration : **A53 Linux**
 * Runtime : **C/C++**
 * Domain : **a53_linux**
 * Sysroot path : `/<ダウンロードしたzcu104-rv-ss-2018-3を展開したディレクトリ>/zcu104_rv_ss/sw/a53_linux/a53_linux/sysroot/aarch64-xilinx-linux`

と設定する。

Templatesでは、**customconv - File I/O**を選択し、Finishをクリック

以上の手順でサンプルプロジェクトを作成できる。

### 3. 本リポジトリからソースファイルをコピー

一度Xilinx SDxを終了する。

作成したプロジェクトの`src`ディレクトリ内のファイルを削除し、

本リポジトリを`git clone`し、`workspace/bloom/src`内のファイルを、作成したプロジェクトの`src`にインポートする。

本リポジトリの`workspace/bloom/project.sdx`を参考に、作成したプロジェクトの`project.sdx`を編集する。

* `location="/eda1/home/inamasu/200116_xfOpenCV-PostProcess/workspace/bloom`となっている部分を自分のプロジェクトのディレクトリに変更する。

### 4. ビルド

再度 Xilinx SDxを起動し、ビルド設定をReleaseに変更し、ビルドを実行する。

ビルドに成功すれば完了。(初回ビルドにはIntel Xeon E5-2620 v4にて1時間程度かかった。)

## 各ファイルの説明

`workspace/bloom/src`内のファイルの内容は、以下の通りである。

|ファイル名|内容|
|:-------:|----|
|data| ビルドした際に`Release/sd_card/`にコピーされるファイル。テスト用の画像やopenCVのライブラリファイルを配置してある。|
|main.cpp | メイン関数|
|xf_arithm_*|xfOpenCVにおけるadd関数を使うためのファイル (xfOpenCVのサンプルまま) |
|xf_config_params.h | xfOpenCVの各設定を行うためのパラメータ|
|xf_custom_convolution_*|xfOpenCVにおけるfilter2D関数を使うためのファイル (xfOpenCVのサンプルまま) |
|xf_headers.h|openCVなどのインクルードがされるファイル(ほぼxfOpenCVのサンプルまま) |
|xf_lut_*|xfOpenCVにおけるLUT関数を使うためのファイル (xfOpenCVのサンプルまま) |

### xf_config_params.hのパラメータについて

```
#define  FILTER_HEIGHT  15
#define  FILTER_WIDTH  	15
```
にて、Filter2Dのフィルタサイズを15に設定している。

```
/*  set the optimization type  */
#define NO  1  	// Normal Operation
#define RO  0  	// Resource Optimized
```

にて、N0を1にするとxfOpenCVの各処理を1ピクセルずつ処理を行う。
R0にすると8ピクセルずつ処理を行う。

しかし、R0に設定するとFilter2Dの結果が真っ黒になってしまう問題が発生したため、N0を指定した。

```
#define GRAY 0
#define RGBA 1
```

グレースケールで処理するか、カラーで処理するかを指定する。今回はカラーで実行したいため`RGBA`を指定

```
#define ARRAY	1
#define SCALAR	0
//macros for accel
#define FUNCT_NAME add // change from mutiply to add by inamasu
//#define EXTRA_ARG  0.05 // comment out by inamasu
#define EXTRA_PARM XF_CONVERT_POLICY_SATURATE

//OpenCV reference macros
#define CV_FUNCT_NAME add // change from mutiply to add
#define CV_EXTRA_ARG  0.05
```
ここで、画像(mat)+画像のadd関数が実行されるよう設定している。



