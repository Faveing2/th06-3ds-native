#include <3ds.h>
#include <citro3d.h>
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

    gfxInitDefault();
    C3D_Init(0x100000);

    //SDL_Init(SDL_INIT_GAMECONTROLLER);

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
    //AttrInfo_AddLoader(self->attrInfo, DIFFUSE_ATTRIBUTE_INDEX, GPU_UNSIGNED_BYTE, 4); // diffuse

    // Setup the Buffer
    //vbo_data = linearAlloc()

	self->env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(self->env);
	C3D_TexEnvSrc(self->env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(self->env, C3D_Both, GPU_REPLACE);

    C3D_TexEnvColor(self->env, 0xFF0000FF);

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
        useTexCoord = enable;
    }
    if (attr & VERTEX_ATTR_DIFFUSE){
        useDiffuse = enable;
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
    
}
void GfxCitro3d::SetTextureFactor(ZunColor){
    
}

void GfxCitro3d::SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix){
    //utils::DebugPrint("Setting transform matrix");

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
    
}

void GfxCitro3d::SetClearColor(f32 r, f32 g, f32 b, f32 a){
    
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
    //C3D_TexBind(0, &this->boundTexture3ds->texObject);
    this->boundTexture3ds = this->textures3ds[handle.id].get();
}

void GfxCitro3d::SetContextFlags(){

}

void GfxCitro3d::Clear(u32 clearBits){

}

