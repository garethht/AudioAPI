#include "Audio.h"
#include <fmod.hpp>
#include <vector>


namespace AudioAPI
{

const float gMaxDistance = 10000.0f;

class AudioManagerFMOD;
class AudioClipFMOD;
class AudioClipInstanceFMOD;


class AudioManagerFMOD : public AudioManager
{
public:

    AudioManagerFMOD( AudioLog& log, int groups );
    virtual ~AudioManagerFMOD();

    FMOD::System* getSystem() { return m_system; }
    FMOD::ChannelGroup* getGroup( int group ) { return m_channelGroups[ group ]; }

    void registerClip( AudioClipFMOD* clip );
    void unregisterClip( AudioClipFMOD* clip );

    void registerClipInstance( AudioClipInstanceFMOD* clipInstance, bool systemManaged );
    void unregisterClipInstance( AudioClipInstanceFMOD* clipInstance, bool systemManaged );

private:

    void setGroupVolume( int group, float volume ) override final;
    AudioClip* createAudioClip( const char* path, unsigned int properties ) override final;
    void play2D( AudioClip* clip, int group, AudioClipInstance** userManagedInstance ) override final;
    void play3D( AudioClip* clip, int group, const Vector3& position, float minDistance, AudioClipInstance** userManagedInstance ) override final;

    void update( float frameTime, const Vector3& position, const Vector3& forward, const Vector3& up ) override final;

    AudioLog&                               m_log;

    FMOD::System*                           m_system = nullptr;

    const int                               m_numGroups;
    FMOD::ChannelGroup**                    m_channelGroups = nullptr;

    FMOD_VECTOR                             m_lastPosition = {};

    std::vector<AudioClipFMOD*>             m_clips = {};
    std::vector<AudioClipInstanceFMOD*>     m_clipInstances = {};
    std::vector<AudioClipInstanceFMOD*>     m_systemManagedClipInstances = {};
};


class AudioClipFMOD : public AudioClip
{
public:

    AudioClipFMOD( AudioManagerFMOD& manager, FMOD::Sound& sound, const char* name );
    virtual ~AudioClipFMOD();

    FMOD::Sound* getSound() { return &m_sound; }
    const char* getName() const { return m_name; }

private:

    AudioManagerFMOD&   m_manager;
    FMOD::Sound&        m_sound;
    char                m_name[ 256 ] = {};
};


class AudioClipInstanceFMOD : public AudioClipInstance
{
public:

    AudioClipInstanceFMOD( AudioManagerFMOD& manager, AudioClipFMOD& clip, int group, bool systemManaged, const Vector3& position, float minDistance );
    virtual ~AudioClipInstanceFMOD();

    bool isPlaying() const;
    const char* getName() const { return m_clipName; }

private:

    void setPaused( bool paused ) override final;
    void setVolume( float volume ) override final;
    void setPosition( const Vector3& position, const Vector3* velocity ) override final;

