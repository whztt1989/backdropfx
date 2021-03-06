// Copyright (c) 2010 Skew Matrix Software. All rights reserved.

#include <backdropFX/RenderingEffects.h>
#include <backdropFX/RenderingEffectsStage.h>
#include <backdropFX/Effect.h>
#include <backdropFX/Utils.h>
#include <backdropFX/RTTViewport.h>
#include <osg/Texture2D>
#include <osg/Depth>
#include <osgwTools/Shapes.h>
#include <osgwTools/Version.h>
#include <backdropFX/Utils.h>
#include <osgwTools/FBOUtils.h>

#include <sstream>


namespace backdropFX
{


Effect::Effect()
  : osg::Object(),
    _width( 0 ),
    _height( 0 )
{
    // TBD need to share the uniform and VBO across all BackdropCommon classes.

    _fstp = osgwTools::makePlane(
        osg::Vec3( -1,-1,1 ), osg::Vec3( 2,0,0 ), osg::Vec3( 0,2,0 ) );
    _fstp->setColorBinding( osg::Geometry::BIND_OFF );
    _fstp->setNormalBinding( osg::Geometry::BIND_OFF );
    _fstp->setTexCoordArray( 0, NULL );
    _fstp->setUseDisplayList( false );
    _fstp->setUseVertexBufferObjects( true );

    _textureUniform.resize( 4 );
    osg::Uniform* uniform;
    int idx;
    for( idx=0; idx<4; idx++ )
    {
        std::ostringstream ostr;
        ostr << "inputTexture" << idx;
        std::string name( ostr.str() );
        uniform = new osg::Uniform( osg::Uniform::SAMPLER_2D, name, 1 );
        uniform->set( idx );
        _textureUniform[ idx ] = uniform;
    }

    _depth = new osg::Depth( osg::Depth::ALWAYS, 0., 1., true );
}
Effect::Effect( const Effect& rhs, const osg::CopyOp& copyop )
  : osg::Object( rhs ),
    _program( rhs._program ),
    _textureUniform( rhs._textureUniform ),
    _uniforms( rhs._uniforms ),
    _inputs( rhs._inputs ),
    _output( rhs._output ),
    _width( rhs._width ),
    _height( rhs._height ),
    // TBD need to share this with all Effect classes (a static, perhaps?)
    _fstp( rhs._fstp ),
    _depth( rhs._depth )
{
}
Effect::~Effect()
{
}


void
Effect::addInput( const unsigned int unit, osg::Texture* texture )
{
    _inputs[ unit ] = texture;
}
osg::Texture*
Effect::getInput( const unsigned int unit ) const
{
    IntTextureMap::const_iterator itr = _inputs.find( unit );
    if( itr != _inputs.end() )
        return( itr->second.get() );
    else
        return( NULL );
}
int
Effect::getInputUnit( const osg::Texture* texture ) const
{
    IntTextureMap::const_iterator itr;
    for( itr = _inputs.begin(); itr != _inputs.end(); itr++ )
    {
        if( itr->second == texture )
            return( itr->first );
    }
    return( -1 );
}
bool
Effect::removeInput( const unsigned int unit )
{
    IntTextureMap::iterator itr = _inputs.find( unit );
    if( itr != _inputs.end() )
    {
        _inputs.erase( itr );
        return( true );
    }
    else
        return( false );
}
bool
Effect::removeInput( osg::Texture* texture )
{
    int unit = getInputUnit( texture );
    if( unit < 0 )
        return( false );
    else
        return( removeInput( (unsigned int)unit ) );
}

void
Effect::setOutput( osg::FrameBufferObject* fbo )
{
    _output = fbo;
}
osg::FrameBufferObject*
Effect::getOutput() const
{
    return( _output.get() );
}


void
Effect::setProgram( osg::Program* program )
{
    _program = program;
}

void
Effect::draw( RenderingEffectsStage* rfxs, osg::RenderInfo& renderInfo, bool last )
{
    osg::State& state = *( renderInfo.getState() );
    const unsigned int contextID( state.getContextID() );
    RenderingEffects* renderingEffects = rfxs->getRenderingEffects();

    // Bind the output FBO.
    osg::FrameBufferObject* fbo;
    if( _output.valid() )
    {
        osg::notify( osg::INFO ) << "Effect: applying local Effect FBO." << std::endl;

        // We have an FBO assigned, use it.
        fbo = _output.get();
    }
    else
    {
        osg::notify( osg::INFO ) << "Effect: applying RenderingEffects FBO." << std::endl;

        // Use RenderingEffects output
        fbo = renderingEffects->getFBO();
    }
    osg::FBOExtensions* fboExt( osg::FBOExtensions::instance( contextID, true ) );
    if( fbo != NULL )
    {
        fbo->apply( state );
    }
    else
    {
        // Bind the default framebuffer.
        osgwTools::glBindFramebuffer( fboExt, GL_FRAMEBUFFER_EXT, 0 );
    }
    UTIL_GL_FBO_ERROR_CHECK( (getName() + std::string(" Effect::draw") ), fboExt );

    osg::Viewport* vp = rfxs->getViewport();
    backdropFX::RTTViewport* rttvp = dynamic_cast< backdropFX::RTTViewport* >( vp );
    if( rttvp != NULL )
        rttvp->applyFullViewport( state );
    else
        state.applyAttribute( rfxs->getViewport() );


    // Use the specified program.
    if( _program.valid() )
    {
        osg::notify( osg::INFO ) << "Effect: applying program." << std::endl;
        state.applyAttribute( _program.get() );
    }

    // Bind the input textures and set their sampler uniforms.
    osg::GL2Extensions* gl2Ext( osg::GL2Extensions::Get( contextID, true ) );
    IntTextureMap::const_iterator inItr;
    for( inItr = _inputs.begin(); inItr != _inputs.end(); inItr++ )
    {
        osg::notify( osg::INFO ) << "Effect: applying input texture." << std::endl;

        osg::Uniform* uniform = _textureUniform[ inItr->first ].get();
#if OSG_SUPPORTS_UNIFORM_ID
        GLint location = state.getUniformLocation( uniform->getNameID() );
#else
        GLint location = state.getUniformLocation( uniform->getName() );
#endif
        if( location >= 0 )
            uniform->apply( gl2Ext, location );

        state.setActiveTextureUnit( inItr->first );
        osg::Texture* texture = inItr->second.get();
        state.applyTextureAttribute( inItr->first, texture );
    }
    UniformVector::const_iterator uItr;
    for( uItr = _uniforms.begin(); uItr != _uniforms.end(); uItr++ )
    {
        osg::Uniform* uniform = uItr->get();
#if OSG_SUPPORTS_UNIFORM_ID
        GLint location = state.getUniformLocation( uniform->getNameID() );
#else
        GLint location = state.getUniformLocation( uniform->getName() );
#endif
        if( location >= 0 )
            uniform->apply( gl2Ext, location );
    }
    {
        osg::Uniform* uniform = rfxs->getTexturePercentUniform();
#if OSG_SUPPORTS_UNIFORM_ID
        GLint location = state.getUniformLocation( uniform->getNameID() );
#else
        GLint location = state.getUniformLocation( uniform->getName() );
#endif
        if( location >= 0 )
            uniform->apply( gl2Ext, location );
    }
    rfxs->getRenderingEffects()->applyAllGlobalUniforms( state, gl2Ext );

    UTIL_GL_ERROR_CHECK( (getName() + std::string(" Effect::draw()." ) ) ) \

    // Draw.
    internalDraw( renderInfo );

    if( ( renderingEffects->getDebugMode() & backdropFX::BackdropCommon::debugImages ) != 0 )
        dumpImage( vp, renderingEffects->debugImageBaseFileName( contextID ) );

    if( rttvp != NULL )
        // State doesn't know we changed the viewport. Reset it the way it was.
        rttvp->apply( state );
}

bool Effect::attachOutputTo( Effect* effect, unsigned int unit )
{
    osg::Texture2D* tex;
    if( _output == NULL )
    {
        osg::notify( osg::INFO ) << "BDFX: Effect::attachOutputTo implicitly creating output FBO." << std::endl;

        tex = new osg::Texture2D();
        tex->setInternalFormat( GL_RGBA );
        tex->setBorderWidth( 0 );
        tex->setWrap( osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE );
        tex->setWrap( osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE );
        tex->setFilter( osg::Texture::MIN_FILTER, osg::Texture::LINEAR );
        tex->setFilter( osg::Texture::MAG_FILTER, osg::Texture::LINEAR );
        tex->setTextureSize( _width, _height );

        _output = new osg::FrameBufferObject();
        _output->setAttachment( osg::Camera::COLOR_BUFFER0, osg::FrameBufferAttachment( tex ) );
    }
    else
    {
        const osg::FrameBufferAttachment& fba = _output->getAttachment( osg::Camera::COLOR_BUFFER0 );
        osg::Texture* rawTex = const_cast< osg::FrameBufferAttachment* >( &fba )->getTexture();
        tex = static_cast< osg::Texture2D* >( rawTex );
    }

    if( tex != NULL )
    {
        effect->addInput( unit, tex );
        return( true );
    }
    else
    {
        osg::notify( osg::WARN ) << "BDFX: Effect::attachOutputTo can not attach NULL texture." << std::endl;
        return( false );
    }
}

void
Effect::setTextureWidthHeight( unsigned int texW, unsigned int texH )
{
    _width = texW;
    _height = texH;

    if( _output != NULL )
    {
        const osg::FrameBufferAttachment& fba = _output->getAttachment( osg::Camera::COLOR_BUFFER0 );
        osg::Texture2D* tex = const_cast< osg::Texture2D* >( 
            dynamic_cast< const osg::Texture2D* >( fba.getTexture() ) );
        if( tex != NULL )
        {
            tex->setTextureSize( _width, _height );
            tex->dirtyTextureObject();

            _output = new osg::FrameBufferObject();
            _output->setAttachment( osg::Camera::COLOR_BUFFER0, osg::FrameBufferAttachment( tex ) );
        }
    }
}

void
Effect::internalDraw( osg::RenderInfo& renderInfo )
{
    osg::notify( osg::INFO ) << "Effect: internalDraw rendering TRIANGLE_PAIR." << std::endl;

    // Disable depth test for fullscreen tri pair.
    osg::State& state = *( renderInfo.getState() );
    state.applyAttribute( _depth.get() );
    state.applyMode( GL_DEPTH_TEST, true );
    // Force disable blending, otherwise we get this psychadelic blur effect.
    state.applyMode( GL_BLEND, false );

    _fstp->draw( renderInfo );

    UTIL_GL_ERROR_CHECK( (getName() + std::string(" Effect::internamDraw().") ) ) \
}

void
Effect::dumpImage( const osg::Viewport* vp, const std::string baseFileName )
{
    const GLint x( 0 );
    const GLint y( 0 );
    const GLsizei w( vp->width() );
    const GLsizei h( vp->height() );

    std::string fileName;
    {
        std::ostringstream ostr;
        ostr << baseFileName;
        ostr << "effect-" << getName() << ".png";
        fileName = std::string( ostr.str() );
    }

    char* pixels = new char[ w * h * 4 ];
    glReadPixels( x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)pixels );
    backdropFX::debugDumpImage( fileName, pixels, w, h );
    delete[] pixels;
}