void GfxCitro3d::DeleteTexture(GfxTextureHandle handle){
    if (handle.id >= textures3ds.size())
        return;
    if (!textures3ds[handle.id])
        return;
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
static int NextPowerOfTwo(int value)
{
    int result = 1;

    while (result < value)
        result <<= 1;

    // Citro3D requires texture dimensions of at least 8.
    return std::max(result, 8);
}

// Morton order inside an 8x8 tile.
static size_t MortonIndex8(int x, int y)
{
    return
        ((x & 1)      ) |
        ((y & 1) << 1 ) |
        ((x & 2) << 1 ) |
        ((y & 2) << 2 ) |
        ((x & 4) << 2 ) |
        ((y & 4) << 3 );
}

bool UploadRGBTexture(
    C3D_Tex* texture,
    const void* data,
    u32 width,
    u32 height,
    size_t sourcePitch
) {
    if (!texture || !data || width <= 0 || height <= 0)
        return false;

    const int textureWidth  = NextPowerOfTwo(width);
    const int textureHeight = NextPowerOfTwo(height);

    const auto* source =
        static_cast<const uint8_t*>(data);

    const int tilesAcross = textureWidth / 8;

    // Four bytes per pixel: R, G, B, A.
    std::vector<uint8_t> tiledPixels(
        static_cast<size_t>(textureWidth) *
        textureHeight *
        4
    );

    for (u32 y = 0; y < textureHeight; y++)
    {
        // Clamp padding to the final source row.
        const int sourceY =
            std::min(y, height - 1);

        for (u32 x = 0; x < textureWidth; x++)
        {
            // Clamp padding to the final source column.
            const int sourceX =
                std::min(x, width - 1);

            const uint8_t* sourcePixel =
                source +
                sourceY * sourcePitch +
                sourceX * 3;

            const int tileX = x / 8;
            const int tileY = y / 8;

            const int localX = x % 8;
            const int localY = y % 8;

            const size_t tileIndex =
                static_cast<size_t>(tileY) * tilesAcross +
                tileX;

            const size_t pixelInTile =
                MortonIndex8(localX, localY);

            const size_t destinationPixel =
                (tileIndex * 64 + pixelInTile) * 4;

            tiledPixels[destinationPixel + 0] = sourcePixel[0]; // R
            tiledPixels[destinationPixel + 1] = sourcePixel[1]; // G
            tiledPixels[destinationPixel + 2] = sourcePixel[2]; // B
            tiledPixels[destinationPixel + 3] = 255;            // A
        }
    }

    if (!C3D_TexInit(
            texture,
            textureWidth,
            textureHeight,
            GPU_RGBA8))
    {
        return false;
    }

    C3D_TexUpload(texture, tiledPixels.data());
    C3D_TexFlush(texture);

    C3D_TexSetFilter(
        texture,
        GPU_LINEAR,
        GPU_LINEAR
    );

    C3D_TexSetWrap(
        texture,
        GPU_CLAMP_TO_EDGE,
        GPU_CLAMP_TO_EDGE
    );

    return true;
}

void GfxCitro3d::SetTextureImage(u32 width, u32 height, PixelFormat fmt, PixelDataType type, const void *data){
    //utils::DebugPrint("Setting Texture Image");
    if (this->boundTexture3ds)
    {
        u32 bpp = 2;
        if (type == PIXEL_UNSIGNED_BYTE)
        {
            if (fmt == PIXEL_RGB)
                bpp = 3;
            else
                bpp = 4;
        }

        this->boundTexture3ds->width = width;
        this->boundTexture3ds->height = height;
        this->boundTexture3ds->format = fmt;
        this->boundTexture3ds->type = type;

        //UploadRGBTexture(&this->boundTexture3ds->texObject, data, width, height, bpp);
        // std::vector<u32> linear(width * height);
        // std::vector<u32> tiled(width * height);
        // u32 bpp = 2;
        // if (type == PIXEL_UNSIGNED_BYTE)
        // {
        //     if (fmt == PIXEL_RGB)
        //         bpp = 3;
        //     else
        //         bpp = 4;
        // }
        // boundTexture3ds->texels.resize(width * height);
        // if (data){
        //     utils::DebugPrint("Writing data to texture");
        //     SDL_ConvertPixels(width, height, GetSDLPixelFormat(fmt, type), data, width * bpp, SDL_PIXELFORMAT_ARGB8888,
        //                       linear.data(), width * sizeof(u32));
        //     ConvertLinearToPicaRGBA8(
        //         linear.data(),
        //         tiled.data(),
        //         width,
        //         height
        //     );
        // }
        // this->boundTexture3ds->width = width;
        // this->boundTexture3ds->height = height;
        // this->boundTexture3ds->format = fmt;
        // this->boundTexture3ds->type = type;

        // C3D_TexInit(&this->boundTexture3ds->texObject, width, height, GPU_RGBA8);
        // C3D_TexUpload(&this->boundTexture3ds->texObject, tiled.data());
        // C3D_TexFlush(&this->boundTexture3ds->texObject);
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
    //utils::DebugPrint("Swapping buffers");
    this->first_draw = true;
    C3D_FrameEnd(0);
    //sleep(1);
}

inline void PrintMatrix(const char* name, const C3D_Mtx& matrix)
{
    //printf("%s:\n", name);

    for (int row = 0; row < 4; row++)
    {
        printf("  [% .6f % .6f % .6f % .6f]\n",
               matrix.r[row].x,
               matrix.r[row].y,
               matrix.r[row].z,
               matrix.r[row].w);
    }
}

static bool ValidFloat(float value)
{
    return std::isfinite(value);
}

void GfxCitro3d::Draw(PrimitiveType type, i32 start, i32 count)
{

    //utils::DebugPrint("Draw call\n");

    GPU_Primitive_t C3DPrim;

    switch(type)
    {
    case PRIM_TRIANGLE_STRIP:
        C3DPrim = GPU_TRIANGLE_STRIP;
        return; // GPU_TRIANGLE_STRIP is causing issues with sending incorrect values to the gpu, lets skip for now
        break;
    case PRIM_TRIANGLES:
        C3DPrim = GPU_TRIANGLES;
        break;
    }

    if(vertexData == nullptr || count <= 0 || count % 3 != 0){
        printf("BAD vertex");
        return;
    }

    // C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    // BufInfo_Init(bufInfo);

    // size_t positionSize =
    // (count - 1) * vertexStride + 3 * sizeof(float);

    // size_t texcoordSize =
    //     (count - 1) * texCoordStride + 2 * sizeof(float);

    // vbo_data_pos = static_cast<float*>(
    //     linearAlloc(count * 3 * sizeof(float))
    // );

    // vbo_data_texPos = static_cast<float*>(
    //     linearAlloc(count * 2 * sizeof(float))
    // );

    // for (int i = 0; i < count; i++)
    // {
    //     memcpy((u8*)vbo_data_pos + i * 3 * sizeof(float),
    //         (u8*)vertexData + i * vertexStride,
    //         3 * sizeof(float));

    //     memcpy((u8*)vbo_data_texPos + i * 2 * sizeof(float),
    //         (u8*)texCoordData + i * texCoordStride,
    //         2 * sizeof(float));
    // }

    // vbo_data_diffuse = static_cast<u8*>(
    //     linearAlloc(count * 4)
    // );

    // if (diffuseData)
    // {
    //     for (int i = 0; i < count; i++)
    //     {
    //         memcpy((u8*)vbo_data_diffuse + i * 4,
    //             (u8*)diffuseData + i * diffuseStride,
    //             4);
    //     }
    // }
    // else
    // {
    //     memset(vbo_data_diffuse, 0xFF, count * 4);
    // }

    if(this->first_draw){
        //utils::DebugPrint("First draw");
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_RenderTargetClear(this->target, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
        C3D_FrameDrawOn(this->target);
        this->first_draw = false;
    }

    // Mtx_OrthoTilt(
    //     &projectionMatrix,
    //     0.0f,    // left
    //     640.0f,  // right
    //     480.0f,    // bottom
    //     0.0f,  // top
    //     0.0f,    // near
    //     1.0f,    // far
    //     true     // account for 3DS screen orientation
    // );

    //PrintMatrix("Projection", this->projectionMatrix);
    // PrintMatrix("Modelview", this->modelViewMatrix);
    // PrintMatrix("Texture", this->textureMatrixMatrix);

    //Mtx_Identity(&this->modelView);
    //Mtx_Identity(&this->textureMatrixMatrix);

    //printf("Sending over uniforms\n");

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_projection,&this->projectionMatrix);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_modelView,&this->modelViewMatrix);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,this->uLoc_textureMatrix,&this->textureMatrixMatrix);

    // BufInfo_Add(bufInfo, vbo_data_pos,3 * sizeof(float), 1, 0x0);

    // BufInfo_Add(bufInfo, vbo_data_texPos,2 * sizeof(float), 1, 0x1);

    // BufInfo_Add(bufInfo, vbo_data_diffuse,4 * sizeof(u8), 1, 0x2);


    // C3D_DrawArrays(C3DPrim, start, count);

    // // Vertex 1
    // C3D_ImmSendAttrib(200.0f, 200.0f, 0.5f, 1.0f);
    // C3D_ImmSendAttrib( 0.5f, -0.5f, -0.5f, 1.0f);
    // C3D_ImmSendAttrib( 0.0f,  0.5f, -0.5f, 1.0f);

    // // Vertex 2
    // C3D_ImmSendAttrib(100.0f, 400.0f, 0.5f, 1.0f);
    // C3D_ImmSendAttrib( 0.5f, -0.5f, -0.5f, 1.0f);
    // C3D_ImmSendAttrib( 0.0f,  0.5f, -0.5f, 1.0f);

    // // Vertex 3
    // C3D_ImmSendAttrib(300.0f, 40.0f, 0.5f, 1.0f);
    // C3D_ImmSendAttrib( 0.5f, -0.5f, -0.5f, 1.0f);
    // C3D_ImmSendAttrib( 0.0f,  0.5f, -0.5f, 1.0f);

    const float* vertexDataFloat = static_cast<const float*>(vertexData);
    //const u8* diffuseFloat = static_cast<const u8*>(diffuseData);

    //int vertexWidth = static_cast<int>vertexStride)

    // int total_count = count * (static_cast<int>(vertexStride) + static_cast<int>(texCoordStride));

    // printf("Totalcount %i", count*(3+2));

    // for (int i = 0; i <= count*(3+2); i++){
    //     C3D_ImmSendAttrib(vertexDataFloat[i + 0], vertexDataFloat[i + 1], vertexDataFloat[i + 2], 1.0f);
    //     C3D_ImmSendAttrib(vertexDataFloat[i + 3], vertexDataFloat[i + 4], 1.0f, 1.0f);
    //     C3D_ImmSendAttrib(256,256,256,256);

    //     printf("pos=(%f,%f,%f)",vertexDataFloat[i + 0],vertexDataFloat[i + 1],vertexDataFloat[i + 2]);
    //     printf("tex=(%f,%f)",vertexDataFloat[i + 3],vertexDataFloat[i + 4]);
    // }

    // printf("Vertex.x, %f\n", vertexDataFloat[0]);
    // printf("Vertex.y, %f\n", vertexDataFloat[1]);
    // printf("Vertex.z, %f\n", vertexDataFloat[2]);
    // printf("Vertex.r, %f\n", vertexDataFloat[3]);
    // printf("Vertex.h, %f\n", vertexDataFloat[4]);
    // printf("Vertex.w, %f\n", vertexDataFloat[5]);
    // sleep(5);
    // printf("Vertex.x, %f\n", vertexDataFloat[6]);
    // printf("Vertex.y, %f\n", vertexDataFloat[7]);
    // printf("Vertex.z, %f\n", vertexDataFloat[8]);
    // printf("Vertex.r, %f\n", vertexDataFloat[9]);
    // printf("Vertex.h, %f\n", vertexDataFloat[10]);
    // printf("Vertex.w, %f\n", vertexDataFloat[11]);
    // sleep(5);

    // Check buffer health
    // printf(
    //     "Command buffer usage: %.2f%%\n",
    //     C3D_GetCmdBufUsage() * 100.0f
    // );

    C3D_ImmDrawBegin(C3DPrim);
    for (int i = 0; i < count; i++)
    {

        if (!ValidFloat(vertexDataFloat[i * 6 + 0]) || !ValidFloat(vertexDataFloat[i * 6 + 1]) || !ValidFloat(vertexDataFloat[i * 6 + 2]) || !ValidFloat(vertexDataFloat[i * 6 + 4]) || !ValidFloat(vertexDataFloat[i * 6 + 5])){
            printf("BAD VERTEX");
            C3D_ImmDrawEnd();
            return;
        }

        C3D_ImmSendAttrib(vertexDataFloat[i * 6 + 0], vertexDataFloat[i * 6 + 1], vertexDataFloat[i * 6 + 2], 1.0f);
        C3D_ImmSendAttrib(vertexDataFloat[i * 6 + 4], vertexDataFloat[i * 6 + 5], 1.0f, 1.0f);
        // printf("pos=(%f,%f,%f)",vertexDataFloat[i * 6 + 0],vertexDataFloat[i * 6 + 1],vertexDataFloat[i * 6 + 2]);
        // printf("tex=(%f,%f)",vertexDataFloat[i * 6 + 4],vertexDataFloat[i * 6 + 5]);

        //C3D_ImmSendAttrib(diffuseFloat[i * 4 + 0], diffuseFloat[i * 4 + 1], diffuseFloat[i * 4 + 2], diffuseFloat[i * 4 + 3]);
        //C3D_ImmSendAttrib(255,255,255,255); // Diffuse data is not being set so lets just submit white for now
    }
    C3D_ImmDrawEnd();
}


