// Matti Kariluoma Jan 2010 <matti.kariluoma@gmail.com>
// compile:
// gcc rotateNrightNleft.c -o RotateAll -ljpeg
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <jpeglib.h>


typedef struct imageAttrib {
  int width;
  int height;
  int depth;
  unsigned char* img;
  char* myDir;
  char* filename;
} imageAttrib;

void saveJpeg(imageAttrib* output, int orientation)
{
  int i,j,k;

  unsigned char *data = malloc(output->width * output->height * output->depth);
  unsigned char *orig = output->img;
  //memcpy(data, output->img, (output->width * output->height * output->depth));

  if (orientation == 1) //rotate right, e.g. CW
    for (i=output->width - 1; i >= 0; i--)
      for (j=0; j < output->height; j++)
        for (k=0; k < output->depth; k++)
          data[i*output->depth*output->height + j*output->depth + k] = orig[j*output->depth*output->width + i*output->depth + k];
  else
     for (i=0; i < output->width; i++)
      for (j=0; j < output->height; j++)
        for (k=0; k < output->depth; k++)
          data[i*output->depth*output->height + (output->height - 1 - j)*output->depth + k] = orig[j*output->depth*output->width + i*output->depth + k];

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  FILE *outjpeg;
  JSAMPROW row_pointer[1];
  int row_stride;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  char* outname = malloc(4098);

  strcpy(outname, output->myDir);
  strcat(outname, output->filename);

  if ((outjpeg = fopen(outname, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", outname);
    exit(1);
  }

  jpeg_stdio_dest(&cinfo, outjpeg);
  cinfo.image_width = output->height;
  cinfo.image_height = output->width;
  cinfo.input_components = output->depth;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 93, TRUE);

  jpeg_start_compress(&cinfo, TRUE);
  row_stride = cinfo.image_width * output->depth;

  if (orientation == 1) //rotate right, e.g. CW
    for (i=cinfo.image_height-1; i>=0; i--)
    {
    row_pointer[0] = &data[i * row_stride];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
  else
    for (i=0; i < cinfo.image_height; i++)
    {
    row_pointer[0] = &data[i * row_stride];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }


  jpeg_finish_compress(&cinfo);

  fclose(outjpeg);
  
  jpeg_destroy_compress(&cinfo);
  
  free(data);
  free(outname);
  
  return;
}

imageAttrib* loadJpeg(imageAttrib* input)
{
  int i;

  FILE *fd;

  char* inname = malloc(4098);

  strcpy(inname, input->myDir);
  strcat(inname, input->filename);

  fd = fopen(inname, "rb");

  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  unsigned long location = 0;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);


    jpeg_stdio_src(&cinfo, fd);
    jpeg_read_header(&cinfo, 0);
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    jpeg_start_decompress(&cinfo);
    input->width = cinfo.output_width;
    input->height = cinfo.output_height;
    input->depth = cinfo.num_components; //should always be 3
    input->img = (unsigned char *) malloc(input->width * input->height * input->depth);
    row_pointer[0] = (unsigned char *) malloc(input->width * input->depth);
    /* read one scan line at a time */
    while( cinfo.output_scanline < cinfo.output_height )
    {
        jpeg_read_scanlines( &cinfo, row_pointer, 1 );
        for( i=0; i< (input->width * input->depth); i++)
                input->img[location++] = row_pointer[0][i];
    }
    fclose(fd);
    jpeg_finish_decompress(&cinfo);

  jpeg_destroy_decompress(&cinfo);

  free(inname);
  free(row_pointer[0]);

  return input;

}

void sort_files(imageAttrib* input[], int length)
{
  int i,j,min;
  imageAttrib *temp;

  for (i = 0; i < length; i++)
  {
    min = i;
    for (j = i+1; j < length; j++)
    {
      if ( strcmp(input[j]->filename, input[min]->filename) < 0 )
        min = j;
    }
  temp = input[min];
  input[min] = input[i];
  input[i] = temp;
  }
}

int loadDirectory(char *myDir)
{
   DIR  *dip;
   struct dirent *dit;
   int n, NUM_PIC;
   imageAttrib **images;

   if ((dip = opendir(myDir)) == NULL)
   {
     fprintf(stderr, "can't open directory %s\n", myDir);
     return FALSE;
   }

   NUM_PIC = 0;

   while ((dit = readdir(dip)) != NULL)
   {
     if(strstr(dit->d_name, ".JPG") != NULL || strstr(dit->d_name, ".jpg") != NULL || strstr(dit->d_name, ".jpeg") != NULL || strstr(dit->d_name, ".JPEG") != NULL)
     {
       NUM_PIC++;
     }
   }

   images = malloc(sizeof(imageAttrib*)*NUM_PIC);
   for (n=0; n<NUM_PIC; n++)
     images[n] = malloc(sizeof(imageAttrib));

   rewinddir(dip);

   n = 0;

   while ((dit = readdir(dip)) != NULL)
   {
     if(strstr(dit->d_name, ".JPG") != NULL || strstr(dit->d_name, ".jpg") != NULL || strstr(dit->d_name, ".jpeg") != NULL || strstr(dit->d_name, ".JPEG") != NULL)
     {
       images[n]->myDir = malloc(4098);
       images[n]->filename = malloc(4098);
       strcpy(images[n]->myDir, myDir);
       strcat(images[n]->myDir, "/");
       strcpy(images[n]->filename, dit->d_name);

       if((n+1) < NUM_PIC)
         n++;
     }
   }

   sort_files(images, NUM_PIC);

   n = 0;
   char* temp = malloc(10);
   while(n<NUM_PIC)
   {
    loadJpeg(images[n]);
    free(images[n]->filename);
    if (n < NUM_PIC / 2)
    {
      sprintf(temp, "%06d-a.jpg", n+1);
      images[n]->filename = temp;
      saveJpeg(images[n], 0);
    }
    else
    {
      sprintf(temp, "%06d-b.jpg", n+1-NUM_PIC/2);
      images[n]->filename = temp;
      saveJpeg(images[n], 1);
    }
    printf("Processed image %d/%d: %s\n", n+1, NUM_PIC, images[n]->filename);
    free(images[n]->myDir);
    free(images[n]->img);
    n++;
   }
   closedir(dip);

  return TRUE;
}

int main(int argc,char **argv)
{

   if (argc < 2)
   {
     printf("Usage: %s <directory>\n", argv[0]);
     return 1;
   }

   loadDirectory(argv[1]);

   return 0;
}

