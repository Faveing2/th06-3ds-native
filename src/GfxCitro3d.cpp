#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <tex3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GfxCitro3d.hpp"
#include "GameWindow.hpp"
#include "Supervisor.hpp"
#include "utils.hpp"
#include <vector>
#include <memory>
#include <SDL2/SDL.h>
#include "Controller.hpp"
#include "AnmManager.hpp"
#include <algorithm>
#include "Supervisor.hpp"

// Compiled vertex shader
#include "ff_shbin.h"

#define CLEAR_COLOR 0x68B0D8FF

// Defines a fixed sized vertex buffer. Surely 20000 verticies will be enough :) This is 5.7MB of ram, should probably make this a dynamically allocated array probably
#define MAX_VERTICES 20000

static void* vbo_data;
static PrintConsole bottomScreen;

#define POSITION_ATTRIBUTE_INDEX 0
#define TEX_CORDS_ATTRIBUTE_INDEX 1
#define DIFFUSE_ATTRIBUTE_INDEX 2

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

// The two antialiasing modes, XY is rendering the game at 2x resolution, and X is doubling the X resolution
#define DISPLAY_TRANSFER_FLAG_ANTI \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_XY))

#define DISPLAY_TRANSFER_FLAG_ANTI_X \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_X))

GfxInterface *GfxCitro3d::Create(){

    GfxCitro3d *interface = new GfxCitro3d();

    return interface;
}

GfxInterface *GfxCitro3d::Init(){

    GfxCitro3d *self = new GfxCitro3d;

    gfxInitDefault();

    consoleInit(GFX_BOTTOM, &bottomScreen);
    consoleSelect(&bottomScreen);

    C3D_Init(0x100000);

    self->leftTarget = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    self->rightTarget = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);

    C3D_RenderTargetSetOutput(self->leftTarget,GFX_TOP,GFX_LEFT,DISPLAY_TRANSFER_FLAGS);
    C3D_RenderTargetSetOutput(self->rightTarget,GFX_TOP,GFX_RIGHT,DISPLAY_TRANSFER_FLAGS);   

    gfxSet3D(false);

    // Create the vertex shader
    self->shader_dvlb = DVLB_ParseFile((u32*)ff_shbin, ff_shbin_size);
    shaderProgramInit(&self->program);
    shaderProgramSetVsh(&self->program, &self->shader_dvlb->DVLE[0]);
    C3D_BindProgram(&self->program);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    // Get location of uniforms
    self->uLoc_projection = shaderInstanceGetUniformLocation(self->program.vertexShader, "projection");
    self->uLoc_modelView = shaderInstanceGetUniformLocation(self->program.vertexShader, "modelView");
    self->uLoc_textureMatrix = shaderInstanceGetUniformLocation(self->program.vertexShader, "textureMatrix");

    Mtx_Identity(&self->modelMatrix);
    Mtx_Identity(&self->viewMatrix);
    Mtx_Identity(&self->projectionMatrix);
    Mtx_Identity(&self->textureMatrixMatrix);
    Mtx_Identity(&self->modelViewMatrix);

    self->attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(self->attrInfo);
    AttrInfo_AddLoader(self->attrInfo, POSITION_ATTRIBUTE_INDEX, GPU_FLOAT, 3); //v0 position
    AttrInfo_AddLoader(self->attrInfo, TEX_CORDS_ATTRIBUTE_INDEX, GPU_FLOAT, 2); //v1 texCords
    AttrInfo_AddLoader(self->attrInfo, DIFFUSE_ATTRIBUTE_INDEX, GPU_UNSIGNED_BYTE, 4); // diffuse

	self->env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(self->env);

    // OpenGL GL_TEXTURE_ENV_MODE = GL_COMBINE
    // Explicitly select the equivalent of source 0.
    GPU_TEVSRC source1;

    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
        source1 = GPU_CONSTANT;
    else
        source1 = GPU_PRIMARY_COLOR;

    // GL_SRC0_ALPHA = texture alpha
    // GL_SRC1_ALPHA = constant or primary color alpha
    C3D_TexEnvSrc(
        self->env,
        C3D_Alpha,
        GPU_TEXTURE0,
        source1,
        GPU_PRIMARY_COLOR
    );

    // GL_OPERAND0_ALPHA = GL_SRC_ALPHA
    // GL_OPERAND1_ALPHA = GL_SRC_ALPHA
    C3D_TexEnvOpAlpha(
        self->env,
        GPU_TEVOP_A_SRC_ALPHA,
        GPU_TEVOP_A_SRC_ALPHA,
        GPU_TEVOP_A_SRC_ALPHA
    );

    u8 alpha_test_ref = 4;
    C3D_AlphaTest(true, GPU_GEQUAL, alpha_test_ref);

    // GL_COMBINE_ALPHA = GL_MODULATE or GL_REPLACE
    if (((g_Supervisor.cfg.opts >> GCOS_NO_COLOR_COMP) & 1) == 0)
        C3D_TexEnvFunc(self->env, C3D_Alpha, GPU_MODULATE);
    else
        C3D_TexEnvFunc(self->env, C3D_Alpha, GPU_REPLACE);


    // GL_SRC0_RGB = texture color
    // GL_SRC1_RGB = constant or primary color
    C3D_TexEnvSrc(
        self->env,
        C3D_RGB,
        GPU_TEXTURE0,
        source1,
        GPU_PRIMARY_COLOR
    );

    // GL_OPERAND0_RGB = GL_SRC_COLOR
    // GL_OPERAND1_RGB = GL_SRC_COLOR
    C3D_TexEnvOpRgb(
        self->env,
        GPU_TEVOP_RGB_SRC_COLOR,
        GPU_TEVOP_RGB_SRC_COLOR,
        GPU_TEVOP_RGB_SRC_COLOR
    );

    // GL_COMBINE_RGB = GL_MODULATE or GL_REPLACE
    if (((g_Supervisor.cfg.opts >> GCOS_NO_COLOR_COMP) & 1) == 0)
        C3D_TexEnvFunc(self->env, C3D_RGB, GPU_MODULATE);
    else
        C3D_TexEnvFunc(self->env, C3D_RGB, GPU_REPLACE);

    C3D_CullFace(GPU_CULL_NONE);

    Mtx_Identity(&self->correctionMatrix);
    Mtx_RotateZ(
        &self->correctionMatrix,
        -M_PI / 2.0f,
        false
    );

    // Define the fixed sized vertex buffer
    self->vertexBuffer = static_cast<Vertex3DS*>(linearAlloc(MAX_VERTICES * sizeof(Vertex3DS)));

    self->vertexBufferInfo = C3D_GetBufInfo();
	BufInfo_Init(self->vertexBufferInfo);
	BufInfo_Add(self->vertexBufferInfo, self->vertexBuffer, sizeof(Vertex3DS), 3, 0x210);

    C3D_DirtyTexEnv(self->env);

    if(g_Supervisor.cfg.windowed){
        self->SetFullScreen(false);
    }else{
        self->SetFullScreen(true);
    }

    return self;
}   

