/*
 *      Copyright (C) 2012-2017 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "ServiceBroker.h"
#include "RetroPlayerAudio.h"
#include "cores/AudioEngine/Interfaces/AE.h"
#include "cores/AudioEngine/Interfaces/AEStream.h"
#include "cores/AudioEngine/Utils/AEChannelInfo.h"
#include "cores/AudioEngine/Utils/AEUtil.h"
#include "cores/RetroPlayer/process/RPProcessInfo.h"
#include "threads/Thread.h"
#include "utils/log.h"

using namespace KODI;
using namespace RETRO;

CRetroPlayerAudio::CRetroPlayerAudio(CRPProcessInfo& processInfo) :
  m_processInfo(processInfo),
  m_pAudioStream(nullptr),
  m_bAudioEnabled(true)
{
  CLog::Log(LOGDEBUG, "RetroPlayer[AUDIO]: Initializing audio");
}

CRetroPlayerAudio::~CRetroPlayerAudio()
{
  CLog::Log(LOGDEBUG, "RetroPlayer[AUDIO]: Deinitializing audio");

  CloseStream();
}

unsigned int CRetroPlayerAudio::NormalizeSamplerate(unsigned int samplerate) const
{
  //! @todo List comes from AESinkALSA.cpp many moons ago
  static unsigned int sampleRateList[] = { 5512, 8000, 11025, 16000, 22050, 32000, 44100, 48000, 0 };

  for (unsigned int *rate = sampleRateList; ; rate++)
  {
    const unsigned int thisValue = *rate;
    const unsigned int nextValue = *(rate + 1);

    if (nextValue == 0)
    {
      // Reached the end of our list
      return thisValue;
    }

    if (samplerate < (thisValue + nextValue) / 2)
    {
      // samplerate is between this rate and the next, so use this rate
      return thisValue;
    }
  }

  return samplerate; // Shouldn't happen
}

bool CRetroPlayerAudio::OpenPCMStream(AEDataFormat format, unsigned int samplerate, const CAEChannelInfo& channelLayout)
{
  if (m_pAudioStream != nullptr)
    CloseStream();

  CLog::Log(LOGINFO, "RetroPlayer[AUDIO]: Creating audio stream, sample rate = %d", samplerate);

  // Resampling is not supported
  if (NormalizeSamplerate(samplerate) != samplerate)
  {
    CLog::Log(LOGERROR, "RetroPlayer[AUDIO]: Resampling to %d not supported", NormalizeSamplerate(samplerate));
    return false;
  }

  AEAudioFormat audioFormat;
  audioFormat.m_dataFormat = format;
  audioFormat.m_sampleRate = samplerate;
  audioFormat.m_channelLayout = channelLayout;
  m_pAudioStream = CServiceBroker::GetActiveAE()->MakeStream(audioFormat);

  if (!m_pAudioStream)
  {
    CLog::Log(LOGERROR, "RetroPlayer[AUDIO]: Failed to create audio stream");
    return false;
  }

  m_processInfo.SetAudioChannels(audioFormat.m_channelLayout);
  m_processInfo.SetAudioSampleRate(audioFormat.m_sampleRate);
  m_processInfo.SetAudioBitsPerSample(CAEUtil::DataFormatToUsedBits(audioFormat.m_dataFormat));

  return true;
}

bool CRetroPlayerAudio::OpenEncodedStream(AVCodecID codec, unsigned int samplerate, const CAEChannelInfo& channelLayout)
{
  CLog::Log(LOGERROR, "RetroPlayer[AUDIO]: Encoded audio stream not supported");

  return true; //! @todo
}

void CRetroPlayerAudio::AddData(const uint8_t* data, size_t size)
{
  if (m_bAudioEnabled)
  {
    if (m_pAudioStream)
    {
      const size_t frameSize = m_pAudioStream->GetChannelCount() * (CAEUtil::DataFormatToBits(m_pAudioStream->GetDataFormat()) >> 3);
      m_pAudioStream->AddData(&data, 0, static_cast<unsigned int>(size / frameSize));
    }
  }
}

void CRetroPlayerAudio::CloseStream()
{
  if (m_pAudioStream)
  {
    CLog::Log(LOGDEBUG, "RetroPlayer[AUDIO]: Closing audio stream");

    CServiceBroker::GetActiveAE()->FreeStream(m_pAudioStream);
    m_pAudioStream = nullptr;
  }
}
