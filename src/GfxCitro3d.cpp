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

// Compiled vertex shader
#include "ff_shbin.h"
static DVLB_s* vertex_dvlb;
static shaderProgram_s program;
static int uLoc_projection, uLoc_modelView, uLoc_textureMatrix;
static void* vbo_data;
static PrintConsole bottomScreen;

#define POSITION_ATTRIBUTE_INDEX 0
#define TEX_CORDS_ATTRIBUTE_INDEX 0
#define DIFFUSE_ATTRIBUTE_INDEX 2

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

GfxInterface *GfxCitro3d::Create(){

    GfxCitro3d *interface = new GfxCitro3d();

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    C3D_RenderTarget* target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    consoleInit(GFX_BOTTOM, &bottomScreen);
    consoleSelect(&bottomScreen);

    printf("Hello World");

    return interface;
}

bool GfxCitro3d::Init(){

    printf("Initializing Graphics");

    ZunMatrix identityMatrix;

    // Create the vertex shader
    vertex_dvlb = DVLB_ParseFile((u32*)ff_shbin, ff_shbin_size);
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &vertex_dvlb->DVLE[0]);
    C3D_BindProgram(&program);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    // Get location of uniforms
    uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
    uLoc_modelView = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
    uLoc_textureMatrix = shaderInstanceGetUniformLocation(program.vertexShader, "textureMatrix");

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); //v0 position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); //v1 texCords
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 1); // diffuse

    // Setup the Buffer
    //vbo_data = linearAlloc()

    return true;
}   

void GfxCitro3d::Exit(){

}

void GfxCitro3d::SetFogRange(f32 nearPlane, f32 farplane){
    
}

void GfxCitro3d::SetFogColor(ZunColor color){
    
}

void GfxCitro3d::ToggleVertexAttribute(u8 attr, bool enable){
    
}

void GfxCitro3d::SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr){
    
}
void GfxCitro3d::SetColorOp(TextureOpComponent component, ColorOp op){
    
}
void GfxCitro3d::SetTextureFactor(ZunColor){
    
}

void GfxCitro3d::SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix){
    
}

void GfxCitro3d::SetTextureFilter(){
    
}

void GfxCitro3d::GetViewport(u32 *viewport){
    
}

void GfxCitro3d::GetDepthRange(f32 *depthRange){
    
}

void GfxCitro3d::SetViewport(i32 x, i32 y, i32 width, i32 height){
    
}

void GfxCitro3d::SetDepthRange(f32 nearPlan, f32 farPlane){
    
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
    
}

void GfxCitro3d::BindTexture(GfxTextureHandle handel){

}

void GfxCitro3d::SetContextFlags(){

}

void GfxCitro3d::Clear(u32 clearBits){

}

void GfxCitro3d::DeleteTexture(GfxTextureHandle handel){
    
}

void GfxCitro3d::SetTextureImage(u32 width, u32 height, PixelFormat fmt, PixelDataType type, const void *data){
    
}

void GfxCitro3d::SetTextureSubImage(i32 xoffset, i32 yoffset, i32 width, i32 height, const void *data){
    
}

void GfxCitro3d::ReadPixels(i32 x, i32 y, i32 width, i32 height, const void *pixels){
    
}

void GfxCitro3d::SwapBuffers(){

}


void GfxCitro3d::Draw(PrimitiveType type, i32 start, i32 count){

    GPU_Primitive_t C3DPrim;

    switch(type)
    {
    case PRIM_TRIANGLE_STRIP:
        C3DPrim = GPU_TRIANGLE_STRIP;
        break;
    case PRIM_TRIANGLES:
        C3DPrim = GPU_TRIANGLES;
        break;
    }
    C3D_DrawArrays(C3DPrim, start, count);
}
