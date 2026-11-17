/*
Homework:
parsercodegen - HW4 PL/0 compiler: parser, symbol table, and code generator

Author(s): Leonel Hernandez

Language: C only

To Compile:
  gcc -Wall -Wextra -std=c11 -O2 parsercodegen.c -o parsercodegen

To Execute (on Eustis):
  ./lex <input_file> && ./parsercodegen && ./vm elf.txt

where:
  <input_file> is the path to a text file holding a PL/0 source program;
  ./lex writes tokens.txt and nametable.txt, ./parsercodegen reads them
  from the current directory, takes no arguments, and writes elf.txt, and
  ./vm runs elf.txt

Notes:
  - Implements the recursive-descent compiler described in the homework
    instructions: procedures, lexical levels, and PM/0 code generation.
  - Prints three sections to standard output: Generated Code, Symbol
    Table and Result.
  - Writes elf.txt, one instruction per line as OP L M, only when the
    whole program is accepted, and then exits with status 0.
  - Stops at the first error, reports it, and exits with status 1.
  - Tested on Eustis.

Class: COP 3402 - Systems Software

Instructor: Jie Lin, Ph.D.

Due Date: 11/20/2026
*/

#include <stdio.h>
#include <stdlib.h>
//Defines tokens
    typedef struct{
        int tCode;
        int val;
        char text[64];
    } Token;
//Defines identifiers
    typedef struct {
        int index;
        char name[13];
        int line;
        int col;
    } Identifier;
//Defines symbols
    typedef struct{
        int type;
        int nameIdx;
        int value;
        int level;
        int address;
        int mark;
    }  Symbol;
//Defines instructions
    typedef struct{
        int op;
        int l;
        int m;
    } Instruction;

//Counting global variables
int tCount = 0;
int iCount = 0;
int symbolCount = 0;
int cCount = 0;
int level = 0;

int currentToken = 0;
int nextAddress = 3;

//Create Token/Identifier/Symbol tables
Token tokenTable[5000];
Identifier identifierTable[200];
Symbol symbolTable[100];
Instruction code[200];

const char* opNames[] = {"", "LIT", "OPR", "LOD", "STO",
    "CAL", "INC", "JMP", "JPC", "SYS"
};

//Function defs

//Adds Symbol to symbol table
void addSymbol(int type, int nameIdx, int value, int address);
//Declares action for a constant variable
void constDecleration();
//Declares action for a variable
void varDecleration(int* localAddress);
//Declares action for procedure
void procDecleration();
//Non-terminal for code block in program
void block();
//Non-terminal for whole program
void program();
//Helper function to find symbolTable index
int findSymbol(int nameIdx);
//Non-terminal for statement in block
void statement();
//Collection of statements
void statementList();
//Non-terminal for factor in term
void factor();
//Non terminal for expression in statement
void expression();
//Non-terminal for terms in expressions
void term();
//Non terminal for conditional expressions
void condition();
//Handles error and graceful exit logic
void error(int errCode);
//Gets identifiers string name for error output
const char* getTokenText(int tokenIdx);
//Creates output when a token is consumed
void consumeToken(const char* owner);
//Helper for parse output 
void traceLookup(int symbolIdx);
//Prints nameTable
void printNameTable();
//Prints symbolTable
void printSymbolTable();
//Emits PL/0 code
void emit(int op, int l, int m);

void printCode();

int main(){
    FILE* tokens = fopen("tokens.txt", "r");
    FILE* nametable = fopen("nametable.txt", "r");

    if(tokens == NULL || nametable == NULL){
        error(18);
    }
    
    //Scanning tokens into local tokentable
    Token t;
    while(fscanf(tokens, "%d", &t.tCode) == 1){
        if(t.tCode == 1){
            fscanf(tokens, "%d", &t.val);
        }
        else if(t.tCode == 2){
            fscanf(tokens, "%63s", t.text);
            t.val = atoi(t.text);
        }
        else{
            t.val = 0;
        }
        tokenTable[tCount] = t;
        tCount++;
    }
    fclose(tokens);

    //Scanning names into local nametable
    Identifier i;
    while(fscanf(nametable, "%d %s %d %d", &i.index, i.name, &i.line, &i.col) == 4){
        identifierTable[iCount] = i;
        iCount++;
    }
    fclose(nametable);

    //Scanning tokenTable to get symbolTable
    program();

    //Prints Generated Code
    printCode();

    //Prints symbolTable
    printSymbolTable();

    printf("Result:\n\n");
    printf("program accepted\n");

    return 0;
}