void GfxCitro3d::Exit(){
    C3D_Fini();
    gfxExit();
}

void GfxCitro3d::SetFogRange(f32 nearPlane, f32 farplane){
    // FogLut_Exp(&this->fog_Lut, 
    // //     0.5f,   // Density
    // //     1.5f,   // Gradient (controls the falloff curve sharpness)
    // //     -0.00001f,   // Near distance mapping
    // //     1.0f  // Far distance mapping
    // // );
    // // C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
    // // C3D_FogLutBind(&this->fog_Lut);
}

void GfxCitro3d::SetFogColor(ZunColor color){
    //C3D_FogColor(u32 color);
    C3D_FogColor(color); 
}

void GfxCitro3d::ToggleVertexAttribute(u8 attr, bool enable){
    if (attr & VERTEX_ATTR_TEX_COORD)
        this->useTexCoord = enable;

    if (attr & VERTEX_ATTR_DIFFUSE){
        this->useDiffuse = enable;
    }

    const GPU_TEVSRC source0 =
        this->useTexCoord
            ? GPU_TEXTURE0
            : GPU_PRIMARY_COLOR;

    const GPU_TEVSRC source1 =
        this->useDiffuse
            ? GPU_PRIMARY_COLOR
            : GPU_CONSTANT;

    C3D_TexEnvSrc(
        this->env,
        C3D_Both,
        source0,
        source1,
        GPU_PRIMARY_COLOR
    );

    C3D_DirtyTexEnv(this->env);
}

