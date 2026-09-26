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

static void* vbo_data;
static PrintConsole bottomScreen;

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

    //gfxInitDefault();
    gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, false);
    // Increased the CMD buffer size
    C3D_Init(0x100000);

    self->target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_SetViewport(0,0,400,240);
    C3D_RenderTargetSetOutput(self->target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    consoleInit(GFX_BOTTOM, &bottomScreen);
    consoleSelect(&bottomScreen);

    printf("gfxCitro3d Initialized");

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

    Mtx_OrthoTilt(
    &self->projectionMatrix,
    0.0f,    // left
    640.0f,  // right
    480.0f,    // bottom
    0.0f,  // top
    0.0f,    // near
    1.0f,    // far
    true     // account for 3DS screen orientation
    );

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
    // Implement later
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
    // case MATRIX_MODEL:
    //     for (int i = 0; i < 4; i++)
    //     {
    //         this->modelViewMatrix.r[i].x = matrix.m[0][i];
    //         this->modelViewMatrix.r[i].y = matrix.m[1][i];
    //         this->modelViewMatrix.r[i].z = matrix.m[2][i];
    //         this->modelViewMatrix.r[i].w = matrix.m[3][i];
    //     }
    //     break;
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
            // this->projectionMatrix.r[i].x = matrix.m[0][i];
            // this->projectionMatrix.r[i].y = matrix.m[1][i];
            // this->projectionMatrix.r[i].z = matrix.m[2][i];
            // this->projectionMatrix.r[i].w = matrix.m[3][i];
            // this->projectionMatrix.r[i].x = matrix.m[i][0];
            // this->projectionMatrix.r[i].y = matrix.m[i][1];
            // this->projectionMatrix.r[i].z = matrix.m[i][2];
            // this->projectionMatrix.r[i].w = matrix.m[i][3];
        }
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

void GfxCitro3d::SetViewport(i32 x, i32 y, i32 width, i32 height){
    //utils::DebugPrint("Set Viewport");
    viewport3ds[0] = x;
    viewport3ds[1] = y;
    viewport3ds[2] = width;
    viewport3ds[3] = height;

    // Viewport should never have to be changed?
    //C3D_SetViewport(x, y, width, height);
}

void GfxCitro3d::SetDepthRange(f32 nearPlane, f32 farPlane){
    depthNear3ds = nearPlane;
    depthFar3ds = farPlane;
}

void GfxCitro3d::Enable(Capabilities cap){
    
}

bool GfxCitro3d::HasError(){
    return false;
}

void GfxCitro3d::SetBlendMode(BlendMode mode){
    
}

void GfxCitro3d::SetDepthMask(bool enable){
    
}

void GfxCitro3d::SetDepthFunc(DepthFunc func){
    
}

void GfxCitro3d::SetClearDepth(f32 depth){
    if (depth < 0.0f)
        depth = 0.0f;
    if (depth > 1.0f)
        depth = 1.0f;

    this->C3D_cleardepth = (u32)(depth * 0xFFFFFFu + 0.5f);
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
        if (fmt == PIXEL_RGB)
            return SDL_PIXELFORMAT_RGB24;
        else
            return SDL_PIXELFORMAT_RGBA32;
    case PIXEL_UNSIGNED_SHORT_4_4_4_4:
        return SDL_PIXELFORMAT_RGBA4444;
    case PIXEL_UNSIGNED_SHORT_5_5_5_1:
        return SDL_PIXELFORMAT_RGBA5551;
    case PIXEL_UNSIGNED_SHORT_5_6_5:
        return SDL_PIXELFORMAT_RGB565;
    }
}



