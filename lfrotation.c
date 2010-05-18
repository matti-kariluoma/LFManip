// OpenGL routines to play with light field camera images
//
// OpenGL setups and other misc. repurposed from Chris Halsall
// (chalsall@chalsall.com) whose original work was for the
// O'Reilly Network on Linux.com (oreilly.linux.com).
// May 2000.
//
// Matti Kariluoma <matti.kariluoma@gmail.com> Jan 2010

//32-bit compile with gcc lfmanip.c -o lfmanip -lm -lglut -ljpeg -lcv -lhighgui -I/usr/include/opencv
//64-bit compile with gcc lfmanip.c -o lfmanip -lm -lglut -ljpeg -lcv -lhighgui -I/usr/include/opencv -Dx64

#define PROGRAM_TITLE "Simple Light Field Manipulator"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>    // For our FPS stats.
#include <GL/gl.h>   // OpenGL itself.
#include <GL/glu.h>  // GLU support library.
#include <GL/glut.h> // GLUT support library.
#include <jpeglib.h>
#include <sys/types.h>
#include <dirent.h>
#include "cv.h"
#include <pthread.h>

#if !defined(GLUT_WHEEL_UP)
#  define GLUT_WHEEL_UP   3
#  define GLUT_WHEEL_DOWN 4
#endif

#if !defined(GL_TEXTURE_RECTANGLE_ARB)
#  define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif

#define SCALE 2

#define MAX_PIC 50
int NUM_PIC  = 9;

typedef struct linkedFloatVector {
  float x;
  float y;
  int i;
  struct linkedFloatVector *next;
} linkedFloatVector;

typedef struct linkedInteger {
  int i;
  struct linkedInteger *next;
} linkedInteger;

typedef struct parallelPack {
  const CvSeq* keypoints;
  const CvSeq* descriptors;
  CvSURFParams params;
  linkedFloatVector* pair;
  char* file;
} parallelPack;
// Some global variables.

// Window and texture IDs, window width and height.
int Texture_ID;
int Window_ID;
int Window_Width = 600;
int Window_Height = 600;

// Our display mode settings.
int Light_On = 0;
int Blend_On = 1;
int Texture_On = 1;
int Filtering_On = 0;
int Alpha_Add = 0;
int Render_mode = 0;
char Render_string[80] = "Additive";

int Curr_TexMode = 0;
char *TexModesStr[] = {"GL_MODULATE","GL_DECAL","GL_BLEND","GL_REPLACE"};
GLint TexModes[] = {GL_MODULATE,GL_DECAL,GL_BLEND,GL_REPLACE};
GLuint arrow;
GLuint img[MAX_PIC];
GLuint texture[MAX_PIC];
GLuint features[MAX_PIC];
GLuint more_features[MAX_PIC];
float delta = 0.001f;
int wid[MAX_PIC],hei[MAX_PIC],dep[MAX_PIC];
unsigned char *image[MAX_PIC];
char *filenames[MAX_PIC];
float diffx[MAX_PIC];
float diffy[MAX_PIC];
float rotate[MAX_PIC];
float aligned_feature[MAX_PIC*2];
int pivot;
float offset = 0;
float offset_change = 0.001f;
float offset_delta = 20;
float offset_on = 1;
float offset_off = 0;
float rotate_offset = 0.0f;
int printid = 0;
char dirString[4098];
int menu_selected = 0;
#define FILE_ENTRIES 2
#define EDIT_ENTRIES 1
#define VIEW_ENTRIES 7
#define HELP_ENTRIES 2
int file[FILE_ENTRIES];
int edit[EDIT_ENTRIES];
int view[VIEW_ENTRIES];
int help[HELP_ENTRIES];

int box[4] = {0,0,0,0};
int dragged = 0;

pthread_mutex_t extracting = PTHREAD_MUTEX_INITIALIZER;

//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t makestorage = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t removestorage = PTHREAD_MUTEX_INITIALIZER;

int Text_on = 0; //OSD display (fps, commands)
int global_state = 0;  //set the state between selection (0 == false) and light field manipulation (1 == true)
int display_features = FALSE;
int display_more_features = FALSE;
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
float Light_Ambient[]=  { 1.0f, 1.0f, 1.0f, 1.0f };
float Light_Diffuse[]=  { 10.2f, 10.2f, 10.2f, 2.0f };
float Light_Position[]= { 0.0f, 0.0f, -100.0f, 1.0f };

// Routine to keep the render code cleaner; not used for the default render mode
float slide(int id)
{
  float pos;

  if (id == pivot)
    pos = 0.0f;
  else if (id > pivot)
    pos = (id % pivot) + 1.0f;
  else if (id < pivot)
    pos = -(id % pivot) - 1.0f;

// printf("id: %d pos: %f\n", id, pos);

//  pos /= 10.0f;

  return pos;
}

void saveOffsets()
{
  FILE *outfile;
  
  char outname[300];
  strcpy(outname, dirString);
  strcat(outname, "/");
  strcat(outname, "offsets.txt");
  
  outfile = fopen(outname, "a"); //append
  
  fprintf(outfile, "%d ", NUM_PIC);
  
  int i;
  for (i=0; i<NUM_PIC; i++)
    fprintf(outfile, "\t%f ", aligned_feature[2*i]);
  
  fprintf(outfile, "\n%d ", NUM_PIC);
  
  for (i=0; i<NUM_PIC; i++)
    fprintf(outfile, "\t%f ", aligned_feature[2*i+1]);
  
  fprintf(outfile, "\n%d ", NUM_PIC);
  
  for (i=0; i<NUM_PIC; i++)
    fprintf(outfile, "\t%f ", diffx[i]);
    
  fprintf(outfile, "\n%d ", NUM_PIC);
  
  for (i=0; i<NUM_PIC; i++)
    fprintf(outfile, "\t%f ", diffy[i]);
  
  fprintf(outfile, "\n\n");
  
  fclose(outfile);
}

int loadOffsets()
{
  FILE *infile;
  int i, garbage;
  
  char outname[300];
  strcpy(outname, dirString);
  strcat(outname, "/");
  strcat(outname, "offsets.txt");
  
  infile = fopen(outname, "r");
  
  if (infile == NULL)
    return FALSE;
  
  //first value one each line is the number of images in this set
  if (EOF == fscanf(infile, "%d", &NUM_PIC)) //Let's allow the user to do strange things
    return FALSE;
  //else printf("Loaded %d\n",NUM_PIC);
    
  //First line is the x-coord in each image
  for (i=0; i<NUM_PIC; i++)
    if( EOF == fscanf(infile, "%f", &aligned_feature[2*i]))
      return FALSE;
    //else printf("Loaded %f\n",aligned_feature[2*i]);
  
  if( EOF == fscanf(infile, "%d", &garbage))
    return FALSE;
  if( NUM_PIC != garbage)
    return FALSE;
  //else printf("Skipped %d\n",garbage);
  //Second line is the y-coord in each image
  for (i=0; i<NUM_PIC; i++)
    if( EOF == fscanf(infile, "%f", &aligned_feature[2*i+1]))
      return FALSE;
    //else printf("Loaded %f\n",aligned_feature[2*i+1]);
  
  if( EOF == fscanf(infile, "%d", &garbage))
    return FALSE;
  if( NUM_PIC != garbage)
    return FALSE;
  //else printf("Skipped %d\n",garbage);
  //Third line is the calculated differences between each image in the x
  //We could calculate it from the above data...
  for (i=0; i<NUM_PIC; i++)
    if( EOF == fscanf(infile, "%f", &diffx[i]))
      return FALSE;
    //else printf("Loaded %f\n",diffx[i]);

  if( EOF == fscanf(infile, "%d", &garbage))
    return FALSE;
  if( NUM_PIC != garbage)
    return FALSE;
  //else printf("Skipped %d\n",garbage);
  //Last line is the calculate diffs in each y direction
  for (i=0; i<NUM_PIC; i++)
    if( EOF == fscanf(infile, "%f", &diffy[i]))
      return FALSE;
    //else printf("Loaded %f\n",diffy[i]);
      
  return TRUE;
  
}

void saveJpeg(char *filename, int x, int y, int w, int h)
{
  int i;
  
  unsigned char *data = malloc(w*h*3);

// Produces an upside down image
  glPixelStorei(GL_PACK_ALIGNMENT, 1); //stops ogengl from padding our data
  glReadPixels(x,y,w,h,GL_RGB,GL_UNSIGNED_BYTE,data);
  
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  FILE *outjpeg;
  JSAMPROW row_pointer[1];
  int row_stride;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  
  if ((outjpeg = fopen(filename, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    exit(1);
  }

  jpeg_stdio_dest(&cinfo, outjpeg);
  cinfo.image_width = w;
  cinfo.image_height = h;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB; 
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 93, TRUE);

  jpeg_start_compress(&cinfo, TRUE);
  row_stride = w * 3;
  
  for( i=cinfo.image_height-1; i>=0; i--)
  {
    row_pointer[0] = &data[i * row_stride];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);

  fclose(outjpeg);
  
  free(data);
  
  jpeg_destroy_compress(&cinfo);

}

void loadJpeg(char *filename, int index)
{
  int i;

  FILE *fd;
  
  fd = fopen(filename, "rb");


  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  unsigned long location = 0;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);


    jpeg_stdio_src(&cinfo, fd);
    jpeg_read_header(&cinfo, 0);
    cinfo.scale_num = 1;
    cinfo.scale_denom = SCALE;
    jpeg_start_decompress(&cinfo);
    wid[index] = cinfo.output_width;
    hei[index] = cinfo.output_height;
    dep[index] = cinfo.num_components; //should always be 3
    image[index] = (unsigned char *) malloc(wid[index] * hei[index] * dep[index]);
    row_pointer[0] = (unsigned char *) malloc(wid[index] * dep[index]);
    /* read one scan line at a time */
    while( cinfo.output_scanline < cinfo.output_height )
    {
        jpeg_read_scanlines( &cinfo, row_pointer, 1 );
        for( i=0; i< (wid[index] * dep[index]); i++)
                image[index][location++] = row_pointer[0][i];
    }
    fclose(fd);
    jpeg_finish_decompress(&cinfo);

  jpeg_destroy_decompress(&cinfo);

}

