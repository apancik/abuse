#include "file_utils.h"
#include <stdlib.h>
#include <string.h>

static char *filename_prefix = NULL;
static char *save_filename_prefix = NULL;

void set_filename_prefix(char const *prefix)
{
  if (filename_prefix)
  {
    free(filename_prefix);
  }

  if (prefix)
  {
    filename_prefix = strcpy((char *)malloc(strlen(prefix) + 2), prefix);
    int len = strlen(prefix);
    if (prefix[len - 1] != '\\' && prefix[len - 1] != '/')
    {
      filename_prefix[len] = PATH_SEPARATOR_CHAR;
      filename_prefix[len + 1] = 0;
    }
  }
  else
  {
    filename_prefix = NULL;
  }
}

char *get_filename_prefix()
{
  return filename_prefix;
}

void set_save_filename_prefix(char const *save_prefix)
{
  if (save_filename_prefix)
  {
    free(save_filename_prefix);
  }

  if (save_prefix)
  {
    int len = strlen(save_prefix);
    save_filename_prefix = (char *)malloc(len + 1);
    strcpy(save_filename_prefix, save_prefix);
  }
  else
  {
    save_filename_prefix = NULL;
  }
}

char *get_save_filename_prefix()
{
  return save_filename_prefix;
}

FILE *prefix_fopen(const char *filename, const char *mode)
{
  char tmp_name[512];
  const char *final_name = filename;

  // fprintf(stderr, "prefix_fopen: called with filename='%s', mode='%s'\n", filename, mode);

  // For write/append modes, always use save prefix if available
  if (strchr(mode, 'w') || strchr(mode, 'a'))
  {
    // fprintf(stderr, "prefix_fopen: write/append mode detected (mode='%s')\n", mode);

    if (get_save_filename_prefix())
    {
      snprintf(tmp_name, sizeof(tmp_name), "%s%s", get_save_filename_prefix(), filename);
      final_name = tmp_name;
      // fprintf(stderr, "prefix_fopen: using save prefix because it's write/append mode, final_name='%s'\n", final_name);
    }
    else
    {
      // fprintf(stderr, "prefix_fopen: no save prefix available, proceeding with original filename='%s'\n", filename);
    }
  }
  // For read mode, try save prefix first, then regular prefix
  else if (strchr(mode, 'r'))
  {
    // fprintf(stderr, "prefix_fopen: read mode detected (mode='%s')\n", mode);

    if (get_save_filename_prefix())
    {
      snprintf(tmp_name, sizeof(tmp_name), "%s%s", get_save_filename_prefix(), filename);
      FILE *fp = fopen(tmp_name, mode);
      if (fp)
      {
        // fprintf(stderr, "prefix_fopen: successfully opened with save prefix, tmp_name='%s'\n", tmp_name);
        return fp;
      }
      else
      {
        // fprintf(stderr, "prefix_fopen: failed to open with save prefix, tmp_name='%s'\n", tmp_name);
      }
    }

    if (get_filename_prefix())
    {
      snprintf(tmp_name, sizeof(tmp_name), "%s%s", get_filename_prefix(), filename);
      final_name = tmp_name;
      // fprintf(stderr, "prefix_open: using regular prefix after failing save prefix, final_name='%s'\n", final_name);
    }
    else
    {
      // fprintf(stderr, "prefix_fopen: no regular prefix available, proceeding with original filename='%s'\n", filename);
    }
  }

  FILE *fp = fopen(final_name, mode);
  // if (fp)
  //   // fprintf(stderr, "prefix_fopen: successfully opened final_name='%s'\n", final_name);
  // else
  //   // fprintf(stderr, "prefix_fopen: failed to open final_name='%s'\n", final_name);

  return fp;
}

int prefix_open(const char *filename, int flags, mode_t mode)
{
  char tmp_name[512];
  const char *final_name = filename;

  // fprintf(stderr, "prefix_open: called with filename='%s', flags='%d', mode='%d'\n", filename, flags, mode);

  // For write modes, always use save prefix if available
  if (flags & (O_WRONLY | O_RDWR | O_CREAT | O_APPEND))
  {
    // fprintf(stderr, "prefix_open: write mode detected (flags='%d')\n", flags);

    if (get_save_filename_prefix())
    {
      snprintf(tmp_name, sizeof(tmp_name), "%s%s", get_save_filename_prefix(), filename);
      final_name = tmp_name;
      // fprintf(stderr, "prefix_open: using save prefix because of write flags, final_name='%s'\n", final_name);
    }
    else
    {
      // fprintf(stderr, "prefix_open: no save prefix available, proceeding with original filename='%s'\n", filename);
    }
  }
  // For read-only mode, try save prefix first, then regular prefix
  else
  {
    // fprintf(stderr, "prefix_open: read-only mode detected (flags='%d')\n", flags);

    if (get_save_filename_prefix())
    {
      snprintf(tmp_name, sizeof(tmp_name), "%s%s", get_save_filename_prefix(), filename);
      int fd = open(tmp_name, flags, mode);
      if (fd != -1)
      {
        // fprintf(stderr, "prefix_open: successfully opened with save prefix, tmp_name='%s'\n", tmp_name);
        return fd;
      }
      else
      {
        // fprintf(stderr, "prefix_open: failed to open with save prefix, tmp_name='%s'\n", tmp_name);
      }
    }

    if (get_filename_prefix())
    {
      snprintf(tmp_name, sizeof(tmp_name), "%s%s", get_filename_prefix(), filename);
      final_name = tmp_name;
      // fprintf(stderr, "prefix_open: using regular prefix after failing save prefix, final_name='%s'\n", final_name);
    }
    else
    {
      // fprintf(stderr, "prefix_open: no regular prefix available, proceeding with original filename='%s'\n", filename);
    }
  }

  int fd = open(final_name, flags, mode);
  // if (fd != -1)
  //   // fprintf(stderr, "prefix_open: successfully opened final_name='%s'\n", final_name);
  // else
  //   // fprintf(stderr, "prefix_open: failed to open final_name='%s'\n", final_name);

  return fd;
}