inline GPU_TEXCOLOR Get3DSPixelFormat(PixelFormat fmt, PixelDataType type)
{
    switch (type)
    {
    case PIXEL_UNSIGNED_BYTE:
        if (fmt == PIXEL_RGB)
            return GPU_RGB8;
        else
            return GPU_RGBA8;
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

// std::vector<u8> SwizzleTexture(
//     const void* source,
//     u32 width,
//     u32 height,
//     GPU_TEXCOLOR format)
// {
//     const std::size_t bytesPerPixel = BytesPerPixel(format);

//     if (!source || bytesPerPixel == 0)
//         return {};

//     //PICA textures must be arranged in complete 8x8 tiles.
//     if ((width % 8) != 0 || (height % 8) != 0){
//         return {};
//     }

//     const u8* input = static_cast<const u8*>(source);

//     std::vector<u8> output(
//         static_cast<std::size_t>(width) *
//         height *
//         bytesPerPixel
//     );

//     const u32 tilesAcross = width / 8;

//     for (u32 y = 0; y < height; ++y)
//     {
//         for (u32 x = 0; x < width; ++x)
//         {
//             const u32 tileX = x / 8;
//             const u32 tileY = y / 8;

//             const u32 localX = x % 8;
//             const u32 localY = y % 8;

//             const u32 tileIndex =
//                 tileY * tilesAcross + tileX;

//             const u32 pixelIndex =
//                 tileIndex * 64 +
//                 MortonIndex8(localX, localY);

//             const std::size_t sourceOffset =
//                 (static_cast<std::size_t>(y) * width + x) *
//                 bytesPerPixel;

//             const std::size_t outputOffset =
//                 static_cast<std::size_t>(pixelIndex) *
//                 bytesPerPixel;

//             std::memcpy(
//                 output.data() + outputOffset,
//                 input + sourceOffset,
//                 bytesPerPixel
//             );
//         }
//     }

//     return output;
// }

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


    // This is not correctly padding :(
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

            std::memcpy(
                output.data() + outputOffset,
                input + sourceOffset,
                bytesPerPixel
            );
        }
    }

    return output;
}
u8* ReverseTextureRBValues(const u8* data, u32 width, u32 height){
    
    u32 pixelcount = width*height*3;

    u8* output = new u8[pixelcount];
    
    for(u32 i = 0; i < pixelcount; i++){
        const u32 offset = i*3;

        output[offset + 0] = data[offset + 2];
        output[offset + 1] = data[offset + 1];
        output[offset + 2] = data[offset + 0];
    }
    
    return output;
}

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
    C3D_FrameEnd(0);
}

static bool ValidFloat(float value)
{
    return std::isfinite(value);
}

void GfxCitro3d::SetRhw(bool enable){
    this->useRhw = enable;
}

