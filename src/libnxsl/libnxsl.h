/* 
** NetXMS - Network Management System
** NetXMS Scripting Language Interpreter
** Copyright (C) 2003-2021 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: libnxsl.h
**
**/

#ifndef _libnxsl_h_
#define _libnxsl_h_

#include <nms_common.h>
#include <nms_util.h>
#include <nxcpapi.h>
#include <nxsl.h>
#include <nxqueue.h>

union YYSTYPE;
typedef void *yyscan_t;

//
// Various defines
//

#define MAX_STRING_SIZE    8192

/**
 * Instruction opcodes
 */
#define OPCODE_NOP            0
#define OPCODE_RETURN         1
#define OPCODE_JMP            2
#define OPCODE_CALL           3
#define OPCODE_CALL_EXTERNAL  4
#define OPCODE_PUSH_CONSTANT  5
#define OPCODE_PUSH_VARIABLE  6
#define OPCODE_EXIT           7
#define OPCODE_POP            8
#define OPCODE_SET            9
#define OPCODE_ADD            10
#define OPCODE_SUB            11
#define OPCODE_MUL            12
#define OPCODE_DIV            13
#define OPCODE_REM            14
#define OPCODE_EQ             15
#define OPCODE_NE             16
#define OPCODE_LT             17
#define OPCODE_LE             18
#define OPCODE_GT             19
#define OPCODE_GE             20
#define OPCODE_BIT_AND        21
#define OPCODE_BIT_OR         22
#define OPCODE_BIT_XOR        23
#define OPCODE_AND            24
#define OPCODE_OR             25
#define OPCODE_LSHIFT         26
#define OPCODE_RSHIFT         27
#define OPCODE_RET_NULL       28
#define OPCODE_JZ             29
#define OPCODE_PRINT          30
#define OPCODE_CONCAT         31
#define OPCODE_BIND           32
#define OPCODE_INC            33
#define OPCODE_DEC            34
#define OPCODE_NEG            35
#define OPCODE_NOT            36
#define OPCODE_BIT_NOT        37
#define OPCODE_CAST           38
#define OPCODE_GET_ATTRIBUTE  39
#define OPCODE_INCP           40
#define OPCODE_DECP           41
#define OPCODE_JNZ            42
#define OPCODE_LIKE           43
#define OPCODE_ILIKE          44
#define OPCODE_MATCH          45
#define OPCODE_IMATCH         46
#define OPCODE_CASE           47
#define OPCODE_ARRAY          48
#define OPCODE_GET_ELEMENT    49
#define OPCODE_SET_ELEMENT    50
#define OPCODE_SET_ATTRIBUTE  51
#define OPCODE_NAME           52
#define OPCODE_FOREACH        53
#define OPCODE_NEXT           54
#define OPCODE_GLOBAL         55
#define OPCODE_GLOBAL_ARRAY   56
#define OPCODE_JZ_PEEK        57
#define OPCODE_JNZ_PEEK       58
#define OPCODE_ADD_TO_ARRAY   59
#define OPCODE_SAFE_GET_ATTR  60
#define OPCODE_CALL_METHOD    61
#define OPCODE_CASE_CONST     62
#define OPCODE_INC_ELEMENT    63
#define OPCODE_DEC_ELEMENT    64
#define OPCODE_INCP_ELEMENT   65
#define OPCODE_DECP_ELEMENT   66
#define OPCODE_ABORT          67
#define OPCODE_CATCH          68
#define OPCODE_PUSH_CONSTREF  69
#define OPCODE_HASHMAP_SET    70
#define OPCODE_NEW_ARRAY      71
#define OPCODE_NEW_HASHMAP    72
#define OPCODE_CPOP           73
#define OPCODE_STORAGE_READ   74
#define OPCODE_STORAGE_WRITE  75
#define OPCODE_SELECT         76
#define OPCODE_PUSHCP         77
#define OPCODE_STORAGE_INC    78
#define OPCODE_STORAGE_INCP   79
#define OPCODE_STORAGE_DEC    80
#define OPCODE_STORAGE_DECP   81
#define OPCODE_PEEK_ELEMENT   82
#define OPCODE_PUSH_VARPTR    83
#define OPCODE_SET_VARPTR     84
#define OPCODE_CALL_EXTPTR    85
#define OPCODE_INC_VARPTR     86
#define OPCODE_DEC_VARPTR     87
#define OPCODE_INCP_VARPTR    88
#define OPCODE_DECP_VARPTR    89
#define OPCODE_IN             90
#define OPCODE_PUSH_EXPRVAR   91
#define OPCODE_SET_EXPRVAR    92
#define OPCODE_UPDATE_EXPRVAR 93
#define OPCODE_CLEAR_EXPRVARS 94
#define OPCODE_GET_RANGE      95
#define OPCODE_CASE_LT        96
#define OPCODE_CASE_CONST_LT  97
#define OPCODE_CASE_GT        98
#define OPCODE_CASE_CONST_GT  99
#define OPCODE_PUSH_PROPERTY  100