/*Adds symbols to symbol table (needs a function unlike identifier and tokens
since it requires context of tokens to decide symbol paramters)*/
void addSymbol(int type, int nameIdx, int value, int address){
    Symbol s;
    s.type = type;
    s.value = value;
    s.address = address;
    s.level = level;
    s.mark = 0;
    s.nameIdx = nameIdx;
    symbolTable[symbolCount] = s;

    symbolCount++;
}

void constDecleration(){
    //Moves past const keyword
    consumeToken("const-decl");

    while(1){
        //Checks if not identifier
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 1){
            //Error 2: token is not identifier
            error(2);
        }

        //Gets nameTable index and moves past identifer
        int nameIdx = tokenTable[currentToken].val;

        consumeToken("const-decl");

        int symbolIdx = findSymbol(nameIdx);

        //Checks if identifer already used in current scope
        if(symbolIdx >= 0 && symbolTable[symbolIdx].level == level){
            //Error 14: Identifier already declared in block
            currentToken--;
            error(14);
        }


        //Checks if ":=" is missing
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 19){
            //Error 3: Expected :=
            error(3);
        }
        //Consumes ":="
        consumeToken("const-decl");

        //Checks if the assigned value is not a constant
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 2){
            //Error 3: Expected a number
            error(3);
        }

        //gets the value assigned to the identifier then consumes value
        int value = tokenTable[currentToken].val;
        
        //Consumes number
        consumeToken("const-decl");

        //We now have the tokenTable context to create the symbol
        addSymbol(1, nameIdx, value, 0);
        
        //Looks for comma
        if(currentToken < tCount && tokenTable[currentToken].tCode == 15){
            //Consumes comma keeps looping
            consumeToken("const-decl");
        }
        else{
            break;
        }
    }

    //Checks if semi-colon is missing
    if(currentToken >= tCount || tokenTable[currentToken].tCode != 16){
        //Error 4: Expected semi-colon
        error(4);
    }
    //Consumes semi-colon
    consumeToken("const-decl");
}

void varDecleration(int* localAddress){
    //Moves past var keyword
    consumeToken("var-decl");

    while(1){

        //Check for identifier
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 1){
            //Error 2: Expected identifier
            error(2);
        }

        int nameIdx = tokenTable[currentToken].val;

        //Consume identifier
        consumeToken("var-decl");

        int symbolIdx = findSymbol(nameIdx);
        
        //Checks for duplicates on the same level
        if(symbolIdx >= 0 && symbolTable[symbolIdx].level == level){
            //Error 14: Identifier already declared in block
            currentToken--;
            error(14);
        }

        

        // Check whether the symbol table is full
        if(symbolCount >= 100){
            // Point back to the identifier for the error message
            currentToken--;
            error(17);
        }

        //Adds var to symbol table
        addSymbol(2, nameIdx, 0, *localAddress);
        //Increments adress
        (*localAddress)++;

        //Check for more variables
        if(currentToken < tCount && tokenTable[currentToken].tCode == 15){
            //Consume comma
            consumeToken("var-decl");
        }

        else{
            break;
        }
    }

    //Checks for lack of semicolon
    if(currentToken >= tCount || tokenTable[currentToken].tCode != 16){
        //Error 4: Expected semi-colon
        error(4);
    }
    //Consume semi-colon
    consumeToken("var-decl");


} 

