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

        HSAMPLE sample;
        short* decoded;

        // Current track
        static U32 track;

        List<PathIdent> tracks;

        // Next poll time
        static U32 pollTime;

        // Is enabled
        static Bool enabled;

        // Maximum volume
        enum { MIN_VOLUME = 0, MAX_VOLUME = 127 };


        //
        // Init
        //
        // Initialize systems
        //
        void Init()
        {
            ASSERT(!initialized);

            driver = nullptr;
            sample = nullptr;
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
                sample = AIL_allocate_sample_handle(driver);

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
            if (initialized)
            {
                if (driver)
                {
                    // Stop playing anything
                    Stop();

                    tracks.DisposeAll();

                    // Clear driver pointer
                    driver = nullptr;

                    AIL_release_sample_handle(sample);
                    sample = nullptr;

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
                // Reset our track counter
                track = 0;

                // Stop the device
                AIL_stop_sample(sample);
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
                    // Update our track count
                    track = newTrack;

                    FileSys::DataFile* dataFile;

                    std::string path = std::string("music\\") + tracks[track - 2]->str;

                    if (FileSys::DataFile* dataFile = FileSys::Open(path.c_str()))
                    {
                        int channels;
                        int sample_rate;

                        free(decoded);

                        unsigned long file_size = dataFile->Size();

                        int num_samples = stb_vorbis_decode_memory((U8*)dataFile->GetMemoryPtr(), file_size, &channels,
                                                                   &sample_rate, &decoded);

                        FileSys::Close(dataFile);

                        if (sample)
                        {
                            // Initialize the voice.
                            AIL_init_sample(sample);

                            S32 buff_id = AIL_sample_buffer_ready(sample);

                            if (-1 != buff_id)
                            {
                                // Load the data.
                                AIL_set_sample_playback_rate(sample, sample_rate);
                                AIL_set_sample_type(sample, DIG_F_STEREO_16, 0);

                                AIL_set_sample_address(sample, decoded, (num_samples * channels * sizeof(short)));

                                char* last_error = AIL_last_error();

                                // Reset the volume.
                                SetVolume(Sound::Redbook::Volume());

                                // Start the stream playing.
                                AIL_start_sample(sample);
                            }
                        }
                    }
                }
                else
                {
                    Stop();
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
            switch (AIL_sample_status(sample))
            {
                    // Finished playing track, play next.
                case SMP_DONE:
                {
                    Play(track + 1);
                }
                break;

                    // case SMP_FREE:
                    // {
                    // }
                    // break;

                    // Still playing track.
                case SMP_PLAYING:
                {
                    unsigned long ms = AIL_ms_count();
                    LOG_DEV(("MS = %d", ms));

                    sample->len;
                }
                break;

                    // case SMP_PLAYINGBUTRELEASED:
                    // {
                    // }
                    // break;

                    // case SMP_STOPPED:
                    // {
                    // }
                    // break;

                    // default:
                    //     Stop();
                    //     break;
            }
        }


        //
        // TrackCount
        //
        // The number of tracks in the "music" folder.
        //
        U32 TrackCount()
        {
            int numFiles = 0;

            FilePath path = "music";

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
            if (initialized && driver)
            {
                AIL_set_sample_volume(sample, Clamp<S32>(
                                          MIN_VOLUME, static_cast<S32>((volume * static_cast<F32>(MAX_VOLUME)) + 0.5F),
                                          MAX_VOLUME));
            }
        }


        //
        // Volume
        //
        // Returns the current audio volume
        //
        F32 Volume()
        {
            if (initialized && driver)
            {
                return (static_cast<F32>(AIL_sample_volume(sample)) / static_cast<F32>(MAX_VOLUME));
            }

            return 0.0f;
        }
    }
}
