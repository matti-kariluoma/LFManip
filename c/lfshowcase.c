// OpenGL routines to play with light field camera images
//
// OpenGL setups and other misc. repurposed from Chris Halsall
// (chalsall@chalsall.com) whose original work was for the
// O'Reilly Network on Linux.com (oreilly.linux.com).
// May 2000.
//
// Matti Kariluoma <matti.kariluoma@gmail.com> Jan 2010

//compile with gcc this.c -lglut -ljpeg

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
#include <math.h>

#if !defined(GLUT_WHEEL_UP)
#  define GLUT_WHEEL_UP   3
#  define GLUT_WHEEL_DOWN 4
#endif

#if !defined(GL_TEXTURE_RECTANGLE_ARB)
#  define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif

#define SCALE 2

#define MAX_PIC 20
int NUM_PIC[2];
int DATASET = 0;

// Some global variables.

// Window and texture IDs, window width and height.
int Texture_ID;
int Window_ID;
int Window_Width = 600;
int Window_Height = 600;

// Our display mode settings.


GLuint img[3][MAX_PIC];
GLuint texture[3][MAX_PIC];
float delta = 0.001f;
FILE *fd[2][MAX_PIC];
int wid[2][MAX_PIC],hei[2][MAX_PIC],dep[2][MAX_PIC];
unsigned char *image[2][MAX_PIC];
float aligned_feature[2*MAX_PIC];
float diffx[MAX_PIC];
float diffy[MAX_PIC];
float offset = 0;
float offset_change = 0.001f;
float offset_delta = 10;
float offset_on = 1;
float offset_off = 0;
float rotate_offset = 0;
float    offset_max = 0.85f;
float    offset_min = -0.35;
char dirString[256];
int ZOOM = FALSE;
int DONE_ZOOM = TRUE;

// Object and scene global variables.

#define ZOOM_NORMAL -40.0f
#define ZOOM_ITER 20
int ZOOM_STEPS = ZOOM_ITER;

// Cube position and rotation speed variables.
float X_Rot   = 90.0f;
float Y_Rot   = 0.0f;
float Z_Rot   = 0.0f;
float Z_Off   = ZOOM_NORMAL;
float X_Cam   = 0.0f;
float Y_Cam   = 0.0f;

float Light_Ambient[]=  { 1.0f, 1.0f, 1.0f, 1.0f };
float Light_Diffuse[]=  { 10.2f, 10.2f, 10.2f, 2.0f };
float Light_Position[]= { 0.0f, 0.0f, -100.0f, 1.0f };