void GfxCitro3d::SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr){

    switch (attr)
    {
    case VERTEX_ARRAY_POSITION:
        this->vertexData = ptr;
        this->vertexStride = stride;
        break;
    case VERTEX_ARRAY_TEX_COORD:
        this->texCoordData = ptr;
        this->texCoordStride = stride;
        break;
    case VERTEX_ARRAY_DIFFUSE:
        this->diffuseData = ptr;
        this->diffuseStride = stride;
        break;
    }
}
void GfxCitro3d::SetColorOp(TextureOpComponent component, ColorOp op){
    const GPU_COMBINEFUNC opEnums[3] {GPU_MODULATE, GPU_ADD, GPU_REPLACE};

    switch(component){
        case COMPONENT_ALPHA:
            C3D_TexEnvFunc(this->env, C3D_Alpha, opEnums[op]);
        break;
        default:
            C3D_TexEnvFunc(this->env, C3D_RGB, opEnums[op]);
        break;
    }
    C3D_DirtyTexEnv(this->env);
}
void GfxCitro3d::SetTextureFactor(ZunColor factor){
    // The original code has this bit shifting code so i'll keep it in, I'm not exactly sure why the factor must be bitshifted.
    u8 tfactorColor[4] = {((factor >> 16) & 0xFF), ((factor >> 8) & 0xFF),(factor & 0xFF), ((factor >> 24) & 0xFF)};

    u32 color;
    std::memcpy(&color, tfactorColor, sizeof(color));

    C3D_TexEnvColor(this->env, color);

    // We must specify that the texture state has been modified
    C3D_DirtyTexEnv(this->env);
}

void GfxCitro3d::SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix){

    // Matricies need to be transposed since ZunMatrix and C3D_Mtx are stored differently
    switch (type)
    {
    case MATRIX_MODEL:
        for (int i = 0; i < 4; i++)
        {
            this->modelViewMatrix.r[i].x = matrix.m[0][i];
            this->modelViewMatrix.r[i].y = matrix.m[1][i];
            this->modelViewMatrix.r[i].z = matrix.m[2][i];
            this->modelViewMatrix.r[i].w = matrix.m[3][i];
        }
        break;
    case MATRIX_VIEW:
        for (int i = 0; i < 4; i++)
        {
            this->modelViewMatrix.r[i].x = matrix.m[0][i];
            this->modelViewMatrix.r[i].y = matrix.m[1][i];
            this->modelViewMatrix.r[i].z = matrix.m[2][i];
            this->modelViewMatrix.r[i].w = matrix.m[3][i];
        }
        break;
    case MATRIX_PROJECTION:
        for (int i = 0; i < 4; i++)
        {
            // Using precalculated projection matrix
            this->projectionMatrix.r[i].x = matrix.m[0][i];
            this->projectionMatrix.r[i].y = matrix.m[1][i];
            this->projectionMatrix.r[i].z = matrix.m[2][i];
            this->projectionMatrix.r[i].w = matrix.m[3][i];
        }

        //Rotate the projection by 90 degrees
        Mtx_Multiply(
            &this->correctedMatrix,
            &this->correctionMatrix,
            &this->projectionMatrix
        );
        break;
    case MATRIX_TEXTURE:
        for (int i = 0; i < 4; i++)
        {
            this->textureMatrixMatrix.r[i].x = matrix.m[0][i];
            this->textureMatrixMatrix.r[i].y = matrix.m[1][i];
            this->textureMatrixMatrix.r[i].z = matrix.m[2][i];
            this->textureMatrixMatrix.r[i].w = matrix.m[3][i];
        }
        break;
    case MATRIX_PROJECTION_LEFT:
        for (int i = 0; i < 4; i++)
        {
            // Using precalculated projection matrix
            this->projectionMatrixLeft.r[i].x = matrix.m[0][i];
            this->projectionMatrixLeft.r[i].y = matrix.m[1][i];
            this->projectionMatrixLeft.r[i].z = matrix.m[2][i];
            this->projectionMatrixLeft.r[i].w = matrix.m[3][i];

            //printf("(%f,%f,%f,%f)\n", this->projectionMatrixLeft.r[i].x, this->projectionMatrixLeft.r[i].y, this->projectionMatrixLeft.r[i].z, this->projectionMatrixLeft.r[i].w); 
        }

        //Rotate the projection by 90 degrees
        Mtx_Multiply(
            &this->correctedMatrixLeft,
            &this->correctionMatrix,
            &this->projectionMatrixLeft
        );
        break;
    case MATRIX_PROJECTION_RIGHT:
        for (int i = 0; i < 4; i++)
        {
            // Using precalculated projection matrix
            this->projectionMatrixRight.r[i].x = matrix.m[0][i];
            this->projectionMatrixRight.r[i].y = matrix.m[1][i];
            this->projectionMatrixRight.r[i].z = matrix.m[2][i];
            this->projectionMatrixRight.r[i].w = matrix.m[3][i];
        }

        //Rotate the projection by 90 degrees
        Mtx_Multiply(
            &this->correctedMatrixRight,
            &this->correctionMatrix,
            &this->projectionMatrixRight
        );
        break;
    }
}