//     EffectVector _subEffects;

CompositeEffect::CompositeEffect()
{
}
CompositeEffect::CompositeEffect( const CompositeEffect& rhs, const osg::CopyOp& copyop )
  : _subEffects( rhs._subEffects )
{
}
CompositeEffect::~CompositeEffect()
{
}

void CompositeEffect::draw( RenderingEffectsStage* rfxs, osg::RenderInfo& renderInfo, bool last )
{
    EffectVector::iterator it;
    for( it = _subEffects.begin(); it != _subEffects.end(); it++ )
        (*it)->draw( rfxs, renderInfo, last );
}

void CompositeEffect::setTextureWidthHeight( unsigned int texW, unsigned int texH )
{
    EffectVector::iterator it;
    for( it = _subEffects.begin(); it != _subEffects.end(); it++ )
        (*it)->setTextureWidthHeight( texW, texH );
}

void CompositeEffect::addInput( const unsigned int unit, osg::Texture* texture )
{
    EffectVector::iterator it = _subEffects.begin();
    if( it == _subEffects.end() )
    {
        // We don't have any sub effects. Should be able to support this, but for
        // now display a warning and return.
        osg::notify( osg::WARN ) << "backdropFX: CompositeEffect: No sub effects, can't add input." << std::endl;
        return;
    }
    (*it)->addInput( unit, texture );
}
osg::Texture* CompositeEffect::getInput( const unsigned int unit ) const
{
    EffectVector::const_iterator it = _subEffects.begin();
    if( it == _subEffects.end() )
    {
        // We don't have any sub effects. Should be able to support this, but for
        // now display a warning and return.
        osg::notify( osg::WARN ) << "backdropFX: CompositeEffect: No sub effects, can't get input." << std::endl;
        return( NULL );
    }
    return( (*it)->getInput( unit ) );
}
int CompositeEffect::getInputUnit( const osg::Texture* texture ) const
{
    EffectVector::const_iterator it = _subEffects.begin();
    if( it == _subEffects.end() )
    {
        // We don't have any sub effects. Should be able to support this, but for
        // now display a warning and return.
        osg::notify( osg::WARN ) << "backdropFX: CompositeEffect: No sub effects, can't get input unit." << std::endl;
        return( -1 );
    }
    return( (*it)->getInputUnit( texture ) );
}
bool CompositeEffect::removeInput( const unsigned int unit )
{
    EffectVector::iterator it = _subEffects.begin();
    if( it == _subEffects.end() )
    {
        // We don't have any sub effects. Should be able to support this, but for
        // now display a warning and return.
        osg::notify( osg::WARN ) << "backdropFX: CompositeEffect: No sub effects, can't remove input by unit." << std::endl;
        return( false );
    }
    return( (*it)->removeInput( unit ) );
}
bool CompositeEffect::removeInput( osg::Texture* texture )
{
    EffectVector::iterator it = _subEffects.begin();
    if( it == _subEffects.end() )
    {
        // We don't have any sub effects. Should be able to support this, but for
        // now display a warning and return.
        osg::notify( osg::WARN ) << "backdropFX: CompositeEffect: No sub effects, can't remove input by texture." << std::endl;
        return( false );
    }
    return( (*it)->removeInput( texture ) );
}

