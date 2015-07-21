/**
* Hello Triangle example, made by Lectem
*
* Draws a white triangle using the 3DS GPU.
* This example should give you enough hints and links on how to use the GPU for basic non-textured rendering.
* Another version of this example will be made with colors.
*
* Thanks to smea, fincs, neobrain, xerpi and all those who helped me understand how the 3DS GPU works
*/


#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpuframework.h"




#define BG_COLOR_U8 {0xFF,0xFF,0xFF,0xFF}
//Our data
static const vertex_pos_col test_mesh[] =
        {
                {{0.0f  , 0.0f,       0.5f},BG_COLOR_U8,{-0.5f,-0.0f}},
                {{400.0f, 0.0f, 0.5f},BG_COLOR_U8,{1.0f,-0.0f}},
                {{400.0f, 240.0f, 0.5f},BG_COLOR_U8,{1.0f,2.5f}},
                {{400.0f, 240.0f, 0.5f},BG_COLOR_U8,{1.0f,2.5f}},
                {{0.0f, 240.0f, 0.5f},BG_COLOR_U8,{-0.5f,2.5f}},
                {{0.0f  , 0.0f,       0.5f},BG_COLOR_U8,{-0.5f,-0.0f}}
        };

static void* test_data = NULL;

static u32* test_texture=NULL;
static const u16 test_texture_w=256;
static const u16 test_texture_h=256;
u8 wrap_s = 0;
u8 wrap_t = 0;

extern const struct {
  u32       width;
  u32       height;
  u32       bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */ 
  u8      pixel_data[256 * 256 * 4 + 1];
} texture_data;
int main(int argc, char** argv)
{

    srvInit();
    aptInit();
    hidInit(NULL);
    sdmcInit();

    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);


    gpuUIInit();

    printf("hello triangle !\n");
    test_data = linearAlloc(sizeof(test_mesh));     //allocate our vbo on the linear heap
    memcpy(test_data, test_mesh, sizeof(test_mesh)); //Copy our data
    //Allocate a RGBA8 texture with dimensions of 1x1
    test_texture = linearMemAlign(test_texture_w*test_texture_h*sizeof(u32),0x80);

    int block_mode=0;
    int i;    
    copyTextureAndTile((u8*)test_texture,texture_data.pixel_data,texture_data.width ,texture_data.height);
    
    if(!test_texture)printf("couldn't allocate test_texture\n");
    do{
        hidScanInput();
        u32 keys = keysDown();
        if(keys&KEY_START)break; //Stop the program when Start is pressed
        if(keys&KEY_DOWN && wrap_s >0) { wrap_s--; printf("wrap_s : %d\n",wrap_s);}
        if(keys&KEY_UP && wrap_s <3) { wrap_s++; printf("wrap_s : %d\n",wrap_s);}
        if(keys&KEY_LEFT && wrap_t >0) { wrap_t--; printf("wrap_t : %d\n",wrap_t);}
        if(keys&KEY_RIGHT && wrap_t<3) { wrap_t++; printf("wrap_t : %d\n",wrap_t);}

        if(keys&KEY_A){block_mode^=1;printf("block_mode : %d\n",block_mode);}
        
        
        gpuStartFrame();

        // Change the block mode
        GPUCMD_AddWrite(GPUREG_011B, block_mode);

        
        
        //Setup the buffers data
        GPU_SetAttributeBuffers(
                3, // number of attributes
                (u32 *) osConvertVirtToPhys((u32) test_data),
                GPU_ATTRIBFMT(0, 3, GPU_FLOAT)|GPU_ATTRIBFMT(1, 4, GPU_UNSIGNED_BYTE)|GPU_ATTRIBFMT(2, 2, GPU_FLOAT),
                0xFFF8,//Attribute mask, in our case 0b1110 since we use only the first one
                0x210,//Attribute permutations (here it is the identity)
                1, //number of buffers
                (u32[]) {0x0}, // buffer offsets (placeholders)
                (u64[]) {0x210}, // attribute permutations for each buffer (identity again)
                (u8[]) {3} // number of attributes for each buffer
        );

        GPU_SetTextureEnable(GPU_TEXUNIT0);

        GPU_SetTexture(
                GPU_TEXUNIT0,
                (u32 *)osConvertVirtToPhys((u32) test_texture),
                // width and height swapped?
                test_texture_h,
                test_texture_w,
                GPU_TEXTURE_MAG_FILTER(GPU_NEAREST) | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST) |
                GPU_TEXTURE_WRAP_S(wrap_s) | GPU_TEXTURE_WRAP_T(wrap_t),
                GPU_RGBA8
        );
        
        int texenvnum=0;
        GPU_SetTexEnv(
                texenvnum,
                GPU_TEVSOURCES(GPU_TEXTURE0, GPU_TEXTURE0, 0),
                GPU_TEVSOURCES(GPU_TEXTURE0, GPU_TEXTURE0, 0),
                GPU_TEVOPERANDS(0, 0, 0),
                GPU_TEVOPERANDS(0, 0, 0),
                GPU_REPLACE, GPU_REPLACE,
                0xAABBCCDD
        );

        //Display the buffers data
        GPU_DrawArray(GPU_TRIANGLES, sizeof(test_mesh) / sizeof(test_mesh[0]));

        gpuEndFrame();
    }while(aptMainLoop() );


    if(test_data)
    {
        linearFree(test_data);
    }
    if(test_texture)
    {
        linearFree(test_texture);
    }

    gpuUIExit();


    gfxExit();
    sdmcExit();
    hidExit();
    aptExit();
    srvExit();

    return 0;
}