// void GfxCitro3d::Draw(PrimitiveType type, i32 start, i32 count){

//     utils::DebugPrint("Drawing Frame");

//     // Mtx_OrthoTilt(
//     // &projectionMatrix,
//     // 0.0f, 400.0f,
//     // 240.0f, 0.0f,
//     // 0.0f, 1.0f,
//     // true
//     // );

//     size_t positionSize =
//     (count - 1) * vertexStride + 3 * sizeof(float);

//     size_t texcoordSize =
//         (count - 1) * texCoordStride + 2 * sizeof(float);

//     vbo_data_pos = static_cast<float*>(
//         linearAlloc(count * 3 * sizeof(float))
//     );

//     vbo_data_texPos = static_cast<float*>(
//         linearAlloc(count * 2 * sizeof(float))
//     );

//     for (int i = 0; i < count; i++)
//     {
//         memcpy((u8*)vbo_data_pos + i * 3 * sizeof(float),
//             (u8*)vertexData + i * vertexStride,
//             3 * sizeof(float));

//         memcpy((u8*)vbo_data_texPos + i * 2 * sizeof(float),
//             (u8*)texCoordData + i * texCoordStride,
//             2 * sizeof(float));
//     }

//     vbo_data_diffuse = static_cast<u8*>(
//         linearAlloc(count * 4)
//     );

