//=============================
// 1.各種インクルード・リンカー設定
//=============================

// XAudio2関連
#pragma comment(lib, "xaudio2.lib")
#include<xaudio2.h>

// マルチメディア関連
#pragma comment ( lib, "winmm.lib" )
#include<mmsystem.h>

// 文字列
#include<string>

#include <stdexcept>

using namespace std;


//=============================
// 2.オブジェクト・変数の宣言
//=============================
IXAudio2* pXAudio2;
IXAudio2MasteringVoice* pMasteringVoice;
IXAudio2SourceVoice* pSourceVoice;
//=============================
// 6.Waveファイルの読み込み
//=============================

struct WaveData
{
	WAVEFORMATEX m_wavFormat;
	char* m_soundBuffer;
	DWORD m_size;

	~WaveData() { free(m_soundBuffer); }
};

WaveData waveData;

bool LoadWaveFile(const std::wstring& wFilePath, WaveData* outData)
{
	// 中身入ってるもの来たら、一旦解放しとく
	// (じゃないと、もとの中身のサウンドバッファーがある場合、メモリリークする)
	if (outData)
	{
		free(outData->m_soundBuffer);
	}
	// nullptrが来たらリターンする
	else
	{
		throw -1;
		return false;
	}


	HMMIO mmioHandle = nullptr;

	// チャンク情報
	MMCKINFO chunkInfo{};

	// RIFFチャンク用
	MMCKINFO riffChunkInfo{};


	// WAVファイルを開く
	mmioHandle = mmioOpen(
		(LPWSTR)wFilePath.data(),
		NULL,
		MMIO_READ
	);

	if (!mmioHandle)
	{
		// Wavファイルを開けませんでした
		throw - 1;
		return false;
	}

	// RIFFチャンクに侵入するためにfccTypeにWAVEを設定をする
	riffChunkInfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');

	// RIFFチャンクに侵入する
	if (mmioDescend(
		mmioHandle,		//MMIOハンドル
		&riffChunkInfo,	//取得したチャンクの情報
		nullptr,		//親チャンク
		MMIO_FINDRIFF	//進入するチャンクの種類
	) != MMSYSERR_NOERROR)
	{
		// 失敗
		// Riffチャンクに侵入失敗しました
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}

	// 侵入先のチャンクを"fmt "として設定する
	chunkInfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (mmioDescend(
		mmioHandle,
		&chunkInfo,
		&riffChunkInfo,
		MMIO_FINDCHUNK
	) != MMSYSERR_NOERROR)
	{
		// fmtチャンクがないです
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}

	// fmtデータの読み込み
	DWORD readSize = mmioRead(
		mmioHandle,						//ハンドル
		(HPSTR)&outData->m_wavFormat,	// 読み込み用バッファ
		chunkInfo.cksize				//バッファサイズ
	);

	if (readSize != chunkInfo.cksize)
	{
		// 読み込みサイズが一致していません
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}

	// フォーマットチェック
	if (outData->m_wavFormat.wFormatTag != WAVE_FORMAT_PCM)
	{
		// Waveフォーマットエラーです
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}

	// fmtチャンクを退出する
	if (mmioAscend(mmioHandle, &chunkInfo, 0) != MMSYSERR_NOERROR)
	{
		// fmtチャンク退出失敗
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}

	// dataチャンクに侵入
	chunkInfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if (mmioDescend(mmioHandle, &chunkInfo, &riffChunkInfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
	{
		// dataチャンク侵入失敗
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}
	// サイズ保存
	outData->m_size = chunkInfo.cksize;

	// dataチャンク読み込み
	outData->m_soundBuffer = new char[chunkInfo.cksize];
	readSize = mmioRead(mmioHandle, (HPSTR)outData->m_soundBuffer, chunkInfo.cksize);
	if (readSize != chunkInfo.cksize)
	{
		// dataチャンク読み込み失敗
		mmioClose(mmioHandle, MMIO_FHOPEN);
		delete[] outData->m_soundBuffer;
		throw - 1;
		return false;
	}

	// ファイルを閉じる
	mmioClose(mmioHandle, MMIO_FHOPEN);

	return true;
}


//=============================
// 7.再生関数の作成
//=============================

bool PlayWaveSound(const std::wstring& fileName, WaveData* outData, bool loop)
{
	if (!LoadWaveFile(fileName, outData))
	{
		//Waveファイル読み込み失敗
		throw runtime_error("Failed to load file.");
		return false;
	}

	//=======================
	// SourceVoiceの作成
	//=======================
	WAVEFORMATEX waveFormat{};

	// 波形フォーマットの設定
	memcpy(&waveFormat, &outData->m_wavFormat, sizeof(outData->m_wavFormat));

	// 1サンプル当たりのバッファサイズを算出
	waveFormat.wBitsPerSample = outData->m_wavFormat.nBlockAlign * 8 / outData->m_wavFormat.nChannels;

	// ソースボイスの作成 ここではフォーマットのみ渡っている
	HRESULT result = pXAudio2->CreateSourceVoice(&pSourceVoice, &waveFormat);
	if (FAILED(result))
	{
		// SourceVoice作成失敗
		return false;
	}

	//================================
	// 波形データ(音データ本体)をソースボイスに渡す
	//================================
	XAUDIO2_BUFFER xAudio2Buffer{};
	xAudio2Buffer.pAudioData = (BYTE*)outData->m_soundBuffer;
	xAudio2Buffer.Flags = XAUDIO2_END_OF_STREAM;
	xAudio2Buffer.AudioBytes = outData->m_size;

	// 三項演算子を用いて、ループするか否かの設定をする
	xAudio2Buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	pSourceVoice->SubmitSourceBuffer(&xAudio2Buffer);

	// 実際に音を鳴らす
	pSourceVoice->Start();

	return true;
}



int main()
{
	//=============================
	// 3.COMの初期化
	//=============================
	HRESULT result;
	result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(result))
	{
		throw runtime_error("COMの初期化に失敗" + to_string(result));
		return 0;
	}
	//=============================
	// 4.XAudio2の初期化
	//=============================
	result = XAudio2Create(&pXAudio2);
	if (FAILED(result))
	{
		throw runtime_error("XAudio2の初期化に失敗" + to_string(result));
		return 0;
	}

	result = pXAudio2->CreateMasteringVoice(&pMasteringVoice);
	if (FAILED(result))
	{
		throw runtime_error("マスタリングボイスの初期化に失敗" + to_string(result));
		return 0;
	}

	//=============================
	// 8.実際に再生
	//=============================
	bool isPlay = PlayWaveSound(L"baseballLegend.wav", &waveData, true);

	if (isPlay)
	{
		bool isRunning = true;

		XAUDIO2_VOICE_STATE state;
		pSourceVoice->GetState(&state);

		while ((state.BuffersQueued > 0) != 0)
		{
			pSourceVoice->GetState(&state);
		}
	}


	//=============================
	// 5.忘れる前に解放するものはしとく
	//=============================
	pSourceVoice->DestroyVoice();
	pMasteringVoice->DestroyVoice();
	pXAudio2->Release();

	CoUninitialize();

	return 0;
}