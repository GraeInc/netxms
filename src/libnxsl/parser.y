%{

#ifdef _WIN32
#pragma warning(disable : 4065 4102)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define YYERROR_VERBOSE
#define YYINCLUDED_STDLIB_H
#define YYDEBUG			1

#include <nms_common.h>
#include "libnxsl.h"
#include "parser.tab.hpp"

void yyerror(yyscan_t scanner, NXSL_Lexer *pLexer, NXSL_Compiler *pCompiler,
             NXSL_Program *pScript, const char *pszText);
int yylex(YYSTYPE *lvalp, yyscan_t scanner);

%}

%expect		1
%pure-parser
%lex-param		{yyscan_t scanner}
%parse-param	{yyscan_t scanner}
%parse-param	{NXSL_Lexer *pLexer}
%parse-param	{NXSL_Compiler *pCompiler}
%parse-param	{NXSL_Program *pScript}

%union
{
	INT32 valInt32;
	UINT32 valUInt32;
	INT64 valInt64;
	UINT64 valUInt64;
	char *valStr;
	identifier_t valIdentifier;
	double valReal;
	NXSL_Value *pConstant;
	NXSL_Instruction *pInstruction;
}

%token T_ABORT
%token T_ARRAY
%token T_BREAK
%token T_CASE
%token T_CATCH
%token T_CONST
%token T_CONTINUE
%token T_DEFAULT
%token T_DO
%token T_ELSE
%token T_EXIT
%token T_FALSE
%token T_FOR
%token T_FOREACH
%token T_GLOBAL
%token T_IF
%token T_NEW
%token T_NULL
%token T_PRINT
%token T_PRINTLN
%token T_RANGE
%token T_RETURN
%token T_SELECT
%token T_SUB
%token T_SWITCH
%token T_TRUE
%token T_TRY
%token T_TYPE_BOOLEAN
%token T_TYPE_INT32
%token T_TYPE_INT64
%token T_TYPE_REAL
%token T_TYPE_STRING
%token T_TYPE_UINT32
%token T_TYPE_UINT64
%token T_USE
%token T_WHEN
%token T_WHILE
%token T_WITH

%token <valIdentifier> T_COMPOUND_IDENTIFIER
%token <valIdentifier> T_IDENTIFIER
%token <valStr> T_STRING
%token <valInt32> T_INT32
%token <valUInt32> T_UINT32
%token <valInt64> T_INT64
%token <valUInt64> T_UINT64
%token <valReal> T_REAL

%right '=' T_ASSIGN_ADD T_ASSIGN_SUB T_ASSIGN_MUL T_ASSIGN_DIV T_ASSIGN_REM T_ASSIGN_CONCAT T_ASSIGN_AND T_ASSIGN_OR T_ASSIGN_XOR
%right ':'
%left '?'
%left '.'
%left T_OR
%left T_AND
%left '|'
%left '^'
%left '&'
%left T_NE T_EQ T_LIKE T_ILIKE T_MATCH T_IMATCH T_IN
%left '<' T_LE '>' T_GE
%left T_LSHIFT T_RSHIFT
%left '+' '-'
%left '*' '/' '%'
%right T_INC T_DEC T_NOT '~' NEGATE
%left T_REF '@'
%left T_POST_INC T_POST_DEC '[' ']'
%left T_RANGE

%type <pConstant> Constant
%type <valIdentifier> AnyIdentifier
%type <valIdentifier> FunctionName
%type <valInt32> BuiltinType
%type <valInt32> ParameterList
%type <valInt32> SelectList
%type <pInstruction> SimpleStatementKeyword

%destructor { MemFree($$); } <valStr>
%destructor { pScript->destroyValue($$); } <pConstant>
%destructor { delete $$; } <pInstruction>

%start Script


%%

Script:
	Module
{
	char szErrorText[256];

	// Add implicit main() function
	if (!pScript->addFunction("$main", 0, szErrorText))
	{
		pCompiler->error(szErrorText);
		YYERROR;
	}
	
	// Implicit return
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_RET_NULL));
}
|	ExpressionStatement
{
	char szErrorText[256];

	// Add implicit return
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_RETURN));

	// Add implicit main() function
	if (!pScript->addFunction("$main", 0, szErrorText))
	{
		pCompiler->error(szErrorText);
		YYERROR;
	}
}
|
{
	char szErrorText[256];

	// Add implicit return
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_RET_NULL));

	// Add implicit main() function
	if (!pScript->addFunction("$main", 0, szErrorText))
	{
		pCompiler->error(szErrorText);
		YYERROR;
	}
}
;

