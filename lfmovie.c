// Utility to make movies from light field camera images
//
// Matti Kariluoma (matti.kariluoma@gmail.com) Nov 2009

#define PROGRAM_TITLE "Utiltity to make movies from sequential image frames"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#define MAX_PIC 1000

int NUM_PIC = 0;
int printid = 0;

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
/*
  for(i = 0; i< length; i++)
  {
    printf("SORT %s\n", files[i]);
  }
*/
}

void make_file(char *dir, char *file)
{
  char id[5];
  sprintf(id, "%d", printid);
  char outname[100];  //"out_0000.jpg";

  strcpy(outname, dir);
  strcat(outname, "/");
  int n = 0;
char *temp = malloc(10);
   while(n<NUM_PIC)
   {
    if (n < NUM_PIC / 2)
    {
      sprintf(temp, "%06d-a.jpg", n+1);
      strcat(outname, temp);
    }
    else
    {
      sprintf(temp, "%06d-b.jpg", n+1-NUM_PIC/2);
      strcat(outname, temp);
    }
    printf("Processed image %d/%d: %s\n", n+1, NUM_PIC, images[n]->filename);
    n++;
      printf("cp -f %s %s\n", file, outname);
  char *args[] = {"/bin/cp", "-f", file, outname, (char *) 0};
  if (vfork() == 0) //shared memory fork; if child
    execv("/bin/ln", args);
   }
  
  

}

int main(int argc,char **argv)
{
   if (argc < 2)
   {
     printf("Usage: %s <directory>\n", argv[0]);
     return 1;
   }
   
   DIR  *dip, *dic;
   struct dirent *dit;
   int n;
   char *dir_and_file[MAX_PIC];
   
   if ((dip = opendir(argv[1])) == NULL)
   {
     fprintf(stderr, "can't open directory %s\n", argv[1]);
     return 1;
   }
   
   printid = 0;
   NUM_PIC = 0;
   
   while ((dit = readdir(dip)) != NULL)
   {
     if(strstr(dit->d_name, ".JPG") != NULL || strstr(dit->d_name, ".jpg") != NULL || strstr(dit->d_name, ".jpeg") != NULL || strstr(dit->d_name, ".JPEG") != NULL)
     {
       dir_and_file[NUM_PIC] = malloc(100);
       strcpy(dir_and_file[NUM_PIC], argv[1]);
       strcat(dir_and_file[NUM_PIC], dit->d_name);
       NUM_PIC++;
//       printf("READ %s %d\n", dir_and_file[NUM_PIC-1], NUM_PIC - 1);
     }
     if(strstr(dit->d_name, "data_") != NULL)
     {
       printid++;
     }
   }
 sort_files(dir_and_file, NUM_PIC);
 n = 0;
 while (n<NUM_PIC)
  {
    if (n != 0)
      make_file(argv[1], dir_and_file[NUM_PIC - n - 1]);
//    printf("%d\n", n);
    n++;
    if ((n+1) == NUM_PIC)
      n++;
  }
   closedir(dip);
   
   return 0;
}