void printscreen()
{
  
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

  saveJpeg(outname, 0, 0, Window_Height, Window_Width);
  
  return;
}

void changePivot(int n)
{
  
  if ( pivot > 0 && n < 0       ||
       pivot < NUM_PIC-1 && n > 0  )
  {
  pivot += n;
  //TODO: animate the change!
  }
  
  //printf("Image changed to #%d\n", pivot);
  
  return;
}

linkedFloatVector* getCoords(int search, linkedFloatVector *data)
{
  linkedFloatVector *current = data;
  
  while (current)
  {
    if (current->i == search)
      return current;
    
    current = current->next;
  }
  
  return NULL;
}

void drawMatchedFeatures(linkedInteger* search, linkedFloatVector** data)
{
  float scale_x;
  float scale_y;

  linkedFloatVector *current;
  linkedInteger *searcher;
  int i;
  for (i=0; i<NUM_PIC; i++)
  {
    glNewList(more_features[i], GL_COMPILE);
    glEnable(GL_POINT_SMOOTH);
    glBegin(GL_POINTS);
      
      glColor4f(0,0,.9,1);
      //glVertex3f(0,0,0);
      //glVertex3f(56,0,42);
      searcher = search;
      while (searcher)
      {
        current = data[i];
        while (current)
        {
          if (searcher->i == current->i)
            {
                if (hei[i] < wid[i])
                {
                  scale_x = wid[i] * SCALE / 56.0f;
                  scale_y = hei[i] * SCALE / 42.0f;
                } else
                {
                  scale_x = wid[i] * SCALE / 42.0f;
                  scale_y = hei[i] * SCALE / 56.0f;
                }
              glVertex3f(current->x/scale_x, 0.0f, current->y/scale_y);
              current = NULL;
            } else
              current = current->next;
        }
        searcher = searcher->next;
      }
    
    glEnd();
    glDisable(GL_POINT_SMOOTH);
    glEndList();
  }
  
  display_more_features = TRUE;
  //printf("Feature points drawn\n");
  return;
}

void drawFeatures(int index, linkedFloatVector* coords)
{
  
  float scale_x;
  float scale_y;
  
  if (hei[index] < wid[index])
  {
    scale_x = wid[index] * SCALE / 56.0f;
    scale_y = hei[index] * SCALE / 42.0f;
  } else
  {
    scale_x = wid[index] * SCALE / 42.0f;
    scale_y = hei[index] * SCALE / 56.0f;
  }

  glNewList(features[index], GL_COMPILE);
  glEnable(GL_POINT_SMOOTH);
  glBegin(GL_POINTS);
    
    glColor4f(.9,0,0,1);
    //glVertex3f(0,0,0);
    //glVertex3f(56,0,42);
    while (coords)
    {
      
    glVertex3f(coords->x/scale_x, 0.0f, coords->y/scale_y);
    
    coords = coords->next;
    }
  
  glEnd();
  glDisable(GL_POINT_SMOOTH);
  glEndList();

  display_features = TRUE;
  //printf("Feature points drawn\n");
  return;
}

double compareSURFDescriptors( const float* d1, const float* d2, double best, int length )
{
    //cribbed directly from find_obj.cpp in the openCV examples

    double total_cost = 0;
    assert( length % 4 == 0 );
    
    double t0,t1,t2,t3;
    
    int i;
    for(i = 0; i < length; i += 4 )
    {
        t0 = d1[i] - d2[i];
        t1 = d1[i+1] - d2[i+1];
        t2 = d1[i+2] - d2[i+2];
        t3 = d1[i+3] - d2[i+3];
        total_cost += t0*t0 + t1*t1 + t2*t2 + t3*t3;
        if( total_cost > best )
            break;
    }
    return total_cost;
}

int naiveNearestNeighbor( const float* vec, int laplacian,
                          const CvSeq* model_keypoints,
                          const CvSeq* model_descriptors )
{
    //cribbed directly from find_obj.cpp in the openCV examples

    int length = (int)(model_descriptors->elem_size/sizeof(float));
    int i, neighbor = -1;
    double d, dist1 = 1e6, dist2 = 1e6;
    CvSeqReader reader, kreader;
    cvStartReadSeq( model_keypoints, &kreader, 0 );
    cvStartReadSeq( model_descriptors, &reader, 0 );

    for( i = 0; i < model_descriptors->total; i++ )
    {
        const CvSURFPoint* kp = (const CvSURFPoint*)kreader.ptr;
        const float* mvec = (const float*)reader.ptr;
        CV_NEXT_SEQ_ELEM( kreader.seq->elem_size, kreader );
        CV_NEXT_SEQ_ELEM( reader.seq->elem_size, reader );
        if( laplacian != kp->laplacian )
            continue;
        d = compareSURFDescriptors( vec, mvec, dist2, length );
        if( d < dist1 )
        {
            dist2 = dist1;
            dist1 = d;
            neighbor = i;
        }
        else if ( d < dist2 )
            dist2 = d;
    }
    if ( dist1 < 0.6*dist2 )
        return neighbor;
    return -1;
}

int compareSURF(const CvSeq* selKeypoints, const CvSeq* selDescriptors,
                const CvSeq* keypoints, const CvSeq* descriptors, linkedFloatVector* out)
{
  
  //cribbed directly from find_obj.cpp in the openCV examples
  
  CvSeqReader keyReader, descReader;
  cvStartReadSeq(selKeypoints, &keyReader, 0);
  cvStartReadSeq(selDescriptors, &descReader, 0);
  const CvSURFPoint* keypoint;
  const float* descriptor;
  int i;
    
  linkedFloatVector *head, *current;
  head = NULL;
  
  for (i = 0; i < selDescriptors->total; i++)
    {
        //const CvSURFPoint* keypoint = (const CvSURFPoint*)cvGetSeqElem( selKeypoints, i);
        //const CvSeq* descriptor = (const CvSeq*)cvGetSeqElem( selDescriptors, i);
        //float* descVector = (float*)cvGetSeqElem(descriptor,0);
        //const float* descriptor = (const float*)cvGetSeqElem( selDescriptors, i);
        //const float* descriptor = (const float*) CV_NEXT_SEQ_ELEM( selDescriptors, i);
        keypoint = (const CvSURFPoint*)keyReader.ptr;
        descriptor = (const float*)descReader.ptr;
        CV_NEXT_SEQ_ELEM( keyReader.seq->elem_size, keyReader );
        CV_NEXT_SEQ_ELEM( descReader.seq->elem_size, descReader );
        //printf("Object current: %d, %f\n", keypoint->size, descriptor[0]);

        int nearest_neighbor = naiveNearestNeighbor( descriptor, keypoint->laplacian, keypoints, descriptors );
        if( nearest_neighbor >= 0 )
        {
            //current[i][0] = i; 
            //current[i][1] = nearest_neighbor;
            
            current = malloc(sizeof(linkedFloatVector));
            current->x = ((const CvSURFPoint*)cvGetSeqElem(keypoints, nearest_neighbor))->pt.x;
                    //- ((const CvSURFPoint*)cvGetSeqElem(selKeypoints, i))->pt.x;
            current->y = ((const CvSURFPoint*)cvGetSeqElem(keypoints, nearest_neighbor))->pt.y;
                   // - ((const CvSURFPoint*)cvGetSeqElem(selKeypoints, i))->pt.y;
            current->i = i; //save the index of which selection point we map to for later sorting
            current->next = head;
            //printf("%f,%f\n", current->x, current->y);
            head = current;
          //this makes the list look like: head->...->current->next->...->NULL
        }
    }

/*
  current = head;
  while(current)
  {
    printf("%f,%f\n", current->x, current->y);
    current = current->next;
  }
*/
/*
  float x,y;
    
  //first initalize x,y
  linkedFloatVector *temp, *min;
  float tx, ty;
  int n = 0;
  
  //sort, least value first
  current = head;
  while(current)
  {
    min = current;
    temp = current;
    
      if(current->next)
      while (current)
      {
        if ( current->x < min->x )
          min = current;
        current = current->next;
      }
      
    current = temp;
    tx = min->x;
    ty = min->y;
    min->x = current->x;
    min->y = current->y;
    current->x = tx;
    current->y = ty;
    
    current = current->next;
    n++;
  }
  */
/*   
  current = head;
  while(current)
  {
    printf("%f,%f\n", current->x, current->y);
    current = current->next;
  }
*/
/*
  current = head;
  for (i=0; i<n/2; i++) //advance to the middle of our linked list
    current = current->next;
    
  x = current->x; //initialize x,y, to the median values.
  y = current->y;

  int numAccept=0, numReject=0;
  //now search for others to find the "true" (averaged) offset
  current = head;
  while (current)
  {
      if ((current->x) > (x - epsilon) && 
          (current->x) < (x + epsilon)    )
      {
        x += current->x;
        y += current->y;
        x /= 2.0f;
        y /= 2.0f;
        //printf("Difference: %f,%f\n",xi - xs, yi - ys);
        numAccept++;
      }
      else 
      {
        //printf("Difference: %f,%f (rejected)\n",xi - xs, yi - ys);
        numReject++;
      }

  current = current->next;
  }
*/
/*
  current = head;
  while(current)
  {
    temp = current;
    current = current->next;
    free(temp);
  }
*/  
  //if ( ((numAccept+numReject) / 3) < numReject || numAccept < 6)
  //if rejects are 1/3 of total or more
  //or if only 5 or less matches were made
  //if( numReject > numAccept || numAccept < 6)
  //if( numAccept < 6 )
  /*
  if( numAccept < 3 )
  {
    //printf("Not enough accepted matches\n");
    //printf("Please make a new selection\n");
    return FALSE;
  }

  
  *outx = x;
  *outy = y;
*/  
  //printf("\nTotal Difference: %f,%f\n", x, y);
  
  linkedFloatVector *temp, *min;
  float tx, ty;
  int ti;
  int n = 0;
  
  //sort, least value first

  current = head;
  while(current)
  {
    min = current;
    temp = current;
    
      if(current->next)
      while (current)
      {
        if ( current->i < min->i )
          min = current;
        current = current->next;
      }
      
    current = temp;
    tx = min->x;
    ty = min->y;
    ti = min->i;
    min->x = current->x;
    min->y = current->y;
    min->i = current->i;
    current->x = tx;
    current->y = ty;
    current->i = ti;
    //horrible way to do it; meh
    
    current = current->next;
  }
  
  i = 0;
  current = head;
  while (current)
  {
    out[i].x = current->x;
    out[i].y = current->y;
    out[i].i = current->i;
    out[i].next = &out[i+1];
    
    current = current->next;
    if (current)
      out[i].next = &out[i+1];
    else
      out[i].next = NULL;
    i++;
  }
  //out = head;
  if (!head)
    return FALSE;
  
  return TRUE;
}