Module:
	ModuleComponent Module
|	ModuleComponent
;

ModuleComponent:
	ConstStatement
|	Function
|	StatementOrBlock
|	UseStatement
;

ConstStatement:
	T_CONST ConstList ';'
;

ConstList:
	ConstDefinition ',' ConstList
|	ConstDefinition
;

ConstDefinition:
	T_IDENTIFIER '=' Constant
{
	if (!pScript->addConstant($1, $3))
	{
		pCompiler->error("Constant already defined");
		pScript->destroyValue($3);
		$3 = NULL;
		YYERROR;
	}
	$3 = NULL;
}
;

UseStatement:
	T_USE AnyIdentifier ';'
{
	pScript->addRequiredModule($2.v, pLexer->getCurrLine(), false);
}
;

AnyIdentifier:
	T_IDENTIFIER
|	T_COMPOUND_IDENTIFIER
;

Function:
	T_SUB FunctionName 
	{
		char szErrorText[256];

		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, INVALID_ADDRESS));
		
		if (!pScript->addFunction($2, INVALID_ADDRESS, szErrorText))
		{
			pCompiler->error(szErrorText);
			YYERROR;
		}
		pCompiler->setIdentifierOperation(OPCODE_BIND);
	}
	ParameterDeclaration Block
	{
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_RET_NULL));
		pScript->resolveLastJump(OPCODE_JMP);
	}
;

ParameterDeclaration:
	IdentifierList ')'
|	')'
;

IdentifierList:
	T_IDENTIFIER 
	{
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), pCompiler->getIdentifierOperation(), $1));
	}
	',' IdentifierList
|	T_IDENTIFIER
	{
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), pCompiler->getIdentifierOperation(), $1));
	}
;

Block:
	'{' StatementList '}'
;

StatementList:
	StatementOrBlock StatementList
|
;

StatementOrBlock:
	Statement
|	Block
|	TryCatchBlock
;

TryCatchBlock:
	T_TRY 
	{
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CATCH, INVALID_ADDRESS));
	} 
	Block T_CATCH 
	{
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CPOP));
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, INVALID_ADDRESS));
		pScript->resolveLastJump(OPCODE_CATCH);
	} 
	Block
	{
		pScript->resolveLastJump(OPCODE_JMP);
	}
;

Statement:
	ExpressionStatement ';'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_POP, (INT16)1));
}
|	BuiltinStatement
|	';'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NOP));
}
;

ExpressionStatement:
	T_WITH
{
	pScript->enableExpressionVariables();
} 
	WithAssignmentList Expression
{
	pScript->disableExpressionVariables(pLexer->getCurrLine());
}
|	Expression
;

WithAssignmentList:
	WithAssignment ',' WithAssignmentList
|	WithAssignment
;

WithAssignment:
	T_IDENTIFIER
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, INVALID_ADDRESS));
	pScript->registerExpressionVariable($1);
}	 
	'=' WithCalculationBlock
{
	pScript->resolveLastJump(OPCODE_JMP);
}	
;

WithCalculationBlock:
	'{' StatementList '}'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_RET_NULL));
}
|	'{' Expression '}'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_RETURN));
}
;

Expression:
	'(' Expression ')'