void CompositeEffect::setOutput( osg::FrameBufferObject* fbo )
{
    EffectVector::reverse_iterator rit = _subEffects.rbegin();
    if( rit == _subEffects.rend() )
    {
        // We don't have any sub effects. Should be able to support this, but for
        // now display a warning and return.
        osg::notify( osg::WARN ) << "backdropFX: CompositeEffect: No sub effects, can't set output." << std::endl;
        return;
    }
    (*rit)->setOutput( fbo );
}
osg::FrameBufferObject* CompositeEffect::getOutput() const
{
    EffectVector::const_reverse_iterator rit = _subEffects.rbegin();
    if( rit == _subEffects.rend() )
    {
        // We don't have any sub effects. Should be able to support this, but for
        // now display a warning and return.
        osg::notify( osg::WARN ) << "backdropFX: CompositeEffect: No sub effects, can't get output." << std::endl;
        return( NULL );
    }
    return( (*rit)->getOutput() );
}

bool CompositeEffect::attachOutputTo( Effect* effect, unsigned int unit )
{
    EffectVector::reverse_iterator rit = _subEffects.rbegin();
    if( rit == _subEffects.rend() )
    {
        // We don't have any sub effects. Should be able to support this, but for
        // now display a warning and return.
        osg::notify( osg::WARN ) << "backdropFX: CompositeEffect: No sub effects, can't attach output to Effect." << std::endl;
        return( false );
    }
    return( (*rit)->attachOutputTo( effect, unit ) );
}

