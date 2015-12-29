#ifndef _PSEUDO_FAT_H
#define _PSEUDO_FAT_H

#define FAT_UNUSED      65535
#define FAT_FILE_END    65534
#define FAT_BAD_CLUSTER 65533

#define OK  "OK"
#define NOK "NOK"
#define FAI "FAI"

struct boot_record
{
    char volume_descriptor[251];
    int fat_type;
    int fat_copies;
    unsigned int cluster_size;
    long root_directory_max_entries_count;
    unsigned int cluster_count;
    unsigned int reserved_cluster_count;
    char signature[4];
};

struct root_directory
{
    char file_name[13];    
    char file_mod[10];
    short file_type;
    long file_size;
    unsigned int first_cluster;
};

struct root_dir_dyn
{
  struct root_directory *dir;
  struct root_dir_dyn *next;
};

struct root_dir
{
  int size;
  struct root_dir_dyn *first;
  struct root_dir_dyn *actual;
  struct root_dir_dyn *last;
};

struct fat_table
{
  unsigned int size;
  unsigned int *fat;
};

struct cluster_dyn
{
  unsigned int position;
  char *cluster;
  struct cluster_dyn *next;
};

struct cluster
{
  unsigned int size;
  struct cluster_dyn *first;
  struct cluster_dyn *actual;
  struct cluster_dyn *last;
};

#endif