void procDecleration(){
    //Consumes "procedure" keyword
    consumeToken("proc-decl");

    //Checks for lack of indentifier
    if(currentToken >= tCount || tokenTable[currentToken].tCode != 1){
        error(2);
    }

    int nameIdx = tokenTable[currentToken].val;
    int symbolIdx = (findSymbol(nameIdx));
    if(symbolIdx >= 0 && symbolTable[symbolIdx].level == level){
        //Checks if procedure already exists
        error(14);
    }

    //Adds symbol with the addres of the procedure start
    addSymbol(3, nameIdx, 0, 200 + 3 * cCount);

    //Consumes identifier
    consumeToken("proc-decl");
    //Checks for lack of ;
    if(currentToken >= tCount || tokenTable[currentToken].tCode != 16){
        error(12);
    }

    //Consume ;
    consumeToken("proc-decl");
    //Enter procedures lexical level
    level++;
    //proc Decleration calls block
    block();

    //Leaves procedures lexical level
    level--;
    //Checks for lack of ;
    if(currentToken >= tCount || tokenTable[currentToken].tCode != 16){
        error(12);
    }
    //Consume ;
    consumeToken("proc-decl");
}

void block(){
    int localAddress = 3;
    //Used to start adding symbols from this index to not overwrite symbols from prev scope
    int scopeStart = symbolCount;

    //Holds index that we use to packpatch to after replacing the unfinished jump
    int jumpIdx = cCount;
    //Emit jump with temp address
    emit(7, 0, 0);

    //Checks for const keyword
    if(currentToken < tCount && tokenTable[currentToken].tCode == 31){
        constDecleration();
    }
    //Checks for var keyword
    if(currentToken < tCount && tokenTable[currentToken].tCode == 32){
        varDecleration(&localAddress);
    }
    //Checks for procedure keyword
    while(currentToken < tCount && tokenTable[currentToken].tCode == 33){
        procDecleration();
    }

    //Does the backpatching of the jump instruction
    code[jumpIdx].m = 200 + 3 * cCount;

    emit(6, 0, localAddress);

    statement();

    //Set mark of all used symbols to 1
    for(int i = scopeStart; i < symbolCount; i++){
        if(symbolTable[i].mark == 0){
            symbolTable[i].mark = 1;
        }
    }

    //Returns from procedure
    if(level > 0){
        emit(2, 0, 0);
    }
}

void program(){
    block();
    //Checks if token is period
    if(currentToken < tCount && tokenTable[currentToken].tCode == 17){
        //Consumers period
        consumeToken("program");
    }

    else{
        //Error 1: doesn't end with period
        error(1);
    }

    if(currentToken != tCount){
        //Error 1: program ends but tokens still remain after period
        error(1);
    }

    //Emits code for halitng the program once it ends.
    emit(9, 0, 3);
}

int findSymbol(int nameIdx){
    //Go through symbols and check if nameIdx is taken already or if variable already used
    //If not returns symbolTable index
    for(int i = symbolCount - 1; i >= 0; i--){
        if(symbolTable[i].nameIdx == nameIdx && symbolTable[i].mark == 0){
            return i;
        }
    }
    
    return -1;
}