void GfxCitro3d::SetTextureFilter(){
    //C3D_TexSetFilter(&this->boundTexture3ds->texObject, GPU_LINEAR, GPU_LINEAR);
}

void GfxCitro3d::GetViewport(u32 *viewport){
    for (int i = 0; i < 4; i++)
    {
        viewport[i] = this->viewport3ds[i];
    }
}

void GfxCitro3d::GetDepthRange(f32 *depthRange){
    depthRange[0] = this->depthNear3ds;
    depthRange[1] = this->depthFar3ds;
}

void GfxCitro3d::SetFullScreen(bool enable){
    if(enable){
        this->scalex = 0.625;
        this->scaley = 0.5f;
        this->fullscreen = true;
    }else{
        this->scalex = 0.5f;
        this->scaley = 0.5f;
        this->fullscreen = false;
        this->screenOffset = 40.0f;
    }
}


void GfxCitro3d::SetViewport(i32 x, i32 y, i32 width, i32 height){
    //utils::DebugPrint("Set Viewport");
    viewport3ds[0] = x;
    viewport3ds[1] = y;
    viewport3ds[2] = width;
    viewport3ds[3] = height;

    // const f32 scalex = 0.5f; //0.625f; (For full screen) // x scale
    // const f32 scaley = 0.5f;

    f32 screenX = 0;

    if(this->fullscreen){
        screenX = (f32)x * this->scalex;
    }else{
        screenX = this->screenOffset + (f32)x * this->scalex;
    }
    // const f32 screenX = (f32)x * this->scalex;
    const f32 screenY = (f32)y * this->scaley;
    const f32 screenW = (f32)width * this->scalex;
    const f32 screenH = (f32)height * this->scaley;

    this->targetX = screenY;
    this->targetY = this->resY - (screenX + screenW);
    this->screenH = screenH;
    this->screenW = screenW;
}

void GfxCitro3d::SetDepthRange(f32 nearPlane, f32 farPlane){
    depthNear3ds = nearPlane;
    depthFar3ds = farPlane;

    //C3D_DepthMap(true, nearPlane, farPlane);

    // This works and I'm not exactly sure why
    C3D_DepthMap(true, -1.0, 1.0);
}

void GfxCitro3d::Enable(Capabilities cap){
    
}

bool GfxCitro3d::HasError(){
    return false;
}

void GfxCitro3d::SetBlendMode(BlendMode mode){
    if (mode == BLEND_INV_SRC_ALPHA){
        C3D_AlphaBlend(
            GPU_BLEND_ADD, GPU_BLEND_ADD, 
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, 
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA
        );
    }else{
        C3D_AlphaBlend(
            GPU_BLEND_ADD, GPU_BLEND_ADD, 
            GPU_SRC_ALPHA, GPU_ONE, 
            GPU_SRC_ALPHA, GPU_ONE
        );
    }
}

void GfxCitro3d::SetDepthMask(bool enable){
    // if(enable){
    //     C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL); 
    // }else{
    //     C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_COLOR); 
    // }
}

void GfxCitro3d::SetDepthFunc(DepthFunc func){
    switch (func)
    {
    case DEPTH_FUNC_LEQUAL:
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
        break;
    case DEPTH_FUNC_ALWAYS:
        C3D_DepthTest(true, GPU_ALWAYS, GPU_WRITE_ALL);
        break;
    }
}

void GfxCitro3d::SetClearDepth(f32 depth){
    if (depth < 0.0f)
        depth = 0.0f;
    if (depth > 1.0f)
        depth = 1.0f;

    this->C3D_cleardepth = (u32)(depth * 0xFFFFFF);
}

static u8 FloatToColorByte(f32 value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return (u8)(value * 255.0f + 0.5f);
}

