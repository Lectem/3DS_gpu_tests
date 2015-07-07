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
                {{0.0f  , 0.0f,       0.5f},BG_COLOR_U8,{-0.5f,-0.5f}},
                {{400.0f, 0.0f, 0.5f},BG_COLOR_U8,{1.5f,-0.5f}},
                {{400.0f, 240.0f, 0.5f},BG_COLOR_U8,{1.5f,1.5f}},
                {{400.0f, 240.0f, 0.5f},BG_COLOR_U8,{1.5f,1.5f}},
                {{0.0f, 240.0f, 0.5f},BG_COLOR_U8,{-0.5f,1.5f}},
                {{0.0f  , 0.0f,       0.5f},BG_COLOR_U8,{-0.5f,-0.5f}}
        };

static void* test_data = NULL;

static u32* test_texture=NULL;
static u32* test_texture1=NULL;
static u32* test_texture2=NULL;
static const u16 test_texture_w=8;
static const u16 test_texture_h=8;
u8 colorsource = 0;
u8 alphasource = 0;

void fill_test_textures(u8 tex,u32 color)
{
    int px;
    for(px=0;px < test_texture_h*test_texture_w;++px)
    {
        switch(tex)
        {
            case 0:
            test_texture[px] = color;
                break;
            case 1:
            test_texture1[px] = color;
                break;
            case 2:
            test_texture2[px] = color;
                break;
            default:break;
        }
    }
}

FILE* reportFile = NULL;


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
    test_texture1 = linearMemAlign(test_texture_w*test_texture_h*sizeof(u32),0x80);
    test_texture2 = linearMemAlign(test_texture_w*test_texture_h*sizeof(u32),0x80);

    fill_test_textures(0,RGBA8(0xFF, 0x00, 0x00, 0xFF));
    fill_test_textures(1,RGBA8(0x22, 0x22, 0x22, 0x22));
    fill_test_textures(2,RGBA8(0x33, 0x33, 0x33, 0x33));

    int i;
    for(i=0;i<sizeof(test_mesh) / sizeof(test_mesh[0]);++i)
    {
        ((vertex_pos_col*)test_data)[i].color.r=255;
        ((vertex_pos_col*)test_data)[i].color.g=255;
        ((vertex_pos_col*)test_data)[i].color.b=255;
        ((vertex_pos_col*)test_data)[i].color.a=255;
    }
    reportFile = fopen("gpuTestReport.txt","w");
    int wrap_mode = 0;

    if(!test_texture)printf("couldn't allocate test_texture\n");
    do{
        hidScanInput();
        u32 keys = keysDown();
        if(keys&KEY_START)break; //Stop the program when Start is pressed
        if(keys&KEY_DOWN && alphasource >0) { alphasource--; }
        if(keys&KEY_UP && alphasource <0xF) { alphasource++; }
        if(keys&KEY_LEFT && colorsource >0) { colorsource--; }
        if(keys&KEY_RIGHT && colorsource<0xF) { colorsource++; }
        if(keys&KEY_L && wrap_mode >0) { wrap_mode--; printf("Wrapmode%d\n",wrap_mode);}
        if(keys&KEY_R && wrap_mode<3) { wrap_mode++;  printf("Wrapmode%d\n",wrap_mode);}


        gpuStartFrame();
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
                GPU_TEXTURE_WRAP_S(wrap_mode) | GPU_TEXTURE_WRAP_T(wrap_mode),
                GPU_RGBA8
        );
        GPUCMD_AddWrite(GPUREG_0081, 0xFFFF0000);
        GPU_SetTexture(
                GPU_TEXUNIT1,
                (u32 *)osConvertVirtToPhys((u32) test_texture1),
                // width and height swapped?
                test_texture_h,
                test_texture_w,
                GPU_TEXTURE_MAG_FILTER(GPU_NEAREST) | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST),
                GPU_RGBA8
        );
        GPU_SetTexture(
                GPU_TEXUNIT2,
                (u32 *)osConvertVirtToPhys((u32) test_texture2),
                // width and height swapped?
                test_texture_h,
                test_texture_w,
                GPU_TEXTURE_MAG_FILTER(GPU_NEAREST) | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST),
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
        if(keysDown()&KEY_A)
        {
            printf("cSource=%1x aSource=%1x gpuColor=%x\n",colorsource, alphasource,(unsigned int)gpuColorBuffer[0]);
            fprintf(reportFile,"cSource=%1x aSource=%1x gpuColor=%x\n",colorsource, alphasource,(unsigned int)gpuColorBuffer[0]);
        }
        if(reportFile)
        {
            //fprintf(reportFile,"%x\t%x\n", alphasource,(gpuColorBuffer[0]>>24)&0xFF);
        }
    }while(aptMainLoop() );


    if(reportFile)
    {
        fclose(reportFile);
    }

    if(test_data)
    {
        linearFree(test_data);
    }
    if(test_texture)
    {
        linearFree(test_texture);
    }
    if(test_texture1)
    {
        linearFree(test_texture1);
    }
    if(test_texture2)
    {
        linearFree(test_texture2);
    }

    gpuUIExit();


    gfxExit();
    sdmcExit();
    hidExit();
    aptExit();
    srvExit();

    return 0;
}
