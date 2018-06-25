// Copyright 2009-2018 Jan Niklas Hasse <jhasse@bixense.com>
// For conditions of distribution and use, see copyright notice in LICENSE.txt

#include "jngl.hpp"
#include "audio.hpp"
#include "main.hpp"

#ifdef WEAK_LINKING_OPENAL
 #include "win32/openal.hpp"
#else
 #ifdef __APPLE__
  #include <OpenAL/al.h>
  #include <OpenAL/alc.h>
 #else
  #include <AL/al.h>
  #include <AL/alc.h>
 #endif
 #define OV_EXCLUDE_STATIC_CALLBACKS
 #include <vorbis/vorbisfile.h>
#endif

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <unordered_map>

namespace jngl {

	struct Sound::Params {
		ALenum format;
		ALsizei freq;
	};

	struct Sound::Impl {
		ALuint buffer;
		ALuint source = 0;
	};

	float Sound::masterVolume = 1.0f;

	void checkAlError() {
		switch (alGetError()) {
		case AL_NO_ERROR: break;
		case AL_INVALID_NAME:
			debugLn("Invalid name paramater passed to AL call.");
			break;
		case AL_INVALID_ENUM:
			debugLn("Invalid enum parameter passed to AL call.");
			break;
		case AL_INVALID_VALUE:
			debugLn("Invalid value parameter passed to AL call.");
			break;
		case AL_INVALID_OPERATION:
			debugLn("Illegal AL call.");
			break;
		case AL_OUT_OF_MEMORY:
			debugLn("Not enough memory.");
			break;
		default:
			debugLn("Unknown OpenAL error.");
		}
	}

	Sound::Sound(const Params& params, std::vector<char>& bufferData)
	: impl(std::make_unique<Impl>()) {
		alGenBuffers(1, &impl->buffer);
		checkAlError();
		alGenSources(1, &impl->source);
		checkAlError();
		alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
		checkAlError();
		alSource3f(impl->source, AL_POSITION, 0.0f, 0.0f, 0.0f);
		checkAlError();
		alBufferData(impl->buffer, params.format, &bufferData[0],
		             static_cast<ALsizei>(bufferData.size()), params.freq);
		checkAlError();
		alSourcei(impl->source, AL_BUFFER, impl->buffer);
		checkAlError();
		alSourcePlay(impl->source);
		checkAlError();
		setVolume(masterVolume);
	}
	Sound::~Sound() {
		debug("freeing sound buffer ... ");
		alSourceStop(impl->source);
		checkAlError();
		ALint processedBuffers = 0;
		alGetSourcei(impl->source, AL_BUFFERS_PROCESSED, &processedBuffers);
		checkAlError();
		if (processedBuffers == 1) {
			alSourceUnqueueBuffers(impl->source, processedBuffers, &impl->buffer);
			checkAlError();
		}
		alDeleteSources(1, &impl->source);
		checkAlError();
		alDeleteBuffers(1, &impl->buffer);
		checkAlError();
		debug("OK\n");
	}
	bool Sound::IsPlaying() {
		ALint state;
		alGetSourcei(impl->source, AL_SOURCE_STATE, &state);
		return state == AL_PLAYING;
	}
	bool Sound::Stopped() {
		ALint state;
		alGetSourcei(impl->source, AL_SOURCE_STATE, &state);
		return state == AL_STOPPED;
	}
	void Sound::SetPitch(float p) {
		alSourcef(impl->source, AL_PITCH, p);
	}
	void Sound::setVolume(float v) {
		alSourcef(impl->source, AL_GAIN, v);
	}

	std::unordered_map<std::string, std::shared_ptr<SoundFile>> sounds;

	class Audio {
	public:
		Audio() {
			device_ = alcOpenDevice(nullptr);
			if (device_ == nullptr) {
				throw std::runtime_error("Could not open audio device.");
			}
			context_ = alcCreateContext(device_, nullptr);
			if (context_ == nullptr) {
				throw std::runtime_error("Could not create audio context.");
			}
			alcMakeContextCurrent(context_);
		}
		Audio(const Audio&) = delete;
		Audio& operator=(const Audio&) = delete;
		~Audio() {
			sounds_.clear();
			sounds.clear();
			alcMakeContextCurrent(nullptr);
			alcDestroyContext(context_);
			alcCloseDevice(device_);
		}
		static bool IsStopped(std::shared_ptr<Sound>& s) {
			return s->Stopped();
		}
		void Play(std::shared_ptr<Sound>& sound) {
			sounds_.erase(std::remove_if(sounds_.begin(), sounds_.end(), IsStopped), sounds_.end());
			sounds_.push_back(sound);
		}
		void Stop(std::shared_ptr<Sound>& sound) {
			std::vector<std::shared_ptr<Sound>>::iterator i;
			if ((i = std::find(sounds_.begin(), sounds_.end(), sound)) != sounds_.end()) {
				sounds_.erase(i);
			}
		}
	private:
		std::vector<std::shared_ptr<Sound>> sounds_;
		ALCdevice* device_ = nullptr;
		ALCcontext* context_ = nullptr;
	};