void GfxCitro3d::SetClearColor(f32 r, f32 g, f32 b, f32 a){
    // Not exactly sure why this exists when the game renders over the entire screen. I'm going to disable it so the edges of the screen stay black. However it can be enabled it if causes problems later on
    //this->C3D_clearcolor = C2D_Color32(FloatToColorByte(r),FloatToColorByte(g),FloatToColorByte(b),FloatToColorByte(a));
}

float GfxCitro3d::getVertexBufferUsage(){
    float usage = this->vertexCount/static_cast<float>(MAX_VERTICES);
    return usage;
}

GfxTextureHandle GfxCitro3d::CreateTexture(){
    std::unique_ptr<Texture3ds> texture = std::unique_ptr<Texture3ds>(new Texture3ds());

    u32 id;
    if (!this->freeTextures3ds.empty())
    {
        id = this->freeTextures3ds.back();
        this->freeTextures3ds.pop_back();
        this->textures3ds[id] = std::move(texture);
    }
    else
    {
        id = this->textures3ds.size();
        this->textures3ds.push_back(std::move(texture));
    }

    return {id};
}

void GfxCitro3d::BindTexture(GfxTextureHandle handle){
    //utils::DebugPrint("Binding Texture");
    if (handle >= this->textures3ds.size())
        return;
    if (!this->textures3ds[handle.id])
        return;
    this->boundTexture3ds = this->textures3ds[handle.id].get();
    C3D_TexBind(0, &this->boundTexture3ds->texObject);
}

void GfxCitro3d::SetContextFlags(){

}

void GfxCitro3d::Clear(u32 clearBits){
    C3D_ClearBits mask = (C3D_ClearBits)0;

    if (clearBits & CLEAR_COLOR_BUFFER){
        mask = (C3D_ClearBits)(mask | C3D_CLEAR_COLOR);
    }
    if (clearBits & CLEAR_DEPTH_BUFFER)
        mask = (C3D_ClearBits)(mask | C3D_CLEAR_DEPTH);
    
    if(this->use3d){
        C3D_RenderTargetClear(
            this->leftTarget,mask, this->C3D_clearcolor, this->C3D_cleardepth
        );
        C3D_RenderTargetClear(
            this->rightTarget,mask, this->C3D_clearcolor, this->C3D_cleardepth
        );
    }else{
        C3D_RenderTargetClear(
            this->leftTarget,mask, this->C3D_clearcolor, this->C3D_cleardepth
        );
    }
}

void GfxCitro3d::DeleteTexture(GfxTextureHandle handle){
    if (handle.id >= textures3ds.size())
        return;
    if (!textures3ds[handle.id])
        return;
    C3D_TexDelete(&textures3ds[handle.id]->texObject);
    textures3ds[handle.id].reset();
    freeTextures3ds.push_back(handle.id);
}

inline GPU_TEXCOLOR Get3DSPixelFormat(PixelFormat fmt, PixelDataType type)
{
    switch (type)
    {
    case PIXEL_UNSIGNED_BYTE:
        if (fmt == PIXEL_RGB){
            return GPU_RGB8;
        }else{
            return GPU_RGBA8;
        }
    case PIXEL_UNSIGNED_SHORT_4_4_4_4:
        return GPU_RGBA4;
    case PIXEL_UNSIGNED_SHORT_5_5_5_1:
        return GPU_RGBA5551;
    case PIXEL_UNSIGNED_SHORT_5_6_5:
        return GPU_RGB565;
    }
}

static std::size_t BytesPerPixel(GPU_TEXCOLOR format)
{
    switch (format)
    {
        case GPU_RGB8:
            return 3;

        case GPU_RGBA8:
            return 4;

        case GPU_RGBA4:
        case GPU_RGBA5551:
        case GPU_RGB565:
            return 2;

        default:
            return 0;
    }
}

static unsigned MortonIndex8(unsigned x, unsigned y)
{
    return ((x & 1) << 0) |
           ((y & 1) << 1) |
           ((x & 2) << 1) |
           ((y & 2) << 2) |
           ((x & 4) << 2) |
           ((y & 4) << 3);
}

