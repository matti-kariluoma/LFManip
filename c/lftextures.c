// OpenGL routines to play with light field camera images
//
// OpenGL setups and other misc. repurposed from Chris Halsall
// (chalsall@chalsall.com) whose original work was for the
// O'Reilly Network on Linux.com (oreilly.linux.com).
// May 2000.
//
// Matti Kariluoma (matti.kariluoma@gmail.com) Nov 2009

// compile with:
// gcc lftextures.c -o lftextures -lglut -ljpeg -lGLU 


#define PROGRAM_TITLE "Simple Light Field Manipulator"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>    // For our FPS stats.
#include <GL/gl.h>   // OpenGL itself.
#include <GL/glu.h>  // GLU support library.
#include <GL/glut.h> // GLUT support library.
#include <jpeglib.h>
#include <sys/types.h>
#include <dirent.h>

#if !defined(GLUT_WHEEL_UP)
#  define GLUT_WHEEL_UP   3
#  define GLUT_WHEEL_DOWN 4
#endif

#if !defined(GL_TEXTURE_RECTANGLE_ARB)
#  define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif

#define SCALE 2

#define MAX_PIC 22
int NUM_PIC  = 9;

// Some global variables.

// Window and texture IDs, window width and height.
int Texture_ID;
int Window_ID;
int Window_Width = 300;
int Window_Height = 300;

// Our display mode settings.
int Light_On = 0;
int Blend_On = 1;
int Texture_On = 1;
int Filtering_On = 0;
int Alpha_Add = 0;
int Render_mode = 0;
int Text_on = 1;
char Render_string[80] = "Additive";

int Curr_TexMode = 0;
char *TexModesStr[] = {"GL_MODULATE","GL_DECAL","GL_BLEND","GL_REPLACE"};
GLint TexModes[] = {GL_MODULATE,GL_DECAL,GL_BLEND,GL_REPLACE};
GLuint cube,sphere,rectangle,img[MAX_PIC];
GLuint texture[MAX_PIC];
float delta = 0.001f;
FILE *fd[MAX_PIC];
int wid[MAX_PIC],hei[MAX_PIC],dep[MAX_PIC];
unsigned char *image[MAX_PIC];
float offset = 0;
float offset_change = 0.001f;
float offset_delta = 20;
float offset_on = 1;
float offset_off = 0;
float rotate_offset = 0.0f;
int printid = 0;
char dirString[256];
int menu_selected = 0;
#define FILE_ENTRIES 2
#define EDIT_ENTRIES 1
#define VIEW_ENTRIES 7
#define HELP_ENTRIES 2
int file[FILE_ENTRIES];
int edit[EDIT_ENTRIES];
int view[VIEW_ENTRIES];
int help[HELP_ENTRIES];

int state = 1;  //set the state between selection (0, false) and light field manipulation (1, true)
//remember, most cpus assume true when checking conditions, and only backtrack if wrong.

// Object and scene global variables.

// Cube position and rotation speed variables.
float X_Rot   = 90.0f;
float Y_Rot   = 0.0f;
float Z_Rot   = 0.0f;
float Z_Off   = -70.0f;
float X_Cam   = 0.0f;
float Y_Cam   = 0.0f;

// Settings for our light.  Try playing with these (or add more lights).
float Light_Ambient[]=  { 0.1f, 0.1f, 0.1f, 1.0f };
float Light_Diffuse[]=  { 2.2f, 2.2f, 2.2f, 2.0f };
float Light_Position[]= { 2.0f, 2.0f, 0.0f, 1.0f };



// Frames per second (FPS) statistic variables and routine.
#define FRAME_RATE_SAMPLES 50
int FrameCount=0;
float FrameRate=0;

// Routine to keep the render code cleaner; not used for the default render mode
float slide(int id)
{
  float pos;
  int piv = NUM_PIC/2;

  if (id == piv)
    pos = 0.0f;
  else if (id > piv)
    pos = (id % piv) + 1.0f;
  else if (id < piv)
    pos = -(id % piv) - 1.0f;

// printf("id: %d pos: %f\n", id, pos);

//  pos /= 10.0f;

  return pos;
}

