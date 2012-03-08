/* read XDI file in C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define LF 10            /* Line Feed */
#define CR 13            /* Carriage Return */
#define CRLF "\n\r"
#define EOF_WIN 26       /* End of File, Win */

#define MAX_LINE_LENGTH 8192 /* Max chars in a line */
#define MAX_LINES 16384      /* Max number of lines */
#define MAX_WORDS 128

/* Read function */
int readlines(char *filename, char **lines);
void show_syntax(void);

typedef struct {
  char *key;
  char *val;
} mapping;

typedef struct {
  char *xdi_version;
  char *extra_version;
  char *filename;
  char element[2];
  char edge[2];
  double dspacing;
  mapping *metadata;
  long nmetadata;
  long nrows;
  long npts;
  char *comments;
  char *array_names;
  double **array;
} XDIFile;

/*-------------------------------------------------------*/
/* show syntax */
void show_syntax(void) {
  /* show command line syntax */
  printf("\nSyntax: xdi_reader filename\n");
}

/*-------------------------------------------------------*/
/* read array of text lines from an open data file  */
int readlines(char *filename, char **textlines) {
  /* returns number of lines read, textlines should be pre-declared
      as char *text[MAX] */

  FILE *inpFile;
  char  thisline[MAX_LINE_LENGTH];
  char *file_text, *c;
  long file_length, index, i, ilen;
  int  is_newline;

  inpFile = fopen(filename, "r");
  if (inpFile == NULL) {
    printf("Error opening %s: %s\n", filename, strerror(errno));
    return -2;
  }

  fseek(inpFile, 0L, SEEK_END);
  file_length = ftell(inpFile);
  rewind(inpFile);

  file_text = calloc(file_length + 1, sizeof(char));

  if(file_text == NULL ) {
    printf("\nnot enough memory to read file.\n");
    return -1;
  }

  fread(file_text, file_length, 1, inpFile);
  fclose(inpFile);

  ilen = -1;

  c = file_text;
  while (*c) {
    index = 0;
    is_newline = 0;
    while (*c) {
      if (!is_newline) {
	if (*c == CR || *c == LF) {
	  is_newline = 1;
	}
      } else if (*c != CR  && *c != LF) {
	break;
      }
      thisline[index++] = *c++;
    }
    thisline[index] = '\0';
    ++ilen;
    textlines[ilen] = calloc(index+1, sizeof(char));
    strcpy(textlines[ilen], thisline);
    if (ilen >= MAX_LINES-1) {
      printf("\nfile has too many lines.  Limit is %d \n " , MAX_LINES);
      return -2;
    }
  }
  free(file_text);
  return ilen;

}
/*-------------------------------------------------------*/
int make_words(char *input, char *out[], int maxwords) {
  char *p = input;
  int  i, nwords;
  nwords = 0;
  for (i = 0; i < maxwords; i++) {
    /* skip leading whitespace */
    while (isspace(*p)) {
      p++;
    }
    if (*p != '\0') {
      out[nwords++] = p;
    } else {
      out[nwords] = 0;
      break;
    }
    while (*p != '\0' && !isspace(*p)) {
      p++;
    }
    /* terminate arg: */
    if (*p != '\0' && i < maxwords-1){
      *p++ = '\0';
    }
  }
  return nwords;
}


