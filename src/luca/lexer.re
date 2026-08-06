#include "lexer.hpp"

std::expected<token, lex_err> lexer::next() {
  for(;;) {
    tok_ = cur_;
  /*!re2c
    re2c:define:YYCTYPE     = "char";
    re2c:define:YYCURSOR    = "cur_";
    re2c:define:YYMARKER    = "mark_";
    re2c:define:YYLIMIT     = "lim_";
    re2c:yyfill:enable      = 0;
    re2c:eof                = 0;

    id_start     = [a-zA-Z_] ;
    id_char      = [a-zA-Z0-9_\-] ;
    id           = id_start id_char* ;
    number       = [0-9]+ ;
    ws           = [ \t\r\n]+ ;
    glued_err    = number [a-zA-Z_\-] id_char* ;

    * { return std::unexpected{lex_err_unknown{}}; }
    $ { return std::unexpected{lex_err_eof{}}; }

    id         { return tk::id{.name = lexeme()}; }
    number     { return tk::literal_int{.value = lexeme()}; }
    glued_err  { return std::unexpected{lex_err_unknown{}}; }
    ws         { continue; }
  */
  }
}