void removeSURF(CvSeq* keypoints, CvSeq* descriptors, 
                int sel_x, int sel_y, int sel_width, int sel_height )
{
  CvSeqReader keyReader, descReader;
  cvStartReadSeq(keypoints, &keyReader, 0);
  cvStartReadSeq(descriptors, &descReader, 0);
  
  linkedInteger *head, *current;
  head = NULL;
  
  //FILE *logfile = fopen("log.txt", "w");
  
  //populate the list with elements we want to remove.
  int i;
  for (i = 0; i < descriptors->total; i++)
  {
    const CvSURFPoint* keypoint = (const CvSURFPoint*)keyReader.ptr;
    const float* descriptor = (const float*)descReader.ptr;
    CV_NEXT_SEQ_ELEM( keyReader.seq->elem_size, keyReader );
    CV_NEXT_SEQ_ELEM( descReader.seq->elem_size, descReader );

    if( keypoint->pt.x < sel_x                ||
        keypoint->pt.x > sel_x + sel_width    ||
        keypoint->pt.y < sel_y                ||
        keypoint->pt.y > sel_y + sel_height    )
    {
      current = malloc(sizeof(linkedInteger));
      current->i = i;
      current->next = head;
      head = current;
    //this makes the list look like: head->...->current->next->...->NULL
    } else
    {
    }
    //fprintf(logfile,"Selection before: %f, %f\n", keypoint->pt.x, keypoint->pt.y);
  }

/* 
  current = head;
  while(current)
  {
    printf("%d\n", current->i);
    current = current->next;
  }
*/

  current = head;
  while(current)
  {
    cvSeqRemove(keypoints, current->i);
    cvSeqRemove(descriptors, current->i);
    current = current->next;
  }
  //this works because the indexes are 321->...->123->119->...NULL
  //so removing from the highest index first conserves the indexing
  //for the next remove
/*  
  //done with our list, throw it away
  linkedInteger *previous;
  current = head;
  while(current)
  {
    previous = current;
    current = current->next;
    free(previous);
  }
*/  
/*
  cvStartReadSeq(keypoints, &keyReader, 0);
  cvStartReadSeq(descriptors, &descReader, 0);
  for (i = 0; i < descriptors->total; i++)
  {
    const CvSURFPoint* keypoint = (const CvSURFPoint*)keyReader.ptr;
    const float* descriptor = (const float*)descReader.ptr;
    CV_NEXT_SEQ_ELEM( keyReader.seq->elem_size, keyReader );
    CV_NEXT_SEQ_ELEM( descReader.seq->elem_size, descReader );
    
    fprintf(logfile,"Selection after: %f, %f\n", keypoint->pt.x, keypoint->pt.y);
  }

  fclose(logfile);
*/
  return;
}

int matchAllIndex(int search, linkedFloatVector **data)
{
  linkedFloatVector *current;
  
  int i, found = 1;
  for (i=0; i<NUM_PIC; i++)
    if (i!=pivot)
    {
      current = data[i];
      while (current)
      {
        if(current->i == search)
          found++;
          
        current = current->next;
      }
    }
    
  if (found == NUM_PIC)
    return TRUE;
  else
    return FALSE;
}

void *parallelExec(void *data)
{
  //printf("Entered thread\n");
  
  const CvSeq* keypoints;
  const CvSeq* descriptors;
  CvSURFParams params;
  linkedFloatVector* pair;
  char* file;
  
  parallelPack *packed = (parallelPack*)data;
  
  keypoints = packed->keypoints;
  descriptors = packed->descriptors;
  params = packed->params;
  pair = packed->pair;
  file = packed->file;
  
  /*
  CvMemStorage **newstorage;
  CvSeq **newkeypoints = 0;
  CvSeq **newdescriptors = 0;
  
  newstorage = malloc(sizeof(CvMemStorage *) * NUM_PIC);
  newkeypoints = malloc(sizeof(CvSeq *) * NUM_PIC);
  newdescriptors = malloc(sizeof(CvSeq *) * NUM_PIC);
  for (i=0; i<NUM_PIC; i++)
  {
    newstorage[i] = cvCreateMemStorage(0);
    newkeypoints[i] = 0;
    newdescriptors[i] = 0;
  }
*/
  
  pthread_mutex_lock( &extracting );
  
  //IplImage* desImg;

  //newstorage = cvCreateMemStorage(0);
  //srcImg = (IplImage*)cvLoadImage(filenames[i], 0);
  //pthread_mutex_lock( &mutex );
  #ifdef x64
  IplImage* srcImg = (IplImage*)(int64_t)cvLoadImage(file, 0);
  #else
  IplImage* srcImg = (IplImage*)cvLoadImage(file, 0);
  #endif
  //pthread_mutex_unlock( &mutex );
  /*
  desImg = cvCreateImage(cvSize(sel_width, sel_height), srcImg->depth, srcImg->nChannels);
  cvGetRectSubPix(srcImg, desImg, 
    cvPoint2D32f((float) ((sel_x + diffx[i-1])  + (sel_width/2 + 20)),(float) ((sel_y + diffy[i-1]) + (sel_height/2 + 20)) )
    //We need to compute the center of our selection
    );
  */
  //pthread_mutex_lock( &makestorage );
  CvMemStorage *newstorage = cvCreateMemStorage(0);
  CvSeq *newkeypoints = 0;
  CvSeq *newdescriptors = 0;
  //pthread_mutex_unlock( &makestorage );
  //pthread_mutex_lock( &mutex );
  cvExtractSURF( srcImg, 0, &newkeypoints, &newdescriptors, newstorage, params, 0 );
  //pthread_mutex_unlock( &mutex );
  //pthread_mutex_unlock( &makestorage );
  //cvExtractSURF( desImg, 0, &newkeypoints, &newdescriptors, newstorage, params, 0 );
  //printf("Image Descriptors: %d\n", newdescriptors->total);

  pthread_mutex_unlock( &extracting );

  //pthread_mutex_lock( &mutex );
    if(compareSURF(keypoints, descriptors, newkeypoints, newdescriptors, pair))
    {
      
      //printf("Total Difference: %f,%f\n", diffx[i], diffy[i]);
      //printf("Adjusted Difference: %f,%f\n\n", diffx[i]-sel_x, diffy[i]-sel_y);
    } else
    {
      printf("Not enough matched features\n");
      printf("Please make a new selection\n\n");
      //return FALSE;
      //pairs[i] = NULL;
      packed->pair = NULL;
    }
    //pthread_mutex_unlock( &mutex );
    
    //pthread_mutex_lock( &removestorage );
    cvReleaseMemStorage(&newstorage);
    //pthread_mutex_unlock( &removestorage );
    cvReleaseImage(&srcImg);
    //cvReleaseImage(&desImg);


  //return TRUE;
  return;
}

