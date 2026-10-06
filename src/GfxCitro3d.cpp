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

// Defines a fixed sized vertex buffer. Surely 20000 verticies will be enough :)
#define MAX_VERTICES 20000

static void* vbo_data;
static PrintConsole bottomScreen;

static C3D_FogLut fog_Lut;

#define POSITION_ATTRIBUTE_INDEX 0
#define TEX_CORDS_ATTRIBUTE_INDEX 1
#define DIFFUSE_ATTRIBUTE_INDEX 2

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

GfxInterface *GfxCitro3d::Create(){

    GfxCitro3d *interface = new GfxCitro3d();

    return interface;
}

GfxInterface *GfxCitro3d::Init(){

    GfxCitro3d *self = new GfxCitro3d;

    gfxInitDefault();

    // Increased the CMD buffer size
    C3D_Init(0x100000);

    self->target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    //C3D_SetViewport(0,0,400,240);
    C3D_RenderTargetSetOutput(self->target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    consoleInit(GFX_BOTTOM, &bottomScreen);
    consoleSelect(&bottomScreen);

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

    C3D_TexEnvColor(self->env, self->C3D_clearcolor);

    C3D_CullFace(GPU_CULL_NONE);

    // Mtx_OrthoTilt(
    // &self->projectionMatrix,
    // 0.0f,    // left
    // 640.0f,  // right
    // 480.0f,    // bottom
    // 0.0f,  // top
    // 0.0f,    // near
    // 1.0f,    // far
    // true     // account for 3DS screen orientation
    // );


    Mtx_Identity(&self->correctionMatrix);
    Mtx_RotateZ(
        &self->correctionMatrix,
        -M_PI / 2.0f,
        false
    );

    //Mtx_Identity(&self->correctionMatrix);

    // FogLut_Exp(&fog_Lut, 0.05f, 1.5f, 0.01f, 20.0f);
	// C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
	// C3D_FogColor(0xD8B068);
	// C3D_FogLutBind(&fog_Lut);

    self->vertexBuffer = static_cast<Vertex3DS*>(linearAlloc(MAX_VERTICES * sizeof(Vertex3DS)));

    self->vertexBufferInfo = C3D_GetBufInfo();
	BufInfo_Init(self->vertexBufferInfo);
	BufInfo_Add(self->vertexBufferInfo , self->vertexBuffer, sizeof(Vertex3DS), 3, 0x210);

    return self;
}   

void GfxCitro3d::Exit(){
    C3D_Fini();
    gfxExit();
}

void GfxCitro3d::SetFogRange(f32 nearPlane, f32 farplane){
    // Implement later
}

void GfxCitro3d::SetFogColor(ZunColor color){
    //C3D_FogColor(u32 color);
}

void GfxCitro3d::ToggleVertexAttribute(u8 attr, bool enable){
    if (attr & VERTEX_ATTR_TEX_COORD){
        GPU_TEVSRC source0 = enable ? GPU_TEXTURE0 : GPU_PRIMARY_COLOR;

        GPU_TEVSRC source1 = (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0) ? GPU_CONSTANT: GPU_PRIMARY_COLOR;

        C3D_TexEnvSrc(this->env, C3D_Both, source0, source1, GPU_PRIMARY_COLOR);

        useTexCoord = enable;
    }
    if (attr & VERTEX_ATTR_DIFFUSE){
        useTexCoord = !enable;
    }
}

void GfxCitro3d::SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr){
    switch (attr)
    {
    case VERTEX_ARRAY_POSITION:
        //utils::DebugPrint("Setting Attribute Position");
        this->vertexData = ptr;
        this->vertexStride = stride;
        break;
    case VERTEX_ARRAY_TEX_COORD:
        //utils::DebugPrint("Setting Attribute Tex CORD");
        this->texCoordData = ptr;
        this->texCoordStride = stride;
        break;
    case VERTEX_ARRAY_DIFFUSE:
        //utils::DebugPrint("Setting Attribute Diffuse");
        this->diffuseData = ptr;
        this->diffuseStride = stride;
        break;
    }
}
void GfxCitro3d::SetColorOp(TextureOpComponent component, ColorOp op){
    const GPU_COMBINEFUNC opEnums[3] {GPU_MODULATE, GPU_ADD, GPU_REPLACE};

    if (component > COMPONENT_ALPHA || op > COLOR_OP_REPLACE){
        return;
    }

    switch(component){
        case COMPONENT_ALPHA:
        C3D_TexEnvFunc(this->env, C3D_Alpha, opEnums[op]);
        break;
        default:
        C3D_TexEnvFunc(this->env, C3D_RGB, opEnums[op]);
        break;
    }
}
void GfxCitro3d::SetTextureFactor(ZunColor){
    
}

