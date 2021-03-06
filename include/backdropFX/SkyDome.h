// Copyright (c) 2010 Skew Matrix Software. All rights reserved.

#ifndef __BACKDROPFX_SKYDOME_H__
#define __BACKDROPFX_SKYDOME_H__ 1


#ifdef __APPLE__
#  ifndef _DARWIN
#    define _DARWIN 1
#  endif
#endif

#include <backdropFX/Export.h>
#include <backdropFX/BackdropCommon.h>
#include <osg/Group>
#include <osg/FrameBufferObject>
#include <osgEphemeris/DateTime.h>
#include <osgEphemeris/CelestialBodies.h>
#include <backdropFX/SunBody.h>
#include <backdropFX/MoonBody.h>
#include <osgUtil/CullVisitor>

#include <backdropFX/SkyDomeStage.h>

#include <map>


namespace backdropFX {


// Forward
class SkyDomeUpdateCB;
class SkyDomeCullCB;
class LocationCB;


/** \class backdropFX::SkyDome SkyDome.h backdropFX/SkyDome.h

\brief Class for rendering a sky dome with Sun and Moon.

\section Sun Lighting and Clear Control

If you enable the SkyDome, and you use lighting shader modules that support Sun lighting
(that's an option to ShaderModuleVisitor), then one of the lights in your scene is the Sun.
We're not using FFP, so the Sun is a new light source outside the FFP GL_LIGHTn paradigm.

Additionally, when SkyDome is enabled, clear color is meaningless (because you clear to the
sky dome).

If you disable SkyDome, clear color becomes meaningful, and clear is performed by the next
backdropFX component in the render pipeline, which is DepthPartition.

Also when SkyDome is disabled, the Sun light source goes away; it behaves as if it is
disabled.

So, if you want Sun lighting, you must enable the SkyDome, but if you want control over
the clear color you must disable the SkyDome. As a result, there is no way to have both
Sun lighting and control over the clear color.
*/
class BACKDROPFX_EXPORT SkyDome : public osg::Group, public backdropFX::BackdropCommon
{
    friend class SkyDomeUpdateCB;
    friend class LocationCB;

public:
    SkyDome();
    SkyDome( const SkyDome& skydome, const osg::CopyOp& copyop=osg::CopyOp::SHALLOW_COPY );

    META_Node( backdropFX, SkyDome );

    void traverse( osg::NodeVisitor& nv );


    /** Enable or disable automatic time advancing. Off by default.
    "Time" means backdropFX::LocationData::setDateTime(), which affects
    the Sun and Moon position and sky coloring.
    
    Updating is handled in the SkyDomeUpdateCB, so advanced once per
    frame. The scale factor defaults to 1.0 which maps to real-time.
    Set to 2.0 to advance time at a 2x rate, 0.5 at a 1/2x rate, etc.
    Negative scaling isn't supported. */
    void setAutoAdvanceTime( bool advance=true, float scale=1.f );
    bool getAutoAdvanceTime() const;
    bool getAutoAdvanceTime( float& scale ) const;
    
    /** Set the radius of the skydome, which represents the
    distance to the Sun and Moon bodies. By default, the radius
    is 384403 (distance to the moon in kilometers). */
    void setRadius( float radius );
    float getRadius() const;

    /** Set the Sun scale factor. The default is 1.0, resulting in
    drawing the Sun with radius 1738. (This is the Moon radius
    in kilometers, but the same value works for the Sun because
    parallax causes the Sun and Moon to have the
    same apparent diameter when viewed from Earth.) Both the Sun
    and Moon should draw with the same actual radius, scale, and distance. */
    void setSunScale( float sunScale );
    float getSunScale() const;
    /** Set the Sun geodesic sphere subdivisions. See
    osgwTools/Shapes.h for documentation. */
    void setSunSubdivisions( unsigned int sunSub );
    unsigned int getSunSubdivisions() const;

    /** Set the Moon scale factor. The default is 1.0, resulting in
    the Moon drawing with radius 1738. This is the Moon's radius
    in kilometers. */
    void setMoonScale( float moonScale );
    float getMoonScale() const;
    /** Set the Moon geodesic sphere subdivisions. See
    osgwTools/Shapes.h for documentation. */
    void setMoonSubdivisions( unsigned int moonSub );
    unsigned int getMoonSubdivisions() const;

    /** Override to employ dirty bit. If other than DEBUG_OFF, display wireframe
    RA/Dec sphere with osgText labels displays as a debugging aid. If set
    to DEBUG_IMAGES, dump final rendered image to file. */
    virtual void setDebugMode( unsigned int debugMode );

    /** When set to false, SkyDome just clears and renders nothing.
    The default is true (enabled). */
    void setEnable( bool enable=true ) { _enable = enable; }
    bool getEnable() const { return( _enable ); }

    // TBD Prototype, not fully functional.
    void useTexture( osg::TextureCubeMap* texture );



    //
    // For internal use
    //

    void setRenderingCache( osg::Object* rc ) { _renderingCache = rc; }
    osg::Object* getRenderingCache() { return _renderingCache.get(); }
    const osg::Object* getRenderingCache() const { return _renderingCache.get(); }

    void resizeGLObjectBuffers( unsigned int maxSize );
    void releaseGLObjects( osg::State* state ) const;

protected:
    ~SkyDome();

    void rebuild();
    void updateLocationData();
    void updateDebug();

    void setSunPosition( backdropFX::SunBody* sunBody, const osgEphemeris::DateTime& dateTime );
    void setMoonPosition( backdropFX::MoonBody* moonBody, const osgEphemeris::DateTime& dateTime );

    static const unsigned int RebuildDirty;
    static const unsigned int LocationDataDirty;
    static const unsigned int DebugDirty;
    static const unsigned int AllDirty;

    bool _timeAdvance;
    float _timeAdvanceScale;

    unsigned int _dirty;
    float _radius;
    float _sunScale;
    unsigned int _sunSub;
    float _moonScale;
    unsigned int _moonSub;

    osg::ref_ptr< osg::Uniform > _orientationUniform;

    osg::ref_ptr< backdropFX::SunBody > _sunBody;
    osg::ref_ptr< backdropFX::MoonBody > _moonBody;

    osg::ref_ptr< osgEphemeris::Sun > _cSun;
    osg::ref_ptr< osgEphemeris::Moon > _cMoon;

    osg::ref_ptr< SkyDomeUpdateCB > _updateCB;
    osg::ref_ptr< SkyDomeCullCB > _cullCB;

    // TBD Prototype, not fully functional.
    osg::ref_ptr< osg::TextureCubeMap > _texture;

    /** Map CullVisitor address to StateSets.
    StateSet stores uniform for modified
    view and projection matrices concatenation. */
    typedef struct std::pair< osg::ref_ptr< osg::StateSet >, osg::Uniform* > UniformBundle;
    typedef std::map< osgUtil::CullVisitor*, UniformBundle > StateSetPerCull;
    StateSetPerCull _stateSetPerCull;
    OpenThreads::Mutex _stateSetMapLock;

    UniformBundle* updateViewProjUniforms( osgUtil::CullVisitor* cv );

    bool _enable;

    osg::ref_ptr< osg::Object > _renderingCache;
};


// namespace backdropFX
}

// __BACKDROPFX_SKYDOME_H__
#endif
