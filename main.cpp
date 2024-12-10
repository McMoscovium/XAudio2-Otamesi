//=============================
// 1.�e��C���N���[�h�E�����J�[�ݒ�
//=============================

// XAudio2�֘A
#pragma comment(lib, "xaudio2.lib")
#include<xaudio2.h>

// �}���`���f�B�A�֘A
#pragma comment ( lib, "winmm.lib" )
#include<mmsystem.h>

// ������
#include<string>

#include <stdexcept>

using namespace std;


//=============================
// 2.�I�u�W�F�N�g�E�ϐ��̐錾
//=============================
IXAudio2* pXAudio2;
IXAudio2MasteringVoice* pMasteringVoice;
IXAudio2SourceVoice* pSourceVoice;
//=============================
// 6.Wave�t�@�C���̓ǂݍ���
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
	// ���g�����Ă���̗�����A��U������Ƃ�
	// (����Ȃ��ƁA���Ƃ̒��g�̃T�E���h�o�b�t�@�[������ꍇ�A���������[�N����)
	if (outData)
	{
		free(outData->m_soundBuffer);
	}
	// nullptr�������烊�^�[������
	else
	{
		throw -1;
		return false;
	}


	HMMIO mmioHandle = nullptr;

	// �`�����N���
	MMCKINFO chunkInfo{};

	// RIFF�`�����N�p
	MMCKINFO riffChunkInfo{};


	// WAV�t�@�C�����J��
	mmioHandle = mmioOpen(
		(LPWSTR)wFilePath.data(),
		NULL,
		MMIO_READ
	);

	if (!mmioHandle)
	{
		// Wav�t�@�C�����J���܂���ł���
		throw - 1;
		return false;
	}

	// RIFF�`�����N�ɐN�����邽�߂�fccType��WAVE��ݒ������
	riffChunkInfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');

	// RIFF�`�����N�ɐN������
	if (mmioDescend(
		mmioHandle,		//MMIO�n���h��
		&riffChunkInfo,	//�擾�����`�����N�̏��
		nullptr,		//�e�`�����N
		MMIO_FINDRIFF	//�i������`�����N�̎��
	) != MMSYSERR_NOERROR)
	{
		// ���s
		// Riff�`�����N�ɐN�����s���܂���
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}

	// �N����̃`�����N��"fmt "�Ƃ��Đݒ肷��
	chunkInfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (mmioDescend(
		mmioHandle,
		&chunkInfo,
		&riffChunkInfo,
		MMIO_FINDCHUNK
	) != MMSYSERR_NOERROR)
	{
		// fmt�`�����N���Ȃ��ł�
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}

	// fmt�f�[�^�̓ǂݍ���
	DWORD readSize = mmioRead(
		mmioHandle,						//�n���h��
		(HPSTR)&outData->m_wavFormat,	// �ǂݍ��ݗp�o�b�t�@
		chunkInfo.cksize				//�o�b�t�@�T�C�Y
	);

	if (readSize != chunkInfo.cksize)
	{
		// �ǂݍ��݃T�C�Y����v���Ă��܂���
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}

	// �t�H�[�}�b�g�`�F�b�N
	if (outData->m_wavFormat.wFormatTag != WAVE_FORMAT_PCM)
	{
		// Wave�t�H�[�}�b�g�G���[�ł�
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}

	// fmt�`�����N��ޏo����
	if (mmioAscend(mmioHandle, &chunkInfo, 0) != MMSYSERR_NOERROR)
	{
		// fmt�`�����N�ޏo���s
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}

	// data�`�����N�ɐN��
	chunkInfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if (mmioDescend(mmioHandle, &chunkInfo, &riffChunkInfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
	{
		// data�`�����N�N�����s
		mmioClose(mmioHandle, MMIO_FHOPEN);
		throw - 1;
		return false;
	}
	// �T�C�Y�ۑ�
	outData->m_size = chunkInfo.cksize;

	// data�`�����N�ǂݍ���
	outData->m_soundBuffer = new char[chunkInfo.cksize];
	readSize = mmioRead(mmioHandle, (HPSTR)outData->m_soundBuffer, chunkInfo.cksize);
	if (readSize != chunkInfo.cksize)
	{
		// data�`�����N�ǂݍ��ݎ��s
		mmioClose(mmioHandle, MMIO_FHOPEN);
		delete[] outData->m_soundBuffer;
		throw - 1;
		return false;
	}

	// �t�@�C�������
	mmioClose(mmioHandle, MMIO_FHOPEN);

	return true;
}


//=============================
// 7.�Đ��֐��̍쐬
//=============================

bool PlayWaveSound(const std::wstring& fileName, WaveData* outData, bool loop)
{
	if (!LoadWaveFile(fileName, outData))
	{
		//Wave�t�@�C���ǂݍ��ݎ��s
		throw runtime_error("Failed to load file.");
		return false;
	}

	//=======================
	// SourceVoice�̍쐬
	//=======================
	WAVEFORMATEX waveFormat{};

	// �g�`�t�H�[�}�b�g�̐ݒ�
	memcpy(&waveFormat, &outData->m_wavFormat, sizeof(outData->m_wavFormat));

	// 1�T���v��������̃o�b�t�@�T�C�Y���Z�o
	waveFormat.wBitsPerSample = outData->m_wavFormat.nBlockAlign * 8 / outData->m_wavFormat.nChannels;

	// �\�[�X�{�C�X�̍쐬 �����ł̓t�H�[�}�b�g�̂ݓn���Ă���
	HRESULT result = pXAudio2->CreateSourceVoice(&pSourceVoice, &waveFormat);
	if (FAILED(result))
	{
		// SourceVoice�쐬���s
		return false;
	}

	//================================
	// �g�`�f�[�^(���f�[�^�{��)���\�[�X�{�C�X�ɓn��
	//================================
	XAUDIO2_BUFFER xAudio2Buffer{};
	xAudio2Buffer.pAudioData = (BYTE*)outData->m_soundBuffer;
	xAudio2Buffer.Flags = XAUDIO2_END_OF_STREAM;
	xAudio2Buffer.AudioBytes = outData->m_size;

	// �O�����Z�q��p���āA���[�v���邩�ۂ��̐ݒ������
	xAudio2Buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	pSourceVoice->SubmitSourceBuffer(&xAudio2Buffer);

	// ���ۂɉ���炷
	pSourceVoice->Start();

	return true;
}



int main()
{
	//=============================
	// 3.COM�̏�����
	//=============================
	HRESULT result;
	result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(result))
	{
		throw runtime_error("COM�̏������Ɏ��s" + to_string(result));
		return 0;
	}
	//=============================
	// 4.XAudio2�̏�����
	//=============================
	result = XAudio2Create(&pXAudio2);
	if (FAILED(result))
	{
		throw runtime_error("XAudio2�̏������Ɏ��s" + to_string(result));
		return 0;
	}

	result = pXAudio2->CreateMasteringVoice(&pMasteringVoice);
	if (FAILED(result))
	{
		throw runtime_error("�}�X�^�����O�{�C�X�̏������Ɏ��s" + to_string(result));
		return 0;
	}

	//=============================
	// 8.���ۂɍĐ�
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
	// 5.�Y���O�ɉ��������̂͂��Ƃ�
	//=============================
	pSourceVoice->DestroyVoice();
	pMasteringVoice->DestroyVoice();
	pXAudio2->Release();

	CoUninitialize();

	return 0;
}