void GfxCitro3d::SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix){

    // Matricies need to be transposed
    switch (type)
    {
    case MATRIX_MODEL:
        for (int i = 0; i < 4; i++)
        {
            this->modelViewMatrix.r[i].x = matrix.m[0][i];
            this->modelViewMatrix.r[i].y = matrix.m[1][i];
            this->modelViewMatrix.r[i].z = matrix.m[2][i];
            this->modelViewMatrix.r[i].w = matrix.m[3][i];
            // this->modelViewMatrix.r[i].x = matrix.m[i][0];
            // this->modelViewMatrix.r[i].y = matrix.m[i][1];
            // this->modelViewMatrix.r[i].z = matrix.m[i][2];
            // this->modelViewMatrix.r[i].w = matrix.m[i][3];
        }
        break;
    case MATRIX_VIEW:
        for (int i = 0; i < 4; i++)
        {
            this->modelViewMatrix.r[i].x = matrix.m[0][i];
            this->modelViewMatrix.r[i].y = matrix.m[1][i];
            this->modelViewMatrix.r[i].z = matrix.m[2][i];
            this->modelViewMatrix.r[i].w = matrix.m[3][i];
            // this->modelViewMatrix.r[i].x = matrix.m[i][0];
            // this->modelViewMatrix.r[i].y = matrix.m[i][1];
            // this->modelViewMatrix.r[i].z = matrix.m[i][2];
            // this->modelViewMatrix.r[i].w = matrix.m[i][3];
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
            // this->projectionMatrix.r[i].x = matrix.m[i][0];
            // this->projectionMatrix.r[i].y = matrix.m[i][1];
            // this->projectionMatrix.r[i].z = matrix.m[i][2];
            // this->projectionMatrix.r[i].w = matrix.m[i][3];
        }

        //Rotate the projectino by 90 degrees
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

            // this->textureMatrixMatrix.r[i].x = matrix.m[i][0];
            // this->textureMatrixMatrix.r[i].y = matrix.m[i][1];
            // this->textureMatrixMatrix.r[i].z = matrix.m[i][2];
            // this->textureMatrixMatrix.r[i].w = matrix.m[i][3];
        }
        break;
    }
}

void GfxCitro3d::SetTextureFilter(){
    C3D_TexSetFilter(&this->boundTexture3ds->texObject, GPU_LINEAR, GPU_LINEAR);
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

void GfxCitro3d::ToggleFullScreen(){
    if(!this->fullscreen){
        this->scalex = 0.625;
        this->scaley = 0.5f;
        this->fullscreen = true;
    }else{
        this->scalex = 0.5f;
        this->scaley = 0.5f;
        this->fullscreen = false;
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
        screenX = 40.0f + (f32)x * this->scalex;
    }
    // const f32 screenX = (f32)x * this->scalex;
    const f32 screenY = (f32)y * this->scaley;
    const f32 screenW = (f32)width * this->scalex;
    const f32 screenH = (f32)height * this->scaley;

    this->targetX = screenY;
    this->targetY = 400.0f - (screenX + screenW);
    this->screenH = screenH;
    this->screenW = screenW;

    // const f32 offsetX = (400.0f - (640.0f * scalex)) * 0.5f;
    
    // const f32 vx = (offsetX + ((f32)x*scalex));
    // const f32 vy = 240.0f - ((f32)(y + height) * scalex);
    // const f32 vw = (f32)width*scalex;
    // const f32 vh = (f32)height*scaley;

    // printf("(%i,%i,%i,%i)\n",x,y,width,height);
    // printf("(%f,%f,%f,%f)\n",vy,vx,vh,vw);
    // C3D_SetViewport(vy,vx,vh,vw);

    // C3D_SetViewport(
    //     0,      // x
    //     40,     // padding corresponding to physical side bars
    //     240,    // logical target width
    //     320     // logical target height
    // );
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
    return static_cast<u8>(value * 255.0f + 0.5f);
}

void GfxCitro3d::SetClearColor(f32 r, f32 g, f32 b, f32 a){

    this->C3D_clearcolor = C2D_Color32(FloatToColorByte(r),FloatToColorByte(g),FloatToColorByte(b),FloatToColorByte(a));
    C3D_TexEnvColor(this->env, this->C3D_clearcolor);
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

    C3D_TexInit(&texture->texObject, 1, 1, GPU_RGB8);

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
    
    C3D_RenderTargetClear(
        this->target,
        mask,
        this->C3D_clearcolor,
        this->C3D_cleardepth
    );
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

inline SDL_PixelFormatEnum GetSDLPixelFormat(PixelFormat fmt, PixelDataType type)
{
    switch (type)
    {
    case PIXEL_UNSIGNED_BYTE:
        if (fmt == PIXEL_RGB){
            printf("SDL RGB32");
            return SDL_PIXELFORMAT_RGB24;
        }else{
            printf("SDL ARGB32");
            return SDL_PIXELFORMAT_RGBA32;
        }
    case PIXEL_UNSIGNED_SHORT_4_4_4_4:
        printf("SDL ARGB24");
        return SDL_PIXELFORMAT_RGBA4444;
    case PIXEL_UNSIGNED_SHORT_5_5_5_1:
        printf("SDL RGBA5551");
        return SDL_PIXELFORMAT_RGBA5551;
    case PIXEL_UNSIGNED_SHORT_5_6_5:
        printf("SDL RGB565");
        return SDL_PIXELFORMAT_RGB565;
    }
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

            // std::memcpy(
            //     output.data() + outputOffset,
            //     input + sourceOffset,
            //     bytesPerPixel
            // );
        }
    }

    return output;
}
// u8* ReverseTextureRBValues(const u8* data, u32 width, u32 height){
    