void printscreen()
{
  
  unsigned char *data = malloc(Window_Width*Window_Height*3);
  
  int i,j;

// Produces an upside down image
  glReadPixels(0,0,Window_Width,Window_Height,GL_RGB,GL_UNSIGNED_BYTE,data);

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  FILE *outjpeg;
  JSAMPROW row_pointer[1];
  int row_stride;
  
  char id[4];
  sprintf(id, "%d", printid);
  char outname[300];  //"out_000.jpg";
  
   char *temp;
   while ((temp=strchr( dirString, '/')) != NULL)
   {
     strcpy(temp, "_");
   }
   while ((temp=strchr( dirString, '\\')) != NULL)
   {
     strcpy(temp, "_");
   }
  
  strcpy(outname, dirString);
    if (printid < 10)
    {
      strcat(outname, "000");
      strcat(outname, id);
    }
    else if (printid < 100)
    {
      strcat(outname, "00");
      strcat(outname, id);
    }
    else if (printid < 1000)
    {
      strcat(outname, "0");
      strcat(outname, id);
    }
    else
    {
      strcat(outname, id);
    }
  strcat(outname, ".jpg");
  printid++;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  
  if ((outjpeg = fopen(outname, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", outname);
    exit(1);
  }

  jpeg_stdio_dest(&cinfo, outjpeg);
  cinfo.image_width = Window_Width;
  cinfo.image_height = Window_Height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB; 
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 93, TRUE);

  jpeg_start_compress(&cinfo, TRUE);
  row_stride = Window_Width * 3;
  
  for( i=cinfo.image_height-1; i>=0; i--)
  {
    row_pointer[0] = &data[i * row_stride];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);

  fclose(outjpeg);
  
  free(data);
  
  jpeg_destroy_compress(&cinfo);

  return;
}

void sort_files(char *files[], int length)
{
  int i,j,min;
  char *temp[1];
  
  for (i = 0; i < length; i++)
  {
    min = i;
    for (j = i+1; j < length; j++)
    {
      if (strcmp(files[j],files[min]) < 0)
        min = j;
    }
  temp[0] = files[min];
  files[min] = files[i];
  files[i] = temp[0];
  }
  
  for(i = 0; i< length; i++)
  {
//    printf("SORT %s\n", files[i]);
  }
}

static void ourDoFPS()
{
   static clock_t last=0;
   clock_t now;
   float delta;

   if (++FrameCount >= FRAME_RATE_SAMPLES) {
      now  = clock();
      delta= (now - last) / (float) CLOCKS_PER_SEC;
      last = now;

      FrameRate = FRAME_RATE_SAMPLES / delta;
      FrameCount = 0;
   }
}


// String rendering routine; leverages on GLUT routine.
static void ourPrintString(void *font, char *str)
{
   int i,l=strlen(str);

   for(i=0;i<l;i++)
      glutBitmapCharacter(font,*str++);
}


// Routine which actually does the drawing
void cbRenderScene( void )
{
   char buf[80]; // For our strings.

   // Enables, disables or otherwise adjusts as
   // appropriate for our current settings.

   if (Texture_On)
   {
      glEnable(GL_TEXTURE_RECTANGLE_ARB);
//      glEnable(GL_TEXTURE_RECTANGLE_NV);
   }
   else
   {
      glDisable(GL_TEXTURE_RECTANGLE_ARB);
//      glDisable(GL_TEXTURE_RECTANGLE_NV);
   }
   if (Light_On)
      glEnable(GL_LIGHTING);
   else
      glDisable(GL_LIGHTING);

    if (Alpha_Add)
       glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    else
       glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // If we're blending, we don't want z-buffering.
    if (Blend_On)
    {
       glDisable(GL_DEPTH_TEST);
       glEnable(GL_BLEND);
     }
    else
       glEnable(GL_DEPTH_TEST);

    if (Filtering_On) {
       glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,
                                           GL_LINEAR_MIPMAP_LINEAR);
       glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    } else {
       glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,
                                           GL_NEAREST_MIPMAP_NEAREST);
       glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }


   // Need to manipulate the ModelView matrix to move our model around.
   glMatrixMode(GL_MODELVIEW);

   // Reset to 0,0,0; no rotation, no scaling.
   glLoadIdentity();

   // Clear the color and depth buffers.
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

if (state) //is false for the selection portion, then turns to true for the remainder of the program
{
   // Move the object back from the screen.
   glTranslatef(X_Cam,Y_Cam,Z_Off);

   // Rotate the calculated amount.
   glRotatef(X_Rot,1.0f,0.0f,0.0f);
   glRotatef(Y_Rot,0.0f,1.0f,0.0f);
   glRotatef(Z_Rot,0.0f,0.0f,1.0f);

   int i;

   for(i=0; i < NUM_PIC; i++)
   {
    glPushMatrix();
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture[i]);
     switch(Render_mode)
     {
     case 0: glTranslatef(offset_on*(offset*i*offset_delta)+offset_off*(offset*i),0.0f,0.0f); strcpy(Render_string, "Additive"); break;
     case 1: glTranslatef(offset*offset_delta*slide(i),0.0f,0.0f); strcpy(Render_string, "Slide"); break;
     case 2: glTranslatef(0.0f,-offset*offset_delta*2*i,0.0f); strcpy(Render_string, "3D"); break;
     case 3: glTranslatef(offset*offset_delta*i,-offset*offset_delta*2*i,0.0f); strcpy(Render_string, "3D & Additive"); break;
     case 4: glTranslatef(offset*offset_delta*slide(i),-offset*offset_delta*2*i,0.0f); strcpy(Render_string, "3D & Slide"); break;
     default: break;
     }
    glBegin(GL_QUADS);
      glColor4f(0.8f, 0.8f, 0.8f, (1.0f/(i+1)) );

      glTexCoord2f(0.0f, 0.0f);
       glVertex3f(0.0f-wid[i]/40.0f, i/10.0f,0.0f-hei[i]/40.0f);

      glTexCoord2f(wid[i], 0.0f);
       glVertex3f(wid[i]/40.0f, i/10.0f,0.0f-hei[i]/40.0f);

      glTexCoord2f(wid[i], hei[i]);
       glVertex3f(wid[i]/40.0f, i/10.0f,hei[i]/40.0f);

      glTexCoord2f(0.0f, hei[i]);
       glVertex3f(0.0f-wid[i]/40.0f, i/10.0f,hei[i]/40.0f);

    glEnd();
    glPopMatrix();
   }

}
   // Lit or textured text looks awful.
   glDisable(GL_TEXTURE_RECTANGLE_ARB);
   glDisable(GL_LIGHTING);

   // We don't want depth-testing either.
   glDisable(GL_DEPTH_TEST);

   // Move back to the origin (for the text, below).
   glLoadIdentity();

   // We need to change the projection matrix for the text rendering.
   glMatrixMode(GL_PROJECTION);

   // But we like our current view too; so we save it here.
   glPushMatrix();

   // Now we set up a new projection for the text.
   glLoadIdentity();
   glOrtho(0,Window_Width,0,Window_Height,-1.0,1.0);