int loadOffsets()
{
  FILE *infile;
  int i, garbage;
  
  char outname[300];
  strcpy(outname, "lf");
  strcat(outname, "/");
  strcat(outname, "offsets.txt");
  
  infile = fopen(outname, "r");
  
  if (infile == NULL)
    return FALSE;
  
  //first value one each line is the number of images in this set
  if (EOF == fscanf(infile, "%d", &NUM_PIC[DATASET])) //Let's allow the user to do strange things
    return FALSE;
  //else printf("Loaded %d\n",NUM_PIC);
    
  //First line is the x-coord in each image
  for (i=0; i<NUM_PIC[DATASET]; i++)
    if( EOF == fscanf(infile, "%f", &aligned_feature[2*i]))
      return FALSE;
    //else printf("Loaded %f\n",aligned_feature[2*i]);
  
  if( EOF == fscanf(infile, "%d", &garbage))
    return FALSE;
  if( NUM_PIC[DATASET] != garbage)
    return FALSE;
  //else printf("Skipped %d\n",garbage);
  //Second line is the y-coord in each image
  for (i=0; i<NUM_PIC[DATASET]; i++)
    if( EOF == fscanf(infile, "%f", &aligned_feature[2*i+1]))
      return FALSE;
    //else printf("Loaded %f\n",aligned_feature[2*i+1]);
  
  if( EOF == fscanf(infile, "%d", &garbage))
    return FALSE;
  if( NUM_PIC[DATASET] != garbage)
    return FALSE;
  //else printf("Skipped %d\n",garbage);
  //Third line is the calculated differences between each image in the x
  //We could calculate it from the above data...
  for (i=0; i<NUM_PIC[DATASET]; i++)
    if( EOF == fscanf(infile, "%f", &diffx[i]))
      return FALSE;
    //else printf("Loaded %f\n",diffx[i]);

  if( EOF == fscanf(infile, "%d", &garbage))
    return FALSE;
  if( NUM_PIC[DATASET] != garbage)
    return FALSE;
  //else printf("Skipped %d\n",garbage);
  //Last line is the calculate diffs in each y direction
  for (i=0; i<NUM_PIC[DATASET]; i++)
    if( EOF == fscanf(infile, "%f", &diffy[i]))
      return FALSE;
    //else printf("Loaded %f\n",diffy[i]);
      
  return TRUE;
  
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

void gravitate(float x, float y)
{
    if (X_Cam > x+1.0f)
      X_Cam -= 0.5f;
    else if (X_Cam > x+0.1f)
      X_Cam -= 0.05f;
    else if (X_Cam < x-1.0f)
      X_Cam += 0.5f;
    else if (X_Cam < x-0.1f)
      X_Cam += 0.05f;
      
    if (Y_Cam > y+1.0f)
      Y_Cam -= 0.5f;
    else if (Y_Cam > y+0.1f)
      Y_Cam -= 0.05f;
    else if (Y_Cam < y-1.0f)
      Y_Cam += 0.5f;
    else if (Y_Cam < y-0.1f)
      Y_Cam += 0.05f;
}

void smooth_pan()
{
  
  if (ZOOM)
  {
     if ( (offset+.108) > -0.143f )
     {
       gravitate(3.36303857f - 13.177629f*(0-(offset+.108)), 2.50690389f*powf(.216129672,-(offset+.108)));
     } else
     if ( (offset+.108) <= -0.143f )
     {
       gravitate(16.777777f+111.11111f*(offset+.108), 1.0f);
       if (X_Cam < -7.0f)
         X_Cam = -7.0f;
     }
     
  } else
  {
  gravitate(0.0f, 0.0f);
  }
  
  usleep(1);
  return;
}

void cbRenderScene( void )
{

   // Need to manipulate the ModelView matrix to move our model around.
   glMatrixMode(GL_MODELVIEW);

   // Reset to 0,0,0; no rotation, no scaling.
   glLoadIdentity();

  #define ZOOM_GRAN 1.0f

   if (ZOOM && ZOOM_STEPS > 0)
   {
     Z_Off += ZOOM_GRAN;
     ZOOM_STEPS--;
   } else
   if (Z_Off > ZOOM_NORMAL && DONE_ZOOM)
   {
     Z_Off -= ZOOM_GRAN;
     
     if (X_Cam > 1.0f)
      X_Cam -= 1.0f;
     else if (X_Cam > 0.5f)
      X_Cam -= 0.3f;
     else if (X_Cam < -1.0f)
      X_Cam += 1.0f;
     else if (X_Cam < -0.5f)
      X_Cam += 0.3f;
     if (Y_Cam > 1.0f)
      Y_Cam -= 1.0f;
     else if (Y_Cam > 0.5f)
      Y_Cam -= 0.3f;
     else if (Y_Cam < -1.0f)
      Y_Cam += 1.0f;
     else if (Y_Cam < -0.5f)
      Y_Cam += 0.3f;
   }


  smooth_pan();
 
   if (Z_Off <= ZOOM_NORMAL && ZOOM_STEPS <= 0)
     ZOOM = FALSE;

   // Move the object back from the screen.
   glTranslatef(X_Cam,Y_Cam,Z_Off);

   // Rotate the calculated amount.
   glRotatef(X_Rot,1.0f,0.0f,0.0f);
   glRotatef(Y_Rot,0.0f,1.0f,0.0f);
   glRotatef(Z_Rot,0.0f,0.0f,1.0f);


   // Clear the color and depth buffers.
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


   int i;

   for(i=0; i < NUM_PIC[DATASET]; i++)
   {
    glTranslatef((diffx[i]/SCALE)/20.0f, 0.0f, (diffy[i]/SCALE)/20.0f);
    glPushMatrix();
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture[DATASET][i]);
    glTranslatef((offset*i),0.0f,0.0f);
    glBegin(GL_QUADS);
      glColor4f(0.8f, 0.8f, 0.8f, (1.0f/(i+1)) );

      glTexCoord2f(0.0f, 0.0f);
       glVertex3f(0.0f-wid[DATASET][i]/40.0f, i/10.0f,0.0f-hei[DATASET][i]/40.0f);

      glTexCoord2f(wid[DATASET][i], 0.0f);
       glVertex3f(wid[DATASET][i]/40.0f, i/10.0f,0.0f-hei[DATASET][i]/40.0f);

      glTexCoord2f(wid[DATASET][i], hei[DATASET][i]);
       glVertex3f(wid[DATASET][i]/40.0f, i/10.0f,hei[DATASET][i]/40.0f);

      glTexCoord2f(0.0f, hei[DATASET][i]);
       glVertex3f(0.0f-wid[DATASET][i]/40.0f, i/10.0f,hei[DATASET][i]/40.0f);

    glEnd();
    glPopMatrix();
   }

   // All done drawing.  Let's show it.
   glutSwapBuffers();

  if (ZOOM)
    usleep(10);
  else
    ZOOM_STEPS = ZOOM_ITER;
}