int parallelExtractSURF(const CvSeq* keypoints, const CvSeq* descriptors, CvSURFParams params, linkedFloatVector** pairs)
{
  parallelPack *data;
  //CvMemStorage **copyStorage;
  //CvSeq **copyKeypoints;
  //CvSeq **copyDescriptors;
  //char **copyFilenames;
  pthread_t *threads;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  threads = malloc(sizeof(pthread_t)*NUM_PIC);
  data = malloc(sizeof(parallelPack)*NUM_PIC);
  //copyStorage = malloc(sizeof(CvMemStorage*)*NUM_PIC);
  //copyKeypoints = malloc(sizeof(CvSeq*)*NUM_PIC);
  //copyDescriptors = malloc(sizeof(CvSeq*)*NUM_PIC);
  //copyFilenames = malloc(sizeof(char*)*NUM_PIC);

  int i;
  for (i=0; i<NUM_PIC; i++)
  if (i != pivot)
  {
    //char *file;
    //strcpy(file, filenames[i]);
    //copyStorage[i] = cvCreateMemStorage(0);
    //data[i].keypoints = (CvSeq*)cvCloneSeq(keypoints, copyStorage[i]);
    //data[i].descriptors = (CvSeq*)cvCloneSeq(descriptors, copyStorage[i]);
    data[i].keypoints = keypoints;
    data[i].descriptors = descriptors;
    data[i].params = params;
    data[i].pair = pairs[i];
    data[i].file = filenames[i];
  }
  
  for (i=0; i<NUM_PIC; i++)
  if (i != pivot)
  {
/*
    if (i != NUM_PIC - 1)
    {
      We do this because we segfault if all the threads try 
      and access the same CvSeq
      copyStorage[i] = cvCreateMemStorage(0);
      copyKeypoints[i] = (CvSeq*)cvCloneSeq(keypoints, copyStorage[i]);
      copyDescriptors[i] = (CvSeq*)cvCloneSeq(descriptors, copyStorage[i]);
    } else
    {
      but we can just use the original if this is the last thread
      copyKeypoints[i] = (CvSeq*)keypoints;
      copyDescriptors[i] = (CvSeq*)descriptors;
    }
*/

    pthread_create( &threads[i], &attr, &parallelExec, (void*)&data[i] );
    //pthread_join(threads[i], NULL);
    //drawFeatures(i, pairs[i]);
    //printf("Started thread %d\n",i);
    //parallelExec((void*)&data[i]);
    //sleep(2);
  }

  for (i=0; i<NUM_PIC; i++)
  if (i != pivot)
  {
    if(!pthread_join(threads[i], NULL))
    {
      //cvReleaseMemStorage(&copyStorage[i]);
      if (pairs[i])
        drawFeatures(i, pairs[i]);
      printf("Processed image %d/%d\n",i, NUM_PIC-1);
    }
  }
  
  return TRUE;
  
}