if (Text_on)
{
   // But, for fun, let's make the text partially transparent too.
   glColor4f(0.6,1.0,0.6,.85);

   // Render our various display mode settings.
//   sprintf(buf,"Mode: %s", TexModesStr[Curr_TexMode]);

     sprintf(buf,"Blend: %s", Blend_On ? "Yes" : "No" );
   glRasterPos2i(2,2); ourPrintString(GLUT_BITMAP_HELVETICA_12,buf);

   sprintf(buf,"Light: %s", Light_On ? "Yes" : "No");
   glRasterPos2i(2,14); ourPrintString(GLUT_BITMAP_HELVETICA_12,buf);

  sprintf(buf,"Mode: %d, %s", Render_mode, Render_string);
   glRasterPos2i(2,26); ourPrintString(GLUT_BITMAP_HELVETICA_12,buf);

      sprintf(buf,"X: %s", offset_on ? "Fast" : "Slow");
   glRasterPos2i(2,38); ourPrintString(GLUT_BITMAP_HELVETICA_12,buf);

   sprintf(buf,"O: Turn off text display");
   glRasterPos2i(2,50); ourPrintString(GLUT_BITMAP_HELVETICA_12,buf);

   sprintf(buf,"P: Print screen");
   glRasterPos2i(2,62); ourPrintString(GLUT_BITMAP_HELVETICA_12,buf);
   
   sprintf(buf,"I/U: Macro Print screen");
   glRasterPos2i(2,74); ourPrintString(GLUT_BITMAP_HELVETICA_12,buf);

   sprintf(buf,"Y/T: Macro Rotate/ Print screen");
   glRasterPos2i(2,86); ourPrintString(GLUT_BITMAP_HELVETICA_12,buf);
   
   sprintf(buf,"R: Reset View");
   glRasterPos2i(2,98); ourPrintString(GLUT_BITMAP_HELVETICA_12,buf);
   // Now we want to render the calulated FPS at the top.

   // To ease, simply translate up.  Note we're working in screen
   // pixels in this projection.

   glTranslatef(6.0f,Window_Height - 14,0.0f);

   // Make sure we can read the FPS section by first placing a
   // dark, mostly opaque backdrop rectangle.
   glColor4f(0.2,0.2,0.2,0.75);

   glBegin(GL_QUADS);
   glVertex3f(  0.0f, -16.0f, 0.0f);
   glVertex3f(  0.0f, -4.0f, 0.0f);
   glVertex3f(140.0f, -4.0f, 0.0f);
   glVertex3f(140.0f, -16.0f, 0.0f);
   glEnd();
   
   glColor4f(0.9,0.2,0.2,.75);
   sprintf(buf,"FPS: %f F: %2d", FrameRate, FrameCount);
   glRasterPos2i(6,-16);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,buf);
   glLoadIdentity();
   glOrtho(0,Window_Width,0,Window_Height,-1.0,1.0);
}

   glTranslatef(6.0f,Window_Height - 14,0.0f);

   glBegin(GL_QUADS);  
   glColor4f(.75,.75,.75,1.0);
   glVertex3f(-6, -4, 0);
   glVertex3f(-6, 14, 0);
   glVertex3f(Window_Width, 14, 0);
   glVertex3f(Window_Width, -4, 0);
   glEnd();
   
   glBegin(GL_LINES);
   glColor4f(0,0,0,1.0);
   glVertex3f(Window_Width-7, -4, 0);
   glVertex3f(-6, -4, 0);
   glEnd();
   
   switch(menu_selected)
   {
     case 0: break;
     case 1: //File menu
   glBegin(GL_QUADS); //draw drop-down menu
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  -2.0f, -4.0f, 0);
   glVertex3f(  -2.0f, 13.0f, 0);
   glVertex3f(  25.0f, 13.0f, 0);
   glVertex3f(  25.0f, -4.0f, 0);
   
   glColor4f(.75,.75,.75,1.0);
   glVertex3f(-2, -38, 0);
   glVertex3f(-2, -4, 0);
   glVertex3f(98, -4, 0);
   glVertex3f(98, -38, 0);
   glEnd();
   
   glColor4f(0,0,0,1);
   if(file[0])
   {
   glBegin(GL_QUADS); //draw blue selection box
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  -1.0f, -5.0f, 0);
   glVertex3f(  -1.0f, -20.0f, 0);
   glVertex3f(  97.0f, -20.0f, 0);
   glVertex3f(  97.0f, -5.0f, 0);
   glEnd();
   glColor4f(1,1,1,1);
   }
   glRasterPos2i(2, -16);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"Open      Ctrl+O");
   glBegin(GL_LINES); //draw light grey divider
   glColor4f(.6,.6,.6,1);
   glVertex3f(-1,-22,0);
   glVertex3f(99,-22,0);
   glEnd();
   //begin next menu entry
   glColor4f(0,0,0,1);
   if(file[1])
   {
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  -1.0f, -23.0f, 0);
   glVertex3f(  -1.0f, -36.0f, 0);
   glVertex3f(  97.0f, -36.0f, 0);
   glVertex3f(  97.0f, -23.0f, 0);
   glEnd();
   glColor4f(1,1,1,1);
   }
   glRasterPos2i(2, -34);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"Exit               Q");
     break;
     case 2: //Edit  
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  28.0f, -4.0f, 0);
   glVertex3f(  28.0f, 13.0f, 0);
   glVertex3f(  54.0f, 13.0f, 0);
   glVertex3f(  54.0f, -4.0f, 0);
   
   glColor4f(.75,.75,.75,1.0);
   glVertex3f(28, -8, 0);
   glVertex3f(28, -4, 0);
   glVertex3f(77, -4, 0);
   glVertex3f(77, -8, 0);
   glEnd();
     break;
     case 3: //View
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  58.0f, -4.0f, 0);
   glVertex3f(  58.0f, 13.0f, 0);
   glVertex3f(  92.0f, 13.0f, 0);
   glVertex3f(  92.0f, -4.0f, 0);
   
   glColor4f(.75,.75,.75,1.0);
   glVertex3f(58, -122, 0);
   glVertex3f(58, -4, 0);
   glVertex3f(164, -4, 0);
   glVertex3f(164, -122, 0);
   glEnd();
   
   glColor4f(0,0,0,1);
   if(view[0])
   {
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  59.0f, -5.0f, 0);
   glVertex3f(  59.0f, -19.0f, 0);
   glVertex3f(  163.0f, -19.0f, 0);
   glVertex3f(  163.0f, -5.0f, 0);
   glEnd();
   glColor4f(1,1,1,1);
   }
   glRasterPos2i(62, -16);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"Zoom in      PgUp");
   glColor4f(0,0,0,1);
   if(view[1])
   {
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  59.0f, -34.0f, 0);
   glVertex3f(  59.0f, -19.0f, 0);
   glVertex3f(  163.0f, -19.0f, 0);
   glVertex3f(  163.0f, -34.0f, 0);
   glEnd();
   glColor4f(1,1,1,1);
   }
   glRasterPos2i(62, -30);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"Zoom out    PgDn");
   glColor4f(0,0,0,1);
   if(view[2])
   {
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  59.0f, -34.0f, 0);
   glVertex3f(  59.0f, -49.0f, 0);
   glVertex3f(  163.0f, -49.0f, 0);
   glVertex3f(  163.0f, -34.0f, 0);
   glEnd();
   glColor4f(1,1,1,1);
   }
   glRasterPos2i(62, -44);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"Reset view       R");
   glBegin(GL_LINES);
   glColor4f(.6,.6,.6,1);
   glVertex3f(59,-50,0);
   glVertex3f(163,-50,0);
   glEnd();
   glColor4f(0,0,0,1);
   glRasterPos2i(62, -62);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"Set mode:");
   glColor4f(0,0,0,1);
   if(view[3])
   {
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  59.0f, -65.0f, 0);
   glVertex3f(  59.0f, -79.0f, 0);
   glVertex3f(  163.0f, -79.0f, 0);
   glVertex3f(  163.0f, -65.0f, 0);
   glEnd();
   glColor4f(1,1,1,1);
   }
   glRasterPos2i(110, -76);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"  Default");
   glColor4f(0,0,0,1);
   if(view[4])
   {
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  59.0f, -92.0f, 0);
   glVertex3f(  59.0f, -79.0f, 0);
   glVertex3f(  163.0f, -79.0f, 0);
   glVertex3f(  163.0f, -92.0f, 0);
   glEnd();
   glColor4f(1,1,1,1);
   }
   glRasterPos2i(110, -89);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"     Slide");
   glColor4f(0,0,0,1);
   if(view[5])
   {
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  59.0f, -92.0f, 0);
   glVertex3f(  59.0f, -107.0f, 0);
   glVertex3f(  163.0f, -107.0f, 0);
   glVertex3f(  163.0f, -92.0f, 0);
   glEnd();
   glColor4f(1,1,1,1);
   }
   glRasterPos2i(110, -104);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"        3D");
   glColor4f(0,0,0,1);
   if(view[6])
   {
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  59.0f, -120.0f, 0);
   glVertex3f(  59.0f, -107.0f, 0);
   glVertex3f(  163.0f, -107.0f, 0);
   glVertex3f(  163.0f, -120.0f, 0);
   glEnd();
   glColor4f(1,1,1,1);
   }
   glRasterPos2i(110, -117);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"3D Slide");
   
     break;
     case 4: //Help
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  94.0f, -4.0f, 0);
   glVertex3f(  94.0f, 13.0f, 0);
   glVertex3f(  128.0f, 13.0f, 0);
   glVertex3f(  128.0f, -4.0f, 0);
   
   glColor4f(.75,.75,.75,1.0);
   glVertex3f(94, -22, 0);
   glVertex3f(94, -4, 0);
   glVertex3f(174, -4, 0);
   glVertex3f(174, -22, 0);
   glEnd();
   
   glColor4f(0,0,0,1);
   if(help[0])
   {
   glBegin(GL_QUADS);
   glColor4f(.04,.14,.39,.8);
   glVertex3f(  95.0f, -5.0f, 0);
   glVertex3f(  95.0f, -20.0f, 0);
   glVertex3f(  173.0f, -20.0f, 0);
   glVertex3f(  173.0f, -5.0f, 0);
   glEnd();
   glColor4f(1,1,1,1);
   }
   glRasterPos2i(98, -16);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"About");
     break;
     default: menu_selected = 0; break;
   }
   
   glColor4f(0,0,0,1);
   glRasterPos2i(2, 1);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"File");
   if(menu_selected == 1) //write over it in white
   {
    glColor4f(1,1,1,1);
    glRasterPos2i(2, 1);
    ourPrintString(GLUT_BITMAP_HELVETICA_12,"File");
   }
   glRasterPos2i(2, 1);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"_");
   glColor4f(0,0,0,1);
   glRasterPos2i(31, 1);
   ourPrintString(GLUT_BITMAP_HELVETICA_12, "Edit");
   if(menu_selected == 2)
   {
    glColor4f(1,1,1,1);
    glRasterPos2i(31, 1);
    ourPrintString(GLUT_BITMAP_HELVETICA_12,"Edit");
   }
   glRasterPos2i(31, 1);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"_");
   glColor4f(0,0,0,1);
   glRasterPos2i(61, 1);
   ourPrintString(GLUT_BITMAP_HELVETICA_12, "View");
   if(menu_selected == 3)
   {
    glColor4f(1,1,1,1);
    glRasterPos2i(61, 1);
    ourPrintString(GLUT_BITMAP_HELVETICA_12,"View");
   }
   glRasterPos2i(61, 1);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"_");
   glColor4f(0,0,0,1);
   glRasterPos2i(97, 1);
   ourPrintString(GLUT_BITMAP_HELVETICA_12, "Help");
   if(menu_selected == 4)
   {
    glColor4f(1,1,1,1);
    glRasterPos2i(97, 1);
    ourPrintString(GLUT_BITMAP_HELVETICA_12,"Help");
   }
   glRasterPos2i(97, 1);
   ourPrintString(GLUT_BITMAP_HELVETICA_12,"_");
   

   
   // Done with this special projection matrix.  Throw it away.
   glPopMatrix();

   
   // All done drawing.  Let's show it.
   glutSwapBuffers();


   // And collect our statistics.
   ourDoFPS();
}

