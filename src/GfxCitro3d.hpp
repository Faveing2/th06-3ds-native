#pragma once

#include <citro3d.h>
#include "GfxInterface.hpp"
#include <vector>
#include <memory>
#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>
#include <stdio.h>

struct Texture3ds
{
    std::vector<u32> texels; // ARGB8888
    C3D_Tex texObject;
    i32 width, height;
    PixelFormat format;
    PixelDataType type;
    inline ZunColor GetPixel(i32 x, i32 y);
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
    virtual void SwapBuffers();

    private:
        ZunMatrix model;
        ZunMatrix view;
        ZunMatrix projection;
        ZunMatrix textureMatrix;

        std::vector<std::unique_ptr<Texture3ds>> textures3ds;
        std::vector<u32> freeTextures3ds;

        Texture3ds *boundTexture3ds = nullptr;

        int uLoc_projection, uLoc_modelView, uLoc_textureMatrix;

        DVLB_s* vertex_dvlb;
        shaderProgram_s program;

        C3D_RenderTarget* target;

        bool useTexCoord = false;
        bool useDiffuse = false;

        DVLB_s* shader_dvlb;
        C3D_AttrInfo* attrInfo;
        C3D_TexEnv* env;

        f32 depthNear3ds, depthFar3ds;
        i32 viewport3ds[4];
        float position[3];
        float texcoord[2];
        u8 diffuse[4];

        void *vertexData;
        std::size_t vertexStride;
        void *texCoordData;
        std::size_t texCoordStride;
        void *diffuseData;
        std::size_t diffuseStride;

        float* vbo_data_pos;
        float* vbo_data_texPos;
        u8*    vbo_data_diffuse;

        C3D_Mtx modelMatrix;
        C3D_Mtx viewMatrix;
        C3D_Mtx modelViewMatrix;;
        C3D_Mtx projectionMatrix;
        C3D_Mtx textureMatrixMatrix;

        void *vertexBuffer;
        std::size_t vertexCapacity;
};