int matchFeatures(int sel_x, int sel_y, int sel_width, int sel_height)
{
  int i;
  #ifdef x64
  IplImage *srcImg = (IplImage*)(int64_t)cvLoadImage(filenames[pivot], 0);
  #else
  IplImage *srcImg = (IplImage*)cvLoadImage(filenames[pivot], 0);
  #endif
/*
  IplImage *desImg = cvCreateImage(cvSize(sel_width, sel_height), srcImg->depth, srcImg->nChannels);
  cvGetRectSubPix(srcImg, desImg, 
    cvPoint2D32f((float) (sel_x + sel_width/2),(float) (sel_y + sel_height/2) )
    //We need to compute the center of our selection
    );
*/
/*
  CvRect rect = cvRect((float)sel_x,(float)sel_y),sel_width,sel_height);
  CvMat* temp = cvCreateMat(sel_width, sel_height, CV_8UC1);
  cvGetSubRect(srcImg, temp, rect );
*/

  CvMemStorage *storage = cvCreateMemStorage(0);

  CvSeq *keypoints = 0;
  CvSeq *descriptors = 0;
  CvSURFParams params = cvSURFParams(300, 1);
  //cvExtractSURF( desImg, 0, &keypoints, &descriptors, storage, params, 0 );
  cvExtractSURF( srcImg, 0, &keypoints, &descriptors, storage, params, 0 );
  //printf("Object Descriptors: %d\n", descriptors->total);

  removeSURF(keypoints, descriptors, sel_x, sel_y, sel_width, sel_height);

  if (descriptors->total == 0)
  {
    printf("Not enough features in selected area\n");
    printf("Please make a new selection\n");
    cvReleaseMemStorage(&storage);
    cvReleaseImage(&srcImg);
    return FALSE;
  } 
  
  //drawFeatures(pivot, keypoints, (linkedFloatVector*)NULL, .9f, 0f, 0f);
  printf("Selection feature points: %d\n", descriptors->total);
  diffx[pivot] = 0.0f;
  diffy[pivot] = 0.0f;
  
  cvReleaseImage(&srcImg);

  linkedFloatVector **pairs;  
  //allocate enough space for all matches found
  pairs = malloc(sizeof(linkedFloatVector*) * NUM_PIC);
  for (i=0; i<NUM_PIC; i++)
    pairs[i] = malloc(sizeof(linkedFloatVector) * descriptors->total);

//It turns out cvExtractSurf likes to seqfault when being threaded.
//EDIT: trying again, this time we'll mutex lock the extractSURF part
  if (parallelExtractSURF(keypoints, descriptors, params, pairs))
  {
  } else
  {
    cvReleaseMemStorage(&storage);
    cvReleaseImage(&srcImg);
    return FALSE;
  }

/*
  CvMat **src;
  src = malloc(sizeof(CvMat*)*NUM_PIC);

  for(i=0; i<NUM_PIC; i++)
  if (i != pivot)
  {

  #ifdef x64
  srcImg = (IplImage*)(int64_t)cvLoadImage(filenames[i], 0);
  #else
  srcImg = (IplImage*)cvLoadImage(filenames[i], 0);
  #endif
  CvMat src_head;
  src[i] = cvGetMat(srcImg, &src_head, NULL, 0);
  cvReleaseImage(&srcImg);
  }
  // Bad idea, it trashed cache. duh; trying to load 12 images off disk
*/  
/*
//#ifdef _OPENMP
//#pragma omp parallel for num_threads(NUM_PIC) schedule(dynamic)
//#endif
  for(i=0; i<NUM_PIC; i++)
  if (i != pivot)
  {
  #ifdef x64
  srcImg = (IplImage*)(int64_t)cvLoadImage(filenames[i], 0);
  #else
  srcImg = (IplImage*)cvLoadImage(filenames[i], 0);
  #endif
  //CvMat src_head, *src = cvGetMat(srcImg, &src_head, NULL, 0);

  
//  desImg = cvCreateImage(cvSize(sel_width, sel_height), srcImg->depth, srcImg->nChannels);
//  cvGetRectSubPix(srcImg, desImg, 
//    cvPoint2D32f((float) ((sel_x + diffx[i-1])  + (sel_width/2 + 20)),(float) ((sel_y + diffy[i-1]) + (sel_height/2 + 20)) )
//    //We need to compute the center of our selection
//    );
  

  CvMemStorage *newstorage = cvCreateMemStorage(0);
  CvSeq *newkeypoints = 0;
  CvSeq *newdescriptors = 0;

  cvExtractSURF( srcImg, NULL, &newkeypoints, &newdescriptors, newstorage, params, 0 );
  //cvExtractSURF( src, NULL, &newkeypoints, &newdescriptors, newstorage, params, 0 );
  //cvExtractSURF( desImg, 0, &newkeypoints, &newdescriptors, newstorage, params, 0 );
  
  //printf("Image Descriptors: %d\n", newdescriptors->total);
  
  if(compareSURF(keypoints, descriptors, newkeypoints, newdescriptors, pairs[i]))
  {
    drawFeatures(i, pairs[i]);
    printf("Processed image %d/%d\n",i, NUM_PIC-1);
  } else
  {
    printf("No features were matched in image %d\n", i);
    //printf("Please make a new selection\n\n");
    pairs[i] = NULL;
  }
    cvReleaseImage(&srcImg);
    cvReleaseMemStorage(&newstorage);
  }
*/
  
  //copy our keypoint data into pairs[pivot] for easier implementation
  CvSeqReader keyReader, descReader;
  cvStartReadSeq(keypoints, &keyReader, 0);
  cvStartReadSeq(descriptors, &descReader, 0);
  const CvSURFPoint* keypoint;
  const float* descriptor;
  
  for (i=0; i<descriptors->total; i++)
  {
    keypoint = (const CvSURFPoint*)keyReader.ptr;
    descriptor = (const float*)descReader.ptr;
    CV_NEXT_SEQ_ELEM( keyReader.seq->elem_size, keyReader );
    CV_NEXT_SEQ_ELEM( descReader.seq->elem_size, descReader );
    
    pairs[pivot][i].x = keypoint->pt.x;
    pairs[pivot][i].y = keypoint->pt.y;
    pairs[pivot][i].i = i;
    if (i+1 < descriptors->total)
      pairs[pivot][i].next = &pairs[pivot][i+1];
    else
      pairs[pivot][i].next = NULL;
  }
  
  drawFeatures(pivot, pairs[pivot]);
  
  //printf("Start post processing\n");
  
  //weighting for sorting, descending weight:
  //criteria 1: feature appears in all images
  //criteria 2: median (~mode) of features
  //criteria 3: spatial proximity to higher-ranked features
  
  //TODO: consider processing images in pairs instead of all at once
  
  //let's sort pairs[pivot] by the criteria listed above
  //criteria 1: features appear in all images
  linkedFloatVector *current;
  linkedInteger *weighted_list = NULL, *placeholder = NULL;
  int num_matched = 0;
  
  current = pairs[pivot]; //the complete set of features
  while (current)
  {
    if ( matchAllIndex(current->i, pairs) )
    {
      weighted_list = malloc(sizeof(linkedInteger));
      weighted_list->i = current->i;
      weighted_list->next = placeholder;
      placeholder = weighted_list;
      num_matched++;
    }    
    current = current->next;
  }
  
  drawMatchedFeatures(weighted_list, pairs);

/*  
  placeholder = weighted_list;
  while (placeholder)
  {
    //printf("Matched in all: %d\n", placeholder->i);
    placeholder = placeholder->next;
  }
*/
  
  if (num_matched < 2) //let's only find more if we have to.
  {
    //criteria 2: median (~mode) of features
    current = pairs[pivot];
    while (current)
    {
      //TODO: implement median of features sort
      current = current->next;
    }
  }
  if (num_matched < 2)
  {
    //criteria 3: spatial proximity to higher ranked features
    current = pairs[pivot];
    while (current)
    {
      //TODO implement spatial proximity sort
      current = current->next;
    }
  }
  
  //now we pick one point to align to
  //and then use that and another point to fix rotation
  int rotated = FALSE;
  int num_processed = 0;
  linkedInteger *prev;
  
  if (placeholder)
  {
    
    //first we write all our findings to offsets.txt through saveOffsets()
    placeholder = weighted_list;
    while (placeholder)
    {
      diffx[0] = 0.0f;
      diffy[0] = 0.0f;
      for (i=1; i<NUM_PIC; i++)
      {
        diffx[i] = getCoords(placeholder->i, pairs[i-1])->x 
                   - getCoords(placeholder->i, pairs[i])->x;
        diffy[i] = getCoords(placeholder->i, pairs[i-1])->y 
                   - getCoords(placeholder->i, pairs[i])->y;
      }
      for (i=0; i<NUM_PIC; i++)
      {
        aligned_feature[2*i] = getCoords(placeholder->i, pairs[i])->x;
        aligned_feature[2*i+1] = getCoords(placeholder->i, pairs[i])->y;
        //printf("Stored feature point #%d: %f,%f\n", i, aligned_feature[2*i], aligned_feature[2*i+1]);
      }
      saveOffsets();
      placeholder = placeholder->next;
    }
    
    //do it again, just this time only until we fix rotation
    placeholder = weighted_list;
    while (placeholder && !rotated)
    {
      if (num_processed > 0)
      {
        //now we have the previous points values in diff[x,y] and aligned_feature
        //we can now use this point to do rotation adjustments
        //rotation will be from the center of the image.
        //TODO: better results will come from using largely spaced points
        float new_x, new_y;
        #define ALLOWED_ERROR 1.0f
        float theta = 0.0f;
        float pi = acosf(1.0f);
        float center_x, center_y;
        float r_prev_x, r_prev_y, r_prev, theta_prev;
        float r_curr_x, r_curr_y, r_curr, theta_curr;
        
        //printf("Begin rotation adjustment\n");
        rotate[0] = 0.0f;
        for (i=1; i<NUM_PIC; i++)
        {
          new_x =  getCoords(placeholder->i, pairs[i-1])->x 
                   - getCoords(placeholder->i, pairs[i])->x;
          new_y =  getCoords(placeholder->i, pairs[i-1])->y 
                   - getCoords(placeholder->i, pairs[i])->y;
          if ( new_x < diffx[i] + ALLOWED_ERROR &&
               new_x > diffx[i] - ALLOWED_ERROR &&  //new_x == diffx +- ALLOWED_ERROR
               new_y < diffy[i] + ALLOWED_ERROR &&
               new_y > diffy[i] - ALLOWED_ERROR   ) //new_y == diffy +- ALLOWED_ERROR
          {
            printf("No rotation needed for image %d\n", i);
            rotate[i] = 0.0f;
          } else
          {
            //We'll need to rotate the image by it's center
            //then readjust the previous point's offsets
            //then check again to see if we're within allowed error
            //additionally we'll need a mechanism to allow graceful failure
            while (  !(new_x < diffx[i] + ALLOWED_ERROR && new_x > diffx[i] - ALLOWED_ERROR &&
                       new_y < diffy[i] + ALLOWED_ERROR && new_y > diffy[i] - ALLOWED_ERROR)
                  && theta < 2*pi
                  )
            {
              printf("Trying rotation adjustment on %d of %f radians\n", i, theta);
              center_x = wid[i] * SCALE / 2;
              center_y = hei[i] * SCALE / 2;
              
              r_prev_x = getCoords(prev->i, pairs[i])->x - center_x;
              r_prev_y = center_y - getCoords(prev->i, pairs[i])->y;
              
              r_prev = hypotf(r_prev_x, r_prev_y); //returns sqrt(x*x + y*y)
              theta_prev = atan2f(r_prev_y, r_prev_x); //returns radians -pi -> pi
              
              r_curr_x = getCoords(placeholder->i, pairs[i])->x - center_x;
              r_curr_y = center_y - getCoords(placeholder->i, pairs[i])->y;
              
              r_curr = hypotf(r_curr_x, r_curr_y);
              theta_curr = atan2f(r_curr_y, r_curr_x);
              
              theta_prev += theta;
              theta_curr += theta;
              
              r_prev_x = r_prev * cosf( theta_curr );
              r_prev_y = r_prev * sinf( theta_curr );
              
              r_curr_x = r_curr * cosf( theta_curr );
              r_curr_y = r_curr * sinf( theta_curr );
              
              getCoords(prev->i, pairs[i])->x = r_prev_x + center_x;
              getCoords(prev->i, pairs[i])->y = center_y - r_prev_y;
              
              getCoords(placeholder->i, pairs[i])->x = r_curr_x + center_x;
              getCoords(placeholder->i, pairs[i])->y = center_y - r_curr_y;
              
              diffx[i] = getCoords(prev->i, pairs[i-1])->x 
                         - getCoords(prev->i, pairs[i])->x;
              diffy[i] = getCoords(prev->i, pairs[i-1])->y 
                         - getCoords(prev->i, pairs[i])->y;
                     
              new_x = getCoords(placeholder->i, pairs[i-1])->x 
                      - getCoords(placeholder->i, pairs[i])->x;
              new_y =  getCoords(placeholder->i, pairs[i-1])->y 
                       - getCoords(placeholder->i, pairs[i])->y;
              
              theta += pi / 1800.0f; //increase rotation .1 deg == .0017 radian
            }
            if ( new_x < diffx[i] + ALLOWED_ERROR &&
                 new_x > diffx[i] - ALLOWED_ERROR && 
                 new_y < diffy[i] + ALLOWED_ERROR &&
                 new_y > diffy[i] - ALLOWED_ERROR  )
            {
              rotate[i] = theta;
              printf("Rotated image %d %f radians\n", i, theta);
            } else
            {
              printf("Rotated image %d %f radians\n", i, 0.0f);
              rotate[i] = 0.0f;
            }
          }
        } //end for (i=1; i<NUM_PIC; i++)
                
        rotated = TRUE;
      } else
      {
        diffx[0] = 0.0f;
        diffy[0] = 0.0f;
        for (i=1; i<NUM_PIC; i++)
        {
          diffx[i] = getCoords(placeholder->i, pairs[i-1])->x 
                     - getCoords(placeholder->i, pairs[i])->x;
          diffy[i] = getCoords(placeholder->i, pairs[i-1])->y 
                     - getCoords(placeholder->i, pairs[i])->y;
        }
        for (i=0; i<NUM_PIC; i++)
        {
          aligned_feature[2*i] = getCoords(placeholder->i, pairs[i])->x;
          aligned_feature[2*i+1] = getCoords(placeholder->i, pairs[i])->y;
        }
      prev = placeholder;
      }
      
      num_processed++;
      placeholder = placeholder->next;
    }
    
  } else
  {
    printf("They're weren't enough features to align the images.\n");
    printf("(Two are needed, found %d)\n", num_matched);
    cvReleaseMemStorage(&storage);
    cvReleaseImage(&srcImg);
    return FALSE;
  }

  

/*
  current = pairs[0];
  while(current)
  {
    printf("data: %f,%f\n",current->x,current->y);
    current = current->next;
  }
*/
/*
  //now pick one!?
  //we have a list in pairs[0] containing all pairs that have 
  //matches in all images. We now need to sort them and find the
  //mode. Problem is, this "mode" will have to be +/- some error
  //as we will not have exact matches
  iterator = pairs[0];
  
  if (iterator) //the case may be that pairs[0] is empty...
  {
    linkedFloatVector *q1, *q2, *q3, *q4, *h1, *h2, *h3, *h4;
    h1 = NULL; h2 = NULL; h3 = NULL; h4 = NULL;
    int n1, n2, n3, n4;
    n1 = 0; n2 = 0; n3 = 0; n4 = 0;
    
    while (iterator)
    {
      //we now need to sort pairs[0] based on x,y
      //sorting by magnitude is insufficient
      //we'll use four buckets, one for each cartesian quadrant
      //TODO: remove outliers before this step
      if (iterator->x > 0.0f && iterator->y > 0.0f)
      {
        q1 = malloc(sizeof(linkedFloatVector));
        q1->x = iterator->x;
        q1->y = iterator->y;
        q1->i = iterator->i;
        q1->next = h1;
        h1 = q1; //confusing shorthand, anyone? yes? no? sold!
        n1++;
      } else if (iterator->x > 0.0f && iterator->y < 0.0f)
      {
        q2 = malloc(sizeof(linkedFloatVector));
        q2->x = iterator->x;
        q2->y = iterator->y;
        q2->i = iterator->i;
        q2->next = h2;
        h2 = q2;
        n2++;
      } else if (iterator->x < 0.0f && iterator->y < 0.0f)
      {
        q3 = malloc(sizeof(linkedFloatVector));
        q3->x = iterator->x;
        q3->y = iterator->y;
        q3->i = iterator->i;
        q3->next = h3;
        h3 = q3;
        n3++;
      } else if (iterator->x < 0.0f && iterator->y > 0.0f)
      {
        q4 = malloc(sizeof(linkedFloatVector));
        q4->x = iterator->x;
        q4->y = iterator->y;
        q4->i = iterator->i;
        q4->next = h4;
        h4 = q4;
        n4++;
      } else
      {
        diffx[i] = 0;
        diffy[i] = 0;    
      }
      iterator = iterator->next;
    }
    
    printf("Matches sorted into quadrants\n");
*/
/*
    if (n1 > n2)
      if (n1 > n3)
      {    // n1 is largest so far
        if (n1 > n4)
          iterator = h1;
        else
          iterator = h4;
      }
      else // n3 is largest so far
      {
        if (n3 > n4)
          iterator = h3;
        else
          iterator = h4;
      }
    else
      if (n2 > n3)
      {    // n2 is largest so far
        if (n2 > n4)
          iterator = h2;
        else
          iterator = h4;
      }
      else // n3 is largest so far
      {
        if (n3 > n4)
          iterator = h3;
        else
          iterator = h4;
      }
*/
/*    
    //now iterator is at the head of the quadrant with the most members
    //or in the case of excessive ties ~h4
    //now sort by magnitude
    //TODO: deal with outliers screwing us up
    linkedFloatVector *min;
    float tx, ty;
    int ti, tn = 0;
    temperator = iterator; //hold onto our head
    
    while (iterator) //we already checked earlier on pairs[0],
                     //this should contain at least one element
    {
      min = iterator;
      prev = iterator;
      
        if(iterator->next)
        while (iterator)
        {
          if ( (iterator->x*iterator->x + iterator->y*iterator->y) < (min->x*min->x + min->y*min->y) )
            min = iterator;
          iterator = iterator->next;
        }
        
      iterator = prev;
      tx = min->x;
      ty = min->y;
      ti = min->i;
      min->x = iterator->x;
      min->y = iterator->y;
      min->i = iterator->i;
      iterator->x = tx;
      iterator->y = ty;
      iterator->i = ti;
      //horrible way to do it; meh
  
      tn++;
      iterator = iterator->next;
    }
    
    printf("Biggest bin sorted by magnitude\n");
    
    iterator = temperator; //retrieve head
    for (i=0; i<tn/2; i++) //advance to middle of list
      if (iterator->next)  //failsafe?
        iterator = iterator->next;   
*/
/*    
    
    search = iterator->i;
    diffx[0] = iterator->x;
    diffy[0] = iterator->y;
    for (i=1; i<NUM_PIC; i++)
    {
      iterator = pairs[i];
      while (iterator)
      {
        if (iterator->i == search) //this should be guaranteed to exist
        {
          diffx[i] = iterator->x;
          diffy[i] = iterator->y;
          iterator = NULL;
        }
        else
          iterator = iterator->next;
      }
    }
  }
  else
  {
    printf("Not enough matched features\n");
    printf("Please make a new selection\n\n");
    for (i=0; i<NUM_PIC; i++)
    {
      diffx[i] = 0;
      diffy[i] = 0;
    }
  }
*/ 
/*
  for (i=pivot-1; i>=0; i--)
  //if (i != pivot)
  {
    cvReleaseImage(&srcImg);
    cvReleaseImage(&desImg);
    srcImg = (IplImage*)cvLoadImage(filenames[i], 0);
    desImg = cvCreateImage(cvSize(sel_width, sel_height), srcImg->depth, srcImg->nChannels);
    cvGetRectSubPix(srcImg, desImg, 
      cvPoint2D32f((float) ((sel_x + diffx[i+1])  + (sel_width/2 + 20)),(float) ((sel_y + diffy[i+1]) + (sel_height/2 + 20)) )
      //We need to compute the center of our selection
      );
      
    newstorage = cvCreateMemStorage(0);
    CvSeq *newkeypoints = 0;
    CvSeq *newdescriptors = 0;
    //cvExtractSURF( srcImg, 0, &newkeypoints, &newdescriptors, newstorage, params, 0 );
    cvExtractSURF( desImg, 0, &newkeypoints, &newdescriptors, newstorage, params, 0 );
    //printf("Image Descriptors: %d\n", newdescriptors->total);
    
    if(compareSURF(keypoints, descriptors, newkeypoints, newdescriptors, &diffx[i], &diffy[i], 5.0f))
    {
      printf("Total Difference: %f,%f\n", diffx[i], diffy[i]);
      //printf("Adjusted Difference: %f,%f\n\n", diffx[i]-sel_x, diffy[i]-sel_y);
    } else
    {
      printf("Not enough matched features\n");
      printf("Please make a new selection\n\n");
    }
    cvReleaseMemStorage(&newstorage);
  }
*/

  //for (i=0; i<NUM_PIC; i++)
    //printf("Total Difference: %f,%f\n", diffx[i], diffy[i]);
  //printf("----------------------------\n\n");
  cvReleaseMemStorage(&storage);
  cvReleaseImage(&srcImg);
  //cvReleaseImage(&desImg);
  //cvReleaseImage(&bwImg);
  
  return TRUE;
}

