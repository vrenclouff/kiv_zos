#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include "pseudo_fat.h"
#include "fat_rw.h"

/************************************************************/

unsigned int thread_count = 1;
pthread_mutex_t files_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t repair_mutex = PTHREAD_MUTEX_INITIALIZER;

struct boot_record *boot_record;
struct root_dir *root_dir;
struct fat_table *fat_table;
struct cluster *cluster;

/************************************************************/

void print_clusters()
{
  struct cluster_dyn *current;
  current = cluster -> first;
  printf("Cluster size: %d\n", cluster -> size);
  while(current)
  {
    printf("Cluster %d\n%s\n", current -> position, current -> cluster);
    current = current -> next;
  }
}
void print_fat()
{
  int i;
  printf("-------------- FAT TABLE -------------\n");
  for(i=0;i<fat_table->size;i++)
  {
    if (fat_table->fat[i]==FAT_UNUSED) continue;
    else if (fat_table->fat[i]==FAT_FILE_END) printf("%d: FAT_FILE_END\n", i);
    else if (fat_table->fat[i]==FAT_BAD_CLUSTER) printf("%d: FAT_BAD_CLUSTER\n",i);
    else printf("%d: %d\n", i, fat_table->fat[i]);
  }
}

/***********************************************************
 * Validace vstupnich parametru
 * Kontroluje se spravny pocet:
 *  1. param = nazev FAT souboru
 *  2. param = pocet vlaken
 *
 *  argc - pocet argumentu
 *  argv - argumenty
************************************************************/
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

/***********************************************************
 * Vytvoreni struktury cluster
************************************************************/
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

/***********************************************************
 * Vytvoreni struktury pro rood_dir
************************************************************/
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

/***********************************************************
 * Vytvoreni struktury pro boot_record
************************************************************/
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

/***********************************************************
 * Vytvoreni struktury pro fat_table
************************************************************/
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

/***********************************************************
 * Vytvoreni struktury pro vysledky z vlaken
************************************************************/
struct pthread_data *init_result_data()
{
  struct pthread_data *result_data;
  result_data = (struct pthread_data *) malloc (sizeof(struct pthread_data));
  if (!result_data)
  {
    printf("Error malloc result_data\n");
    exit(3);
  }
  result_data -> flag = 0;
  result_data -> count_files = 0;
  return result_data;
}

/***********************************************************
 * Vraci adresar pri zpracovani
************************************************************/
struct root_dir_dyn *get_file()
{
  struct root_dir_dyn *item;
  pthread_mutex_lock( &files_mutex );
  item = root_dir -> actual;
  if (item) root_dir -> actual = item -> next;
  pthread_mutex_unlock( &files_mutex );
  return item;  
}

/***********************************************************
 * Najde nejblizsi volny cluster pro opravu dat
************************************************************/
int find_free_cluster(int actual_position)
{
  int pos, block;
  for(pos = actual_position; pos < fat_table -> size; pos++)
  {
    block = fat_table -> fat[pos];
    if (block == FAT_UNUSED) break;
  }
  return pos;
}

/***********************************************************
 * Oprava dat
************************************************************/
char *repair_data(char *bad_data)
{
  char *data;
  data = calloc(boot_record -> cluster_size, sizeof(char));
  if (!data)
  {
    printf("Error calloc data\n");
    exit(3);
  }
  memcpy(data, &bad_data[strlen(MARK_BAD_CLUSTER)], boot_record -> cluster_size - (2*strlen(MARK_BAD_CLUSTER)));
  return data;  
}

/***********************************************************
 * Odstrani klastr. Je potreba klastr smazat,
 * protoze je poskozeny
 *
 *  cluster - klastr, ktery chceme smazat
************************************************************/
void remove_cluster(struct cluster_dyn *cluster_rm)
{
  struct cluster_dyn *current, *temp;

  current = cluster -> first;
  if (current -> position == cluster_rm -> position)
  {
    temp = current;
    cluster -> first = current -> next;
  }else
  {
    while(current)
    {
      if (current -> next != NULL && current -> next -> position == cluster_rm -> position)
      {
        temp = current -> next; 
        break;
      }
      current = current -> next;
    }
  }
  current -> next = temp -> next;
  free(temp);
}

/***********************************************************
 * Prida novy klastr na spravne misto
 *  data - data ulozena v klastru
 *  position - pozice klastru
************************************************************/
void create_cluster(char *data, int position)
{
  struct cluster_dyn *actual, *temp;

  temp = (struct cluster_dyn *) malloc (sizeof(struct cluster_dyn *));
  if (!temp)
  {
    printf("Error malloc temp\n");
    exit(3);
  }
  temp -> cluster = data;
  temp -> position = position;

  actual = cluster -> first;
  while(actual)
  {
    if (actual -> position > position && actual -> next != NULL && position > actual -> next -> position)
    {
      temp -> next = actual -> next;
      actual -> next = temp;
      break;
    }
    actual = actual -> next;
  }

}