std::vector<u8> SwizzleTexture(
    const void* source,
    u32 width,
    u32 height,
    GPU_TEXCOLOR format
    )
{

    u32 paddedWidth;
    u32 paddedHeight;

    const std::size_t bytesPerPixel = BytesPerPixel(format);

    if (!source || bytesPerPixel == 0 || width == 0 || height == 0)
        return {};

    // Round each dimension up to the next multiple of 8.

    paddedWidth = BitCeil(width);
    paddedHeight = BitCeil(height);

    //Not correctly calculating the padding values
    // paddedWidth  = (width  + 7) & ~7u;
    // paddedHeight = (height + 7) & ~7u;
    const u8* input = static_cast<const u8*>(source);

    std::vector<u8> output(
        static_cast<std::size_t>(paddedWidth) *
        paddedHeight *
        bytesPerPixel,
        0
    );

    const u32 tilesAcross = paddedWidth / 8;

    for (u32 y = 0; y < height; ++y)
    {
        for (u32 x = 0; x < width; ++x)
        {
            const u32 tileX = x / 8;
            const u32 tileY = y / 8;

            const u32 localX = x % 8;
            const u32 localY = y % 8;

            const u32 tileIndex =
                tileY * tilesAcross + tileX;

            const u32 pixelIndex =
                tileIndex * 64 +
                MortonIndex8(localX, localY);

            const std::size_t sourceOffset =
                (static_cast<std::size_t>(y) * width + x) *
                bytesPerPixel;

            const std::size_t outputOffset =
                static_cast<std::size_t>(pixelIndex) *
                bytesPerPixel;

            // For some reason this pixel format is reversed?
            if (format == GPU_RGBA8){
                const u8* src = input + sourceOffset;
                u8* dst = output.data() + outputOffset;

                dst[0] = src[3];
                dst[1] = src[2];
                dst[2] = src[1];
                dst[3] = src[0];
            }else{
            std::memcpy(
                output.data() + outputOffset,
                input + sourceOffset,
                bytesPerPixel
            );   
            }
        }
    }

    return output;
}

void GfxCitro3d::SetTextureImage(u32 width, u32 height, PixelFormat fmt, PixelDataType type, const void *data){
    //utils::DebugPrint("Setting Texture Image");

    u32 paddedwidth;
    u32 paddedheight;

    if(!data){
        return;
    }

    if (this->boundTexture3ds)
    {
        GPU_TEXCOLOR textureFormat = Get3DSPixelFormat(fmt, type);
        const u8* pixels = static_cast<const u8*>(data);

        u32 bpp = 2;
        if (type == PIXEL_UNSIGNED_BYTE)
        {
            if (fmt == PIXEL_RGB){
                bpp = 3;
            }else{
                bpp = 4;
            }
        }

        paddedwidth = BitCeil(width);
        paddedheight = BitCeil(height);

        this->boundTexture3ds->width = width;
        this->boundTexture3ds->height = height;
        this->boundTexture3ds->format = fmt;
        this->boundTexture3ds->type = type;

        std::vector<u8> converted = SwizzleTexture(
            data,
            width,
            height,
            textureFormat
        );

        C3D_TexDelete(&this->boundTexture3ds->texObject);

        C3D_TexInit(&this->boundTexture3ds->texObject, paddedwidth, paddedheight, textureFormat);
        C3D_TexSetFilter(&this->boundTexture3ds->texObject, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&this->boundTexture3ds->texObject, GPU_REPEAT, GPU_REPEAT);

        C3D_TexUpload(&this->boundTexture3ds->texObject, converted.data());
        C3D_TexFlush(&this->boundTexture3ds->texObject);
    }
}

void GfxCitro3d::SetTextureSubImage(i32 xoffset, i32 yoffset, i32 width, i32 height, const void *data){
    // Rerouted all calls to this function to SetTextureImage
}

// Ya this is going to be a nightmare to implement :(
void GfxCitro3d::ReadPixels(i32 x, i32 y, i32 width, i32 height, const void *pixels){
    // u16 width_gfx = (u16)(width) * this->scalex;
    // u16 height_gfx = (u16)(height) * this->scaley;

    // u8 *dst = (u8 *)pixels;

    // u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width_gfx, &height_gfx);

    // i32 pitch = width*4;

    // for (i32 row = 0; row < height; row++){
    //     const u8 *src = (u8 *)fb + ((GAME_WINDOW_HEIGHT - 1 - (y+row)) * GAME_WINDOW_WIDTH + x)*4;
    //     memcpy(dst + row * pitch, src, pitch);
    // }
}

void GfxCitro3d::SwapBuffers(){
    this->first_draw = true;
    this->vertexWriteOffset = 0;
    C3D_FrameEnd(0);
}