void statement(){
    if(currentToken >= tCount){
        return;
    }

    //Checks for assignment statements
    if(currentToken < tCount && tokenTable[currentToken].tCode == 1){
        int nameIdx = tokenTable[currentToken].val;
        int symbolIdx = findSymbol(nameIdx);

        //Consumes identifier
        consumeToken("statement");

        if(symbolIdx < 0){
            //Error 15: undeclared identifier
            currentToken--;
            error(15);
        }

        if(symbolTable[symbolIdx].type != 2){
            //Error 16: can't assign to number
            currentToken--;
            error(16);
        }

        //Checks for "="
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 18){
            //Error 5: expected =
            error(5);
        }

        //Consumes =
        consumeToken("statement");

        expression();

        //Emits assignment code
        emit(4, level - symbolTable[symbolIdx].level, symbolTable[symbolIdx].address);
    }

    //Looks for begin statements
    else if(currentToken < tCount && tokenTable[currentToken].tCode == 20){
        //Consumes begin
        consumeToken("statement");
        statementList();
        //Looks for end
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 21){
            //Error 6: missing end statement
            error(6);
        }
        //Consumes end
        consumeToken("statement");
    }

    //Looks for If then statements
    else if(currentToken < tCount && tokenTable[currentToken].tCode == 22){
        //Consumes if and checks for condition
        consumeToken("statement");
        condition();

        //Saves jpcIdx for backpatching later
        int jpcIdx = cCount;
        emit(8, 0, 0);

        //Check for lack of then
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 24){
            //Error 7: no then statment
            error(7);
        }
        //Consumes then
        consumeToken("statement");

        statementList();


        //Checks for else
        if(currentToken < tCount && tokenTable[currentToken].tCode == 36){
            //Saves current position in text seg
            int jmpIdx = cCount;
            //Jumps past else
            emit(7, 0, 0);
            code[jpcIdx].m = 200 + 3 * cCount;

            //Consumes else
            consumeToken("statement");
            statementList();

            //Backpatch orinal JPC to after else statement
            code[jmpIdx].m = 200 + 3 * cCount;
        }
        else{
            //Backpactch original JPC to after conditional statements
            code[jpcIdx].m = 200 + 3 * cCount;
        }

        
        //Checks for "fi"
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 23){
            //Error 8: no fi to show statement end
            error(8);
        }
        //Consume fi
        consumeToken("statement");
    }

    //Looks for while/do statements
    else if(currentToken < tCount && tokenTable[currentToken].tCode == 25){
        //Saves the begging of the loop for future use
        int loopStart = 200 + 3 * cCount;

        //Consumes while
        consumeToken("statement");
        condition();

        //Save JPC code index for backpatching
        int jpcIdx = cCount;
        //JPC for the loops condition
        emit(8, 0, 0);

        if(currentToken >= tCount || tokenTable[currentToken].tCode != 24){
            //Error 7: no then statmement
            error(7);
        }
        //Consume then
        consumeToken("statement");
        statementList();

        //Jumps back to loop start for re-evaluation
        emit(7, 0, loopStart);

        //Backpatch the JPC to exit loop if false
        code[jpcIdx].m = 200 + 3 * cCount;

        //Checks for elihw finishing of while
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 26){
            //Error 8: no elihw statmement
            error(8);
        }
        //Consumes elihw
        consumeToken("statement");
    }

    //Looks for read
    else if(tokenTable[currentToken].tCode == 35){
        //Consume read
        consumeToken("statement");

        //Checks if its not identifier
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 1){
            //Error 2: Expected identifier
            error(2);
        }

        int nameIdx = tokenTable[currentToken].val;
        int symbolIdx = findSymbol(nameIdx);

        //Consumes identifer
        consumeToken("statement");

        if(symbolIdx < 0){
            //Error 15 Undeclared identifer
            currentToken--;
            error(15);
        }

        //Makes sure identifier is a variable
        if (symbolTable[symbolIdx].type != 2){
            //Error 16: Can only read variables
            currentToken--;
            error(16);
        }

        //Reads top of stack
        emit(9, 0, 2);
        //Stores it at variable address
        emit(4, level - symbolTable[symbolIdx].level, symbolTable[symbolIdx].address);
        
    }

    //Looks for write
    else if(tokenTable[currentToken].tCode == 34){
    // Consume write
    consumeToken("statement");

    //Parses expression that is written
    expression();
    //Emits write code
    emit(9, 0, 1);
    }

    //Looks for call keyword
    else if(tokenTable[currentToken].tCode == 30){
        //consumes call keyword
        consumeToken("statement");

        //Checks if identifer missing
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 1){
            error(2);
        }

        int nameIdx = tokenTable[currentToken].val;
        int symbolIdx = findSymbol(nameIdx);

        //Consume identifer
        consumeToken("statement");

        //Checks if identifer does not exist yet
        if(symbolIdx < 0){
            currentToken--;
            error(15);
        }

        //Verify that it is the correct sybmol type
        if(symbolTable[symbolIdx].type != 3){
            currentToken--;
            error(13);
        }
        //Emits CALL
        emit(5, level - symbolTable[symbolIdx].level, symbolTable[symbolIdx].address);
    }

}

