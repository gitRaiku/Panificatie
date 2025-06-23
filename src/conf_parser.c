#include "conf_parser.h"

/* I hate this */

#define shforeach(shm, res) for (size_t sl__shm ##res = shlenu(shm), res = 0; res < sl__shm ##res; ++res)

void parse_file(char *fname);

char *readfile(char *fname, uint32_t *len) {
  int fd = open(fname, O_RDONLY);
  ENEG(fd, "Could not open file \"%s\"!", fname);
  struct stat s;
  ENEG(fstat(fd, &s), "Could not stat \"%s\"!", fname);
  *len = s.st_size;
  char *buf = malloc(*len + 1);
  ENEG(read(fd, buf, *len), "Could not read \"%s\"!", fname);
  ENEG(close(fd), "Could not close \"%s\"!", fname);
  buf[*len] = '\0';
  return buf;
}

/*
static void print_token(stb_lexer *lexer) {
   switch (lexer->token) {
      case CLEX_id        : printf("_%s", lexer->string); break;
      case CLEX_eq        : printf("=="); break;
      case CLEX_noteq     : printf("!="); break;
      case CLEX_lesseq    : printf("<="); break;
      case CLEX_greatereq : printf(">="); break;
      case CLEX_andand    : printf("&&"); break;
      case CLEX_oror      : printf("||"); break;
      case CLEX_shl       : printf("<<"); break;
      case CLEX_shr       : printf(">>"); break;
      case CLEX_plusplus  : printf("++"); break;
      case CLEX_minusminus: printf("--"); break;
      case CLEX_arrow     : printf("->"); break;
      case CLEX_andeq     : printf("&="); break;
      case CLEX_oreq      : printf("|="); break;
      case CLEX_xoreq     : printf("^="); break;
      case CLEX_pluseq    : printf("+="); break;
      case CLEX_minuseq   : printf("-="); break;
      case CLEX_muleq     : printf("*="); break;
      case CLEX_diveq     : printf("/="); break;
      case CLEX_modeq     : printf("%%="); break;
      case CLEX_shleq     : printf("<<="); break;
      case CLEX_shreq     : printf(">>="); break;
      case CLEX_eqarrow   : printf("=>"); break;
      case CLEX_dqstring  : printf("\"%s\"", lexer->string); break;
      case CLEX_sqstring  : printf("'\"%s\"'", lexer->string); break;
      case CLEX_charlit   : printf("'%s'", lexer->string); break;
      #if defined(STB__clex_int_as_double) && !defined(STB__CLEX_use_stdlib)
      case CLEX_intlit    : printf("#%g", lexer->real_number); break;
      #else
      case CLEX_intlit    : printf("#%ld", lexer->int_number); break;
      #endif
      case CLEX_floatlit  : printf("%g", lexer->real_number); break;
      default:
         if (lexer->token >= 0 && lexer->token < 256)
            printf("%c", (char)lexer->token);
         else {
            printf("<<<UNKNOWN TOKEN %ld >>>\n", lexer->token);
         }
         break;
   }
}*/

enum LEX_STATE {
  LEX_PACMAN,
  LEX_AUR,
  LEX_GIT,
  LEX_CONFIG,
  LEX_INCLUDE,
  /* ... TODO: ADD ALL */

  LEX_LAST
};

DEF_VEC(enum LEX_STATE, lexstatev);
struct lexstatev *statev;

char curstr[256];
struct panix_config *pc;
stb_lexer *lex;
char *curfile;

void parsing_error(char *msg) {
  stb_lex_location loc = {0};
  stb_c_lexer_get_location(lex, lex->where_firstchar, &loc);
  fprintf(stderr, "Parsing error: %s in \"%s\" at %u:%u!\n", msg, curfile, loc.line_number, loc.line_offset);
  exit(1);
}

void parsing_warning(char *msg) {
  stb_lex_location loc = {0};
  stb_c_lexer_get_location(lex, lex->where_firstchar, &loc);
  fprintf(stderr, "Parsing warning: %s in \"%s\" at %u:%u!\n", msg, curfile, loc.line_number, loc.line_offset);
}