void GfxCitro3d::Enable3d(bool enable, i32 strength){

    if(enable == this->use3d){
        return;
    }

    if(enable){
        gfxSet3D(true);
        this->use3d = true;    
    }else{
        gfxSet3D(false);
        this->use3d = false;
    }
}

void GfxCitro3d::Toggle3d(){
    if(this->use3d){
        this->Enable3d(true, 0);
    }else{
        this->Enable3d(false, 0);
    }
}

void GfxCitro3d::Draw(PrimitiveType type, i32 start, i32 count)
{

    GPU_Primitive_t C3DPrim;

    switch(type)
    {
    case PRIM_TRIANGLES:
        C3DPrim = GPU_TRIANGLES;
        break;
    case PRIM_TRIANGLE_STRIP:
        C3DPrim = GPU_TRIANGLE_STRIP;
        break;
    }

    if(this->first_draw){

        float slider = osGet3DSliderState();

        if(slider > 0.0f){
            this->Enable3d(true, 0);
        }else{
            this->Enable3d(false, 0);
        }
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        this->first_draw = false;
        this->vertexWriteOffset = 0;
        this->vertexCount = 0;
    }

    u32 firstVertex = this->vertexWriteOffset;

    const u8* base_pos = static_cast<const u8*>(this->vertexData);
    const u8* base_tex = static_cast<const u8*>(this->texCoordData);
    const u8* base_diffuse = static_cast<const u8*>(this->diffuseData);

    for(int i = 0; i < count; i++){

        const u8* vertex_pos = base_pos + i * this->vertexStride;

        std::memcpy(&this->vertexBuffer[firstVertex+i].x, vertex_pos + 0,  sizeof(f32));
        std::memcpy(&this->vertexBuffer[firstVertex+i].y, vertex_pos + 4,  sizeof(f32));
        std::memcpy(&this->vertexBuffer[firstVertex+i].z, vertex_pos + 8,  sizeof(f32));

        if(this->useTexCoord){
            const u8* vertex_tex = base_tex + i * this->texCoordStride;
            std::memcpy(&this->vertexBuffer[firstVertex+i].u, vertex_tex + 0, sizeof(f32));
            std::memcpy(&this->vertexBuffer[firstVertex+i].v, vertex_tex + 4, sizeof(f32));
        }else{
            this->vertexBuffer[firstVertex+i].u = 0;
            this->vertexBuffer[firstVertex+i].v = 0;
        }

        if(this->useDiffuse){
            const u8* vertex_diffuse = base_diffuse + i * this->diffuseStride;
            std::memcpy(&this->vertexBuffer[firstVertex+i].r, vertex_diffuse + 0,  sizeof(u8));
            std::memcpy(&this->vertexBuffer[firstVertex+i].g, vertex_diffuse + 1,  sizeof(u8));
            std::memcpy(&this->vertexBuffer[firstVertex+i].b, vertex_diffuse + 2,  sizeof(u8));
            std::memcpy(&this->vertexBuffer[firstVertex+i].a, vertex_diffuse + 3,  sizeof(u8));
        }else{
            this->vertexBuffer[firstVertex+i].r = 0.0f;
            this->vertexBuffer[firstVertex+i].g = 0.0f;
            this->vertexBuffer[firstVertex+i].b = 0.0f;
            this->vertexBuffer[firstVertex+i].a = 0.0f;               
        }
    }

    this->vertexWriteOffset += count;
    this->vertexCount += count;

    C3D_FrameDrawOn(this->leftTarget);
    C3D_SetViewport(this->targetX,this->targetY,this->screenH,this->screenW);

    if(this->use3d){
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_projection,&this->correctedMatrixLeft);
    }else{
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_projection,&this->correctedMatrix);
    }
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_modelView,&this->modelViewMatrix);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_textureMatrix,&this->textureMatrixMatrix);
    C3D_DrawArrays(C3DPrim, firstVertex, count); 

    //If 3D is set then also render to the right eye
    if(this->use3d){
        C3D_FrameDrawOn(this->rightTarget);
        C3D_SetViewport(this->targetX,this->targetY,this->screenH,this->screenW);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_projection,&this->correctedMatrixRight);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_modelView,&this->modelViewMatrix);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_textureMatrix,&this->textureMatrixMatrix);
        C3D_DrawArrays(C3DPrim, firstVertex, count); 
    }
}