#pragma once

#include <citro3d.h>
#include "GfxInterface.hpp"
#include <vector>
#include <memory>
#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>
#include <stdio.h>

// 
#define FOG_TABLES 16

struct Vertex3DS
{
    f32 x, y, z;
    f32 u, v;
    u8 r,g,b,a;
};

struct fogLut
{
    C3D_FogLut table;
    f32 fogNear;
    f32 fogFar;
    f32 depthNear;
    f32 depthFar;
};

struct Texture3ds
{
    std::vector<u32> texels; // ARGB8888 Not using this rn
    C3D_Tex texObject;
    i32 width, height;
    PixelFormat format;
    PixelDataType type;
    std::vector<u8> data;
    inline ZunColor GetPixel(i32 x, i32 y);

    // Add a deconstructor to make sure that the vram allocation is removed
    ~Texture3ds(){
        C3D_TexDelete(&texObject);
    }
};

struct GfxCitro3d : GfxInterface
{
    static void SetContextFlags();
    static GfxInterface *Create();

    static GfxInterface *Init();
    virtual void Exit();

    virtual void SetFogRange(f32 nearPlane, f32 farPlane);
    virtual void SetFogColor(ZunColor color);
    virtual void ToggleVertexAttribute(u8 attr, bool enable);
    virtual void SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr);
    virtual void SetColorOp(TextureOpComponent component, ColorOp op);
    virtual void SetTextureFactor(ZunColor factor);
    virtual void SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix);

    virtual void SetTextureFilter();

    virtual void GetViewport(u32 *viewport);
    virtual void GetDepthRange(f32 *depthRange);
    virtual void SetViewport(i32 x, i32 y, i32 width, i32 height);
    virtual void SetDepthRange(f32 nearPlane, f32 farPlane);

    virtual void Enable(Capabilities cap);
    virtual bool HasError();
    virtual void SetBlendMode(BlendMode mode);
    virtual void SetDepthMask(bool enable);
    virtual void SetDepthFunc(DepthFunc func);

    virtual void SetClearDepth(f32 depth);
    virtual void SetClearColor(f32 r, f32 g, f32 b, f32 a);
    virtual void Clear(u32 clearBits);

    virtual GfxTextureHandle CreateTexture();
    virtual void BindTexture(GfxTextureHandle handle);
    virtual void DeleteTexture(GfxTextureHandle handle);
    virtual void SetTextureImage(u32 width, u32 height, PixelFormat fmt, PixelDataType type, const void *data);
    virtual void SetTextureSubImage(i32 xoffset, i32 yoffset, i32 width, i32 height, const void *data);

    virtual void ReadPixels(i32 x, i32 y, i32 width, i32 height, const void *pixels);

    virtual void Draw(PrimitiveType type, i32 start, i32 count);
    virtual void Enable3d(bool enable);
    virtual void SwapBuffers();

    virtual void SetFullScreen(bool enable);

    virtual float getVertexBufferUsage();

    virtual void setFogLut();
    //virtual u8* ReverseTextureRBValues(const u8* data, u32 width, u32 height);

    // Added a deconstructor to make sure that the vertex buffer is released and C3D is released
    ~GfxCitro3d(){
        //linearFree(vertexBuffer);
        C3D_Fini();
        gfxExit();
    }

    private:
        std::vector<std::unique_ptr<Texture3ds>> textures3ds;
        std::vector<u32> freeTextures3ds;

        Texture3ds *boundTexture3ds = nullptr;

        int uLoc_projection, uLoc_modelView, uLoc_textureMatrix;

        DVLB_s* vertex_dvlb;
        shaderProgram_s program;

        u32 C3D_clearcolor = 0x00000000;
        f32 C3D_cleardepth = 0xFFFFFF;

        bool useTexCoord = false;
        bool useDiffuse = false;

        DVLB_s* shader_dvlb;
        C3D_AttrInfo* attrInfo;
        C3D_TexEnv* env;

        f32 depthNear3ds, depthFar3ds;
        i32 viewport3ds[4];

        fogLut fogLuts[FOG_TABLES];
        i32 fogTableAppliedIdx = -1;
        // Not actually a pointer
        i32 fogLutPointer = 0;

        f32 fogNear = 0;
        f32 fogFar = 0;

        bool use3d = false;
        bool first_draw=true;

        void *vertexData;
        std::size_t vertexStride;
        void *texCoordData;
        std::size_t texCoordStride;
        void *diffuseData;
        std::size_t diffuseStride;

        f32* dummyTex;
        u8* dummyDiffuse;

        Vertex3DS* vertexBuffer;
        C3D_BufInfo* vertexBufferInfo; 

        u32 vertexWriteOffset;

        C3D_Mtx modelViewMatrix;;
        C3D_Mtx textureMatrixMatrix;
        C3D_Mtx rotationMatrix;
        C3D_Mtx projectionMatrix;
        C3D_Mtx projectionMatrixLeft;
        C3D_Mtx projectionMatrixRight;

        f32 targetY = 0.0f;
        f32 targetX = 0.0f;
        f32 screenW = 0.0f;
        f32 screenH = 0.0f;
        f32 scalex = 0.5f;
        f32 scaley = 0.5f;

        bool fullscreen = true;
        f32 screenOffset = 40.0f;
        f32 resY = 400.0f;

        C3D_RenderTarget* leftTarget;
        C3D_RenderTarget* rightTarget;

        std::size_t vertexCapacity;
        i32 vertexCount;
};