//     u32 pixelcount = width*height*3;

//     u8* output = new u8[pixelcount];
    
//     for(u32 i = 0; i < pixelcount; i++){
//         const u32 offset = i*3;

//         output[offset + 0] = data[offset + 2];
//         output[offset + 1] = data[offset + 1];
//         output[offset + 2] = data[offset + 0];
//     }
    
//     return output;
// }

void CopyTextureData(Texture3ds& texture, const void* data, u32 width, u32 height, GPU_TEXCOLOR format, u32 bpp){
    std::size_t size = static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*static_cast<std::size_t>(bpp);
    const u8* pixels = static_cast<const u8*>(data);

    texture.data.assign(pixels, pixels + size);
}

u32 NextPowerOfTwo(u32 value)
{
    if (value <= 8)
        return 8;

    --value;

    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;

    return value + 1;
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

        // Okay seems the data needs to be formatted differently for the GPU :) sry 3ds you're gonna have to do this in realtime
        C3D_TexInit(&this->boundTexture3ds->texObject, paddedwidth, paddedheight, textureFormat);
        //C3D_TexUpload(&this->boundTexture3ds->texObject, this->boundTexture3ds->data.data());

        C3D_TexUpload(&this->boundTexture3ds->texObject, converted.data());
        C3D_TexFlush(&this->boundTexture3ds->texObject);

    }
}

void GfxCitro3d::SetTextureSubImage(i32 xoffset, i32 yoffset, i32 width, i32 height, const void *data){
    //utils::DebugPrint("Setting Texture SubImage");
    // if (this->boundTexture3ds)
    // {
    //     SDL_ConvertPixels(width, height, SDL_PIXELFORMAT_RGB24, data, width * 3, SDL_PIXELFORMAT_ARGB8888,
    //                       boundTexture3ds->texels.data() + (yoffset * boundTexture3ds->width) + xoffset,
    //                       boundTexture3ds->width * sizeof(u32));

    //     //C3D_TexUpload(&this->boundTexture3ds->texObject,  data);
    // }

}

void GfxCitro3d::ReadPixels(i32 x, i32 y, i32 width, i32 height, const void *pixels){
    u16 width_gfx = static_cast<u16>(width);
    u16 height_gfx = static_cast<u16>(height);

    u8 *dst = (u8 *)pixels;

    u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width_gfx, &height_gfx);

    i32 pitch = width*4;

    for (i32 row = 0; row < height; row++){
        const u8 *src = (u8 *)fb + ((GAME_WINDOW_HEIGHT - 1 - (y+row)) * GAME_WINDOW_WIDTH + x)*4;
        memcpy(dst + row * pitch, src, pitch);
    }

}

void GfxCitro3d::SwapBuffers(){
    this->first_draw = true;
    this->vertexWriteOffset = 0;
    C3D_FrameEnd(0);
}

void GfxCitro3d::SetRhw(bool enable){
    this->useRhw = enable;
}

void printMatrix(const C3D_Mtx* mtx) {
    for (int i = 0; i < 4; i++) {
        printf("[ %0.1f  %0.1f  %0.1f  %0.1f ]\n", 
               mtx->r[i].x, 
               mtx->r[i].y, 
               mtx->r[i].z, 
               mtx->r[i].w);
    }
    printf("\n");
}

