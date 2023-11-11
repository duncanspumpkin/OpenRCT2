/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AudioContext.h"

#include "../SDLException.h"
#include "AudioMixer.h"
#include "SDLAudioSource.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <SDL.h>
#include <memory>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/audio/AudioSource.h>
#include <openrct2/common.h>
#include <openrct2/core/String.hpp>

namespace OpenRCT2::Audio
{
    class AudioContext final : public IAudioContext
    {
    private:
        static constexpr size_t STREAM_MIN_SIZE = 2 * 1024 * 1024; // 2 MiB

        std::unique_ptr<AudioMixer> _audioMixer;

    public:
        AudioContext()
        {
            if (SDL_Init(SDL_INIT_AUDIO) < 0)
            {
                SDLException::Throw("SDL_Init(SDL_INIT_AUDIO)");
            }
            _audioMixer = std::make_unique<AudioMixer>();
        }

        ~AudioContext() override
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }

        IAudioMixer* GetMixer() override
        {
            return _audioMixer.get();
        }

        std::vector<std::string> GetOutputDevices() override
        {
            std::vector<std::string> devices;
            int32_t numDevices = SDL_GetNumAudioDevices(SDL_FALSE);
            for (int32_t i = 0; i < numDevices; i++)
            {
                devices.emplace_back(String::ToStd(SDL_GetAudioDeviceName(i, SDL_FALSE)));
            }
            return devices;
        }

        void SetOutputDevice(const std::string& deviceName) override
        {
            const char* szDeviceName = nullptr;
            if (!deviceName.empty())
            {
                szDeviceName = deviceName.c_str();
            }
            _audioMixer->Init(szDeviceName);
        }

        IAudioSource* CreateStreamFromCSS(std::unique_ptr<IStream> stream, uint32_t index) override
        {
            auto* rw = StreamToSDL2(std::move(stream));
            if (rw == nullptr)
            {
                return nullptr;
            }

            try
            {
                auto source = CreateAudioSource(rw, index);

                // Stream will already be in memory, so convert to target format
                auto& targetFormat = _audioMixer->GetFormat();
                source = source->ToMemory(targetFormat);

                return AddSource(std::move(source));
            }
            catch (const std::exception& e)
            {
                LOG_VERBOSE("Unable to create audio source: %s", e.what());
                return nullptr;
            }
        }

        IAudioSource* CreateStreamFromWAV(std::unique_ptr<IStream> stream) override
        {
            auto* rw = StreamToSDL2(std::move(stream));
            if (rw == nullptr)
            {
                return nullptr;
            }

            try
            {
                auto source = CreateAudioSource(rw);

                // Load whole stream into memory if small enough
                auto dataLength = source->GetLength();
                if (dataLength < STREAM_MIN_SIZE)
                {
                    auto& targetFormat = _audioMixer->GetFormat();
                    source = source->ToMemory(targetFormat);
                }

                return AddSource(std::move(source));
            }
            catch (const std::exception& e)
            {
                LOG_VERBOSE("Unable to create audio source: %s", e.what());
                return nullptr;
            }
        }

        void StartTitleMusic() override
        {
        }

        void ToggleAllSounds() override
        {
        }
        void PauseSounds() override
        {
        }
        void UnpauseSounds() override
        {
        }

        void StopAll() override
        {
        }
        void StopCrowdSound() override
        {
        }
        void StopRideMusic() override
        {
        }
        void StopTitleMusic() override
        {
        }
        void StopVehicleSounds() override
        {
        }

    private:
        IAudioSource* AddSource(std::unique_ptr<SDLAudioSource> source)
        {
            return _audioMixer->AddSource(std::move(source));
        }

