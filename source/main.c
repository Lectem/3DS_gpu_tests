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
#include "shader_vsh_shbin.h"
#include "3dutils.h"
#include "mmath.h"



/**
* Crappy assert stuff that lets you go back to hbmenu by pressing start. 
*/
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)
#define my_assert(e) ((e) ? (void)0 : _my_assert("assert failed at " __FILE__ ":" LINE_STRING " (" #e ")\n"))
void _my_assert(char * text)
{
    printf("%s\n",text);
    do{
        hidScanInput();
        if(keysDown()&KEY_START)break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }while(aptMainLoop());
    //should stop the program and clean up our mess
}




typedef struct {
    float x, y;
} vector_2f;

typedef struct {
    float x, y, z;
} vector_3f;

typedef struct {
    float r, g, b, a;
} vector_4f;

typedef struct {
    u8 r, g, b, a;
} vector_4u8;

typedef struct __attribute__((__packed__)){
    vector_3f position;
    vector_4u8 color;
    vector_2f texpos;
} vertex_pos_col;




#define BG_COLOR_U8 {0xFF,0xFF,0xFF,0xFF}
//Our data
static const vertex_pos_col test_mesh[] =
        {
                {{0.0f  , 0.0f,       0.5f},BG_COLOR_U8,{0.0f,0.0f}},
                {{400.0f, 0.0f, 0.5f},BG_COLOR_U8,{0.0f,0.0f}},
                {{400.0f, 240.0f, 0.5f},BG_COLOR_U8,{0.0f,0.0f}},
                {{400.0f, 240.0f, 0.5f},BG_COLOR_U8,{0.0f,0.0f}},
                {{0.0f, 240.0f, 0.5f},BG_COLOR_U8,{0.0f,0.0f}},
                {{0.0f  , 0.0f,       0.5f},BG_COLOR_U8,{0.0f,0.0f}}
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

#define GPU_CMD_SIZE 0x40000

//GPU framebuffer address
u32*gpuColorBuffer =(u32*)0x1F119400;
//GPU depth buffer address
u32* gpuDBuffer =(u32*)0x1F370800;

//GPU command buffers
u32* gpuCmd = NULL;

//shader structure
DVLB_s* shader_dvlb;    //the header
shaderProgram_s shader; //the program


Result projUniformRegister      =-1;
Result modelviewUniformRegister =-1;

#define RGBA8(r,g,b,a) (((r)&0xFF) | (((g)&0xFF)<<8) | (((b)&0xFF)<<16) | (((a)&0xFF)<<24))

//The color used to clear the screen
u32 clearColor=0;//RGBA8(0xFF, 0x00, 0x80, 0xFF);

//The projection matrix
static float ortho_matrix[4*4];

void GPU_SetDummyTexEnv(u8 num);
void gpuUIEndFrame();

void gpuDisableEverything()
{

    GPU_SetFaceCulling(GPU_CULL_NONE);
    //No stencil test
    GPU_SetStencilTest(false, GPU_ALWAYS, 0x00, 0xFF, 0x00);
    GPU_SetStencilOp(GPU_KEEP, GPU_KEEP, GPU_KEEP);
    //No blending color
    GPU_SetBlendingColor(0,0,0,0);
    //Fake disable AB. We just ignore the Blending part
    GPU_SetAlphaBlending(
            GPU_BLEND_ADD,
            GPU_BLEND_ADD,
            GPU_ONE, GPU_ZERO,
            GPU_ONE, GPU_ZERO
    );

    GPU_SetAlphaTest(false, GPU_ALWAYS, 0x00);

    GPU_SetDepthTestAndWriteMask(true, GPU_ALWAYS, GPU_WRITE_ALL);
    GPUCMD_AddMaskedWrite(GPUREG_0062, 0x1, 0);
    GPUCMD_AddWrite(GPUREG_0118, 0);

    GPU_SetDummyTexEnv(0);
    GPU_SetDummyTexEnv(1);
    GPU_SetDummyTexEnv(2);
    GPU_SetDummyTexEnv(3);
    GPU_SetDummyTexEnv(4);
    GPU_SetDummyTexEnv(5);
}

void gpuUIInit()
{

    GPU_Init(NULL);//initialize GPU

    gfxSet3D(false);//We will not be using the 3D mode in this example
    Result res=0;

    /**
    * Load our vertex shader and its uniforms
    * Check http://3dbrew.org/wiki/SHBIN for more informations about the shader binaries
    */
    shader_dvlb = DVLB_ParseFile((u32 *)shader_vsh_shbin, shader_vsh_shbin_size);//load our vertex shader binary
    my_assert(shader_dvlb != NULL);
    shaderProgramInit(&shader);
    res = shaderProgramSetVsh(&shader, &shader_dvlb->DVLE[0]);
    my_assert(res >=0); // check for errors

    //In this example we are only rendering in "2D mode", so we don't need one command buffer per eye
    gpuCmd=(u32*)linearAlloc(GPU_CMD_SIZE * (sizeof *gpuCmd) ); //Don't forget that commands size is 4 (hence the sizeof)
    my_assert(gpuCmd != NULL);

    //Reset the gpu
    //This actually needs a command buffer to work, and will then use it as default
    GPU_Reset(NULL, gpuCmd, GPU_CMD_SIZE);

    projUniformRegister = shaderInstanceGetUniformLocation(shader.vertexShader, "projection");
    my_assert(projUniformRegister != -1); // make sure we did get the uniform


    shaderProgramUse(&shader); // Select the shader to use

    initOrthographicMatrix(ortho_matrix, 0.0f, 400.0f, 0.0f, 240.0f, 0.0f, 1.0f); // A basic projection for 2D drawings
    SetUniformMatrix(projUniformRegister, ortho_matrix); // Upload the matrix to the GPU

    GPU_DepthMap(-1.0f, 0.0f);  //Be careful, standard OpenGL clipping is [-1;1], but it is [-1;0] on the pica200
    // Note : this is corrected by our projection matrix !

    gpuDisableEverything();

    //Flush buffers and setup the environment for the next frame
    gpuUIEndFrame();

}

void gpuUIExit()
{
    //do things properly
    linearFree(gpuCmd);
    shaderProgramFree(&shader);
    DVLB_Free(shader_dvlb);
    GPU_Reset(NULL, gpuCmd, GPU_CMD_SIZE); // Not really needed, but safer for the next applications ?
}

void gpuStartFrame()
{

    //Get ready to start a new frame
    GPUCMD_SetBufferOffset(0);

    //Viewport (http://3dbrew.org/wiki/GPU_Commands#Command_0x0041)
    GPU_SetViewport((u32 *)osConvertVirtToPhys((u32)gpuDBuffer),
                    (u32 *)osConvertVirtToPhys((u32) gpuColorBuffer),
                    0, 0,
            //Our screen is 400*240, but the GPU actually renders to 400*480 and then downscales it SetDisplayTransfer bit 24 is set
            //This is the case here (See http://3dbrew.org/wiki/GPU#0x1EF00C10 for more details)
                    240*2, 400);


}

void gpuUIEndFrame()
{
    //Ask the GPU to draw everything (execute the commands)
    GPU_FinishDrawing();
    GPUCMD_Finalize();
    GPUCMD_FlushAndRun(NULL);
    gspWaitForP3D();//Wait for the gpu 3d processing to be done
    //Copy the GPU output buffer to the screen framebuffer
    //See http://3dbrew.org/wiki/GPU#Transfer_Engine for more details about the transfer engine

    if(keysDown()&KEY_A)
    {
        printf("cSource=%1x aSource=%1x gpuColor=%x\n",colorsource, alphasource,(unsigned int)gpuColorBuffer[0]);
        fprintf(reportFile,"cSource=%1x aSource=%1x gpuColor=%x\n",colorsource, alphasource,(unsigned int)gpuColorBuffer[0]);
    }
    if(reportFile)
    {
        //fprintf(reportFile,"%x\t%x\n", alphasource,(gpuColorBuffer[0]>>24)&0xFF);
    }

    GX_SetDisplayTransfer(NULL, gpuColorBuffer, 0x019001E0, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 0x019001E0, 0x01001000);
    gspWaitForPPF();

    //Clear the screen
    GX_SetMemoryFill(NULL, gpuColorBuffer, clearColor, &gpuColorBuffer[0x2EE00],
                     0x201, gpuDBuffer, 0x00000000, &gpuDBuffer[0x2EE00], 0x201);
    gspWaitForPSC0();

    gfxSwapBuffersGpu();

    //Wait for the screen to be updated
    gspWaitForVBlank();
}




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

    fill_test_textures(0,RGBA8(0x11, 0x11, 0x11, 0x11));
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

    if(!test_texture)printf("couldn't allocate test_texture\n");
    do{
        hidScanInput();
        u32 keys = keysDown();
        if(keys&KEY_START)break; //Stop the program when Start is pressed
        if(keys & KEY_DOWN && alphasource >0) { alphasource--; }
        if(keys&KEY_UP && alphasource <0xF) { alphasource++; }
        if(keys & KEY_LEFT && colorsource >0) { colorsource--; }
        if(keys&KEY_RIGHT && colorsource<0xF) { colorsource++; }


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

        GPU_SetTextureEnable(GPU_TEXUNIT0 | GPU_TEXUNIT1 | GPU_TEXUNIT2);

        GPU_SetTexture(
                GPU_TEXUNIT0,
                (u32 *)osConvertVirtToPhys((u32) test_texture),
                // width and height swapped?
                test_texture_h,
                test_texture_w,
                GPU_TEXTURE_MAG_FILTER(GPU_NEAREST) | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST),
                GPU_RGBA8
        );
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
                GPU_TEVSOURCES(colorsource, 0, 0),
                GPU_TEVSOURCES(alphasource, 0, 0),
                GPU_TEVOPERANDS(0, 0, 0),
                GPU_TEVOPERANDS(0, 0, 0),
                GPU_REPLACE, GPU_REPLACE,
                0xCCCCCCCC
        );

        //Display the buffers data
        GPU_DrawArray(GPU_TRIANGLES, sizeof(test_mesh) / sizeof(test_mesh[0]));

        gpuUIEndFrame();
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


void GPU_SetDummyTexEnv(u8 num)
{
    //Don't touch the colors of the previous stages
    GPU_SetTexEnv(num,
                  GPU_TEVSOURCES(GPU_PREVIOUS, GPU_PREVIOUS, GPU_PREVIOUS),
                  GPU_TEVSOURCES(GPU_PREVIOUS, GPU_PREVIOUS, GPU_PREVIOUS),
                  GPU_TEVOPERANDS(0,0,0),
                  GPU_TEVOPERANDS(0,0,0),
                  GPU_REPLACE,
                  GPU_REPLACE,
                  0xFFFFFFFF);
}