// Callback function when the mouse is clicked

void cbMouseClick(int button, int state, int x, int y )
{
  int i;
  
  if (y > 20)
  {
    switch (button)
    {
    case GLUT_WHEEL_UP:
      offset += offset_change; break;
    case GLUT_WHEEL_DOWN:
      offset -= offset_change; break;
    default: break;
    }
    
    if (x < 98 && menu_selected == 1)
    {
      if (y > 20 && y < 34) //file->open selected
    {
      
      menu_selected = 0;
    } else if (y > 36 && y < 52) //file->quit selected
    {
      menu_selected = 0;
      glutDestroyWindow(Window_ID);
      exit(1);
    }
    }
    if (state == GLUT_DOWN)
    {
      menu_selected = 0;
    }
    
  } else if(x < 31 && state == GLUT_DOWN)
  {
    menu_selected = 1;
    for (i=0; i<FILE_ENTRIES; i++)
    { file[i] = 0; }
  } else if(x < 61 && state == GLUT_DOWN)
  {
    menu_selected = 2;
    for (i=0; i<EDIT_ENTRIES; i++)
    { edit[i] = 0; }
  } else if(x < 96 && state == GLUT_DOWN)
  {
    menu_selected = 3;
    for (i=0; i<VIEW_ENTRIES; i++)
    { view[i] = 0; }
  } else if(x < 126 && state == GLUT_DOWN)
  {
    menu_selected = 4;
    for (i=0; i<HELP_ENTRIES; i++)
    { help[i] = 0; }
  } else if (x > 126)
  {
    menu_selected = 0;
  }

}
// Callback function when the mouse is clicked and dragged