void lex_descend() {
  if (!strcmp(curstr, "pacman")) {
    vecp(statev, LEX_PACMAN);
  } else if (!strcmp(curstr, "aur")) { 
    vecp(statev, LEX_AUR);
  } else if (!strcmp(curstr, "git")) { TODO("Add GIT support"); 
  } else if (!strcmp(curstr, "config")) { TODO("Add CONFIG support"); 
  } else if (!strcmp(curstr, "include")) { 
    vecp(statev, LEX_INCLUDE);
  } else {
    char a[512]; snprintf(a, 512, "Expected a section keyword, got \"%s\" instead", curstr);
    parsing_error(a);
  }
}

void lex_next() {
  if (curstr[0] == '\0') { return; }
  if (statev->l == 0) { char a[512]; snprintf(a, 512, "Expected a section keyword, got \"%s\" instead", curstr); parsing_error(a); }
  switch (statev->v[statev->l - 1]) {
    case LEX_PACMAN:
      vecp(pc->pacmanPkgs, strdup(curstr));
      break;
    case LEX_AUR:
      vecp(pc->aurPkgs, strdup(curstr));
      break;
    case LEX_INCLUDE:
      parse_file(curstr);
      break;
    default:
      TODO("Handle all the other cases");
      break;
  }
}

void lex_ascend() {
  if (statev->l == 0) { parsing_error("unmatched '}'"); }
  lex_next();
  --statev->l;
}

#define safestrcat(dst, src) { size_t _sl = VEC_LEN(dst) - strlen(dst) - 1; if (strlen(src) >= _sl) { char a[256]; snprintf(a, 256, "Token \"%s\" too long, truncating to 128-characters\n", src); parsing_warning(a); } strncat(dst, src, _sl); }
void process_token() {
  static uint8_t ka = 0; // Keep Adding
  //print_token(lex); fprintf(stdout, "\n");//return;
  switch (lex->token) {
    case CLEX_id        : /* passthrough */
    case CLEX_dqstring  : /* passthrough */
    case CLEX_sqstring  : if (!ka) { lex_next(); curstr[0] = '\0'; } safestrcat(curstr, lex->string); ka = 0; break;

    case '{'            : ka = 0; lex_descend(); curstr[0] = '\0'; break;
    case '}'            : ka = 0; lex_ascend(); curstr[0] = '\0'; break;
    case ','            : ka = 0; lex_next(); curstr[0] = '\0'; break;

    default:
      if (0 <= lex->token && lex->token <= 255) {
        uint32_t csl = strlen(curstr);
        if (csl >= VEC_LEN(curstr) - 1) { break; }
        curstr[csl] = lex->token;
        curstr[csl + 1] = '\0';
        ka = 1;
      }
      break;
  }
}

char *get_rel_path(char *fname) {
  if (curfile == NULL) { return strdup(fname); }
  char *res = malloc(strlen(fname) + strlen(curfile) + 1);
  strcpy(res, curfile);
  char *lres = res;
  char *cres = res;
  while (*cres != '\0') { if (*cres == '/') { lres = cres; } ++cres; }
  strcpy(lres + 1, fname);
  return res;
}

void parse_file(char *fname) {
  char *oldf = curfile; 
  stb_lexer *oldl = lex;
  curfile = get_rel_path(fname);
  curstr[0] = '\0';

  {
    uint32_t clen;
    char *confdata = readfile(curfile, &clen);
    char *stringStore = malloc(clen);
    uint32_t starting_depth = statev->l;

    stb_lexer clex;
    stb_c_lexer_init(&clex, confdata, confdata+clen, stringStore, clen);
    lex = &clex;

    curstr[0] = '\0';
    while (stb_c_lexer_get_token(&clex)) {
      if (lex->token == CLEX_parse_error) { parsing_error("general parsing error"); }
      process_token();
    }
    if (statev->l != starting_depth) { parsing_error("unmatched '{'"); }

    free(confdata);
    free(stringStore);
  }

  free(curfile);
  curfile = oldf;
  lex = oldl;
}