void statementList(){
    statement();

    while(currentToken < tCount && tokenTable[currentToken].tCode == 16){
        //Consumes ";"
        consumeToken("statement-list");
        //Parses each statement in statement list
        statement();
    }
}

void factor(){
    if(currentToken >= tCount){
        //Error 11: Expected Factor
        error(11);
    }

    int tokenType = tokenTable[currentToken].tCode;

    //Checks for identifier
    if(tokenType == 1){
        int nameIdx = tokenTable[currentToken].val;
        int symbolIdx = findSymbol(nameIdx);

        //Consumes identifier
        consumeToken("factor");

        if(symbolIdx < 0){
            //Error 15: Undeclared identifier
            currentToken--;
            error(15);
        }


        //Traces lookup for symbol
        if(symbolTable[symbolIdx].type == 1){
            emit(1, 0, symbolTable[symbolIdx].value);
        }
        else if(symbolTable[symbolIdx].type == 2){
            emit(3, level - symbolTable[symbolIdx].level, symbolTable[symbolIdx].address);
        }
        else{
            //Procedures cannot be called as factors
            currentToken--;
            error(11);
        }
        
    }

    //Checks for numbers
    else if(tokenType == 2){
        //Emits LIT with number value
        emit(1, 0, tokenTable[currentToken].val);
        //Consumes number token
        consumeToken("factor");
    }

    //Checks for expressions with "("
    else if(tokenType == 13){
        consumeToken("factor");

        //Parse expression in parenthesis
        expression();

        //Checks for valid closing parenthesis ")"
        if(currentToken >= tCount || tokenTable[currentToken].tCode != 14){
            //Error 10: Invalid closing parenthesis
            error(10);
        }
        //Consumes )
        consumeToken("factor");

    }

    else{
        //Error 11: Invalid factor
        error(11);
    }
}

void expression(){
    int negFlag = 0;
    //Checks for optional beggining sign symbol
    if(currentToken < tCount && (tokenTable[currentToken].tCode == 3 || tokenTable[currentToken].tCode == 4)){
        //If its negative we have to negate after parsing term
        if(tokenTable[currentToken].tCode == 4){
            negFlag = 1;
        }
        //Consumes sign symbol
        consumeToken("expression");
    }
    //Parses first term
    term();
    //Negates term if negative
    if(negFlag == 1){
        emit(1, 0, -1);
        emit(2, 0, 3);
    }
    //Checks for "+" or "-"
    while(currentToken < tCount  && (tokenTable[currentToken].tCode == 3 || tokenTable[currentToken].tCode == 4)){
        int m;
        if(tokenTable[currentToken].tCode == 3)
            m = 1;
        else
            m = 2;
        //Consumes "+" or "-"
        consumeToken("expression");
        //Parses next term
        term();
        emit(2, 0, m);
    }

    //Since expression calls turn the recursive call for term will finish first given multiplication and division precedence
}

void term(){
    
    //Consumes the factor in term
    factor();
    //Checks for "*" or "/"
    while(currentToken < tCount && (tokenTable[currentToken].tCode == 5 || tokenTable[currentToken].tCode == 6)){
        int m;
        if(tokenTable[currentToken].tCode == 5)
            m = 3;
        else
            m = 4;
        
        //Consumes "*" or "/"
        consumeToken("term");
        //Parses factor
        factor();
        //Emits the opperation after factor is put on stack
        emit(2, 0, m);
    }
}

void condition(){
    //Parses expression
    expression();
    //Checks for comparison opperators
    if(currentToken < tCount && (tokenTable[currentToken].tCode == 7 || tokenTable[currentToken].tCode == 8 || tokenTable[currentToken].tCode == 9 || tokenTable[currentToken].tCode == 10 || tokenTable[currentToken].tCode == 11 || tokenTable[currentToken].tCode == 12)){
        int oppTcode = tokenTable[currentToken].tCode;
        consumeToken("condition");
        //Parses second expression
        expression();

        //Emits the code for the comparison we use the formula tokenCode - 2 to get the m code for the PL/0 opperation
        emit(2, 0, oppTcode - 2);
    }
    else{
        //Error 9: Invalid comparison opperator
        error(9);
    }
}