//     if (diffuseData)
//     {
//         for (int i = 0; i < count; i++)
//         {
//             memcpy((u8*)vbo_data_diffuse + i * 4,
//                 (u8*)diffuseData + i * diffuseStride,
//                 4);
//         }
//     }
//     else
//     {
//         memset(vbo_data_diffuse, 0xFF, count * 4);
//     }

//     int debugCount = 4;

//     // Print out what is being sent to the GPU for debugging
//     for (int i = 0; i < debugCount; i++)
//     {
//         vbo_data_pos[i * 3 + 2] = 0.5f;
//         printf(
//             "%d: pos=(%f,%f,%f) "
//             "tex=(%f,%f) "
//             "color=(%u,%u,%u,%u)\n",
//             i,

//             vbo_data_pos[i * 3 + 0],
//             vbo_data_pos[i * 3 + 1],
//             vbo_data_pos[i * 3 + 2],

//             vbo_data_texPos[i * 2 + 0],
//             vbo_data_texPos[i * 2 + 1],

//             vbo_data_diffuse[i * 4 + 0],
//             vbo_data_diffuse[i * 4 + 1],
//             vbo_data_diffuse[i * 4 + 2],
//             vbo_data_diffuse[i * 4 + 3]
//         );
//     }

//     C3D_BufInfo* bufInfo = C3D_GetBufInfo();
//     BufInfo_Init(bufInfo);

