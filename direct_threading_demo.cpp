#include <iostream>
#include <vector>
#include <fstream>
#include <map>

#define DEBUG 0
#define break_if( E ) if ( E ) break
#define return_if( E ) if ( E ) return
#define break_unless( E ) if (!(E)) break

typedef union {
    void* opcode;
    int operand;
} instruction;

class CodePlanter {
    std::ifstream input;
    const std::map<char, void *> & instructions;
    std::vector<instruction> & program;
    std::vector<int> indexes;
public:
    CodePlanter( std::string_view filename, const std::map<char, void *> & instructions, std::vector<instruction> & program ) :
        input( filename.data(), std::ios::in ),
        instructions( instructions ),
        program( program )
    {}

    void plantChar( char ch ) {
        auto it = instructions.find(ch);
        return_if( it == instructions.end() );
        program.push_back( { it->second } );
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
    std::map<char, void*> instructions;
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

int main( int argc, char * argv[] ) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    for (auto arg : args) {
        Engine engine;
        engine.runFile( arg );
    }
    exit( EXIT_SUCCESS );
}