void error(int errCode){
    //static string array to hold err msgs
static const char *messages[] = {
        "",
        "the program must end with a period, and nothing may follow it",
        "const, var, and read must be followed by an identifier",
        "a constant must be declared as name := number",
        "declarations must be separated by , and end with ;",
        "an identifier that starts a statement must be followed by =",
        "begin must be closed with end",
        "a condition after if or while must be followed by then",
        "if must be closed with fi, and while with elihw",
        "a condition must contain one of == != < <= > >=",
        "a left parenthesis must be closed with a right parenthesis",
        "a factor must be an identifier, a number, or (",
        "procedure declarations are not part of this grammar",
        "call statements are not part of this grammar",
        "identifier already declared in this block",
        "undeclared identifier",
        "only a variable may be assigned or read",
        "symbol table is full",
        "cannot open tokens.txt or nametable.txt"
    };
    
    //Prints symbolTable and result line
    if(errCode == 18){
    printf("\nGenerated Code:\n\n");
    printf("\nSymbol Table:\n\n");
    }
    else{
        printSymbolTable();
    }

    printf("Result:\n\n");


    //Handles file opening issues
    if(errCode == 18){
        printf("Error 18: %s\n", messages[18]);
    }
    //Handles out of bounds tokens
    else if(currentToken >= tCount){
        printf("Error %d at end of tokens: %s\n", errCode, messages[errCode]);
    }
    else{
        printf("Error %d at token %d '%s': %s\n", errCode, currentToken + 1, getTokenText(currentToken), messages[errCode]);
    }

    exit(1);
}

const char* getTokenText(int tokenIdx){
    static char buffer[32];

    if(tokenIdx < 0 || tokenIdx >= tCount){
        return "";
    }

    Token t = tokenTable[tokenIdx];

    //Checks if identifier
    if(t.tCode == 1){
        int nameIdx = t.val;

        //Gets the string name of identifier
        for(int i = 0; i<iCount; i++){
            if(identifierTable[i].index == nameIdx){
                return identifierTable[i].name;
            }
        }
        return "";
    }

    //Checks if number (must be converted to string)
    if(t.tCode == 2){
        return tokenTable[tokenIdx].text;
    }

    // Operators, punctuation, and keywords
    static const char *tokenNames[] = {
        "", "", "",
        "+", "-", "*", "/",
        "==", "!=", "<", "<=", ">", ">=",
        "(", ")", ",", ";", ".", "=", ":=",
        "begin", "end", "if", "fi", "then",
        "while", "elihw", "do", "od", "odd",
        "call", "const", "var", "procedure",
        "write", "read", "else"
    };

    if(t.tCode >= 3 && t.tCode <= 36){
        return tokenNames[t.tCode];
    }

    return "";
}

void consumeToken(const char* owner){
    if(currentToken >= tCount)
        return;
    currentToken++;
}

void printSymbolTable(){
    printf("\nSymbol Table: \n\n");
    if(symbolCount > 0){
        printf("entry\tkind\tname\tvalue\tlevel\taddress\tmark\n");
        for(int i = 0; i<symbolCount; i++){
            Symbol s = symbolTable[i];
            if(s.type == 1){
                //Constants
                printf("%d\tconstant\t%s\t%d\t%d\t-\t%d\n", i, identifierTable[s.nameIdx].name, s.value, s.level, s.mark);
            }
            else{
                //Variables
                printf("%d\tvariable\t%s\t-\t%d\t%d\t%d\n", i, identifierTable[s.nameIdx].name, s.level, s.address, s.mark);
            }
        }
    }

    printf("\n");
}

void emit(int op, int l, int m){
    //Handles overflow
    if(cCount >= 200){
        exit(1);
    }

    code[cCount].op = op;
    code[cCount].l = l;
    code[cCount].m = m;
    cCount++;
}

void printCode(){
    printf("Generated Code:\n\n");
    for(int i = 0; i<cCount; i++){
        printf("%s\t%d\t%d\n", opNames[code[i].op], code[i].l, code[i].m);
    }
}