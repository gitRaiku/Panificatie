#include "conf_parser.h"

/* I hate this */

#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"

char *readfile(char *fname, uint32_t *len) {
  int fd = open(fname, O_RDONLY);
  ENEG(fd, "Could not open file %s!", fname);
  struct stat s;
  ENEG(fstat(fd, &s), "Could not stat %s!", fname);
  *len = s.st_size;
  char *buf = malloc(*len);
  ENEG(read(fd, buf, *len), "Could not read %s!", fname);
  ENEG(close(fd), "Could not close %s!", fname);
  return buf;
}

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
}

enum LEX_STATE {
  LEX_PACMAN,
  LEX_AUR,
  LEX_GIT,
  LEX_CONFIG,
  /* ... TODO: ADD ALL */

  LEX_LAST
};

DEF_VEC(enum LEX_STATE, lexstatev);
struct lexstatev *statev;

char curstr[128];
struct panix_config *pc;


void lex_descend() {
  if (!strcmp(curstr, "pacman")) {
    vecp(statev, LEX_PACMAN);
  } else if (!strcmp(curstr, "aur")) { TODO("Add AUR support"); 
  } else if (!strcmp(curstr, "git")) { TODO("Add GIT support"); 
  } else if (!strcmp(curstr, "config")) { TODO("Add CONFIG support"); 
  } else {
    EEZERO(0, "Expected a section keyword, got \"%s\" instead", curstr);
  }
}

void lex_next() {
  if (curstr[0] == '\0') { return; }
  EEZERO(statev->l, "Expected a state, got \"%s\" instead", curstr);
  switch (statev->v[statev->l - 1]) {
    case LEX_PACMAN:
      vecp(pc->pacmanPkgs, strdup(curstr));
      fprintf(stdout, "Appending %s\n", curstr);
      break;
    default:
      TODO("Handle all the other cases");
      break;
  }
}

void lex_ascend() {
  EEZERO(statev->l, "Parsing error, unmatched '}'");
  lex_next();
  --statev->l;
}

#define safestrcat(dst, src) { int32_t _sl = VEC_LEN(dst) - strlen(dst) - 1; if (strlen(src) >= _sl) { fprintf(stderr, "Token \"%s\" too long, truncating to 128-characters\n", src); } strncat(dst, src, _sl); }
void process_token(stb_lexer *lex) {
  //print_token(lex); return;
  switch (lex->token) {
    case CLEX_id        : /* passthrough */
    case CLEX_dqstring  : /* passthrough */
    case CLEX_sqstring  : safestrcat(curstr, lex->string); break;

    case '{'            : lex_descend(); curstr[0] = '\0'; break;
    case '}'            : lex_ascend(); curstr[0] = '\0'; break;
    case ','            : lex_next(); curstr[0] = '\0'; break;

    default:
      if (0 <= lex->token && lex->token <= 255) {
        uint32_t csl = strlen(curstr);
        if (csl >= VEC_LEN(curstr) - 1) { break; }
        curstr[csl] = lex->token;
        curstr[csl + 1] = '\0';
      }
      break;
  }
}


struct panix_config *parse_config(char *fname) {
  pc = calloc(sizeof(struct panix_config), 1);
  veci(strv, pc->pacmanPkgs);
  veci(lexstatev, statev);

  {
    uint32_t clen;
    char *confdata = readfile(CONFIG_PATH, &clen);
    char *stringStore = malloc(clen);
    stb_lexer lex;
    stb_c_lexer_init(&lex, confdata, confdata+clen, stringStore, clen);
    while (stb_c_lexer_get_token(&lex)) {
       if (lex.token == CLEX_parse_error) { TODO("Handle parsing errors better");}
       process_token(&lex);
    }
    EENEZ(statev->l, "Parsing error, unmatched '{'");
    free(confdata);
    free(stringStore);
  }

  vecfree(statev);
  return pc;
}