|	T_IDENTIFIER '=' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET, $1));
}
|	T_IDENTIFIER T_ASSIGN_ADD { pScript->addPushVariableInstruction($1, pLexer->getCurrLine()); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_ADD));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET, $1));
}
|	T_IDENTIFIER T_ASSIGN_SUB { pScript->addPushVariableInstruction($1, pLexer->getCurrLine()); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SUB));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET, $1));
}
|	T_IDENTIFIER T_ASSIGN_MUL { pScript->addPushVariableInstruction($1, pLexer->getCurrLine()); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_MUL));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET, $1));
}
|	T_IDENTIFIER T_ASSIGN_DIV { pScript->addPushVariableInstruction($1, pLexer->getCurrLine()); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_DIV));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET, $1));
}
|	T_IDENTIFIER T_ASSIGN_REM { pScript->addPushVariableInstruction($1, pLexer->getCurrLine()); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_REM));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET, $1));
}
|	T_IDENTIFIER T_ASSIGN_CONCAT { pScript->addPushVariableInstruction($1, pLexer->getCurrLine()); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CONCAT));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET, $1));
}
|	T_IDENTIFIER T_ASSIGN_AND { pScript->addPushVariableInstruction($1, pLexer->getCurrLine()); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_AND));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET, $1));
}
|	T_IDENTIFIER T_ASSIGN_OR { pScript->addPushVariableInstruction($1, pLexer->getCurrLine()); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_OR));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET, $1));
}
|	T_IDENTIFIER T_ASSIGN_XOR { pScript->addPushVariableInstruction($1, pLexer->getCurrLine()); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_XOR));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET, $1));
}
|	StorageItem T_ASSIGN_ADD { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_READ, (INT16)1)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_ADD));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_WRITE));
}
|	StorageItem T_ASSIGN_SUB { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_READ, (INT16)1)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SUB));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_WRITE));
}
|	StorageItem T_ASSIGN_MUL { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_READ, (INT16)1)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_MUL));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_WRITE));
}
|	StorageItem T_ASSIGN_DIV { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_READ, (INT16)1)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_DIV));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_WRITE));
}
|	StorageItem T_ASSIGN_REM { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_READ, (INT16)1)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_REM));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_WRITE));
}
|	StorageItem T_ASSIGN_CONCAT { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_READ, (INT16)1)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CONCAT));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_WRITE));
}
|	StorageItem T_ASSIGN_AND { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_READ, (INT16)1)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_AND));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_WRITE));
}
|	StorageItem T_ASSIGN_OR { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_READ, (INT16)1)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_OR));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_WRITE));
}
|	StorageItem T_ASSIGN_XOR { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_READ, (INT16)1)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_XOR));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_WRITE));
}
|	Expression '[' Expression ']' '=' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET_ELEMENT));
}
|	Expression '[' Expression ']' T_ASSIGN_ADD { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PEEK_ELEMENT)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_ADD));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET_ELEMENT));
}
|	Expression '[' Expression ']' T_ASSIGN_SUB { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PEEK_ELEMENT)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SUB));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET_ELEMENT));
}
|	Expression '[' Expression ']' T_ASSIGN_MUL { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PEEK_ELEMENT)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_MUL));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET_ELEMENT));
}
|	Expression '[' Expression ']' T_ASSIGN_DIV { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PEEK_ELEMENT)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_DIV));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET_ELEMENT));
}
|	Expression '[' Expression ']' T_ASSIGN_CONCAT { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PEEK_ELEMENT)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CONCAT));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET_ELEMENT));
}
|	Expression '[' Expression ']' T_ASSIGN_AND { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PEEK_ELEMENT)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_AND));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET_ELEMENT));
}
|	Expression '[' Expression ']' T_ASSIGN_OR { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PEEK_ELEMENT)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_OR));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET_ELEMENT));
}
|	Expression '[' Expression ']' T_ASSIGN_XOR { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PEEK_ELEMENT)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_XOR));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET_ELEMENT));
}
|	Expression '[' Expression ']'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_GET_ELEMENT));
}
|	Expression '[' ExpressionOrNone ':' ExpressionOrNone ']'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_GET_RANGE));
}
|	StorageItem '=' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_WRITE));
}
|	StorageItem
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_READ));
}
|	Expression T_REF T_IDENTIFIER '=' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SET_ATTRIBUTE, $3));
}
|	Expression T_REF T_IDENTIFIER
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_GET_ATTRIBUTE, $3));
}
|	Expression T_REF FunctionName ParameterList ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CALL_METHOD, $3, $4));
}
|	Expression T_REF FunctionName ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CALL_METHOD, $3, 0));
}
|	T_IDENTIFIER '@' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SAFE_GET_ATTR, $1));
}
|	'-' Expression		%prec NEGATE
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NEG));
}
|	T_NOT Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NOT));
}
|	'~' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_NOT));
}
|	T_INC T_IDENTIFIER
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_INCP, $2));
}
|	T_DEC T_IDENTIFIER
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_DECP, $2));
}
|	T_IDENTIFIER T_INC	%prec T_POST_INC
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_INC, $1));
}
|	T_IDENTIFIER T_DEC	%prec T_POST_DEC
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_DEC, $1));
}
|	T_INC StorageItem
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_INCP));
}
|	T_DEC StorageItem
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_DECP));
}
|	StorageItem T_INC	%prec T_POST_INC
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_INC));
}
|	StorageItem T_DEC	%prec T_POST_DEC
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_STORAGE_DEC));
}
|	T_INC '(' Expression '[' Expression ']' ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_INCP_ELEMENT));
}
|	T_DEC '(' Expression '[' Expression ']' ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_DECP_ELEMENT));
}
|	Expression '[' Expression ']' T_INC	%prec T_POST_INC
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_INC_ELEMENT));
}
|	Expression '[' Expression ']' T_DEC	%prec T_POST_DEC
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_DEC_ELEMENT));
}
|	Expression '+' Expression	
{ 
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_ADD));
}
|	Expression '-' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SUB));
}
|	Expression '*' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_MUL));
}
|	Expression '/' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_DIV));
}
|	Expression '%' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_REM));
}
|	Expression T_LIKE Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_LIKE));
}
|	Expression T_ILIKE Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_ILIKE));
}
|	Expression T_MATCH Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_MATCH));
}
|	Expression T_IMATCH Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_IMATCH));
}
|	Expression T_IN Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_IN));
}
|	Expression T_EQ Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_EQ));
}
|	Expression T_NE Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NE));
}
|	Expression '<' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_LT));
}
|	Expression T_LE Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_LE));
}
|	Expression '>' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_GT));
}
|	Expression T_GE Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_GE));
}
|	Expression '&' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_AND));
}
|	Expression '|' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_OR));
}
|	Expression '^' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_BIT_XOR));
}
|	Expression T_AND { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JZ_PEEK, INVALID_ADDRESS)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_AND));
	pScript->resolveLastJump(OPCODE_JZ_PEEK);
}
|	Expression T_OR { pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JNZ_PEEK, INVALID_ADDRESS)); } Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_OR));
	pScript->resolveLastJump(OPCODE_JNZ_PEEK);
}
|	Expression T_LSHIFT Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_LSHIFT));
}
|	Expression T_RSHIFT Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_RSHIFT));
}
|	Expression '.' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CONCAT));
}
|	Expression '?'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JZ, INVALID_ADDRESS));
}
	Expression ':'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, INVALID_ADDRESS));
	pScript->resolveLastJump(OPCODE_JZ);
}	 
	Expression
{
	pScript->resolveLastJump(OPCODE_JMP);
}
|	Operand
;