void GfxCitro3d::Draw(PrimitiveType type, i32 start, i32 count)
{

    GPU_Primitive_t C3DPrim;

    //const VertexDiffuseXyzrhw* vertexdiffuseXyzrhw = nullptr; // This type never actually gets sent think?
    const VertexTex1Xyzrhw* triangleVertices = nullptr; 
    //const VertexTex1DiffuseXyzrhw* stripVerticesrhw = nullptr;
    const VertexTex1DiffuseXyz* stripVertices = nullptr;

    //vertexdiffuseXyzrhw = static_cast<const VertexDiffuseXyzrhw*>(vertexData);
    triangleVertices = static_cast<const VertexTex1Xyzrhw*>(vertexData);
    //stripVerticesrhw = static_cast<const VertexTex1DiffuseXyzrhw*>(vertexData);
    stripVertices = static_cast<const VertexTex1DiffuseXyz*>(vertexData);

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
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_FrameDrawOn(this->target);
        this->first_draw = false;
        this->vertexWriteOffset = 0;
    }

    C3D_SetViewport(this->targetX,this->targetY,this->screenH,this->screenW);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_projection,&this->correctedMatrix);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_modelView,&this->modelViewMatrix);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_textureMatrix,&this->textureMatrixMatrix);

    u32 firstVertex = this->vertexWriteOffset;

    switch(C3DPrim)
    {
    case GPU_TRIANGLE_STRIP:
        if(!this->useRhw & this->useTexCoord){
            for (int i = 0; i < count; i++)
            {
                this->vertexBuffer[firstVertex+i].x = stripVertices[i].position.x;
                this->vertexBuffer[firstVertex+i].y = stripVertices[i].position.y;
                this->vertexBuffer[firstVertex+i].z = stripVertices[i].position.z;

                this->vertexBuffer[firstVertex+i].u = stripVertices[i].textureUV.x;
                this->vertexBuffer[firstVertex+i].v = 1.0f-stripVertices[i].textureUV.y;

                this->vertexBuffer[firstVertex+i].r = 255;
                this->vertexBuffer[firstVertex+i].g = 255;
                this->vertexBuffer[firstVertex+i].b = 255;
                this->vertexBuffer[firstVertex+i].a = 255;
            }  
            this->vertexWriteOffset += count;
            C3D_DrawArrays(C3DPrim, firstVertex, count); 
        }
        // }else if(!this->useRhw & !this->useTexCoord){
        //     for (int i = 0; i < count; i++)
        //     {
        //         this->vertexBuffer[firstVertex+i].x = stripVertices[i].position.x;
        //         this->vertexBuffer[firstVertex+i].y = stripVertices[i].position.y;
        //         this->vertexBuffer[firstVertex+i].z = stripVertices[i].position.z;

        //         this->vertexBuffer[firstVertex+i].u = 0.0f;
        //         this->vertexBuffer[firstVertex+i].v = 0.0f;

        //         this->vertexBuffer[firstVertex+i].r = stripVertices[i].diffuse.r;
        //         this->vertexBuffer[firstVertex+i].g = stripVertices[i].diffuse.g;
        //         this->vertexBuffer[firstVertex+i].b = stripVertices[i].diffuse.b;
        //         this->vertexBuffer[firstVertex+i].a = stripVertices[i].diffuse.a;
        //     }  
        //     this->vertexWriteOffset += count;
        //     C3D_DrawArrays(C3DPrim, firstVertex, count); 
        // }
        break;
    case GPU_TRIANGLES:
        for (int i = 0; i < count; i++)
        {
            this->vertexBuffer[firstVertex+i].x = triangleVertices[i].position.x;
            this->vertexBuffer[firstVertex+i].y = triangleVertices[i].position.y;
            this->vertexBuffer[firstVertex+i].z = triangleVertices[i].position.z;

            this->vertexBuffer[firstVertex+i].u = triangleVertices[i].textureUV.x;
            this->vertexBuffer[firstVertex+i].v = 1.0f-triangleVertices[i].textureUV.y;

            this->vertexBuffer[firstVertex+i].r = 255;
            this->vertexBuffer[firstVertex+i].g = 255;
            this->vertexBuffer[firstVertex+i].b = 255;
            this->vertexBuffer[firstVertex+i].a = 255;
        }
        this->vertexWriteOffset += count;
        C3D_DrawArrays(C3DPrim, firstVertex, count);
        break;
    }
}

