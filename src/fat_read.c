#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat_read.h"

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

  if (root -> size != 0) root -> last -> next = root_dir_tmp;
  else root -> first = root_dir_tmp;

  root -> size++;
  root -> last = root_dir_tmp;

}

void add_cluster(struct cluster *cluster, char *p_cluster, unsigned int position)
{
  struct cluster_dyn *cluster_dyn;
  cluster_dyn = (struct cluster_dyn *) malloc (sizeof(struct cluster_dyn));
  if (!cluster_dyn)
  {
    printf("Error malloc cluster_dyn.\n");
    exit(3);
  }
  cluster_dyn -> cluster = p_cluster;
  cluster_dyn -> position = position;

  if (cluster -> size != 0) cluster -> last -> next = cluster_dyn;
  else cluster -> first = cluster_dyn;

  cluster -> size++;
  cluster -> last = cluster_dyn; 
}

void read_fat(char *file_name, struct boot_record *boot_record, struct root_dir *root_dir, struct fat_table *fat_table, struct cluster *cluster)
{
  
  struct root_directory *p_root_directory;      
  unsigned int *fat_item;
  unsigned int *fat;
  char *p_cluster;
  FILE *p_file;
  int i,j;
         
  p_file = fopen(file_name, "r");

  if (!p_file)
  {
    printf("Error open file\n");
    exit(2);
  }

  fseek(p_file, SEEK_SET, 0); 
  fread(boot_record, sizeof(struct boot_record), 1, p_file);


  if (!strcmp(boot_record -> signature, OK))
  {
    printf("FAT signatura: OK.\n");

  } else if (!strcmp(boot_record -> signature, NOK))
  {
    printf("FAT tabulky nejsou konzistencni.\n");
    exit(1);
  }else if (!strcmp(boot_record -> signature, FAI))
  {
    printf("FAT tabulky obsahuji chyby.\n");
    exit(1);
  }
 
  
  fat_item = (unsigned int *) malloc (sizeof (unsigned int));
  fat = (unsigned int *) calloc(boot_record -> cluster_count, sizeof(unsigned int));

  fat_table -> size = boot_record -> cluster_count;
  
  for (i = 0; i < boot_record -> cluster_count; i++)
  {
    fread(&fat[i], sizeof(unsigned int), 1, p_file);   
  }

  for (j = 1; j < boot_record -> fat_copies; j++)
  {
    for (i = 0; i < boot_record -> cluster_count; i++)
    {
      fread(fat_item, sizeof(unsigned int), 1, p_file);  
      
      if (*fat_item != fat[i])
      {
        printf("Tabulky FAT se neshoduji.\n");
        exit(2);
      }
    }
  }

  printf("FAT table check: OK.\n");
  fat_table -> fat = fat;

  for (i = 0; i < boot_record -> root_directory_max_entries_count; i++)
  {
    p_root_directory = (struct root_directory *) malloc (sizeof (struct root_directory));
    fread(p_root_directory, sizeof(struct root_directory), 1, p_file);
    add_root_directory(root_dir, p_root_directory);
  }

  for (i = 0; i < boot_record -> reserved_cluster_count; i++)
  {   
    p_cluster = malloc(sizeof(char) * boot_record -> cluster_size);
    fread(p_cluster, sizeof(char) * boot_record -> cluster_size, 1, p_file);
    if (fat[i] == FAT_UNUSED) continue;
    add_cluster(cluster, p_cluster, i);
  }

  if (cluster -> size == (boot_record -> reserved_cluster_count - boot_record -> root_directory_max_entries_count)) printf("FAT check cluster: OK\n");
  else printf("FAT check cluster: FAIL\n");

}