    AudioManagerFMOD&   m_manager;
    AudioClipFMOD&      m_clip;
    const bool          m_systemManaged;
    FMOD::Channel*      m_channel = nullptr;
    char                m_clipName[ 256 ] = {};
};


AudioManager* AudioManager::create( AudioLog& log, int groups )
{
    return new AudioManagerFMOD( log, groups );
}


AudioManagerFMOD::AudioManagerFMOD( AudioLog& log, int groups ) : m_log( log ), m_numGroups( groups )
{
    m_log.print( AudioLog::Info, "Initializing audio system...\n" );

    FMOD_RESULT result = FMOD::System_Create( &m_system );
    if ( result != FMOD_OK )
    {
        m_log.print( AudioLog::Error, "Failed to create FMOD\n" );
        return;
    }

    uint32_t version = 0;
    result = m_system->getVersion( &version );
    if ( result == FMOD_OK )
    {
        m_log.print( AudioLog::Info, "FMOD version: %d\n", version );
    }
    else
    {
        m_log.print( AudioLog::Warning, "Failed to get FMOD version\n" );
    }

    char name[ 256 ] = {};
    FMOD_SPEAKERMODE speakerMode = FMOD_SPEAKERMODE_MAX;
    int channels = 0;
    result  = m_system->getDriverInfo( 0, name, _countof( name ), nullptr, nullptr, &speakerMode, &channels );
    if ( result == FMOD_OK )
    {
        m_log.print( AudioLog::Info, "FMOD driver: %s\n", name );
    }
    else
    {
        m_log.print( AudioLog::Warning, "Failed to get FMOD driver info\n" );
    }

    result = m_system->init( 100, FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED, nullptr );
    if ( result != FMOD_OK )
    {
        m_log.print( AudioLog::Error, "Failed to initialize FMOD\n" );
        return;
    }

    result = m_system->set3DSettings( 1.0f, 1.0f, 1.0f );
    if ( result != FMOD_OK )
    {
        m_log.print( AudioLog::Warning, "Failed to set 3d settings in FMOD\n" );
    }

    if ( m_numGroups > 0 )
    {
        m_channelGroups = new FMOD::ChannelGroup*[ m_numGroups ];

        for ( int i = 0; i < m_numGroups; i++ )
        {
            char name[ 32 ] = {};
            sprintf_s( name, _countof( name ), "channelgroup%d", i );
            m_system->createChannelGroup( "2d", &m_channelGroups[ i ] );
        }
    }
}


AudioManagerFMOD::~AudioManagerFMOD()
{
    m_log.print( AudioLog::Info, "Shutting down audio system...\n" );

    for ( int i = 0; i < m_systemManagedClipInstances.size(); i++ )
    {
        //m_log.print( AudioLog::Warning, "to free system managed clip instance %s\n", m_systemManagedClipInstances[ i ]->getName() );
        delete m_systemManagedClipInstances[ i ];
    }

    for ( int i = 0; i < m_clipInstances.size(); i++ )
    {
        //m_log.print( AudioLog::Warning, "Failed to free clip instance %s\n", m_clipInstances[ i ]->getName() );
        delete m_clipInstances[ i ];
    }

    for ( int i = 0; i < m_clips.size(); i++ )
    {
        m_log.print( AudioLog::Warning, "Failed to free clip %s\n", m_clips[ i ]->getName() );
        delete m_clips[ i ];
    }

    if ( m_system )
    {
        if ( m_channelGroups )
        {
            for ( int i = 0; i < m_numGroups; i++ )
            {
                m_channelGroups[ i ]->release();
            }
            delete[] m_channelGroups;
        }

        FMOD_RESULT result = m_system->close();
        if ( result != FMOD_OK )
        {
            m_log.print( AudioLog::Error, "Failed to close FMOD\n" );
        }

        result = m_system->release();
        if ( result != FMOD_OK )
        {
            m_log.print( AudioLog::Error, "Failed to release FMOD\n" );
        }
        m_system = nullptr;
    }

    m_log.print( AudioLog::Info, "done\n" );
}


void AudioManagerFMOD::setGroupVolume( int group, float volume )
{
    if ( group >= 0 && group < m_numGroups )
    {
        m_channelGroups[ group ]->setVolume( volume );
    }
    else
    {
        m_log.print( AudioLog::Error, "setGroupVolume - group out of range: %d\n", group );
    }
}


AudioClip* AudioManagerFMOD::createAudioClip( const char* path, unsigned int properties )
{
    FMOD::Sound* sound = nullptr;

    int flags = properties & AudioClip::Spatial ? FMOD_3D : FMOD_2D;
    if ( properties & AudioClip::Looping )
        flags |= FMOD_LOOP_NORMAL;

    FMOD_RESULT result = m_system->createSound( path, flags, nullptr, &sound );
    if( result  == FMOD_OK && sound )
    {
        return new AudioClipFMOD( *this, *sound, path );
    }
    else
    {
        m_log.print( AudioLog::Error, "Failed to create %s in FMOD\n", path );
        return nullptr;
    }
}


void AudioManagerFMOD::play2D( AudioClip* clip, int group, AudioClipInstance** userManagedInstance )
{
    if ( clip )
    {
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        AudioClipInstanceFMOD* instance = new AudioClipInstanceFMOD( *this, *(AudioClipFMOD*)clip, group, userManagedInstance ? false : true, position, 0.0f );
        if ( userManagedInstance )
        {
            *userManagedInstance = instance;
        }
    }
}


void AudioManagerFMOD::play3D( AudioClip* clip, int group, const Vector3& position, float minDistance, AudioClipInstance** userManagedInstance )
{
    if ( clip )
    {
        AudioClipInstanceFMOD* instance = new AudioClipInstanceFMOD( *this, *(AudioClipFMOD*)clip, group, userManagedInstance ? false : true, position, minDistance );
        if ( userManagedInstance )
        {
            *userManagedInstance = instance;
        }
    }
}


void AudioManagerFMOD::update( float frameTime, const Vector3& position, const Vector3& forward, const Vector3& up )
{
    if ( m_system )
    {
        if ( frameTime > 0.0f && frameTime < 1.0f )
        {
            FMOD_VECTOR fmodPosition = { position.x,position.y, position.z };
            FMOD_VECTOR fmodForward = { forward.x, forward.y, forward.z };
            FMOD_VECTOR fmodUp = { up.x,up.y, up.z };
            FMOD_VECTOR velocity = {};

            velocity.x = (fmodPosition.x - m_lastPosition.x) / frameTime;
            velocity.y = (fmodPosition.y - m_lastPosition.y) / frameTime;
            velocity.z = (fmodPosition.z - m_lastPosition.z) / frameTime;

            m_lastPosition = fmodPosition;

            FMOD_RESULT result = m_system->set3DListenerAttributes( 0, &fmodPosition, &velocity, &fmodForward, &fmodUp );
            if ( result != FMOD_OK )
            {
                m_log.print( AudioLog::Warning, "Failed to set the 3d listener position in FMOD\n" );
            }
        }

        FMOD_RESULT result = m_system->update();
        if ( result != FMOD_OK )
        {
            m_log.print( AudioLog::Warning, "Failed to update FMOD\n" );
        }

        // Delete one system managed instance per frame
        for ( int i = 0; i < (int)m_systemManagedClipInstances.size(); i++ )
        {
            if ( !m_systemManagedClipInstances[ i ]->isPlaying() )
            {
                delete m_systemManagedClipInstances[ i ];
                break;
            }
        }
    }
}


void AudioManagerFMOD::registerClip( AudioClipFMOD* clip )
{
    m_clips.push_back( clip );
}


void AudioManagerFMOD::unregisterClip( AudioClipFMOD* clip )
{
    std::vector<AudioClipFMOD*>::iterator it = std::find( m_clips.begin(), m_clips.end(), clip );
    m_clips.erase( it );
}


void AudioManagerFMOD::registerClipInstance( AudioClipInstanceFMOD* clipInstance, bool systemManaged )
{
    m_clipInstances.push_back( clipInstance );
    if ( systemManaged )
    {
        m_systemManagedClipInstances.push_back( clipInstance );
    }
} 


void AudioManagerFMOD::unregisterClipInstance( AudioClipInstanceFMOD* clipInstance, bool systemManaged )
{
    std::vector<AudioClipInstanceFMOD*>::iterator it = std::find( m_clipInstances.begin(), m_clipInstances.end(), clipInstance );
    m_clipInstances.erase( it );

    if ( systemManaged )
    {
        std::vector<AudioClipInstanceFMOD*>::iterator it = std::find( m_systemManagedClipInstances.begin(), m_systemManagedClipInstances.end(), clipInstance );
        m_systemManagedClipInstances.erase( it );
    }
}


AudioClipFMOD::AudioClipFMOD( AudioManagerFMOD& manager, FMOD::Sound& sound, const char* name ) : m_manager( manager ), m_sound( sound )
{
    m_manager.registerClip( this );

    strncpy_s( m_name, name, _countof( m_name ) );
}


AudioClipFMOD::~AudioClipFMOD()
{
    m_sound.release();
    m_manager.unregisterClip( this );
}


AudioClipInstanceFMOD::AudioClipInstanceFMOD( AudioManagerFMOD& manager, AudioClipFMOD& clip, int group, bool systemManaged, const Vector3& position, float minDistance ) :
    m_manager( manager ),
    m_clip( clip ),
    m_systemManaged( systemManaged )
{
    FMOD::ChannelGroup* channelGroup = m_manager.getGroup( group );

    FMOD_RESULT result = m_manager.getSystem()->playSound( m_clip.getSound(), channelGroup, true, &m_channel );
    if ( result == FMOD_OK && m_channel )
    {
        FMOD_VECTOR pos = { position.x, position.y, position.z };
        m_channel->set3DAttributes( &pos, nullptr );
        m_channel->set3DMinMaxDistance( minDistance, gMaxDistance );
        result = m_channel->setPaused( false );
    }

    strncpy_s( m_clipName, m_clip.getName(), _countof( m_clipName ) );

    m_manager.registerClipInstance( this, m_systemManaged );
}


AudioClipInstanceFMOD::~AudioClipInstanceFMOD()
{
    if ( m_channel )
    {
        m_channel->stop();
    }

    m_manager.unregisterClipInstance( this, m_systemManaged );
}


bool AudioClipInstanceFMOD::isPlaying() const
{
    bool playing = false;
    if ( m_channel )
    {
        m_channel->isPlaying( &playing );
    }

    return playing;
}


void AudioClipInstanceFMOD::setPaused( bool paused )
{
    if ( m_channel )
    {
        m_channel->setPaused( paused );
    }
}


void AudioClipInstanceFMOD::setVolume( float volume )
{
    if ( m_channel )
    {
        m_channel->setVolume( volume );
    }
}


void AudioClipInstanceFMOD::setPosition( const Vector3& position, const Vector3* velocity )
{
    if ( m_channel )
    {
        FMOD_VECTOR pos = { position.x, position.y, position.z };
        if ( velocity )
        {
            FMOD_VECTOR vel = { velocity->x, velocity->y, velocity->z };
            m_channel->set3DAttributes( &pos, &vel );
        }
        else
        {
            m_channel->set3DAttributes( &pos, nullptr );
        }
    }
}

};