void cbMouseHeldMoving( int x, int y )
{
  int i;
  
  if(y > 20 && menu_selected == 0)
  {
  		if (x < 0)
			Z_Rot = 90.0;
		else if (x > Window_Width)
			Z_Rot = -90.0;
		else
			Z_Rot = -180.0 * ((float) x)/Window_Height + 90.0;
/*
		if (y < 0)
			X_Rot = -90.0;
		else if (y > Window_Height)
			X_Rot = 90.0;
		else
			X_Rot = 180.0 * ((float) y)/Window_Width -60.0;
*/
  } else if(x < 98 && menu_selected == 1)
  {
    if (y < 20 && x > 31)
    {
      menu_selected = 2;
      for (i=0; i<FILE_ENTRIES; i++)
      { file[i] = 0; }
    } else if (y > 20 && y < 34)
    {
      for (i=0; i<FILE_ENTRIES; i++)
      { file[i] = 0; }
      file[0] = 1;
    } else if (y > 36 && y < 52)
    {
      for (i=0; i<FILE_ENTRIES; i++)
      { file[i] = 0; }
      file[1] = 1;
    } else
    {
      for (i=0; i<FILE_ENTRIES; i++)
      { file[i] = 0; }
    }
  } else if(x < 77 && menu_selected == 2)
  {
     if (y < 20 && x > 61)
    {
      menu_selected = 3;
      for (i=0; i<EDIT_ENTRIES; i++)
      { edit[i] = 0; }
    } else if (y < 20 && x < 31)
    {
      menu_selected = 1;
      for (i=0; i<EDIT_ENTRIES; i++)
      { edit[i] = 0; }
    }
  } else if(x < 164 && menu_selected == 3)
  {
    if (y < 20 && x > 96)
    {
      menu_selected = 4;
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
    } else if (y < 20 && x < 61)
    {
      menu_selected = 2;
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
    } else if (y > 20 && y < 34)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[0] = 1;
    } else if (y > 35 && y < 50)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[1] = 1;
    } else if (y > 51 && y < 66)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[2] = 1;
    } else if (y > 78 && y < 88)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[3] = 1;
    } else if (y > 89 && y < 104)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[4] = 1;
    } else if (y > 105 && y < 120)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[5] = 1;
    } else if (y > 121 && y < 134)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[6] = 1;
    } else
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
    }
  } else if(x < 174 && menu_selected == 4)
  {
    if (y < 20 && x < 96)
    {
      menu_selected = 3;
      for (i=0; i<HELP_ENTRIES; i++)
      { help[i] = 0; }
    } else if (y > 20 && y < 34)
    {
      for (i=0; i<HELP_ENTRIES; i++)
      { help[i] = 0; }
      help[0] = 1;
    } else
    {
      for (i=0; i<HELP_ENTRIES; i++)
      { help[i] = 0; }
    }
  } else
  {
    
  }
}