class NXSL_Compiler;

/**
 * NXSL program builder
 */
class NXSL_ProgramBuilder : public NXSL_ValueManager
{
   friend class NXSL_Program;

protected:
   ObjectArray<NXSL_Instruction> m_instructionSet;
   StructArray<NXSL_ModuleImport> m_requiredModules;
   NXSL_ValueHashMap<NXSL_Identifier> m_constants;
   StructArray<NXSL_Function> m_functions;
   StructArray<NXSL_IdentifierLocation> *m_expressionVariables;

   uint32_t getFinalJumpDestination(uint32_t addr, int srcJump);
   uint32_t getExpressionVariableCodeBlock(const NXSL_Identifier& identifier);

public:
   NXSL_ProgramBuilder();
   virtual ~NXSL_ProgramBuilder();

   bool addFunction(const NXSL_Identifier& name, uint32_t addr, char *errorText);
   void resolveFunctions();
   void addInstruction(NXSL_Instruction *pInstruction) { m_instructionSet.add(pInstruction); }
   void addPushVariableInstruction(const NXSL_Identifier& name, int line);
   void resolveLastJump(int opcode, int offset = 0);
   void createJumpAt(uint32_t opAddr, uint32_t jumpAddr);
   void addRequiredModule(const char *name, int lineNumber, bool removeLastElement);
   void optimize();
   void removeInstructions(uint32_t start, int count);
   bool addConstant(const NXSL_Identifier& name, NXSL_Value *value);
   void enableExpressionVariables();
   void disableExpressionVariables(int line);
   void registerExpressionVariable(const NXSL_Identifier& identifier);

   uint32_t getCodeSize() const { return m_instructionSet.size(); }
   bool isEmpty() const { return m_instructionSet.isEmpty() || ((m_instructionSet.size() == 1) && (m_instructionSet.get(0)->m_opCode == 28)); }
   StringList *getRequiredModules() const;

   virtual uint64_t getMemoryUsage() const override;

   void dump(FILE *fp) const { dump(fp, m_instructionSet); }
   static void dump(FILE *fp, const ObjectArray<NXSL_Instruction>& instructionSet);
};

/**
 * Modified lexer class
 */
class NXSL_Lexer
{
	friend int yylex(YYSTYPE *, yyscan_t);

protected:
   int m_nSourceSize;
   int m_nSourcePos;
   char *m_pszSourceCode;
   NXSL_Compiler *m_pCompiler;

   int m_nCurrLine;
   int m_nCommentLevel;
   int m_nStrSize;
   char m_szStr[MAX_STRING_SIZE];

public:
	NXSL_Lexer(NXSL_Compiler *pCompiler, const TCHAR *pszCode);
	virtual ~NXSL_Lexer();

	int lexerInput(char *pBuffer, int nMaxSize);

	int getCurrLine() { return m_nCurrLine; }
	void error(const char *pszText);
};

/**
 * Compiler class
 */
class NXSL_Compiler
{
protected:
   TCHAR *m_errorText;
   int m_errorLineNumber;
   NXSL_Lexer *m_lexer;
   NXSL_Stack *m_addrStack;
	NXSL_Stack *m_breakStack;
	NXSL_Stack *m_selectStack;
	int m_idOpCode;
	int m_temporaryStackItems;

public:
   NXSL_Compiler();
   ~NXSL_Compiler();

   NXSL_Program *compile(const TCHAR *pszSourceCode);
   void error(const char *pszMsg);

   const TCHAR *getErrorText() { return CHECK_NULL(m_errorText); }
   int getErrorLineNumber() { return m_errorLineNumber; }

   void pushAddr(uint32_t addr) { m_addrStack->push(CAST_TO_POINTER(addr, void *)); }
   uint32_t popAddr();
   uint32_t peekAddr();

	void addBreakAddr(uint32_t addr);
	void closeBreakLevel(NXSL_ProgramBuilder *pScript);
	BOOL canUseBreak() { return m_breakStack->getSize() > 0; }
	void newBreakLevel() { m_breakStack->push(new Queue); }

	void newSelectLevel() { m_selectStack->push(new Queue); }
   void closeSelectLevel();
   void pushSelectJumpAddr(uint32_t addr);
   uint32_t popSelectJumpAddr();

   void incTemporaryStackItems() { m_temporaryStackItems++; }
   void decTemporaryStackItems() { m_temporaryStackItems--; }
   int getTemporaryStackItems() const { return m_temporaryStackItems; }

	void setIdentifierOperation(int opcode) { m_idOpCode = opcode; }
	int getIdentifierOperation() { return m_idOpCode; }
};

/**
 * Class registry
 */
struct NXSL_ClassRegistry
{
   size_t size;
   size_t allocated;
   NXSL_Class **classes;
};


//
// Global variables
//

extern const TCHAR *g_szTypeNames[];


#endif