//     BufInfo_Add(bufInfo, vbo_data_pos,3 * sizeof(float), 1, 0x0);

//     BufInfo_Add(bufInfo, vbo_data_texPos,2 * sizeof(float), 1, 0x1);

//     BufInfo_Add(bufInfo, vbo_data_diffuse,4 * sizeof(u8), 1, 0x2);

//     GPU_Primitive_t C3DPrim;

//     switch(type)
//     {
//     case PRIM_TRIANGLE_STRIP:
//         C3DPrim = GPU_TRIANGLE_STRIP;
//         break;
//     case PRIM_TRIANGLES:
//         C3DPrim = GPU_TRIANGLES;
//         break;
//     }

//     // C3D_Mtx modelView;

//     // Mtx_Identity(&modelMatrix);

//     // Mtx_Multiply(&modelView, &viewMatrix, &modelMatrix);

//     Mtx_OrthoTilt(
//     &projectionMatrix,
//     0.0f, 640.0f,
//     480.0f, 0.0f,
//     0.0f, 1.0f,
//     true
//     );

//     printf(
//     "projection=%d modelView=%d texture=%d\n",
//     uLoc_projection,
//     uLoc_modelView,
//     uLoc_textureMatrix
//     );

//     // PrintMatrix("Projection", projectionMatrix);
//     // PrintMatrix("viewMatrix", modelViewMatrix);
//     // PrintMatrix("modelMatrix", textureMatrixMatrix);

//     C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection,&projectionMatrix);
//     C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uLoc_modelView,&modelViewMatrix);
//     C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uLoc_textureMatrix,&textureMatrixMatrix);

//     C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
//     C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x68B0D8FF,0);
//     C3D_FrameDrawOn(target);
//     //C3D_CullFace(GPU_CULL_FRONT_CCW);
//     C3D_CullFace(GPU_CULL_NONE);
//     C3D_DrawArrays(C3DPrim, start, count);
//     //C3D_DrawArrays(GPU_LINES, start, count);// Render a wireframe for testing
//     C3D_FrameEnd(0);

//     utils::DebugPrint("Finished frame");

//     sleep(1);

//     // linearFree(vbo_data_pos);
//     // linearFree(vbo_data_texPos);
//     // linearFree(vbo_data_diffuse);
// }