ExpressionOrNone:
	Expression
|
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_CONSTANT, pScript->createValue()));
}
;

Operand:
	FunctionCall
|	TypeCast
|	ArrayInitializer
|	HashMapInitializer
|	New
|	T_IDENTIFIER
{
	pScript->addPushVariableInstruction($1, pLexer->getCurrLine());
}
|	T_COMPOUND_IDENTIFIER
{
   // Special case for VM properties
   if (!strcmp($1.v, "NXSL::Classes") || !strcmp($1.v, "NXSL::Functions"))
      pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_PROPERTY, $1));
   else
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_CONSTREF, $1));
}
|	Constant
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_CONSTANT, $1));
	$1 = NULL;
}
;

TypeCast:
	BuiltinType '(' Expression ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CAST, static_cast<int16_t>($1)));
}
;

ArrayInitializer:
	'%' '(' 
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NEW_ARRAY));
}
	ArrayElements ')'
|	'%' '(' ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NEW_ARRAY));
}
;

ArrayElements:
	Expression 
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_ADD_TO_ARRAY));
}
	',' ArrayElements
|	Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_ADD_TO_ARRAY));
}
;

HashMapInitializer:
	'%' '{' 
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NEW_HASHMAP));
}
	HashMapElements '}'
|	'%' '{' '}'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NEW_HASHMAP));
}
;

