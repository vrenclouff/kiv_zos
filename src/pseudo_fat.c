#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "pseudo_fat.h"
#include "fat_read.h"

unsigned int thread_count = 1;

struct boot_record *boot_record;
struct root_dir *root_dir;
struct fat_table *fat_table;
struct cluster *cluster;


int valid_param(int argc, char **argv)
{
  if (argc < 2)
  {
    printf("Neplatny pocet parametru.\n");
    exit(1);
  }

  if (argc == 3)
  {
    long res = strtol(argv[2], NULL, 10);
    if (!res)
    {
      printf("Neplatny parametr '%s'\n", argv[2]);
      exit(1);
    }

    thread_count =  res;
  }
  return 0;
}

struct cluster *init_cluster()
{
  struct cluster *cluster;
  cluster = (struct cluster *) malloc (sizeof(struct cluster));
  if (!cluster)
  {
    printf("Error malloc cluster\n");
    exit(3);
  }
  cluster -> size = 0;
  return cluster;
}

struct root_dir *init_root_dir()
{
  struct root_dir *root_dir;
  root_dir = (struct root_dir *) malloc (sizeof(struct root_dir));
  if (!root_dir)
  {
    printf("Error malloc root_dir\n");
    exit(3);
  }
  root_dir -> size = 0; 
  return root_dir;
}

struct boot_record *init_boot_record()
{
  struct boot_record *boot_record;
  boot_record = (struct boot_record *) malloc (sizeof (struct boot_record));
  if (!boot_record)
  {
    printf("Error malloc boot_record\n");
    exit(3);
  }
  return boot_record;
}

struct fat_table *init_fat_table()
{
  struct fat_table *fat_table;
  fat_table = (struct fat_table *) malloc (sizeof (struct fat_table));
  if (!fat_table)
  {
    printf("Error malloc fat_table\n");
    exit(3);
  }
  fat_table -> size = 0;
  return fat_table;
}

struct root_dir_dyn *get_file()
{
  struct root_dir_dyn *item;
  item = root_dir -> actual;
  if (item) root_dir -> actual = item -> next;
  return item;  
}

int check_fat_flag(unsigned int *block, int *count, int *bad_files)
{
  switch(*block)
  {
    case FAT_UNUSED: *bad_files = *bad_files + 1; return 0;
    case FAT_BAD_CLUSTER: *bad_files = *bad_files + 1; return 0;
    case FAT_FILE_END: *count = *count + 1; return 0;
  }
  *count = *count + 1;
  return 1;
}

void check_cluster_length(char *flag, int *bad_files)
{
  struct root_dir_dyn *item;
  int *count;
  unsigned int length, *block;

  count = malloc(sizeof(int));
  if (!count)
  {
    printf("Error malloc count\n");
    exit(3);
  }
  block = malloc(sizeof(unsigned int));
  if (!block)
  {
    printf("Error malloc block\n");
    exit(3);
  }

  *flag = 1;
  *bad_files = 0;
  while(item = get_file())
  {
    length = round(item -> dir -> file_size / boot_record -> cluster_size + 0.5f);
    *count = 0;
    *block = fat_table -> fat[item -> dir -> first_cluster];
    while (check_fat_flag(block, count, bad_files))
    {
      *block = fat_table -> fat[*block];
    }
    if (*count != length) *flag = 0;
  }
  free(count);
  free(block);
}

int main(int argc, char **argv)
{
//  valid_param(argc, argv);
//  char *file_name = argv[1];

  char file_name [] = "../output.fat";
//  char file_name [] = "../bigfat.fat";

  root_dir = init_root_dir();
  boot_record = init_boot_record(); 
  fat_table = init_fat_table();
  cluster = init_cluster();

  read_fat(file_name, boot_record, root_dir, fat_table, cluster);

  char *flag = (char *) malloc(sizeof(char));
  int *bad_files = (int *) malloc(sizeof(int));
  check_cluster_length(flag, bad_files);

  if (*flag) printf("Velikost souboru: OK\n");
  else printf("Velikost souboru: FAIL\n");
  
  printf("File with bad cluster: %d\n", *bad_files);
  
  free_fat(boot_record, root_dir, fat_table, cluster);
  free(flag);
  free(bad_files);
  return 0;
}