        static SDL_RWops* StreamToSDL2(std::unique_ptr<IStream> stream)
        {
            auto* rw = SDL_AllocRW();
            if (rw == nullptr)
                return nullptr;
            *rw = {};

            rw->type = SDL_RWOPS_UNKNOWN;
            rw->hidden.unknown.data1 = stream.release();
            rw->seek = [](SDL_RWops* ctx, Sint64 offset, int whence) {
                auto ptr = static_cast<IStream*>(ctx->hidden.unknown.data1);
                ptr->Seek(offset, whence);
                return static_cast<Sint64>(ptr->GetPosition());
            };
            rw->read = [](SDL_RWops* ctx, void* buf, size_t size, size_t maxnum) {
                auto ptr = static_cast<IStream*>(ctx->hidden.unknown.data1);
                return static_cast<size_t>(ptr->TryRead(buf, size * maxnum) / size);
            };
            rw->size = [](SDL_RWops* ctx) {
                auto ptr = static_cast<IStream*>(ctx->hidden.unknown.data1);
                return static_cast<Sint64>(ptr->GetLength());
            };
            rw->close = [](SDL_RWops* ctx) {
                auto* ptr = static_cast<IStream*>(ctx->hidden.unknown.data1);
                delete ptr;
                ctx->hidden.unknown.data1 = nullptr;
                SDL_free(ctx);
                return 0;
            };
            return rw;
        }
    };

    class AudioContextOpenAl final : public IAudioContext
    {
    private:
        static constexpr size_t STREAM_MIN_SIZE = 2 * 1024 * 1024; // 2 MiB

        ALCdevice* _device = nullptr;
        ALCcontext* _context = nullptr;

    public:
        AudioContextOpenAl()
        {
            // Open default device
            _device = alcOpenDevice(nullptr);
            _context = alcCreateContext(_device, nullptr);
            // OpenLoco only ever needs the one context so make this one current for lifetime
            alcMakeContextCurrent(_context);
        }

        ~AudioContextOpenAl() override
        {
            alcCloseDevice(_device);
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(_context);
        }

        IAudioMixer* GetMixer() override
        {
            return nullptr;
        }

        std::vector<std::string> GetOutputDevices() override
        {
            const ALCchar* devices = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);

            std::vector<std::string> devicesVec;
            const char* ptr = devices;
            do
            {
                devicesVec.push_back(std::string(ptr));
                ptr += devicesVec.back().size() + 1;
            } while (*ptr != '\0');

            return devicesVec;
        }

        void SetOutputDevice(const std::string& deviceName) override
        {
            if (_device != nullptr)
            {
                alcCloseDevice(_device);
                alcMakeContextCurrent(nullptr);
                alcDestroyContext(_context);
            }
            _device = alcOpenDevice(deviceName.c_str());
            _context = alcCreateContext(_device, nullptr);
            // OpenLoco only ever needs the one context so make this one current for lifetime
            alcMakeContextCurrent(_context);
        }

        IAudioSource* CreateStreamFromCSS(std::unique_ptr<IStream> stream, uint32_t index) override
        {
            //auto* rw = StreamToSDL2(std::move(stream));
            //if (rw == nullptr)
            //{
            //    return nullptr;
            //}

            //try
            //{
            //    auto source = CreateAudioSource(rw, index);

            //    // Stream will already be in memory, so convert to target format
            //    auto& targetFormat = _audioMixer->GetFormat();
            //    source = source->ToMemory(targetFormat);

            //    return AddSource(std::move(source));
            //}
            //catch (const std::exception& e)
            //{
            //    LOG_VERBOSE("Unable to create audio source: %s", e.what());
            //    return nullptr;
            //}
            return nullptr;
        }

        IAudioSource* CreateStreamFromWAV(std::unique_ptr<IStream> stream) override
        {
            //auto* rw = StreamToSDL2(std::move(stream));
            //if (rw == nullptr)
            //{
            //    return nullptr;
            //}

            //try
            //{
            //    auto source = CreateAudioSource(rw);

            //    // Load whole stream into memory if small enough
            //    auto dataLength = source->GetLength();
            //    if (dataLength < STREAM_MIN_SIZE)
            //    {
            //        auto& targetFormat = _audioMixer->GetFormat();
            //        source = source->ToMemory(targetFormat);
            //    }

            //    return AddSource(std::move(source));
            //}
            //catch (const std::exception& e)
            //{
            //    LOG_VERBOSE("Unable to create audio source: %s", e.what());
            //    return nullptr;
            //}
            return nullptr;
        }

        void StartTitleMusic() override
        {
        }

        void ToggleAllSounds() override
        {
        }
        void PauseSounds() override
        {
        }
        void UnpauseSounds() override
        {
        }

        void StopAll() override
        {
        }
        void StopCrowdSound() override
        {
        }
        void StopRideMusic() override
        {
        }
        void StopTitleMusic() override
        {
        }
        void StopVehicleSounds() override
        {
        }

    private:
        IAudioSource* AddSource(std::unique_ptr<SDLAudioSource> source)
        {
            return nullptr;
            //_audioMixer->AddSource(std::move(source));
        }
    };

    std::unique_ptr<IAudioContext> CreateAudioContext()
    {
        return std::make_unique<AudioContext>();
    }
    std::unique_ptr<IAudioContext> CreateAudioContextOpenAl()
    {
        return std::make_unique<AudioContextOpenAl>();
    }
} // namespace OpenRCT2::Audio
