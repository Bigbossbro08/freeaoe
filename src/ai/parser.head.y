%{
    #include "gen/enums.h"
%}

%skeleton "lalr1.cc"
%require  "3.0"

%defines
%define api.namespace { ai }
%define api.parser.class {ScriptParser}
%define api.value.type variant
%define api.token.constructor
%define parse.assert true
%define parse.error verbose
%locations


%param {ai::ScriptLoader &driver}
%parse-param {ai::ScriptTokenizer &scanner}

%code requires {

    namespace ai {
        class ScriptTokenizer;
        class ScriptLoader;
    }

    #ifndef YYDEBUG
        #define YYDEBUG 1
    #endif

    #define YY_NULLPTR nullptr
}


%{
    #include <cassert>
    #include <iostream>

    #include "ScriptLoader.h"
    #include "ScriptTokenizer.h"

    #include "grammar.gen.tab.hpp"
    #include "location.hh"

    #undef yylex
    #define yylex scanner.yylex
%}

//%destructor { free($$); $$ = NULL; } String SymbolName;


%token<int> Number
%token<std::string> String
%token<std::string> SymbolName

%token OpenParen CloseParen
%token RuleStart ConditionActionSeparator

//%token LessThan LessOrEqual GreaterThan GreaterOrEqual Equal Not Or
%token Not Or

%token LoadIfDefined Else EndIf

%token Space NewLine

%token ScriptEnd 0 "end of script"

%start aiscript
