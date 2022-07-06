#pragma once


namespace AudioAPI
{

struct Vector3
{
    float x, y, z;
};


struct AudioLog
{
    enum Level
    {
        Info,
        Warning,
        Error
    };

    virtual ~AudioLog() {}
    virtual void print( Level level, const char* message, ... ) = 0;
};


struct AudioClip
{
    enum Properties
    {
        Looping = 1 << 0,
        Spatial = 1 << 1
    };
    virtual ~AudioClip() {}
};


struct AudioClipInstance
{
    virtual ~AudioClipInstance() {}

    virtual void setPaused( bool paused ) = 0;
    virtual void setVolume( float volume ) = 0;
    virtual void setPosition( const Vector3& position, const Vector3* velocity ) = 0;
};


struct AudioManager
{
    static AudioManager* create( AudioLog& log, int groups );
    virtual ~AudioManager() {}

    virtual void setGroupVolume( int group, float volume ) = 0;

    // Load the clip into memory. User is responsible for deleting these
    virtual AudioClip* createAudioClip( const char* path, unsigned int properties ) = 0;

    // Play the clip.
    // Either supply a pointer to a AudioClipInstance to manage the instance yourself. Or specify nullptr to let the AudioManager cleanup the instance for you.
    virtual void play2D( AudioClip* clip, int group, AudioClipInstance** userManagedInstance ) = 0;
    virtual void play3D( AudioClip* clip, int group, const Vector3& position, float minDistance, AudioClipInstance** userManagedInstance ) = 0;

    virtual void update( float frameTime, const Vector3& position, const Vector3& forward, const Vector3& up ) = 0;
};

}