void GfxCitro3d::Draw(PrimitiveType type, i32 start, i32 count)
{

    GPU_Primitive_t C3DPrim;

    const VertexDiffuseXyzrhw* vertexdiffuseXyzrhw = nullptr; // This type never actually gets sent think?
    const VertexTex1Xyzrhw* triangleVertices = nullptr; 
    const VertexTex1DiffuseXyzrhw* stripVerticesrhw = nullptr;
    const VertexTex1DiffuseXyz* stripVertices = nullptr;

    vertexdiffuseXyzrhw = static_cast<const VertexDiffuseXyzrhw*>(vertexData);
    triangleVertices = static_cast<const VertexTex1Xyzrhw*>(vertexData);
    stripVerticesrhw = static_cast<const VertexTex1DiffuseXyzrhw*>(vertexData);
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
        //utils::DebugPrint("First draw");
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        //C3D_RenderTargetClear(this->target, C3D_CLEAR_ALL, this->C3D_clearcolor, 0);
        C3D_FrameDrawOn(this->target);
        this->first_draw = false;
    }

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_projection,&this->projectionMatrix);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_modelView,&this->modelViewMatrix);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_textureMatrix,&this->textureMatrixMatrix);

    //const float* vertexDataFloat = static_cast<const float*>(vertexData);

    // switch(C3DPrim){
    //     case GPU_TRIANGLES:
    //     const VertexTex1Xyzrhw* vertices = static_cast<const VertexTex1Xyzrhw*>(vertexData);
    //     break;
    //     case GPU_TRIANGLE_STRIP:
    //     const VertexTex1DiffuseXyzrhw* vertices = static_cast<const VertexTex1DiffuseXyzrhw*>(vertexData);
    //     break;
    // }
    // const VertexTex1Xyzrhw* vertices_tri = static_cast<const VertexTex1Xyzrhw*>(vertexData);
    // const VertexTex1DiffuseXyzrhw* verticies_strip = static_cast<const VertexTex1DiffuseXyzrhw*>(vertexData)

    C3D_ImmDrawBegin(C3DPrim);
    switch(C3DPrim)
    {
    case GPU_TRIANGLE_STRIP:
        if(this->useRhw && this->useTexCoord){
            for(int i = 0; i <= count; i++){
                    C3D_ImmSendAttrib(stripVerticesrhw[i].position.x,stripVerticesrhw[i].position.y,stripVerticesrhw[i].position.z,1.0f);
                    C3D_ImmSendAttrib(stripVerticesrhw[i].textureUV.x,1.0f-stripVerticesrhw[i].textureUV.y,1.0f,1.0f);
                    C3D_ImmSendAttrib(1.0f,1.0f,1.0f,1.0f);
            }
        }else if(this-useRhw && !this->useTexCoord){
            for(int i = 0; i <= count; i++){
                    // C3D_ImmSendAttrib(stripVerticesrhw[i].position.x,stripVerticesrhw[i].position.y,stripVerticesrhw[i].position.z,1.0f);
                    // C3D_ImmSendAttrib(1.0f,1.0f,1.0f,1.0f);
                    // C3D_ImmSendAttrib(stripVerticesrhw[i].diffuse.r,stripVerticesrhw[i].diffuse.g,stripVerticesrhw[i].diffuse.b,stripVerticesrhw[i].diffuse.a);
            }
        }else if(!this->useRhw && this->useTexCoord){
            for(int i = 0; i <= count; i++){
                    C3D_ImmSendAttrib(stripVertices[i].position.x,stripVertices[i].position.y,stripVertices[i].position.z,1.0f);
                    C3D_ImmSendAttrib(stripVertices[i].textureUV.x,1.0f-stripVertices[i].textureUV.y,1.0f,1.0f);
                    C3D_ImmSendAttrib(1.0f,1.0f,1.0f,1.0f);
            }
        }else if(!this->useRhw && !this->useTexCoord){
            for(int i = 0; i <= count; i++){
                    // C3D_ImmSendAttrib(stripVertices[i].position.x,stripVertices[i].position.y,stripVertices[i].position.z,1.0f);
                    // C3D_ImmSendAttrib(1.0f,1.0f,1.0f,1.0f);
                    // C3D_ImmSendAttrib(stripVertices[i].diffuse.r,stripVertices[i].diffuse.g,stripVertices[i].diffuse.b,stripVertices[i].diffuse.a);
            }
        }
        break;
    case GPU_TRIANGLES:
        // if(this->useTexCoord){
        //     for (int i = 0; i < count; i++)
        //     {
        //         C3D_ImmSendAttrib(triangleVertices[i].position.x,triangleVertices[i].position.y,triangleVertices[i].position.z,1.0f);
        //         C3D_ImmSendAttrib(triangleVertices[i].textureUV.x,1.0f-triangleVertices[i].textureUV.y,1.0f,1.0f);
        //         C3D_ImmSendAttrib(1.0f,1.0f,1.0f,1.0f);
        //     }
        // }else{
        //     for (int i = 0; i < count; i++)
        //     {
        //         C3D_ImmSendAttrib(vertexdiffuseXyzrhw[i].position.x,triangleVertices[i].position.y,triangleVertices[i].position.z,1.0f);
        //         C3D_ImmSendAttrib(1.0f,1.0f,1.0f,1.0f);
        //         C3D_ImmSendAttrib(vertexdiffuseXyzrhw[i].diffuse.r,vertexdiffuseXyzrhw[i].diffuse.g,vertexdiffuseXyzrhw[i].diffuse.b,vertexdiffuseXyzrhw[i].diffuse.a);
        //     }
        // }
            for (int i = 0; i < count; i++)
            {
                C3D_ImmSendAttrib(triangleVertices[i].position.x,triangleVertices[i].position.y,triangleVertices[i].position.z,1.0f);
                C3D_ImmSendAttrib(triangleVertices[i].textureUV.x,1.0f-triangleVertices[i].textureUV.y,1.0f,1.0f);
                C3D_ImmSendAttrib(1.0f,1.0f,1.0f,1.0f);
            }
        break;
    }

    // For rn I'll use Imm mode however this is fills up the C3D cmd buffer really fast, will be better to use proper buffers instead later
    // C3D_ImmDrawBegin(C3DPrim);
    // for (int i = 0; i < count; i++)
    // {
    //     printf("TEst %f", vertices[i].position.x);
    //     switch(C3DPrim){
    //         case GPU_TRIANGLES:
    //             if (!ValidFloat(vertexDataFloat[i * 6 + 0]) || !ValidFloat(vertexDataFloat[i * 6 + 1]) || !ValidFloat(vertexDataFloat[i * 6 + 2]) || !ValidFloat(vertexDataFloat[i * 6 + 4]) || !ValidFloat(vertexDataFloat[i * 6 + 5])){
    //                 printf("BAD VERTEX");
    //                 C3D_ImmDrawEnd();
    //                 return;
    //             }
    //             C3D_ImmSendAttrib(vertexDataFloat[i * 6 + 0], vertexDataFloat[i * 6 + 1], vertexDataFloat[i * 6 + 2], 1.0f);
    //             C3D_ImmSendAttrib(vertexDataFloat[i * 6 + 4], 1.0f - vertexDataFloat[i * 6 + 5], 1.0f, 1.0f);
    //         break;
    //         case GPU_TRIANGLE_STRIP:
    //                 if (!ValidFloat(vertexDataFloat[i * 10 + 0]) || !ValidFloat(vertexDataFloat[i * 10 + 1]) || !ValidFloat(vertexDataFloat[i * 10 + 2]) || !ValidFloat(vertexDataFloat[i * 10 + 8]) || !ValidFloat(vertexDataFloat[i * 10 + 9])){
    //                 printf("Bad Vertex = (%f,%f,%f,%f,%f,%f,%f,%f,%f,%f)", vertexDataFloat[i * 10 + 0], vertexDataFloat[i * 10 + 1], vertexDataFloat[i * 10 + 2],vertexDataFloat[i * 10 + 3], vertexDataFloat[i * 10 + 4], vertexDataFloat[i * 10 + 5], vertexDataFloat[i * 10 + 6], vertexDataFloat[i * 10 + 7], vertexDataFloat[i * 10 + 8], vertexDataFloat[i * 10 + 9]);
    //                 C3D_ImmDrawEnd();
    //                 return;
    //             }
    //             C3D_ImmSendAttrib(vertexDataFloat[i * 10 + 0], vertexDataFloat[i * 10 + 1], vertexDataFloat[i * 10 + 2], 1.0f);
    //             C3D_ImmSendAttrib(vertexDataFloat[i * 10 + 8], vertexDataFloat[i * 10 + 9], 1.0f, 1.0f);
    //         break;
    //     }
    //     // C3D_ImmSendAttrib(vertexDataFloat[i * 6 + 0], vertexDataFloat[i * 6 + 1], vertexDataFloat[i * 6 + 2], 1.0f);
    //     // // V cord must be inversed (Not exactly sure why it just works)
    //     // C3D_ImmSendAttrib(vertexDataFloat[i * 6 + 4], 1.0f - vertexDataFloat[i * 6 + 5], 1.0f, 1.0f);
    // }
    C3D_ImmDrawEnd();
}