void cbMouseMoving( int x, int y)
{
  int i;
  
  if(x < 98 && menu_selected == 1)
  {
    if (y < 20 && x > 31)
    {
      menu_selected = 2;
      for (i=0; i<FILE_ENTRIES; i++)
      { file[i] = 0; }
    } else if (y > 20 && y < 34)
    {
      for (i=0; i<FILE_ENTRIES; i++)
      { file[i] = 0; }
      file[0] = 1;
    } else if (y > 36 && y < 52)
    {
      for (i=0; i<FILE_ENTRIES; i++)
      { file[i] = 0; }
      file[1] = 1;
    } else
    {
      for (i=0; i<FILE_ENTRIES; i++)
      { file[i] = 0; }
    }
  } else if(x < 77 && menu_selected == 2)
  {
     if (y < 20 && x > 61)
    {
      menu_selected = 3;
      for (i=0; i<EDIT_ENTRIES; i++)
      { edit[i] = 0; }
    } else if (y < 20 && x < 31)
    {
      menu_selected = 1;
      for (i=0; i<EDIT_ENTRIES; i++)
      { edit[i] = 0; }
    }
  } else if(x < 164 && menu_selected == 3)
  {
    if (y < 20 && x > 96)
    {
      menu_selected = 4;
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
    } else if (y < 20 && x < 61)
    {
      menu_selected = 2;
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
    } else if (y > 20 && y < 34)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[0] = 1;
    } else if (y > 35 && y < 50)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[1] = 1;
    } else if (y > 51 && y < 66)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[2] = 1;
    } else if (y > 78 && y < 88)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[3] = 1;
    } else if (y > 89 && y < 104)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[4] = 1;
    } else if (y > 105 && y < 120)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[5] = 1;
    } else if (y > 121 && y < 134)
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
      view[6] = 1;
    } else
    {
      for (i=0; i<VIEW_ENTRIES; i++)
      { view[i] = 0; }
    }
  } else if(x < 174 && menu_selected == 4)
  {
    if (y < 20 && x < 96)
    {
      menu_selected = 3;
      for (i=0; i<HELP_ENTRIES; i++)
      { help[i] = 0; }
    } else if (y > 20 && y < 34)
    {
      for (i=0; i<HELP_ENTRIES; i++)
      { help[i] = 0; }
      help[0] = 1;
    } else
    {
      for (i=0; i<HELP_ENTRIES; i++)
      { help[i] = 0; }
    }
  } else
  {

  }
}