HashMapElements:
	HashMapElement ',' HashMapElements
|	HashMapElement
;

HashMapElement:
	Expression ':' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_HASHMAP_SET));
}

BuiltinType:
	T_TYPE_BOOLEAN
{
	$$ = NXSL_DT_BOOLEAN;
}
|	T_TYPE_INT32
{
	$$ = NXSL_DT_INT32;
}
|	T_TYPE_INT64
{
	$$ = NXSL_DT_INT64;
}
|	T_TYPE_UINT32
{
	$$ = NXSL_DT_UINT32;
}
|	T_TYPE_UINT64
{
	$$ = NXSL_DT_UINT64;
}
|	T_TYPE_REAL
{
	$$ = NXSL_DT_REAL;
}
|	T_TYPE_STRING
{
	$$ = NXSL_DT_STRING;
}
;

BuiltinStatement:
	SimpleStatement ';'
|	PrintlnStatement
|	IfStatement
|	DoStatement
|	WhileStatement
|	ForStatement
|	ForEachStatement
|	SwitchStatement
|	SelectStatement
|	ArrayStatement
|	GlobalStatement
|	T_BREAK ';'
{
	if (pCompiler->canUseBreak())
	{
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NOP));
		pCompiler->addBreakAddr(pScript->getCodeSize() - 1);
	}
	else
	{
		pCompiler->error("\"break\" statement can be used only within loops, \"switch\", and \"select\" statements");
		YYERROR;
	}
}
|	T_CONTINUE ';'
{
	UINT32 dwAddr = pCompiler->peekAddr();
	if (dwAddr != INVALID_ADDRESS)
	{
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, dwAddr));
	}
	else
	{
		pCompiler->error("\"continue\" statement can be used only within loops");
		YYERROR;
	}
}
;

SimpleStatement:
	SimpleStatementKeyword Expression
{
	pScript->addInstruction($1);
	$1 = NULL;
}
|	SimpleStatementKeyword
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_CONSTANT, pScript->createValue()));
	pScript->addInstruction($1);
	$1 = NULL;
}
;

SimpleStatementKeyword:
	T_ABORT
{
	$$ = new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_ABORT);
}
|	T_EXIT
{
	$$ = new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_EXIT);
}
|	T_RETURN
{
   if (pCompiler->getTemporaryStackItems() > 0)
   {
	   pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_POP, (int16_t)pCompiler->getTemporaryStackItems()));
   }
	$$ = new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_RETURN);
}
|	T_PRINT
{
	$$ = new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PRINT);
}
;

PrintlnStatement:
	T_PRINTLN Expression ';'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_CONSTANT, pScript->createValue(_T("\n"))));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CONCAT));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PRINT));
}
|	T_PRINTLN ';'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_CONSTANT, pScript->createValue(_T("\n"))));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PRINT));
}
;

IfStatement:
	T_IF '(' Expression ')' 
	{
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JZ, INVALID_ADDRESS));
	} 
	IfBody
;

IfBody:
	StatementOrBlock
{
	pScript->resolveLastJump(OPCODE_JZ);
}
|	StatementOrBlock ElseStatement
{
	pScript->resolveLastJump(OPCODE_JMP);
}
;

ElseStatement:
	T_ELSE
	{
		pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, INVALID_ADDRESS));
		pScript->resolveLastJump(OPCODE_JZ);
	}
	StatementOrBlock
;

ForStatement:
	T_FOR '(' Expression ';'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_POP, (INT16)1));
	pCompiler->pushAddr(pScript->getCodeSize());
}
	Expression ';'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JZ, INVALID_ADDRESS));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, INVALID_ADDRESS));
	pCompiler->pushAddr(pScript->getCodeSize());
}
	Expression ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_POP, (INT16)1));
	UINT32 addrPart3 = pCompiler->popAddr();
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, pCompiler->popAddr()));
	pCompiler->pushAddr(addrPart3);
	pCompiler->newBreakLevel();
	pScript->resolveLastJump(OPCODE_JMP);
}	
	StatementOrBlock
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, pCompiler->popAddr()));
	pScript->resolveLastJump(OPCODE_JZ);
	pCompiler->closeBreakLevel(pScript);
}
;