void processArea()
{
  int temp;
  
  dragged = 0;
  //printf("%d,%d -> %d,%d @%dx%d\n", box[0], box[1], box[2], box[3],Window_Width,Window_Height);
  int area_width = abs(box[2] - box[0]);
  int area_height = abs(box[1] - box[3]);
  
  box[1] = Window_Height - box[1];
  box[3] = Window_Height - box[3];
  
  if ( box[2] < box[0])
  {
    temp = box[0];
    box[0] = box[2];
    box[2] = temp;
  }
  if ( box[3] < box[1])
  {
    temp = box[1];
    box[1] = box[3];
    box[3] = temp;
  }
 
  box[1] = Window_Height - box[1]; //I'd like to get rid of this,
  box[3] = Window_Height - box[3]; //but I've already done all the calcs
  
  int calcx, calcy, calcw, calch;
  int ymin, ymax;
  int xmin, xmax;
  int absy, absx;
  float aspect;
  
  //TODO: the following is only valid for landscape, add portrait
  if(hei[pivot] < wid[pivot])
  {
  ymin = (int)((float) Window_Height * .06f);  //Don't ask me why
  ymax = (int)((float) Window_Height * .80f);
  
  aspect = (float) wid[pivot] / (float) hei[pivot]; //landscape ~=1.333
  
  xmin = (int)((float)(Window_Width - Window_Height + 3) / 2.0f); //Don't ask me why
  xmax = ((ymax-ymin) * aspect ) + xmin;
  
  absy = ymax - ymin;
  absx = xmax - xmin;
  
  calcx = box[0] - xmin;
  calcy = ymax - box[1];
  } else
  {
  ymin = (int)((float) Window_Height * -0.06f); //it's offscreen, and I'd have to change every value in this function to zoom out...
  ymax = (int)((float) Window_Height * .926f);
  
  aspect = (float) wid[pivot] / (float) hei[pivot]; //portrait =.75
  
  //xmin = (int)(72.0678943f*powf(1.000492145,(float)(Window_Width - Window_Height + 3))); //Exp Regr
  //xmin = (int)(66.2697376f+0.637373407f*(float)(Window_Width - Window_Height + 3)); //Lin Regr
  xmin = (int)(76.3520977f+.534346503*(float)(Window_Width - Window_Height)); //Lin Regr
  xmax = ((ymax-ymin) * aspect ) + xmin;
  
  absy = ymax - ymin;
  absx = xmax - xmin;
  
  calcx = box[0] - xmin;
  calcy = ymax - box[1];
  }
  
  
  if (calcx < 0)
  {
    box[0] = xmin;
    area_width = abs(box[2] - box[0]);
    calcx = 0;
  } else if (box[2] - xmin > absx)
  {
    box[2] = xmax;
    area_width = abs(box[2] - box[0]);
  }
  if (calcy < 0) //remember, screen y coordinates go top-> bottom
  {
    box[1] = ymax;
    area_height = abs(box[1] - box[3]);
    calcy = 0;
  } else if (ymax - box[3] > absy)
  {
    box[3] = ymin;
    area_height = abs(box[1] - box[3]);
  }
  
  calcx = ((float) calcx / (float) absx) * (float) SCALE * (float) wid[pivot];
  calcy = ((float) calcy / (float) absy) * (float) SCALE * (float) hei[pivot];
  //orign of the selection, in image coord
  //bottom-left is 0,0
  
  calcw = ((float) area_width / (float) absx) * (float) SCALE * (float) wid[pivot];
  calch = ((float) area_height / (float) absy) * (float) SCALE * (float) hei[pivot];
  //width and height of selection, image coord
  
  if(calcw > wid[pivot])
  {
    calcw = wid[pivot] * SCALE;
  }
  if(calch > hei[pivot])
  {
    calch = hei[pivot] * SCALE;
  }
  
  calcy = hei[pivot]*SCALE - calcy - calch;
  //flip y coordinates
  //origin is now top-left
  
  //printf("input: %d,%d %dx%d image: %d,%d %dx%d\nwindow: %dx%d\n", box[0], box[1], area_width,area_height, calcx, calcy, calcw, calch, Window_Width, Window_Height);
  //printf("image: %d,%d %dx%d center: %d,%d\n", calcx, calcy, calcw, calch,(calcx + calcw/2),(hei[pivot]*SCALE - (calcy + calch/2)));
  
  if ( matchFeatures(calcx,calcy,calcw,calch) )
    global_state = TRUE;
  
  box[0] = 0;
  box[1] = 0;
  box[2] = 0;
  box[3] = 0;
  
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

   int i;
   
   // Need to manipulate the ModelView matrix to move our model around.
   glMatrixMode(GL_MODELVIEW);

   // Reset to 0,0,0; no rotation, no scaling.
   glLoadIdentity();

   // Clear the color and depth buffers.
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

if (global_state) //is false for the selection portion, then turns to true for the remainder of the program
{
   // Move the object back from the screen.
   glTranslatef(X_Cam,Y_Cam,Z_Off);

   // Rotate the calculated amount.
   glRotatef(X_Rot,1.0f,0.0f,0.0f);
   glRotatef(Y_Rot,0.0f,1.0f,0.0f);
   glRotatef(Z_Rot,0.0f,0.0f,1.0f);

   for(i=0; i < NUM_PIC; i++)
   {
    glTranslatef((diffx[i]/SCALE)/20.0f, 0.0f, (diffy[i]/SCALE)/20.0f);
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
/*
    glEnable(GL_POINT_SMOOTH);
    glDisable(GL_TEXTURE_RECTANGLE_ARB);
    glTranslatef(0.0f-wid[i]/40.0f, i/10.0f, 0.0f-hei[i]/40.0f);
    glPointSize(3.0f);
    glBegin(GL_POINTS);
      glColor4f(0,0,.9,1);
      //glVertex3f(0,0,0);
      //glVertex3f(64.8,0,48.6);
      glVertex3f((aligned_feature[2*i]/SCALE)/20.0f, 0.0f, (aligned_feature[2*i+1]/SCALE)/20.0f);
    glEnd();
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glDisable(GL_POINT_SMOOTH);
*/    
    glPopMatrix();
   }

} else { 

   glTranslatef(0.0,4.0,-70.0);


   glRotatef(90.0,1.0f,0.0f,0.0f);
   //glRotatef(0.0,0.0f,1.0f,0.0f);
   //glRotatef(0.0,0.0f,0.0f,1.0f);

   for(i=0; i < NUM_PIC; i++)
   {
   if (i > pivot)
   {
    glPushMatrix();
       if (hei[i] < wid[i])
      {
        glTranslatef(28, 0.0f, 0.0f);
        glTranslatef(14*(i - pivot), 0.0f, 4.0f*(i - pivot));
      } else
      {
        glTranslatef(21, 0.0f, 0.0f);
        glTranslatef(10.5*(i - pivot), 0.0f, 4.0f*(i - pivot));
      }
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture[i]);
    glBegin(GL_QUADS);
      glColor4f(0.8f, 0.8f, 0.8f, .9 );

      glTexCoord2f(0.0f, 0.0f);
      if(hei[i] < wid[i])
       glVertex3f(-28, i/10.0f,-21);
      else
       glVertex3f(-21, i/10.0f,-28);
       
      glTexCoord2f(wid[i], 0.0f);
      if(hei[i] < wid[i])
       glVertex3f(28, i/10.0f,-21);
      else
       glVertex3f(21, i/10.0f,-28);

      glTexCoord2f(wid[i], hei[i]);
      if(hei[i] < wid[i])
       glVertex3f(28, i/10.0f,21);
      else
       glVertex3f(21, i/10.0f,28);

      glTexCoord2f(0.0f, hei[i]);
      if(hei[i] < wid[i])
       glVertex3f(-28, i/10.0f,21);
      else
       glVertex3f(-21, i/10.0f,28);

    glEnd();
    glPopMatrix();
   } else if (i < pivot)
   {
    glPushMatrix();
       if (hei[i] < wid[i])
      {
        glTranslatef(-28, 0.0f, 0.0f);
        glTranslatef(-14*(pivot - i), 0.0f, 4.0f*(pivot - i));
      } else
      {
        glTranslatef(-21, 0.0f, 0.0f);
        glTranslatef(-10.5*(pivot - i), 0.0f, 4.0f*(pivot - i));
      }
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture[i]);
    glBegin(GL_QUADS);
      glColor4f(0.8f, 0.8f, 0.8f, .9 );

      glTexCoord2f(0.0f, 0.0f);
      if(hei[i] < wid[i])
       glVertex3f(-28, i/10.0f,-21);
      else
       glVertex3f(-21, i/10.0f,-28);
       
      glTexCoord2f(wid[i], 0.0f);
      if(hei[i] < wid[i])
       glVertex3f(28, i/10.0f,-21);
      else
       glVertex3f(21, i/10.0f,-28);

      glTexCoord2f(wid[i], hei[i]);
      if(hei[i] < wid[i])
       glVertex3f(28, i/10.0f,21);
      else
       glVertex3f(21, i/10.0f,28);

      glTexCoord2f(0.0f, hei[i]);
      if(hei[i] < wid[i])
       glVertex3f(-28, i/10.0f,21);
      else
       glVertex3f(-21, i/10.0f,28);

    glEnd();
    glPopMatrix();
   }
 }
 
 i = pivot;
    glPushMatrix();
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture[i]);
    glBegin(GL_QUADS);
      glColor4f(0.8f, 0.8f, 0.8f, 1 );

      glTexCoord2f(0.0f, 0.0f);
      if(hei[i] < wid[i])
       glVertex3f(-28, 2.0f,-21);
      else
       glVertex3f(-21, 2.0f,-28);
       
      glTexCoord2f(wid[i], 0.0f);
      if(hei[i] < wid[i])
       glVertex3f(28, 2.0f,-21);
      else
       glVertex3f(21, 2.0f,-28);

      glTexCoord2f(wid[i], hei[i]);
      if(hei[i] < wid[i])
       glVertex3f(28, 2.0f,21);
      else
       glVertex3f(21, 2.0f,28);

      glTexCoord2f(0.0f, hei[i]);
      if(hei[i] < wid[i])
       glVertex3f(-28, 2.0f,21);
      else
       glVertex3f(-21, 2.0f,28);

    glEnd();
    glPopMatrix();


    
    glDisable(GL_TEXTURE_RECTANGLE_ARB);
    
    glPushMatrix();
       if(hei[i] < wid[i])
        glTranslatef(15.0f, 3.0f, 21.0f);
      else
        glTranslatef(15.0f, 3.0f, 21.0f);
      glScalef(1.2f, 1.0f, 1.0f);
      glRotatef(90.0f, 1,0,0);
      glCallList(arrow);
    glPopMatrix();
    glPushMatrix();
      if(hei[i] < wid[i])
        glTranslatef(-15.0f, 3.0f, 26.0f);
      else
        glTranslatef(-15.0f, 3.0f, 26.0f);
      glScalef(1.2f, 1.0f, 1.0f);
      glRotatef(90.0f, 1,0,0);
      glRotatef(180.0f, 0,0,1);
      glCallList(arrow);
    glPopMatrix();
    
    if( i == pivot )
    if( display_features )
    {
      glPushMatrix();
      if(hei[i] < wid[i])
        glTranslatef(-28, 2.0f, -21);
      else
        glTranslatef(-21.0f, 2.0f, -28.0f);
      glPointSize(3.0f);
      glCallList(features[i]);
      glPointSize(4.0f);
      if( display_more_features )
        glCallList(more_features[i]);
      glPopMatrix();
    }
    
    glEnable(GL_TEXTURE_RECTANGLE_ARB);

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
     case 0: 
   if (dragged)
   {
   glBegin(GL_QUADS); //draw selecting box on main screen
   glColor4f(.04,.14,.39,.25);
   glVertex3f(  box[0] - 6, -box[1] + 14, 0);
   glVertex3f(  box[0] - 6, -box[3] + 14, 0);
   glVertex3f(  box[2] - 6, -box[3] + 14, 0);
   glVertex3f(  box[2] - 6, -box[1] + 14, 0);
   glEnd();
   glBegin(GL_LINES);
   glColor4f(.04,.14,.39,.75);
   glVertex3f(  box[0] - 6, -box[1] + 14, 0);
   glVertex3f(  box[0] - 6, -box[3] + 14, 0);
   
   glVertex3f(  box[0] - 6, -box[3] + 14, 0);
   glVertex3f(  box[2] - 6, -box[3] + 14, 0);
   
   glVertex3f(  box[2] - 6, -box[3] + 14, 0);
   glVertex3f(  box[2] - 6, -box[1] + 14, 0);
   
   glVertex3f(  box[2] - 6, -box[1] + 14, 0);
   glVertex3f(  box[0] - 6, -box[1] + 14, 0);
   glEnd();
   }
     break;
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

}