// Callback function when the mouse is clicked
void cbMouseClick(int button, int state, int x, int y )
{
  if (state == GLUT_DOWN)
	switch (button) {
	case GLUT_WHEEL_UP:
	  if (offset < offset_max)
	  {
		offset += offset_change;
    //printf("%f\n", offset);
		//usleep(10);
	  }
		break;
	case GLUT_WHEEL_DOWN:
	  if (offset > offset_min)
	  {
		offset -= offset_change;    
    //printf("%f\n", offset);
		//usleep(10);
	  }
		break;
//  case GLUT_LEFT_BUTTON:
//    DATASET = 0; break;
  case GLUT_MIDDLE_BUTTON:
  if(!ZOOM)
  {
    DONE_ZOOM = FALSE;
    ZOOM = TRUE;
  }
/*
    DATASET++;
    DATASET = DATASET % 3; 
    switch (DATASET)
  {
    case 0:
    Z_Off = -40.0f;
    X_Cam = 0.0f;
    offset_max = 0.85f;
    offset_min = -0.35;
    offset = 0.0f;
    break;
    case 1:
    Z_Off = -55.0f;
    X_Cam = 0.0f;
    offset_max = 0.33f;
    offset_min = -2.017f;
    offset = 0.0f;
    break;
    case 2:
    Z_Off = -55.0f;
    X_Cam = -5.0f;
    offset_max = 1.68f;
    offset_min = -0.50f;
    offset = 0.0f;
    break;
    default: break;
  }
*/
    break;
//  case GLUT_RIGHT_BUTTON:
//    DATASET = 2; break;
  //case GLUT_LEFT_BUTTON:
    //glDisable(GL_LIGHTING); break;
  //case GLUT_RIGHT_BUTTON:
    //glEnable(GL_LIGHTING); break;
	default: break;
}
if (state == GLUT_UP && button == GLUT_MIDDLE_BUTTON)
  DONE_ZOOM = TRUE;
  
  //printf("%f\n",offset);
}
// Callback function when the mouse is clicked and dragged
/*
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
      Y_Cam -= 1.0f;
      break;

   case GLUT_KEY_DOWN:
      Y_Cam += 1.0f;
      break;

   case GLUT_KEY_LEFT:
      X_Cam += 1.0f;
      break;

   case GLUT_KEY_RIGHT:
      X_Cam -= 1.0f;
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
    
    printf("X:%f\tY:%f\n", X_Cam, Y_Cam);
}
*/

void ourBuildTextures()
{
  

  int n,i;
  for (i = 0; i<2; i++)
  {
  glGenTextures(MAX_PIC, texture[i]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (n = 0; n < NUM_PIC[i]; n++)
  {
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture[i][n]);
  glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 3, wid[i][n], hei[i][n], 0, GL_RGB, GL_UNSIGNED_BYTE, image[i][n]);

  free( image[i][n] );
  }
  }

}



// ------
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


// ------
// Does everything needed before losing control to the main
// OpenGL event loop.