ForEachStatement:
	ForEach { pCompiler->incTemporaryStackItems(); } ForEachBody { pCompiler->decTemporaryStackItems(); }
;

ForEach:
	T_FOREACH '(' T_IDENTIFIER ':'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_CONSTANT, pScript->createValue($3.v)));
}
|
	T_FOR '(' T_IDENTIFIER ':'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_CONSTANT, pScript->createValue($3.v)));
}
;

ForEachBody:
	Expression ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_FOREACH));
	pCompiler->pushAddr(pScript->getCodeSize());
	pCompiler->newBreakLevel();
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NEXT));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JZ, INVALID_ADDRESS));
}
	StatementOrBlock
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, pCompiler->popAddr()));
	pScript->resolveLastJump(OPCODE_JZ);
	pCompiler->closeBreakLevel(pScript);
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_POP, (INT16)1));
}
;

WhileStatement:
	T_WHILE
{
	pCompiler->pushAddr(pScript->getCodeSize());
	pCompiler->newBreakLevel();
}
	'(' Expression ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JZ, INVALID_ADDRESS));
}
	StatementOrBlock
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, pCompiler->popAddr()));
	pScript->resolveLastJump(OPCODE_JZ);
	pCompiler->closeBreakLevel(pScript);
}
;

DoStatement:
	T_DO
{
	pCompiler->pushAddr(pScript->getCodeSize());
	pCompiler->newBreakLevel();
}
	StatementOrBlock T_WHILE '(' Expression ')' ';'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(),
				OPCODE_JNZ, pCompiler->popAddr()));
	pCompiler->closeBreakLevel(pScript);
}
;

SwitchStatement:
	T_SWITCH
{ 
	pCompiler->newBreakLevel();
}
	'(' Expression ')'
{
	pCompiler->incTemporaryStackItems();
}
	'{' CaseList Default '}'
{
	pCompiler->closeBreakLevel(pScript);
	pCompiler->decTemporaryStackItems();
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_POP, (int16_t)1));
}
;

CaseList:
	Case
{ 
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, pScript->getCodeSize() + 5));
	pScript->resolveLastJump(OPCODE_JZ);
}
	CaseList
|	RangeCase
{ 
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, pScript->getCodeSize() + 5));
	pScript->resolveLastJump(OPCODE_JNZ);
	pScript->resolveLastJump(OPCODE_JNZ);
}
	CaseList
|	Case
{
	pScript->resolveLastJump(OPCODE_JZ);
}
|	RangeCase
{
	pScript->resolveLastJump(OPCODE_JNZ);
	pScript->resolveLastJump(OPCODE_JNZ);
}
;

Case:
	T_CASE Constant
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CASE, $2));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JZ, INVALID_ADDRESS));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NOP));	// Needed to match number of instructions in case and rage case
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NOP));
	$2 = nullptr;
} 
	':' StatementList
|	T_CASE T_IDENTIFIER
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CASE_CONST, $2));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JZ, INVALID_ADDRESS));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NOP));	// Needed to match number of instructions in case and rage case
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NOP));
} 
	':' StatementList
;

RangeCase:
	T_CASE CaseRangeLeft
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JNZ, INVALID_ADDRESS));
}
T_RANGE CaseRangeRight 
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JNZ, INVALID_ADDRESS));
}
	':' StatementList
;

CaseRangeLeft:
	Constant
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CASE_LT, $1));
	$1 = nullptr;
}
|	T_IDENTIFIER
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CASE_CONST_LT, $1));
}
;

CaseRangeRight:
	Constant
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CASE_GT, $1));
	$1 = nullptr;
}
|	T_IDENTIFIER
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CASE_CONST_GT, $1));
}
;

Default:
	T_DEFAULT ':' StatementList
|
;

SelectStatement:
	T_SELECT
{ 
	pCompiler->newBreakLevel();
	pCompiler->newSelectLevel();
}
	T_IDENTIFIER SelectOptions	'{' SelectList '}'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_SELECT, $3, $6));
	pCompiler->closeBreakLevel(pScript);

	UINT32 addr = pCompiler->popSelectJumpAddr();
	if (addr != INVALID_ADDRESS)
	{
		pScript->createJumpAt(addr, pScript->getCodeSize());
	}
	pCompiler->closeSelectLevel();
}
;