// Callback function when the mouse is clicked
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
  } else if (menu_selected == 0 && dragged)
  {
    box[2] = x;
    box[3] = y;
  } else
  {
  }

}

void cbMouseClick(int button, int state, int x, int y )
{
  int i;
  
  if (y > 20)
  {
    if (global_state)
    switch (button)
    {
    case GLUT_WHEEL_UP:
      offset += offset_change; break;
    case GLUT_WHEEL_DOWN:
      offset -= offset_change; break;
    default: break;
    } else if (menu_selected == 0) //i.e. we are drawing a selection box
    {
      if (state == GLUT_DOWN)
      {
        box[0] = x;
        box[1] = y;
        box[2] = x;
        box[3] = y;
        dragged = 1;
      } else if( state == GLUT_UP
                 && (abs(x - box[0]) > 5 || abs(y - box[1]) > 5) )
      {
        box[2] = x;
        box[3] = y;
        processArea();
      } else
      {
        dragged = 0;
      }
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
  } else if(x < 134 && state == GLUT_DOWN)
  {
    menu_selected = 4;
    for (i=0; i<HELP_ENTRIES; i++)
    { help[i] = 0; }
  } else if (x > 134)
  {
    menu_selected = 0;
  }
  if(hei[pivot] < wid[pivot] && state == GLUT_UP)
  {
    if ( y > (float) Window_Height * .80f                   &&
         y < (float) Window_Height * .90f ) 
         //just reusing some of the processArea() code
    {
      float xmin = (float)(Window_Width - Window_Height + 3) / 2.0f / 10.0f;
      
      if (x > xmin + Window_Width * .10f    &&
          x < xmin + 2.2 * Window_Width * .10f )
        changePivot(-1);
      else if ( x > Window_Width - xmin - 2.2 * Window_Width * .10f &&
                x < Window_Width - xmin - Window_Width * .10f      )
        changePivot(1);
    } else
    {
      float xmin = 76.3520977f+.534346503*(float)(Window_Width - Window_Height);
      
      if (x > xmin + Window_Width * .10f    &&
          x < xmin + 2.2 * Window_Width * .10f )
        changePivot(-1);
      else if ( x > Window_Width - xmin - 2.2 * Window_Width * .10f &&
                x < Window_Width - xmin - Window_Width * .10f      )
        changePivot(1);
    }
  }
}
// Callback function when the mouse is clicked and dragged

void cbMouseHeldMoving( int x, int y )
{
  
  if(y > 20 && menu_selected == 0 && global_state)
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
  } else 
  {
    cbMouseMoving(x, y);
  }
}

