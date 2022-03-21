/*
This code demonstrates how to implement direct-threaded code using GNU C++ by
exploiting a little-know extension - the ability to take the address of a label,
store it in a pointer, and jump to it through the pointer.

In this example, we translate Brainf*ck into threaded code, where the program
is a sequence of abstract-machine instructions. Each instruction of the program
is a pointer to a label that represents the code to execute, optionally
followed by some integer data.

We use Brainf*ck because it has very few instructions, which makes the crucial 
method (Engine::runFile) fit on a single screen.
*/


#include <iostream>
#include <vector>
#include <fstream>
#include <map>

#define DEBUG 0
#define break_if( E ) if ( E ) break
#define break_unless( E ) if (!(E)) break

//  We use the address of a label to play the role of an operation-code.
typedef void * OpCode;

//  The instruction stream is mainly OpCodes but there are some
//  integer arguments interspersed. Strictly speaking this makes this
//  interpreter a hybrid between direct/indirect threading.
typedef union {
    OpCode opcode;
    int operand;
} instruction;

//  This class is responsible for translating the stream of source code
//  into a vector<instruction>. It is passed a mapping from characters
//  to the addresses-of-labels, so it can plant (aka append) the exact
//  pointer to the implementing code. 
class CodePlanter {
    std::ifstream input;                //  The source code to be read in.
    const std::map<char, OpCode> & instructions;
    std::vector<instruction> & program; 
    std::vector<int> indexes;           //  Responsible for managing [ ... ] loops.

public:
    CodePlanter( std::string_view filename, const std::map<char, OpCode> & instructions, std::vector<instruction> & program ) :
        input( filename.data(), std::ios::in ),
        instructions( instructions ),
        program( program )
    {}

private:
    void plantChar( char ch ) {
        //  Guard - skip characters that do not correspond to abstract machine
        //  operations.
        auto it = instructions.find(ch);
        if ( it == instructions.end() ) return;

        //  Body
        program.push_back( { it->second } );
        //  If we are dealing with loops, we plant the absolute index of the
        //  operation in the program we want to jump to. This can be improved
        //  fairly easily.
        if ( ch == '[' ) {
            indexes.push_back( program.size() );
            program.push_back( {nullptr} );
        } else if ( ch == ']' ) {
            int end = program.size();
            int start = indexes.back();
            indexes.pop_back();
            program[ start ].operand = end + 1;
            program.push_back( { .operand={ start + 1 } } );
        }
    }

public:
    void plantProgram() {
        for (;;) {
            char ch = input.get();
            break_unless( input.good() );
            plantChar( ch );
        }
        program.push_back( { instructions.at('\0') } );
    }
};

typedef unsigned char num;

class Engine {
    std::map<char, OpCode> instructions;
    std::vector<instruction> program;
    std::vector<num> memory;
public:
    Engine() : 
        memory( 30000 )
    {}

public:
    void runFile( std::string_view filename ) {
        std::cout << "# Executing: " << filename << std::endl;

        instructions = {
            { '+', &&ADD },
            { '-', &&SUB },
            { '<', &&DOWN },
            { '>', &&UP },
            { '[', &&LEFT },
            { ']', &&RIGHT },
            { '.', &&PUT },
            { '\0', &&HALT }
        };
        
        CodePlanter planter( filename, instructions, program );
        planter.plantProgram();

        auto program_data = program.data();
        instruction * pc = &program_data[0];
        num * loc = &memory.data()[0];
        goto *(pc++->opcode);

        ////////////////////////////////////////////////////////////////////////
        //  Control flow does not reach this position! 
        //  Instructions are listed below.
        ////////////////////////////////////////////////////////////////////////

    ADD:
        if ( DEBUG ) std::cout << "ADD" << std::endl;
        *loc += 1;
        goto *(pc++->opcode);
    SUB:
        if ( DEBUG ) std::cout << "SUB" << std::endl;
        *loc -= 1;
        goto *(pc++->opcode);
    UP:
        if ( DEBUG ) std::cout << "UP" << std::endl;
        loc += 1;
        goto *(pc++->opcode);
    DOWN:
        if ( DEBUG ) std::cout << "DOWN" << std::endl;
        loc -= 1;
        goto *(pc++->opcode);
    PUT:
        if ( DEBUG ) std::cout << "PUT" << std::endl;
        {
            num i = *loc;
            std::cout << i;
        }
        goto *(pc++->opcode);
    LEFT:
        if ( DEBUG ) std::cout << "LEFT" << std::endl;
        {
            int n = pc++->operand;
            if ( *loc == 0 ) {
                pc = &program_data[n];
            }
            goto *(pc++->opcode);
        }
    RIGHT:
        if ( DEBUG ) std::cout << "RIGHT" << std::endl;
        {
            int n = pc++->operand;
            if ( *loc != 0 ) {
                pc = &program_data[n];
            }
            goto *(pc++->opcode);
        }
    HALT:
        if ( DEBUG ) std::cout << "DONE!";
        std::cout << std::endl;
        return;
    }
};

/*
Each argument is the name of a Brainf*ck source file to be compiled into
threaded coded and executed.
*/
int main( int argc, char * argv[] ) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    for (auto arg : args) {
        Engine engine;
        engine.runFile( arg );
    }
    exit( EXIT_SUCCESS );
}