void CompositeEffect::dumpImage( const osg::Viewport* vp, const std::string baseFileName )
{
    // TBD not sure how to implement this in CompositeEffect.
    // For now, just dump the last image.

    EffectVector::reverse_iterator rit = _subEffects.rbegin();
    if( rit == _subEffects.rend() )
    {
        // We don't have any sub effects. Should be able to support this, but for
        // now display a warning and return.
        osg::notify( osg::WARN ) << "backdropFX: CompositeEffect: No sub effects, can't dump image." << std::endl;
        return;
    }
    (*rit)->dumpImage( vp, baseFileName );
}



backdropFX::Effect* getEffect( const std::string& className, EffectVector& effectVector )
{
    EffectVector::iterator it;
    for( it = effectVector.begin(); it != effectVector.end(); it++ )
    {
        if( std::string( (*it)->className() ) == className )
            break;
    }

    if( it == effectVector.end() )
    {
        return( NULL );
    }
    else
    {
        return( (*it).get() );
    }
}
bool removeEffect( const std::string& objectName, EffectVector& effectVector )
{
    EffectVector::iterator it;
    for( it = effectVector.begin(); it != effectVector.end(); it++ )
    {
        if( std::string( (*it)->getName() ) == objectName )
            break;
    }

    if( it == effectVector.end() )
    {
        // Not found.
        return( false );
    }
    else
    {
        effectVector.erase( it );
        return( true );
    }
}



// namespace backdropFX
}