/*-------------------------------------------------------*/
int main(int argc, char **argv) {

  char *textlines[MAX_LINES];
  char *header[MAX_LINES];
  char *words[MAX_WORDS];
  char *c, *val, *key, *version_xdi, *version_extra;
  char *filename;
  FILE *inpFile;
  mapping *dict, *map;

  XDIFile *xdifile;

  long  file_length, ilen, index, i, j;
  long  ncol, nrows, nheader, nwords, ndict;
  int   is_newline, fnlen, mode;

  /* require 2 arguments! */
  if (argc < 2) {
    show_syntax();
    return 1;
  }

  /* read file to text lines */
  filename = calloc(strlen(argv[1]) +1, sizeof(char));
  strcpy(filename, argv[1]);

  ilen = readlines(argv[1], textlines);
  if (ilen < 0) {
    return 1;
  }

  printf("#== read %ld lines from %s\n", ilen, argv[1]);
  nheader=0;
  /* check fist line for XDI header, version info */
  if (strncmp(textlines[0], "#", 1) == 0)  {
    val = textlines[0]; val++;
    val[strcspn(val, CRLF)] = '\0';
    while (isspace(*val)) {val++;}
    if (strncmp(val, "XDI/", 4) != 0)  {
      printf(" Not an XDI File!\n");
      return -2;
    }
    val = val+4;
    key = strsep(&val, " ");
    if (val != NULL) {
      while (isspace(*val)) {val++;}
      while (isspace(*key)) {key++;}
      key[strcspn(key, " ")] = '\0';
      version_extra = calloc(strlen(val) + 1, sizeof(char));
      strcpy(version_extra, val);
    } else {
      version_extra = calloc(1, sizeof(char));
      strcpy(version_extra, "");
    }
    version_xdi = calloc(strlen(key) + 1, sizeof(char));
    strcpy(version_xdi, key);
  }
  xdifile = (XDIFile *)calloc(1, sizeof(XDIFile));

  xdifile->xdi_version = version_xdi;
  xdifile->extra_version = version_extra;

  printf( " VERSIONS: %s|%s|\n" , xdifile->xdi_version, xdifile->extra_version);

  nheader = 1;
  for (i = 1; i < ilen ; i++) {
    if (strncmp(textlines[i], "#", 1) == 0)  {
	nheader++;
    } else {
      break;
    }
  }


  xdifile->metadata = (mapping *)calloc(nheader+1, sizeof(mapping));
  ndict = 0;
  mode = 0; /*  metadata (Family.Member: Value) mode */
  for (i = 1; i < nheader; i++) {
    if (strncmp(textlines[i], "#", 1) == 0)  {
      val = calloc(strlen(textlines[i])+1, sizeof(char));
      strcpy(val, textlines[i]);
      val++;
      val[strcspn(val, CRLF)] = '\0';
      while (isspace(*val)) {val++;}
      key = strsep(&val, ":");
      if (val != NULL) {
	while (isspace(*val)) {val++;}
	while (isspace(*key)) {key++;}
	key[strcspn(key, " ")] = '\0';
	xdifile->metadata[ndict].key = calloc(strlen(key) + 1, sizeof(char));
	xdifile->metadata[ndict].val = calloc(strlen(val) + 1, sizeof(char));
	strcpy(xdifile->metadata[ndict].key, key);
	strcpy(xdifile->metadata[ndict].val, val);
	ndict++;
      } else {
	printf("Other word = |%s|\n", key);
      }
    }
  }

  ncol = ilen - nheader + 1;
  printf(" %ld header lines  / %ld data lines\n", nheader, ncol);

  nrows = make_words(textlines[nheader], words, MAX_WORDS);

  printf(" %ld %ld words / cols\n", nrows, ncol);

  xdifile->array = (double **) calloc(nrows, sizeof(double *));
  printf( " calloc 1 \n");
  for (j = 0; j < nrows; j++) {
    xdifile->array[j] = (double *)calloc(ncol, sizeof(double));
    xdifile->array[j][0] = strtod(words[j], NULL);
  }
  for (i = 1; i < ncol; i++ ) {
    nrows = make_words(textlines[nheader+i], words, MAX_WORDS);
    for (j = 0; j < nrows; j++) {
      xdifile->array[j][i] = strtod(words[j], NULL);
    }
  }

  fnlen  = strlen(argv[1]);
  xdifile->filename = calloc(fnlen+1, sizeof(char));
  strcpy(xdifile->filename, argv[1]);
  /*xdifile->filename[fnlen] = '\0';*/

  xdifile->nrows = nrows;
  xdifile->npts = ncol;
  xdifile->nmetadata = ndict;

  printf("END: %s | nmetadata %ld \n",xdifile->filename, xdifile->nmetadata);
  for (i=0; i < ndict ;  i++) {
    printf(" %ld ->:  %s: %s\n" , i, xdifile->metadata[i].key, xdifile->metadata[i].val );
  }

  printf(" Arrays \n");
  for (j = 0; j < nrows ; j++ ) {
    printf(" J=%ld :", j);
    for (i = 0; i < 5; i++) {
       printf(" %f,", xdifile->array[j][i]);
    }
    printf("... \n");
  }

  return 0;
}

