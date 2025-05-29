#include "conf_parser.h"

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

static void print_token(stb_lexer *lexer)
{
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
            printf("%c", (int) lexer->token);
         else {
            printf("<<<UNKNOWN TOKEN %ld >>>\n", lexer->token);
         }
         break;
   }
}

void tester() {
  uint32_t clen;
  char *confdata = readfile(CONFIG_PATH, &clen);
  char *stringStore = malloc(clen);
  
  stb_lexer lex;
  stb_c_lexer_init(&lex, confdata, confdata+clen, stringStore, clen);
  while (stb_c_lexer_get_token(&lex)) {
     if (lex.token == CLEX_parse_error) {
        printf("\n<<<PARSE ERROR>>>\n");
        break;
     }
     print_token(&lex);
     printf("  ");
  }
}
