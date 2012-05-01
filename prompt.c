#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define FMT_FG_RESET   "%{\x1b[0m%}"
#define FMT_FG_BLACK   "%{\x1b[30m%}"
#define FMT_FG_RED     "%{\x1b[31m%}"
#define FMT_FG_GREEN   "%{\x1b[32m%}"
#define FMT_FG_YELLOW  "%{\x1b[33m%}"
#define FMT_FG_BLUE    "%{\x1b[34m%}"
#define FMT_FG_MAGENTA "%{\x1b[35m%}"
#define FMT_FG_WHITE   "%{\x1b[37m%}"
#define FMT_FG_GRAY    "%{\x1b[93m%}"

void get_git_dir(char *git_dir) {
  FILE *fd;

  fd = popen("git rev-parse --git-dir", "r");
  fgets(git_dir, 1023, fd);
  git_dir[strlen(git_dir)-1] = 0;
  pclose(fd);
}

long git_last_commit() {
  FILE *fd;
  char timestamp[1024];

  fd = popen("git log -n1 --format=%ct", "r");
  fgets(timestamp, 1023, fd);
  pclose(fd);

  return atol(timestamp);
}

void git_commit_time_elapsed(char *ret) {
  long last_commit = git_last_commit();

  long diff = time(NULL) - last_commit;
  int diff_min = (int)(diff / 60);

  if (diff_min < 10) {
    sprintf(ret, "%s%d%s", FMT_FG_GREEN, diff_min, FMT_FG_RESET);
  } else if (diff_min < 30) {
    sprintf(ret, "%s%d%s", FMT_FG_YELLOW, diff_min, FMT_FG_RESET);
  } else if (diff_min < 120) {
    sprintf(ret, "%s%d%s", FMT_FG_RED, diff_min, FMT_FG_RESET);
  } else if (diff_min < 1440) {
    sprintf(ret, "%s%dh%s", FMT_FG_RED, diff_min/60, FMT_FG_RESET);
  } else {
    sprintf(ret, "%s%dd%s", FMT_FG_RED, diff_min/1440, FMT_FG_RESET);
  }
}

#define WORKING_DELETED  0x0001
#define WORKING_MODIFIED 0x0004
#define WORKING_ADDED    0x0010
#define WORKING_UNMERGED 0x0040

#define INDEX_DELETED    0x0100
#define INDEX_MODIFIED   0x0400
#define INDEX_ADDED      0x1000
#define INDEX_UNMERGED   0x4000

#define MARKER_DELETED "D"
#define MARKER_MODIFIED "M"
#define MARKER_ADDED "?"
#define MARKER_UNMERGED "U"

#define FMT_STAGED     "%{\x1b[32m%}"
#define FMT_UNSTAGED   "%{\x1b[31m%}"
#define FMT_STAGED_UNSTAGED "%{\x1b[34m%}"

int append_git_status_info(char *stats_part, int stats, int working_mask, int index_mask, char *output) {
  if (stats & working_mask) {
    if (stats & index_mask) {
      strcat(stats_part, FMT_STAGED_UNSTAGED);
    } else {
      strcat(stats_part, FMT_UNSTAGED);
    }
    strcat(stats_part, output);
    return 1;
  } else if (stats & index_mask) {
    strcat(stats_part, FMT_STAGED);
    strcat(stats_part, output);
    return 1;
  }
  return 0;
}

int git_dirty_info(char *stats_part) {
  FILE *fp;
  int stats = 0;
  char line[1024];
  int output_size = 0;

  fp = popen("git status --porcelain", "r");

  while (fgets(line, 1023, fp)) {
    switch(line[1]) {
    case 'D':
      stats |= WORKING_DELETED;
      break;
    case 'M':
      stats |= WORKING_MODIFIED;
      break;
    case '?':
      stats |= WORKING_ADDED;
      break;
    case 'U':
      stats |= WORKING_UNMERGED;
      break;
    }
    switch(line[0]) {
    case 'D':
      stats |= INDEX_DELETED;
      break;
    case 'M':
      stats |= INDEX_MODIFIED;
      break;
    case '?':
      stats |= INDEX_ADDED;
      break;
    case 'U':
      stats |= INDEX_UNMERGED;
      break;
    }
  }
  pclose(fp);

  output_size += append_git_status_info(stats_part, stats, WORKING_DELETED, INDEX_DELETED, MARKER_DELETED);
  output_size += append_git_status_info(stats_part, stats, WORKING_MODIFIED, INDEX_MODIFIED, MARKER_MODIFIED);
  output_size += append_git_status_info(stats_part, stats, WORKING_ADDED, INDEX_ADDED, MARKER_ADDED);
  output_size += append_git_status_info(stats_part, stats, WORKING_UNMERGED, INDEX_UNMERGED, MARKER_UNMERGED);

  strcat(stats_part, FMT_FG_RESET);
  if (output_size > 0) {
    strcat(stats_part, " ");
  }

  return stats;
}

void get_refname(const char *git_dir, char *refname) {
  FILE *fp;
  char *filename = strdup(git_dir);
  strcat(filename, "/HEAD");

  fp = fopen(filename, "r");
  fgets(refname, 1023, fp);
  refname[strlen(refname)-1] = 0;
  fclose(fp);

  if (strncmp(refname, "ref: refs/heads/", 16) == 0) {
    strcpy(refname, refname + 16);
    filename = strdup(git_dir);
  }
}

void get_refname_color(const char *git_dir, const char *refname, const int dirty, char *refname_color) {
  if (dirty) {
    sprintf(refname_color, "%s", FMT_FG_MAGENTA);
  } else {
    sprintf(refname_color, "%s", FMT_FG_GREEN);
  }
}

void get_stash_info(const char *git_dir, char *output) {
  FILE *fp;
  char buf[2000];
  int counter = 0;
  char *filename = strdup(git_dir);
  strcat(filename, "/logs/refs/stash");

  if (access(filename, F_OK)) {
    strcat(output, " ");
  } else {
    fp = fopen(filename, "r");
    while (fgets(buf, 2000, fp) != NULL) counter++;
    sprintf(output, "%s%d ", FMT_FG_WHITE, counter);
  }
}

int main() {
  char refname[128];
  char git_d_info[512];
  char time_elapsed[32];
  char git_dir[512];
  char stash_info[32];
  char refname_color[32];

  int dirty;

  git_commit_time_elapsed(time_elapsed);
  dirty = git_dirty_info(git_d_info);
  get_git_dir(git_dir);
  get_refname(git_dir, refname);
  get_refname_color(git_dir, refname, dirty, refname_color);
  get_stash_info(git_dir, stash_info);

  if (!strcmp(refname, "master")) {
    strcpy(refname, "*");
  }

  printf("%s %s%s%s%s%s",
      time_elapsed,
      refname_color,
      refname,
      stash_info,
      git_d_info,
      FMT_FG_RESET);
  return 0;
}