/***********************************************************
 * Oprava spatneho klastru
 *  file - soubor, ktery obsahuje spatne klastry
 *  actual_cluster - konkretni klastr v souboru
 *  actual_block - blok ve FAT tabulce
************************************************************/
unsigned int repair_bad_cluster(struct cluster_dyn *actual_cluster, unsigned int previous_block)
{
  unsigned int actual_block, next_block;
  int ind_next_free_block, ind_actual_block;
  char *data_copy;

  if (!actual_cluster || !previous_block) return actual_cluster -> position;
  
  data_copy = repair_data(actual_cluster -> cluster);
  actual_block = fat_table -> fat[previous_block];
  next_block = fat_table -> fat[actual_block];
  ind_actual_block = actual_cluster -> position;

  pthread_mutex_lock( &repair_mutex );
  ind_next_free_block = find_free_cluster(actual_cluster -> position);
 
  /* ve fat prislusne oznacit */
  fat_table -> fat[actual_block] = FAT_BAD_CLUSTER;
 
  /* odstranit klastry a pripojit nove */
  create_cluster(data_copy, ind_next_free_block);
//  remove_cluster(actual_cluster);

  /* ve FAT tabulce spravne oznacit zmeny */
  fat_table -> fat[previous_block] = ind_next_free_block;
  if (actual_block == FAT_FILE_END)
  {
    fat_table -> fat[ind_next_free_block] = FAT_FILE_END;
  }
  else
  {
    fat_table -> fat[ind_next_free_block] = next_block;
  }
  pthread_mutex_unlock( &repair_mutex );
  return ind_next_free_block;
}

/***********************************************************
 * Kontrola klastru, zda obsahuje znacky pro spatny klastr
 *  cluster_item - kontrolovany klastr
************************************************************/
int check_bad_cluster(struct cluster_dyn *cluster_item)
{
  char *start_cluster, *end_cluster;
  int flag;
  
  start_cluster = calloc(strlen(MARK_BAD_CLUSTER), sizeof(char));
  end_cluster = calloc(strlen(MARK_BAD_CLUSTER), sizeof(char));
  
  if (!start_cluster || !end_cluster)
  {
    printf("Error calloc start_cluster or end_cluster\n");
    exit(3);
  }

  /* kopirovani startu a konec klastru */
  memcpy(start_cluster, cluster_item -> cluster, strlen(MARK_BAD_CLUSTER));
  memcpy(end_cluster, &(cluster_item -> cluster)[cluster -> size - strlen(MARK_BAD_CLUSTER)-1], strlen(MARK_BAD_CLUSTER));
  
  /* porovnani, zda zacatek a konec klastru obsahuji znacky pro spatny klastr */
  if (!strcmp(start_cluster, MARK_BAD_CLUSTER) && !strcmp(end_cluster, MARK_BAD_CLUSTER))
  {
    flag = 1;
  }else
  {
    flag = 0;
  }
  return flag;
}

/***********************************************************
 * Najde klastr pro dany blok z FAT tabulky 
 *  block - blok ve FAT, pro ktery chceme klastr
************************************************************/
struct cluster_dyn *find_cluster_for_file(unsigned int block)
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
 * Kontrola delky klastru podle FAT tabulky
************************************************************/
void *check_cluster_length()
{
  pthread_data *pthread_data;
  struct root_dir_dyn *file;
  struct cluster_dyn *cluster_file;
  int count;
  unsigned int length, actual_block, previous_block;

  /* inicializace struktury na sber dat s vysledky vlakna */
  pthread_data = init_result_data();
 
  /* nacteni dalsiho souboru ke zpracovani */
  while(file = get_file())
  {
    pthread_data -> count_files++;
    length = round(file -> dir -> file_size / boot_record -> cluster_size + 0.5f);
    count = 1;
    actual_block = file -> dir -> first_cluster;
    previous_block = actual_block;
 
    /* kontrola prvniho klastru */
    cluster_file = find_cluster_for_file(actual_block);
    if (check_bad_cluster(cluster_file))
    {
      previous_block = repair_bad_cluster(cluster_file, previous_block);
      file -> dir -> first_cluster = previous_block;
    }

    actual_block = fat_table -> fat[previous_block];

    /* kontrola dalsich klastru */
    for(; actual_block != FAT_FILE_END; count++)
    {
      cluster_file = find_cluster_for_file(actual_block);

      if (check_bad_cluster(cluster_file))
      {
        printf("Repair file: %s\n", file -> dir -> file_name);
        actual_block = repair_bad_cluster(cluster_file, previous_block);
      }
      previous_block = actual_block;
      actual_block = fat_table -> fat[actual_block];
    }

    /* kontrola spravne delky */
    if (count != length) pthread_data -> flag = 1;
  } 
  
  return (void *) pthread_data;
}

/***********************************************************
 * Main - spousti aplikaci
 *
 * argc - pocet parametru
 * argv - parametry
************************************************************/
int main(int argc, char **argv)
{
  pthread_t *pthread;
  pthread_data *pthread_data, *result_data;
  int i;
  
  valid_param(argc, argv);
  char *file_name = argv[1];

  root_dir = init_root_dir();
  boot_record = init_boot_record(); 
  fat_table = init_fat_table();
  cluster = init_cluster();
  result_data = init_result_data();

  /* nacteni struktur ze souboru fat */
  read_fat(file_name, boot_record, root_dir, fat_table, cluster);
 
  /* vytvoreni vlaken */
  pthread = (pthread_t *) malloc (thread_count * sizeof (pthread_t));
  for (i = 0; i < thread_count; i++)
  {
    pthread_create( &pthread[i], NULL, (void *) check_cluster_length, NULL);
  }

  /* sber dat z vlaken */
  for (i = 0; i < thread_count; i++)
  {
    pthread_join(pthread[i], (void **) &pthread_data);
    printf("Thread %d - %d files\n",i, pthread_data -> count_files);
    result_data -> count_files += pthread_data -> count_files;
    result_data -> flag += pthread_data -> flag;
    free(pthread_data);
  }

  /* kontrola vysledku dat z vlaken */
  if (!result_data -> flag) printf("Length of files: OK\n");
  else printf("Length of files: FAIL\n");

  print_clusters();
  /* zapis struktur do souboru fat */
  write_fat("output-repair.fat", boot_record, root_dir, fat_table, cluster);
  
  /* uvolneni pameti */
  free_fat(boot_record, root_dir, fat_table, cluster);
  free(pthread);
  free(result_data);
  return 0;
}