	Audio& GetAudio();

	SoundFile::SoundFile(const std::string& filename) : params(std::make_unique<Sound::Params>()) {
		debug("Decoding "); debug(filename); debug(" ... ");
#ifdef _WIN32
		FILE* const f = fopen(filename.c_str(), "rb");
#else
		FILE* const f = fopen(filename.c_str(), "rbe");
#endif
		if (f == nullptr) {
			throw std::runtime_error("File not found (" + filename + ").");
		}

		OggVorbis_File oggFile;
		if (ov_open(f, &oggFile, nullptr, 0) != 0) {
			throw std::runtime_error("Could not open OGG file (" + filename + ").");
		}

		vorbis_info* pInfo;
		pInfo = ov_info(&oggFile, -1);
		if (pInfo->channels == 1) {
			params->format = AL_FORMAT_MONO16;
		} else {
			params->format = AL_FORMAT_STEREO16;
		}
		params->freq = static_cast<ALsizei>(pInfo->rate);

		const int bufferSize = 32768;
		char array[bufferSize]; // 32 KB buffers
		const int endian = 0; // 0 for Little-Endian, 1 for Big-Endian
		int bitStream;
		long bytes; // NOLINT
		do {
			bytes = ov_read(&oggFile, array, bufferSize, endian, 2, 1, &bitStream);

			if (bytes < 0) {
				ov_clear(&oggFile);
				throw std::runtime_error("Error decoding OGG file (" + filename + ").");
			}

			buffer_.insert(buffer_.end(), array, array + bytes);
		} while(bytes > 0);

		ov_clear(&oggFile);
		debug("OK\n");
	}
	void SoundFile::Play() {
		sound_.reset(new Sound(*params, buffer_));
		GetAudio().Play(sound_);
	}
	void SoundFile::Stop() {
		if (sound_) {
			GetAudio().Stop(sound_);
			sound_.reset();
		}
	}
	bool SoundFile::IsPlaying() {
		if (sound_) {
			return sound_->IsPlaying();
		}
		return false;
	}
	void SoundFile::SetPitch(float p) {
		if (sound_) {
			sound_->SetPitch(p);
		}
	}
	void SoundFile::setVolume(float v) {
		if (sound_) {
			sound_->setVolume(v);
		}
	}

	bool isOpenALInstalled() {
#ifdef WEAK_LINKING_OPENAL
		try {
			GetAudio();
		} catch(WeakLinkingError& e) {
			debug(e.what()); debug("\n");
			return false;
		}
#endif
		return true;
	}

	SoundFile& GetSoundFile(const std::string& filename) {
		GetAudio();
		auto i = sounds.find(filename);
		if (i == sounds.end()) { // sound hasn't been loaded yet?
			sounds[filename].reset(new SoundFile(pathPrefix + filename));
			return *(sounds[filename]);
		}
		return *(i->second);
	}

	void play(const std::string& filename) {
		GetSoundFile(filename).Play();
	}

	void stop(const std::string& filename) {
		GetSoundFile(filename).Stop();
	}

	void loadSound(const std::string& filename) {
		GetSoundFile(filename);
	}

	bool isPlaying(const std::string& filename)	{
		return GetSoundFile(filename).IsPlaying();
	}

	void setPlaybackSpeed(float speed) {
		auto end = sounds.end();
		for (auto i = sounds.begin(); i != end; ++i) {
			i->second->SetPitch(speed);
		}
	}

	void setVolume(float volume) {
		auto end = sounds.end();
		for (auto i = sounds.begin(); i != end; ++i) {
			i->second->setVolume(volume);
		}
		Sound::masterVolume = volume;
	}

	float getVolume() {
		return Sound::masterVolume;
	}

	Audio& GetAudio() {
		static Audio* audio = new Audio;
		// TODO: We'll leak Audio, which isn't nice, but we can't call OpenAL functions during exit
		// as OpenAL32.dll might already be unloaded.
		return *audio;
	}
} // namespace jngl