// Callback function called when a normal key is pressed.
void cbKeyPressed(unsigned char key, int x, int y )
{
   switch (key) {
      case 113: case 81: case 27: // Q (Escape) - We're outta here.
      glutDestroyWindow(Window_ID);
      exit(1);
      break; // exit doesn't return, but anyway...
 /*
   case 130: case 98: // B - Blending.
      Blend_On = Blend_On ? 0 : 1;
      if (!Blend_On)
         glDisable(GL_BLEND);
      else
         glEnable(GL_BLEND);
      break;
*/
   case 108: case 76:  // L - Lighting
      Light_On = Light_On ? 0 : 1;
      Alpha_Add = Alpha_Add ? 0 : 1;
      break;

   case 109: case 77:  // M - Mode of Blending
    Render_mode++;
    Render_mode = Render_mode % 5;
      break;
/*
   case 116: case 84: // T - Texturing.
      Texture_On = Texture_On ? 0 : 1;
      break;

   case 97: case 65:  // A - Alpha-blending hack.
      Alpha_Add = Alpha_Add ? 0 : 1;
      break;

   case 102: case 70:  // F - Filtering.
      Filtering_On = Filtering_On ? 0 : 1;
      break;
*/
   case 112: case 80:  // P - printscreen
      printscreen();
      break;

   case 115: case 83: case 32:  // S - change state
      state = state ? 0 : 1;
      break;

   case 114: case 82:  // R - reset view
      X_Rot   = 90.0f;
      Y_Rot   = 0.0f;
      Z_Rot   = 0.0f;
      Z_Off   = -70.0f;
      X_Cam   = 0.0f;
      Y_Cam   = 0.0f;
      break;

   case 111: case 79:  // O - toggle text
      Text_on = Text_on ? 0 : 1;
      break;

   case 120: case 88:  // X - extra speed
      if (offset_on == 1)
      {
       offset *= offset_delta;
       offset_on = 0;
       offset_off = 1;
      }
      else
      {
       offset /= offset_delta;
       offset_on = 1;
       offset_off = 0;
      }
      break;

   case 105: case 73:  // i - macro movie capture plus
      offset += 0.001f;
      printscreen();
      break;
  
   case 117: case 85:  // u - macro movie capture negative
      offset -= 0.001f;
      printscreen();
      break;
      
   case 121: case 89:  // y - macro rotate capture plus
      rotate_offset += 5.0f;
      Z_Rot = 180.0 * rotate_offset/Window_Height - 90.0;
      printscreen();
      break;
      
   case 116: case 84:  // t - macro rotate capture negative
      rotate_offset -= 5.0f;
      Z_Rot = 180.0 * rotate_offset/Window_Height - 90.0;
      printscreen();
      break;
      
   default:
      printf ("KP: No action for %d.\n", key);
      break;
    }
}



// Callback Function called when a special key is pressed.
void cbSpecialKeyPressed(int key, int x, int y)
{
   switch (key) {
   case GLUT_KEY_PAGE_UP: // move the cube into the distance.
      Z_Off += 5.0f;
      break;

   case GLUT_KEY_PAGE_DOWN: // move the cube closer.
      Z_Off -= 5.0f;
      break;

   case GLUT_KEY_UP:
      Y_Cam -= 5.0f;
      break;

   case GLUT_KEY_DOWN:
      Y_Cam += 5.0f;
      break;

   case GLUT_KEY_LEFT:
      X_Cam += 5.0f;
      break;

   case GLUT_KEY_RIGHT:
      X_Cam -= 5.0f;
      break;
      
   case 106: // Home - scroll up
      offset += offset_change;
      break;
      
   case 107: // End - scroll down
      offset -= offset_change;
      break;
      
   default:
      printf ("SKP: No action for %d.\n", key);
      break;
    }
}

void ourBuildTextures()
{
  glGenTextures(MAX_PIC, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  int n;
  for (n = 0; n < NUM_PIC; n++)
  {
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture[n]);
  glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 3, wid[n], hei[n], 0, GL_RGB, GL_UNSIGNED_BYTE, image[n]);

  free( image[n] );
  }

}

// Callback routine executed whenever our window is resized.  Lets us
// request the newly appropriate perspective projection matrix for
// our needs.  Try removing the gluPerspective() call to see what happens.
void cbResizeScene(int Width, int Height)
{
   // Let's not core dump, no matter what.
   if (Height == 0)
      Height = 1;

   glViewport(0, 0, Width, Height);

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.7f,200.0f);

   glMatrixMode(GL_MODELVIEW);

   Window_Width  = Width;
   Window_Height = Height;
}



