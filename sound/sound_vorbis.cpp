///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Sound System
//
// 18-AUG-2020
//


//
// Includes
//

#include "sound_private.h"
#include "trackplayer.h"
#include "../3rdparty/stb_vorbis.c"
#include "../styxnet/win32_mutex.h"
#include "../styxnet/win32_thread.h"


///////////////////////////////////////////////////////////////////////////////
//
// Namespace Sound - Sound system
//
namespace Sound
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    // Namespace Vorbis - System dealing with .ogg audio
    //
    namespace Vorbis
    {
        // Is the system initialized
        static Bool initialized = FALSE;

        // Handle to the driver
        static HDIGDRIVER driver;

        // Current track
        static U32 track;

        static F32 vorbisVolume;

        // Next poll time
        static U32 pollTime;

        // Is enabled
        static Bool enabled;

        // Maximum volume
        enum { MIN_VOLUME = 0, MAX_VOLUME = 127 };

        Win32::Thread playerThread;
        U32 STDCALL PlayTrack(void* context);
        static Win32::Mutex playerMutex;

        struct StreamItem
        {
            NBinTree<StreamItem>::Node node;

            HSAMPLE sample = nullptr;
            FileSys::DataFile* file;

            short* decoded = nullptr;

            int channels;
            int sample_rate;

            bool started = false;

            StreamItem(const char* path, HDIGDRIVER driver)
            {
                file = FileSys::Open(path);

                if (file)
                {
                    if (HSAMPLE handle = AIL_allocate_sample_handle(driver))
                    {
                        AIL_init_sample(handle);

                        if (handle)
                        {
                            sample = handle;

                            if (sample && (AIL_sample_buffer_ready(sample) != -1))
                            {
                                playerThread.Start(PlayTrack, this);
                            }
                        }
                    }
                }
            }

            ~StreamItem()
            {
                AIL_release_sample_handle(sample);

                playerThread.Stop();

                if (decoded)
                {
                    free(decoded);
                    decoded = nullptr;
                }

                Close(file);
            }

            Bool IsPlaying()
            {
                return ((AIL_sample_status(sample) != SMP_DONE));
            }

            void SetVolume(F32 volume)
            {
                AIL_set_sample_volume
                (
                    sample, Clamp<S32>
                    (
                        MIN_VOLUME, static_cast<S32>((volume * static_cast<F32>(MAX_VOLUME)) + 0.5F),
                        MAX_VOLUME
                    )
                );
            }

            F32 Volume()
            {
                return (static_cast<F32>(AIL_sample_volume(sample)) / static_cast<F32>(MAX_VOLUME));
            }
        };

        U32 STDCALL PlayTrack(void* context)
        {
            StreamItem* si = (StreamItem*)context;

            int num_samples = stb_vorbis_decode_memory
            (
                static_cast<U8*>(si->file->GetMemoryPtr()),
                si->file->Size(),
                &si->channels,
                &si->sample_rate,
                &si->decoded
            );

            int tracklen = num_samples * si->channels * sizeof(short);

            AIL_set_sample_playback_rate(si->sample, si->sample_rate);
            AIL_set_sample_type(si->sample, DIG_F_STEREO_16, 0);

            AIL_set_sample_address(si->sample, si->decoded, tracklen);

            char* last_error = AIL_last_error();
            // LOG_ERR((last_error));

            // Reset the volume.
            si->SetVolume(vorbisVolume);

            AIL_start_sample(si->sample);

            si->started = true;

            return 0;
        }

        List<PathIdent> tracks;
        static NBinTree<StreamItem> streams(&StreamItem::node);


        //
        // Init
        //
        // Initialize systems
        //
        void Init()
        {
            ASSERT(!initialized);

            driver = nullptr;
            track = 0;
            enabled = TRUE;

            // Set init flag
            initialized = TRUE;
        }


        //
        // Done
        //
        // Shutdown systems
        //
        void Done()
        {
            ASSERT(initialized);

            Release();

            // Clear init flag
            initialized = FALSE;
        }


        //
        // CriticalShutdown
        //
        // Critical shutdown function
        //
        void CriticalShutdown()
        {
            Release();
        }


        //
        // GetEnabled
        //
        // Is vorbis audio enabled
        //
        Bool GetEnabled()
        {
            return (enabled);
        }


        //
        // SetEnabled
        //
        // Set whether vorbis is enabled
        //
        void SetEnabled(Bool flag)
        {
            enabled = flag;
        }


        //
        // SyncEnabled
        //
        // Sync with enabled state by claiming or releasing driver
        //
        void SyncEnabled()
        {
            if (initialized)
            {
                if (enabled)
                {
                    if (!Claimed())
                    {
                        Claim();
                    }
                }
                else
                {
                    if (Claimed())
                    {
                        Release();
                    }
                }
            }
        }


        //
        // Claim
        //
        // Claim driver
        //
        Bool Claim()
        {
            if (initialized)
            {
                Release();

                driver = Digital::GetDriver();

                if (driver)
                {
                    Stop();
                    return true;
                }
            }

            return false;
        }


        //
        // Release
        //
        // Release driver
        //
        Bool Release()
        {
            tracks.DisposeAll();
            streams.DisposeAll();

            if (initialized)
            {
                if (driver)
                {
                    // Stop playing anything
                    Stop();

                    // Clear driver pointer
                    driver = nullptr;

                    return (TRUE);
                }
            }

            return (FALSE);
        }


        //
        // Claimed
        //
        // Do we currently have a driver
        //
        Bool Claimed()
        {
            return (initialized && driver ? TRUE : FALSE);
        }


        //
        // Stop
        //
        // Stop audio
        //
        void Stop()
        {
            if (initialized && driver)
            {
                // Reset our track counter.
                track = 0;

                // Shutdown current music.
                streams.DisposeAll();
            }
        }


        //
        // Play
        //
        // Play a track (request is wrapped, so returns track number started)
        //
        U32 Play(U32 newTrack)
        {
            if (initialized && driver)
            {
                Stop();

                // Number of tracks.
                U32 count = TrackCount();

                // Clip track (wrap around).
                if ((newTrack > count) || (newTrack < 2))
                {
                    newTrack = 2;
                }

                if (newTrack >= 1 && newTrack <= count)
                {
                    // Update current track.
                    track = newTrack;

                    std::string path = std::string("music\\") + tracks[track - 2]->str;

                    // auto current_redbook_volume = Redbook::Volume();
                    // auto current_vorbis_volume = vorbisVolume;

                    // Allocate stream item
                    StreamItem* item = new StreamItem(path.c_str(), driver);
                    streams.Add(0, item);
                }
            }

            return track;
        }

        //
        // Poll
        //
        // Poll the audio (used for proceeding to next track automatically)
        //
        void Poll()
        {
            if (streams.GetCount() > 0)
            {
                if (!streams.GetFirst()->IsPlaying() && streams.GetFirst()->started)
                {
                    Play(track + 1);
                }
            }
        }


        //
        // TrackCount
        //
        // The number of tracks in the "music" folder.
        //
        U32 TrackCount()
        {
            tracks.DisposeAll();

            int numFiles = 0;

            Dir::Find find;
            if (Dir::FindFirst(find, "music", "*.ogg"))
            {
                do
                {
                    // Exclude directories
                    if (!(find.finddata.attrib & File::Attrib::SUBDIR))
                    {
                        tracks.Append(new PathIdent(find.finddata.name));
                        numFiles++;
                    }
                }
                while (Dir::FindNext(find));
            }

            // Pretend there's a '01 -> data' track for specific compatibility.
            // J06_Intro for example requires playing track 01 for silence before track 06 is triggered.
            numFiles = numFiles + 1;

            return ((initialized && driver) ? numFiles : 0);
        }


        //
        // CurrentTrack
        //
        // The current audio track
        //
        U32 CurrentTrack()
        {
            return (initialized ? track : 0);
        }


        //
        // SetVolume
        //
        // Sets the current audio volume
        //
        void SetVolume(F32 volume)
        {
            vorbisVolume = volume;

            if (initialized && driver)
            {
                if (streams.GetCount() > 0)
                {
                    streams.GetFirst()->SetVolume(volume);
                }
            }
        }


        //
        // Volume
        //
        // Returns the current audio volume
        //
        F32 Volume()
        {
            return vorbisVolume;

            // if (initialized && driver)
            // {
            //     if (streams.GetCount() > 0)
            //     {
            //         return streams.GetFirst()->Volume();
            //     }
            // }
            //
            // return 0.0f;
        }
    }
}