SelectOptions:
	'(' Expression ')'
|
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_CONSTANT, pScript->createValue()));
}
;

SelectList:
	SelectEntry	SelectList
{ 
	$$ = $2 + 1; 
}
|	SelectEntry
{
	$$ = 1;
}
;

SelectEntry:
	T_WHEN
{
} 
	Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSHCP, (INT16)2));
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_JMP, INVALID_ADDRESS));
	UINT32 addr = pCompiler->popSelectJumpAddr();
	if (addr != INVALID_ADDRESS)
	{
		pScript->createJumpAt(addr, pScript->getCodeSize());
	}
} 
	':' StatementList
{
	pCompiler->pushSelectJumpAddr(pScript->getCodeSize());
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NOP));
	pScript->resolveLastJump(OPCODE_JMP);
}
;

ArrayStatement:
	T_ARRAY { pCompiler->setIdentifierOperation(OPCODE_ARRAY); } IdentifierList ';'
|	T_GLOBAL T_ARRAY { pCompiler->setIdentifierOperation(OPCODE_GLOBAL_ARRAY); } IdentifierList ';'
;

GlobalStatement:
	T_GLOBAL GlobalVariableList ';'
;

GlobalVariableList:
	GlobalVariableDeclaration ',' GlobalVariableList
|	GlobalVariableDeclaration
;

GlobalVariableDeclaration:
	T_IDENTIFIER
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_GLOBAL, $1, 0));
}
|	T_IDENTIFIER '=' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_GLOBAL, $1, 1));
}
;

New:
	T_NEW T_IDENTIFIER
{
	char fname[256];
	snprintf(fname, 256, "__new@%s", $2.v); 
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CALL_EXTERNAL, fname, 0));
}
|	T_NEW T_IDENTIFIER '(' ParameterList ')'
{
	char fname[256];
	snprintf(fname, 256, "__new@%s", $2.v); 
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CALL_EXTERNAL, fname, $4));
}
|	T_NEW T_IDENTIFIER '(' ')'
{
	char fname[256];
	snprintf(fname, 256, "__new@%s", $2.v); 
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CALL_EXTERNAL, fname, 0));
}
;

FunctionCall:
	FunctionName ParameterList ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CALL_EXTERNAL, $1, $2));
}
|	FunctionName ')'
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_CALL_EXTERNAL, $1, 0));
}
;

ParameterList:
	Parameter ',' ParameterList { $$ = $3 + 1; }
|	Parameter { $$ = 1; }
;

Parameter:
	Expression
|	T_IDENTIFIER ':' Expression
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_NAME, $1));
}
;

FunctionName:
	T_IDENTIFIER '('
{
	$$ = $1;
}
|	T_COMPOUND_IDENTIFIER '('
{
	$$ = $1;
	pScript->addRequiredModule($1.v, pLexer->getCurrLine(), true);
}
;

StorageItem:
	'#' T_IDENTIFIER
{
	pScript->addInstruction(new NXSL_Instruction(pScript, pLexer->getCurrLine(), OPCODE_PUSH_CONSTANT, pScript->createValue($2.v)));
}
|	'#' '(' Expression ')'
;	

Constant:
	T_STRING
{
#ifdef UNICODE
	$$ = pScript->createValue($1);
#else
	char *mbString = MBStringFromUTF8String($1);
	$$ = pScript->createValue(mbString);
	MemFree(mbString);
#endif
	MemFreeAndNull($1);
}
|	T_INT32
{
	$$ = pScript->createValue($1);
}
|	T_UINT32
{
	$$ = pScript->createValue($1);
}
|	T_INT64
{
	$$ = pScript->createValue($1);
}
|	T_UINT64
{
	$$ = pScript->createValue($1);
}
|	T_REAL
{
	$$ = pScript->createValue($1);
}
|	T_TRUE
{
	$$ = pScript->createValue(true);
}
|	T_FALSE
{
	$$ = pScript->createValue(false);
}
|	T_NULL
{
	$$ = pScript->createValue();
}
;
