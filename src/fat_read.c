#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat_read.h"

/***********************************************************
 * Pridani nove root_directory
 *    root - korenova slozka
 *    item - slozka pro pridani
************************************************************/
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

/***********************************************************
 * Pridani noveho klastru
 *    cluster - korenovy klastr
 *    p_cluster - klastr s daty
 *    position - pozice ve fat tabulce
************************************************************/
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
  cluster_dyn -> next = NULL;

  if (cluster -> size != 0) cluster -> last -> next = cluster_dyn;
  else cluster -> first = cluster_dyn;

  cluster -> size++;
  cluster -> last = cluster_dyn; 
}

/***********************************************************
 * Uvolneni pameti
************************************************************/
void free_fat(struct boot_record *boot_record, struct root_dir *root_dir, struct fat_table *fat_table, struct cluster *cluster)
{
  struct root_dir_dyn *root_dir_item;
  struct cluster_dyn *cluster_item;
  
  free(boot_record);
  free(fat_table -> fat);
  free(fat_table);

  while(cluster -> first)
  {
    cluster_item = cluster -> first -> next;
    free(cluster -> first -> cluster);
    free(cluster -> first);
    cluster -> first = cluster_item;
  }
  free(cluster);

  while(root_dir -> first)
  {
    root_dir_item = root_dir -> first -> next;
    free(root_dir -> first -> dir);
    free(root_dir -> first);
    root_dir -> first = root_dir_item;
  }
  free(root_dir);
}

/***********************************************************
 * Nacte dat z fat do struktur
 *    file_name - jmeno fat souboru
 *    boot_record - pocatecni data o fat souboru
 *    root_dir - popis souboru/slozky
 *    fat_table - fat tabulka
 *    cluster - clustry fatky
************************************************************/
void read_fat(char *file_name, struct boot_record *boot_record, struct root_dir *root_dir, struct fat_table *fat_table, struct cluster *cluster)
{
  struct root_directory *p_root_directory;      
  unsigned int *fat_item;
  char *p_cluster;
  FILE *p_file;
  int i,j;
  
  /* otevre soubor pro cteni */
  p_file = fopen(file_name, "r");

  /* kontrola spravne otevreneho souboru */
  if (!p_file)
  {
    printf("Error open file\n");
    exit(2);
  }

  /* skok na zacatek souboru */
  fseek(p_file, SEEK_SET, 0); 
  
  /* nacteni boot_record */
  fread(boot_record, sizeof(struct boot_record), 1, p_file);

  /* kontrola spravnosti signatury u nacteneho boot_record */
  if (!strcmp(boot_record -> signature, OK))
  {
    printf("FAT signatura: OK.\n");

  } else if (!strcmp(boot_record -> signature, NOK))
  {
    printf("FAT tabulky nejsou konzistencni.\n");
    exit(2);
  }else if (!strcmp(boot_record -> signature, FAI))
  {
    printf("FAT tabulky obsahuji chyby.\n");
    exit(2);
  }
  
  fat_table -> fat = (unsigned int *) calloc(boot_record -> cluster_count, sizeof(unsigned int));
  fat_table -> size = boot_record -> cluster_count;
 
  /* cteni fat tabulky */
  for (i = 0; i < boot_record -> cluster_count; i++)
  {
    fread(&(fat_table -> fat)[i], sizeof(unsigned int), 1, p_file);   
  }

  fat_item = (unsigned int *) malloc (sizeof (unsigned int));
  
  for (j = 1; j < boot_record -> fat_copies; j++)
  {
    for (i = 0; i < boot_record -> cluster_count; i++)
    {
      fread(fat_item, sizeof(unsigned int), 1, p_file);  
      
      if (*fat_item != fat_table -> fat[i])
      {
        printf("Tabulky FAT se neshoduji.\n");
        exit(2);
      }
    }
  }
  free(fat_item);

  printf("FAT table check: OK.\n");

  /* cteni root_directory */
  for (i = 0; i < boot_record -> root_directory_max_entries_count; i++)
  {
    p_root_directory = (struct root_directory *) malloc (sizeof (struct root_directory));
    fread(p_root_directory, sizeof(struct root_directory), 1, p_file);
    add_root_directory(root_dir, p_root_directory);
  }

  /* cteni klastru */
  for (i = 0; i < boot_record -> reserved_cluster_count; i++)
  {   
    p_cluster = malloc(sizeof(char) * boot_record -> cluster_size);
    fread(p_cluster, sizeof(char) * boot_record -> cluster_size, 1, p_file);
    if (fat_table -> fat[i] == FAT_UNUSED)
    {
      free(p_cluster);
      continue;
    }
    add_cluster(cluster, p_cluster, i);
  }

  /* kontrola spravneho poctu nactenych klastru */
  if (cluster -> size == (boot_record -> reserved_cluster_count - boot_record -> root_directory_max_entries_count)) printf("FAT check cluster: OK\n");
  else printf("FAT check cluster: FAIL\n");

  /* uzavreni souboru */
  fclose(p_file);
}


