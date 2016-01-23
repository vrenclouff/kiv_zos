#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fat_rw.h"

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

  root_dir_tmp -> dir = item;

  root_dir_tmp -> next = root -> first;
  root -> first = root_dir_tmp;
  root -> size++;

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

  cluster_dyn -> next = cluster -> first;
  cluster -> first = cluster_dyn;
  cluster -> size++;
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
  
  if (!file_name || !boot_record || !root_dir || !fat_table || !cluster) return;
  
  /* otevre soubor pro cteni */
  p_file = fopen(file_name, "rb");

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
    printf("FAT tables are not consistent.\n");
    exit(2);
  }else if (!strcmp(boot_record -> signature, FAI))
  {
    printf("FAT tables contain errors.\n");
    exit(2);
  }
  
  fat_table -> fat = (unsigned int *) calloc(boot_record -> cluster_count, sizeof(unsigned int));
  fat_table -> size = boot_record -> cluster_count;
 
  /* cteni fat tabulky */
  fread(fat_table -> fat, sizeof(unsigned int), boot_record -> cluster_count, p_file);  

  fat_item = (unsigned int *) malloc (sizeof (unsigned int));
  
  for (j = 1; j < boot_record -> fat_copies; j++)
  {
    for (i = 0; i < boot_record -> cluster_count; i++)
    {
      fread(fat_item, sizeof(unsigned int), 1, p_file);  
      
      if (*fat_item != fat_table -> fat[i])
      {
        printf("FAT tables are not same.\n");
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
  if (cluster -> size == (boot_record -> reserved_cluster_count - boot_record -> root_directory_max_entries_count))
  {
    printf("FAT check cluster: OK\n");
  }else
  {
    printf("FAT check cluster: FAIL\n"); 
    exit(2);
  }

  /* nastaveni aktualniho ukazatele na zacatek */
  root_dir -> actual = root_dir -> first;
  
  /* uzavreni souboru */
  fclose(p_file);
  
  printf("FAT read file: OK\n");
}

/***********************************************************
 * Najde klastr pro dany blok z FAT tabulky 
 *  block - blok ve FAT, pro ktery chceme klastr
************************************************************/
struct cluster_dyn *find_cluster(struct cluster *cluster, unsigned int block)
{
  struct cluster_dyn *cluster_file;

  cluster_file = cluster -> first;
  while(cluster_file)
  {
    if (cluster_file -> position == block)
      return cluster_file;

    cluster_file = cluster_file -> next;
  }
}

/***********************************************************
 * Zapise dat ze struktur do fat souboru
 *    file_name - jmeno fat souboru
 *    boot_record - pocatecni data o fat souboru
 *    root_dir - popis souboru/slozky
 *    fat_table - fat tabulka
 *    cluster - clustry fatky
************************************************************/
void write_fat(char *file_name, struct boot_record *boot_record, struct root_dir *root_dir, struct fat_table *fat_table, struct cluster *cluster)
{
  int i, j;
  unsigned int block;
  struct root_dir_dyn *root_dir_temp;
  struct cluster_dyn *cluster_temp;
  FILE *fp;
  
  if (!file_name || !boot_record || !root_dir || !fat_table || !cluster) return;

  char cluster_empty[boot_record -> cluster_size];
  strcpy(cluster_empty, "");

  printf("Repaired FAT file: %s\n", file_name); 

  unlink(file_name);
  fp = fopen(file_name, "wb");

  /* zapis boot_record */
  fwrite(boot_record, sizeof(struct boot_record), 1, fp);

  /* zapis FAT tabulku */
  for (i = 0; i < boot_record -> fat_copies; i++)
  {
      fwrite(fat_table -> fat, sizeof(unsigned int), fat_table -> size, fp);
  }

  /* zapis root_directory  */
  root_dir_temp = root_dir -> first;
  while(root_dir_temp)
  {
    fwrite(root_dir_temp -> dir, sizeof(struct root_directory), 1, fp);
    root_dir_temp = root_dir_temp -> next;
  }
 
  /* zapis klastru  */
  for (i = 0; i < fat_table -> size; i++)
  {
    block = fat_table -> fat[i];
    if (block == FAT_UNUSED)
    {
      fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    }else
    {
      cluster_temp = find_cluster(cluster, i);
      fwrite(cluster_temp -> cluster, boot_record -> cluster_size, 1, fp);
    }
  }
  
  /* uzavreni souboru  */
  fclose(fp);
}