// Does everything needed before losing control to the main
// OpenGL event loop.
void ourInit(int Width, int Height)
{
  int n,i;

  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  unsigned long location = 0;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);


  for (n = 0; n<NUM_PIC; n++)
  {
    jpeg_stdio_src(&cinfo, fd[n]);
    jpeg_read_header(&cinfo, 0);
    cinfo.scale_num = 1;
    cinfo.scale_denom = SCALE;
    jpeg_start_decompress(&cinfo);
    wid[n] = cinfo.output_width;
    hei[n] = cinfo.output_height;
    dep[n] = cinfo.num_components; //should always be 3
    image[n] = (unsigned char *) malloc(wid[n] * hei[n] * dep[n]);
    row_pointer[0] = (unsigned char *) malloc(wid[n] * dep[n]);
    /* read one scan line at a time */
    while( cinfo.output_scanline < cinfo.output_height )
    {
        jpeg_read_scanlines( &cinfo, row_pointer, 1 );
        for( i=0; i< (wid[n] * dep[n]); i++)
                image[n][location++] = row_pointer[0][i];
    }
    location = 0;
    fclose(fd[n]);
    jpeg_finish_decompress(&cinfo);
  }

  jpeg_destroy_decompress(&cinfo);

  ourBuildTextures();

   // Color to clear color buffer to.
   glClearColor(0.1f, 0.1f, 0.1f, 0.0f);

   // Depth to clear depth buffer to; type of test.
   glClearDepth(1.0);
   glDepthFunc(GL_LESS);

   // Enables Smooth Color Shading; try GL_FLAT for (lack of) fun.
   glShadeModel(GL_SMOOTH);

   // Load up the correct perspective matrix; using a callback directly.
   cbResizeScene(Width,Height);

   // Set up a light, turn it on.
   glLightfv(GL_LIGHT1, GL_POSITION, Light_Position);
   glLightfv(GL_LIGHT1, GL_AMBIENT,  Light_Ambient);
   glLightfv(GL_LIGHT1, GL_DIFFUSE,  Light_Diffuse);
   glEnable (GL_LIGHT1);

   // A handy trick -- have surface material mirror the color.
   glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
   glEnable(GL_COLOR_MATERIAL);
   
}


// The main() function.  Inits OpenGL.  Calls our own init function,
// then passes control onto OpenGL.
int main(int argc,char **argv)
{
   if (argc < 2)
   {
     printf("Usage: %s <directory>\n", argv[0]);
     return 1;
   }
   
   strcpy(dirString, argv[1]); 
   
   DIR  *dip, *dic;
   struct dirent *dit;
   int n;
   char *dir_and_file[MAX_PIC];
   
   if ((dip = opendir(argv[1])) == NULL || (dic = opendir(".")) == NULL)
   {
     fprintf(stderr, "can't open directory %s\n", argv[1]);
     return 1;
   }
   
   printid = 0;
   
   while((dit = readdir(dic)) != NULL)
   {
     if(strstr(dit->d_name, "out_") != NULL)
     {
       printid++;
     }
   }
   closedir(dic);
   
   NUM_PIC = 0;
   
   while ((dit = readdir(dip)) != NULL)
   {
     if(strstr(dit->d_name, ".JPG") != NULL || strstr(dit->d_name, ".jpg") != NULL || strstr(dit->d_name, ".jpeg") != NULL || strstr(dit->d_name, ".JPEG") != NULL)
     {
       dir_and_file[NUM_PIC] = malloc(100);
       strcpy(dir_and_file[NUM_PIC], argv[1]);
       strcat(dir_and_file[NUM_PIC], "/");
       strcat(dir_and_file[NUM_PIC], dit->d_name);
//       fd[NUM_PIC] = fopen(dir_and_file, "rb"); // Opens up the files in arbitiary order, no good
       NUM_PIC++;
//       printf("READ %s %d\n", dir_and_file[NUM_PIC-1], NUM_PIC - 1);
     }

   }

   n = 0;

  sort_files(dir_and_file, NUM_PIC);

//  for(n=0; n<NUM_PIC; n++);
   while(n<NUM_PIC)
   {
     fd[n] = fopen(dir_and_file[n], "rb");
//     printf("OPEN %s %d\n", dir_and_file[n], n);
     n++;
   }
   closedir(dip);   
    
    
   glutInit(&argc, argv);
  
   // To see OpenGL drawing, take out the GLUT_DOUBLE request.
   glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
   glutInitWindowSize(Window_Width, Window_Height);

   // Open a window
   Window_ID = glutCreateWindow( PROGRAM_TITLE );

   // Register the callback function to do the drawing.
   glutDisplayFunc(&cbRenderScene);

   // If there's nothing to do, draw.
   glutIdleFunc(&cbRenderScene);

   // It's a good idea to know when our window's resized.
   glutReshapeFunc(&cbResizeScene);

   // And let's get some keyboard input.
   glutKeyboardFunc(&cbKeyPressed);
   glutSpecialFunc(&cbSpecialKeyPressed);

   // Process mouse input
   glutMotionFunc(&cbMouseHeldMoving);
   glutMouseFunc(&cbMouseClick);
   glutPassiveMotionFunc(&cbMouseMoving); 
   
   // OK, OpenGL's ready to go.  Let's call our own init function.
   ourInit(Window_Width, Window_Height);

   // Print out a bit of help dialog.
   printf("\n" PROGRAM_TITLE "\n\n\
Use arrow keys to move.\n\
Page up/down will move away from/towards camera.\n\n\
Use first letter of shown display mode settings to alter.\n\n\
Q or [Esc] to quit; OpenGL window must have focus for input.\n");

   // Pass off control to OpenGL.
   // Above functions are called as appropriate.
   glutMainLoop();

   return 1;
}