// Callback function called when a normal key is pressed.
void cbKeyPressed(unsigned char key, int x, int y )
{
  if (global_state)
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
      global_state = global_state ? 0 : 1;
      Text_on = global_state;
      break;

   case 114: case 82:  // R - reset view
      X_Rot   = 90.0f;
      Y_Rot   = 0.0f;
      Z_Rot   = 0.0f;
      Z_Off   = -70.0f;
      X_Cam   = 0.0f;
      Y_Cam   = 0.0f;
      offset  = 0.0f;
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
  else
   switch (key) {
   case 113: case 81: case 27: // Q (Escape) - We're outta here.
      glutDestroyWindow(Window_ID);
      exit(1);
      break; // exit doesn't return, but anyway...
   case 115: case 83: case 32:  // S - change state
      global_state = global_state ? 0 : 1;
      Text_on = global_state;
      break;
   default:
      printf ("KP: No action for %d.\n", key);
      break;
    }
}

// Callback Function called when a special key is pressed.
void cbSpecialKeyPressed(int key, int x, int y)
{
  if (global_state)
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
  else
   switch(key) {
   case GLUT_KEY_LEFT:
      changePivot(-1);
      break;

   case GLUT_KEY_RIGHT:
      changePivot(1);
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

void ourBuildSprites()
{
  arrow = glGenLists(1);

  glNewList(arrow,GL_COMPILE);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);	// Use The Good Calculations
    glEnable(GL_POLYGON_SMOOTH);
    glBegin(GL_TRIANGLE_FAN);
    
      glColor4f(.8,.8,.8,1);
      
      glTexCoord2f(0.0f, 0.0f);
      glVertex3f(6,   2.5,   0.0f); //tip of arrow
      
      glVertex3f(5.5, 2.1, 0.0f);
      //glVertex3f(5.0, 1.8, 0.0f);
      //glVertex3f(3.5, 0.4, 0.0f);
      glVertex3f(3,   0,   0.0f);
      
      glVertex3f(3.1, 0.5, 0.0f);
      glVertex3f(3.2, 1.0, 0.0f);
      glVertex3f(3.7, 1.8, 0.0f);
      glVertex3f(4, 2.0, 0.0f); 
      
      glVertex3f(0.5,  1.9, 0.0f); //box of arrow
      glVertex3f(0.55, 2.4, 0.0f);
      glVertex3f(0.55, 2.6, 0.0f); //mirrors from here out
      glVertex3f(0.5,  3.1, 0.0f); 
      
      glVertex3f(4, 3.0, 0.0f);
      glVertex3f(3.7, 3.2, 0.0f);
      glVertex3f(3.2, 4.0, 0.0f);
      glVertex3f(3.1, 4.5, 0.0f);
      
      glVertex3f(3,   5,   0.0f);
      //glVertex3f(3.5, 4.6, 0.0f);
      //glVertex3f(5.0, 3.2, 0.0f);
      glVertex3f(5.5, 2.9, 0.0f); 
      
      glTexCoord2f(1.0f, 1.0f);
      glVertex3f(6,   2.5,   0.0f); //back to tip
      
      
    glEnd();  
    glDisable(GL_POLYGON_SMOOTH);
    
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    
      glColor4f(.1,.1,.1,1.0f);
      
      glVertex3f(6,   2.5,   0.0f); //tip of arrow
      
      glVertex3f(5.5, 2.1, 0.0f);
      glVertex3f(5.1, 1.7, 0.0f);
      glVertex3f(3.6, 0.3, 0.0f);
      glVertex3f(3,   0,   0.0f);
      
      glVertex3f(3.1, 0.5, 0.0f);
      glVertex3f(3.2, 1.0, 0.0f);
      glVertex3f(3.7, 1.8, 0.0f);
      glVertex3f(4, 2.0, 0.0f); 
      
      glVertex3f(0.5,  1.9, 0.0f); //box of arrow
      glVertex3f(0.55, 2.4, 0.0f);
      glVertex3f(0.55, 2.6, 0.0f); //mirrors from here out
      glVertex3f(0.5,  3.1, 0.0f); 
      
      glVertex3f(4, 3.0, 0.0f);
      glVertex3f(3.7, 3.2, 0.0f);
      glVertex3f(3.2, 4.0, 0.0f);
      glVertex3f(3.1, 4.5, 0.0f);
      
      glVertex3f(3,   5,   0.0f);
      glVertex3f(3.6, 4.7, 0.0f);
      glVertex3f(5.1, 3.3, 0.0f);
      glVertex3f(5.5, 2.9, 0.0f); 
      
      //glVertex3f(6,   2.5,   0.0f); //back to tip
    
    glEnd();
    glDisable(GL_LINE_SMOOTH);

  glEndList();
  
  //next_sprite = arrow + 1;
}

// Does everything needed before losing control to the main
// OpenGL event loop.
void ourInit(int Width, int Height)
{
  features[0] = glGenLists(NUM_PIC);
  more_features[0] = glGenLists(NUM_PIC);
  int i;
  for (i=0; i<NUM_PIC-1; i++)
  {
    features[i+1] = features[i] + 1;
    more_features[i+1] = more_features[i] + 1;
  }
    
  if ( loadOffsets() )
    global_state = TRUE;
  else
  {
    for (i=0; i<NUM_PIC; i++);
    {
      diffx[i] = 0.0f;
      diffy[i] = 0.0f;
      aligned_feature[2*i] = 2592.0f;
      aligned_feature[2*i+1] = 1944.0f;
    }
  } 
   ourBuildTextures();

   ourBuildSprites();

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
   
   glEnable(GL_LIGHTING);
}

int loadDirectory(char *myDir)
{
   DIR  *dip, *dic;
   struct dirent *dit;
   int n;
   
   if ((dip = opendir(myDir)) == NULL || (dic = opendir(".")) == NULL)
   {
     fprintf(stderr, "can't open directory %s\n", myDir);
     return FALSE;
   }
   
   strcpy(dirString, myDir);
   
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
       filenames[NUM_PIC] = malloc(100);
       strcpy(filenames[NUM_PIC], dirString);
       strcat(filenames[NUM_PIC], "/");
       strcat(filenames[NUM_PIC], dit->d_name);
//       fd[NUM_PIC] = fopen(filenames, "rb"); // Opens up the files in arbitiary order, no good
       NUM_PIC++;
       //printf("READ %s %d\n", filenames[NUM_PIC-1], NUM_PIC - 1);
     }

   }
  
   pivot = NUM_PIC / 2;
//   printf("pivot: %d\n", pivot);
   
   n = 0;

  sort_files(filenames, NUM_PIC);

//  for(n=0; n<NUM_PIC; n++);
   while(n<NUM_PIC)
   {
    loadJpeg(filenames[n], n);
//    printf("READ %s %d\n", filenames[n], n);
//    free(filenames[n]);
    n++;
   }
   closedir(dip);   
  
  return TRUE;
}

// The main() function.  Inits OpenGL.  Calls our own init function,
// then passes control onto OpenGL.
int main(int argc,char **argv)
{
  printf("Usage: %s <directory>\n", argv[0]);
   if (argc < 2)
   {
     loadDirectory("lf");
     //printf("Usage: %s <directory>\n", argv[0]);
     //return 1;
   }
   else
    loadDirectory(argv[1]);
    
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
In the first view, use the drawn arrows to page though your images.\n\
Select an area by dragging a blue box around it.\n\
To cancel a selection make the selection box smaller than 5x5 pixels.\n\n\
In the second view, use arrow keys to move.\n\
Page up/down will move away from/towards camera.\n\n\
Use first letter of shown display mode settings to alter them.\n\n\
Q or [Esc] to quit; OpenGL window must have focus for input.\n");

   // Pass off control to OpenGL.
   // Above functions are called as appropriate.
   glutMainLoop();

   return 1;
}