void ourInit(int Width, int Height)
{
   int n,i,j;

  loadOffsets();

  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  unsigned long location = 0;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);

  for (i = 0; i<2; i++)
  for (n = 0; n<NUM_PIC[i]; n++)
  {
    jpeg_stdio_src(&cinfo, fd[i][n]);
    jpeg_read_header(&cinfo, 0);
    cinfo.scale_num = 1;
    cinfo.scale_denom = SCALE;
    jpeg_start_decompress(&cinfo);
    wid[i][n] = cinfo.output_width;
    hei[i][n] = cinfo.output_height;
    dep[i][n] = cinfo.num_components; //should always be 3
    image[i][n] = (unsigned char *) malloc(wid[i][n] * hei[i][n] * dep[i][n]);
    row_pointer[0] = (unsigned char *) malloc(wid[i][n] * dep[i][n]);
    /* read one scan line at a time */
    while( cinfo.output_scanline < cinfo.output_height )
    {
        jpeg_read_scanlines( &cinfo, row_pointer, 1 );
        for( j=0; j< (wid[i][n] * dep[i][n]); j++)
                image[i][n][location++] = row_pointer[0][j];
    }
    location = 0;
    fclose(fd[i][n]);
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
//   glLightfv(GL_LIGHT1, GL_DIFFUSE,  Light_Diffuse);
   glEnable (GL_LIGHT1);

   // A handy trick -- have surface material mirror the color.
   glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
   glEnable(GL_COLOR_MATERIAL);
   
   glutSetCursor(GLUT_CURSOR_NONE);
   

      glEnable(GL_TEXTURE_RECTANGLE_ARB);


      glDisable(GL_LIGHTING);
      glEnable(GL_LIGHTING);

       glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // If we're blending, we don't want z-buffering.

       glDisable(GL_DEPTH_TEST);
       glEnable(GL_BLEND);



       glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,
                                           GL_NEAREST_MIPMAP_NEAREST);
       glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  
   
}

// ------
// The main() function.  Inits OpenGL.  Calls our own init function,
// then passes control onto OpenGL.

int main(int argc,char **argv)
{

   
   DIR  *dip, *dic;
   struct dirent *dit;
   int n,i;
   char *dir_and_file[2][MAX_PIC];
   
   for (i = 0; i< 2; i ++)
   {
     switch(i)
     {
     case 0: strcpy(dirString, "lf/"); break;
     case 1: strcpy(dirString, "land/"); break;
     case 2: strcpy(dirString, "path/"); break;
     default: strcpy(dirString, "lf/"); break;
     }
     
   if ((dip = opendir(dirString)) == NULL || (dic = opendir(".")) == NULL)
   {
     fprintf(stderr, "can't open directory %s\n", dirString);
     return 1;
   }
   
   
   NUM_PIC[i] = 0;
   
   while ((dit = readdir(dip)) != NULL)
   {
     if(strstr(dit->d_name, ".JPG") != NULL || strstr(dit->d_name, ".jpg") != NULL || strstr(dit->d_name, ".jpeg") != NULL || strstr(dit->d_name, ".JPEG") != NULL)
     {
       int index = NUM_PIC[i];
       dir_and_file[i][index] = malloc(100);
       strcpy(dir_and_file[i][index], dirString);
       strcat(dir_and_file[i][index], dit->d_name);
//       fd[NUM_PIC] = fopen(dir_and_file, "rb"); // Opens up the files in arbitiary order, no good
       NUM_PIC[i]++;
//       printf("READ %s %d\n", dir_and_file[NUM_PIC-1], NUM_PIC - 1);
     }

   }

   n = 0;

  sort_files(dir_and_file[i], NUM_PIC[i]);

//  for(n=0; n<NUM_PIC; n++);
   while(n<NUM_PIC[i])
   {
     fd[i][n] = fopen(dir_and_file[i][n], "rb");
//     printf("OPEN %s %d\n", dir_and_file[n], n);
     n++;
   }
   closedir(dip);

//   printf("%s %d\n", "done", NUM_PIC - 1);
 }
    
   glutInit(&argc, argv);
  

   // To see OpenGL drawing, take out the GLUT_DOUBLE request.
   glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
   glutInitWindowSize(Window_Width, Window_Height);

   // Open a window
   Window_ID = glutCreateWindow( "Light Field Viewer" );

   // Register the callback function to do the drawing.
   glutDisplayFunc(&cbRenderScene);

   // If there's nothing to do, draw.
   glutIdleFunc(&cbRenderScene);

   // It's a good idea to know when our window's resized.
   glutReshapeFunc(&cbResizeScene);


   //glutSpecialFunc(&cbSpecialKeyPressed);
   glutMouseFunc(&cbMouseClick);

   // OK, OpenGL's ready to go.  Let's call our own init function.
   ourInit(Window_Width, Window_Height);

   // Print out a bit of help dialog.
   printf("Light Field Viewer\n");

   // Pass off control to OpenGL.
   // Above functions are called as appropriate.
   glutMainLoop();

   return 1;
}

