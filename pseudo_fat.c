#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pseudo_fat.h"


int valid_param(int argc, char **argv)
{
  if (argc == 2)
  {
    long res = strtol(argv[1], NULL, 10);
    if (!res)
    {
      printf("Neplatny parametr '%s'\n", argv[1]);
      exit(1);
    }

    THREAD_COUNT =  res;
  }
  return 0;
}

void add_root_directory(struct root_dir *root, struct root_directory *item)
{
  struct root_dir_dyn *root_dir_tmp;

  root_dir_tmp = (struct root_dir_dyn *) malloc (sizeof(struct root_dir_dyn));
  
  if (!root_dir_tmp)
  {
    printf("Error malloc root_dir_tmp\n");
    exit(3);
  }

  root_dir_tmp -> next = NULL;
  root_dir_tmp -> dir = item;

  if (root -> size != 0)
  {
    root -> last -> next = root_dir_tmp;

  }else
  {
    root -> first = root_dir_tmp;
  }

  root -> size++;
  root -> last = root_dir_tmp;

}

int main(int argc, char **argv)
{
  valid_param(argc, argv);

  struct boot_record *p_boot_record;
  struct root_directory *p_root_directory;      
  FILE *p_file;

  struct root_dir *root_dir;
  root_dir = (struct root_dir *) malloc (sizeof(struct root_dir));

  if (!root_dir)
  {
    printf("Error malloc root_dir");
    exit(3);
  }

  root_dir -> size = 0;

  
  p_boot_record = (struct boot_record *) malloc (sizeof (struct boot_record));
  
  if (!p_boot_record)
  {
    printf("Error malloc p_boot_record\n");
    exit(3);
  }
         
  p_file = fopen("bigfat.fat", "r");
  fseek(p_file, SEEK_SET, 0);
  
  fread(p_boot_record, sizeof(struct boot_record), 1, p_file);


  if (!strcmp(p_boot_record -> signature, OK))
  {
    printf("FAT signatura: OK.\n");

  } else if (!strcmp(p_boot_record -> signature, NOK))
  {
    printf("FAT tabulky nejsou konzistencni.\n");
    return 1;
  }else if (!strcmp(p_boot_record -> signature, FAI))
  {
    printf("FAT tabulky obsahuji chyby.\n");
    return 1;
  }
 
  int i,j;
  
  unsigned int *fat_item;
  fat_item = (unsigned int *) malloc (sizeof (unsigned int));

  unsigned int *fat;
  fat = (unsigned int *) calloc(p_boot_record -> cluster_count, sizeof(unsigned int));
  
  for (i = 0; i < p_boot_record -> cluster_count; i++)
  {
    fread(&fat[i], sizeof(unsigned int), 1, p_file);   
  }

  for (j = 1; j < p_boot_record -> fat_copies; j++)
  {
    for (i = 0; i < p_boot_record -> cluster_count; i++)
    {
      fread(fat_item, sizeof(unsigned int), 1, p_file);  
      
      if (*fat_item != fat[i])
      {
        printf("Tabulky FAT se neshoduji.\n");
        exit(2);
      }
    }
  }

  printf("FAT kontrola: OK.\n");

  for (i = 0; i < p_boot_record -> root_directory_max_entries_count; i++)
  {
    p_root_directory = (struct root_directory *) malloc (sizeof (struct root_directory));
    fread(p_root_directory, sizeof(struct root_directory), 1, p_file);
    add_root_directory(root_dir, p_root_directory);
  }
 

  printf("%s\n%s\n%s\n", root_dir -> first -> dir -> file_name, root_dir -> first -> next -> dir -> file_name, root_dir -> last -> dir -> file_name);

  return 0;
}