struct panix_config *conf_read_panix(char *fname) {
  pc = calloc(1, sizeof(struct panix_config));
  veci(strv, pc->pacmanPkgs);
  veci(strv, pc->aurPkgs);
  veci(lexstatev, statev);
  curfile = NULL;
  lex = NULL;

  parse_file(fname);

  /*
  fprintf(stdout, "Pacman packages:\n");
  vecforeach(pc->pacmanPkgs, char *, pkg) { fprintf(stdout, "%s ", *pkg); }
  fprintf(stdout, "\nAur packages:\n");
  vecforeach(pc->aurPkgs, char *, pkg) { fprintf(stdout, "%s ", *pkg); }
  fprintf(stdout, "\n");
  */

  vecfree(statev);
  return pc;
}

void conf_free_config(struct panix_config *pc) {
  vecforeach(pc->pacmanPkgs, char *, pkg) { 
    free(*pkg); 
  }
  vecforeach(pc->aurPkgs, char *, pkg) { 
    free(*pkg); 
  }
  vecfree(pc->pacmanPkgs);
  vecfree(pc->aurPkgs);
  free(pc);
}

struct pdb *conf_read_pdb(char *fname) {
  struct pdb *pdc = calloc(1, sizeof(struct pdb));

  int32_t t = open(fname, O_RDONLY);
  if (t < 0) { return pdc; }
  close(t);
  uint32_t clen = 0;
  char *f = readfile(fname, &clen);

  char *ks = f;
  char *vs = NULL;
  for(uint32_t i = 0; i < clen; ++i) {
    if (f[i] == '\n') {
      f[i] = '\0';
      if (ks != NULL && vs != NULL) { shput(pdc->entries, strdup(ks), strdup(vs)); }
      ks = f + i + 1;
      vs = NULL;
    }
    if (f[i] == '=') { f[i] = '\0'; vs = f + i + 1; }
  }
  if (ks != NULL && vs != NULL) { shput(pdc->entries, strdup(ks), strdup(vs)); }

  free(f);
  return pdc;
}

void conf_free_pdb(struct pdb *pdc) {
  shforeach(pdc->entries, i) { 
    free(pdc->entries[i].value); 
    free(pdc->entries[i].key); 
  }
  shfree(pdc->entries);
  free(pdc);
}

char *trim_str(char *str) {
  char *st = str;
  while (*st != '\0' && isspace(*st)) { ++st; }
  if (*st == '\0') { return st; }
  char *ed = st;
  while (*ed != '\0') { ++ed; }
  --ed;
  while (ed > st && isspace(*ed)) { --ed; }
  ++ed;
  *ed = '\0';
  return st;
}

char *find_next(char *str, char c) {
  while (*str != '\0' && *str != c) { ++str; }
  if (*str == '\0') { return NULL; }
  return str;
}

void add_line(struct strEntry **se, char *cl) {
  char *ce = NULL;
  ce = find_next(cl, '=');
  if (ce == NULL) { return; } 
  *ce = '\0';
  cl = trim_str(cl);
  ce = trim_str(ce + 1);
  if (*cl != '\0' && *ce != '\0') {
    if (shgeti(*se, cl) < 0) {
      struct strv *si;
      vecsi(strv, si);
      vecp(si, strdup(ce));
      shput(*se, strdup(cl), si);
    } else {
      struct strv *si = shget(*se, cl);
      vecp(si, strdup(ce));
    }
  }
}

struct strEntry *conf_read_eq(char *fname) {
  int32_t t = open(fname, O_RDONLY); if (t < 0) { return NULL; } close(t);

  uint32_t clen = 0;
  char *f = readfile(fname, &clen);
  char *cl = f, *nl = NULL;

  struct strEntry *se = NULL;

  while ((nl = find_next(cl, '\n')) != NULL) {
    if (*nl == '\0') { break; }
    *nl = '\0';
    add_line(&se, cl);
    cl = nl + 1;
  }
  add_line(&se, cl);

  free(f);
  return se;
}

void conf_free_eq(struct strEntry *se) {
  shforeach(se, i) { 
    vecforeach(se[i].value, char*, str) { free(*str); }
    vecfree(se[i].value);
    free(se[i].key);
  }
  shfree(se);
}
