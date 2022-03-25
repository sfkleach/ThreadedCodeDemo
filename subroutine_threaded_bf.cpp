#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <string>

#define DEBUG 0
#define break_if( E ) if ( E ) break
#define return_if( E ) if ( E ) return
#define break_unless( E ) if (!(E)) break

class Engine;

typedef void(Engine::*OpCode)( union Instruction * & pc );

typedef union Instruction {
    OpCode opcode;
    int operand;
} Instruction;

class CodePlanter {
    std::ifstream input;
    const std::map<char, OpCode> & opcode_map;
    std::vector<Instruction> & program;
    std::vector<int> indexes;

public:
    CodePlanter( 
        std::string_view filename, 
        const std::map<char, OpCode> & opcode_map, 
        std::vector<Instruction> & program 
    ) :
        input( filename.data(), std::ios::in ),
        opcode_map( opcode_map ),
        program( program )
    {}

private:
    void plantChar( char ch ) {
        //  Guard
        auto it = opcode_map.find(ch);
        return_if( it == opcode_map.end() );

        //  Body
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

public:
    void plantProgram() {
        for (;;) {
            char ch = input.get();
            break_unless( input.good() );
            plantChar( ch );
        }
        program.push_back( { opcode_map.at('\0') } );
    }
};

typedef unsigned char num;

class Engine {
    std::map<char, OpCode> opcode_map;
    std::vector<Instruction> program;
    std::vector<num> memory;
    Instruction * program_data = nullptr;
    num * loc = nullptr;

public:
    Engine() : 
        memory( 30000 )
    {}

public:
    void INCR( Instruction * & pc ) {
        if ( DEBUG ) std::cout << "INCR" << std::endl;
        *loc += 1;
    }

    void DECR( Instruction * & pc ) {
        if ( DEBUG ) std::cout << "DECR" << std::endl;
        *loc -= 1;
    }

    void RIGHT( Instruction * & pc ) {
        if ( DEBUG ) std::cout << "RIGHT" << std::endl;
        loc += 1;
    }

    void LEFT( Instruction * & pc ) {
        if ( DEBUG ) std::cout << "LEFT" << std::endl;
        loc -= 1;
    }

    void PUT( Instruction * & pc ) {
        num i = *loc;
        if ( DEBUG ) std::cout << "PUT: " << (int)i << std::endl;
        std::cout << i;
    }

    void GET( Instruction * & pc ) {
        if ( DEBUG ) std::cout << "GET" << std::endl;
        {
            char ch;
            std::cin.get( ch );
            if (std::cin.good()) {
                *loc = ch;
            }
        }
    }

    void OPEN( Instruction * & pc ) {
        if ( DEBUG ) std::cout << "OPEN" << std::endl;
        int n = pc++->operand;
        if ( *loc == 0 ) {
            pc = &program_data[n];
        }
    }

    void CLOSE( Instruction * & pc ) {
        if ( DEBUG ) std::cout << "CLOSE" << std::endl;
        int n = pc++->operand;
        if ( *loc != 0 ) {
            pc = &program_data[n];
        }
    }

    void HALT( Instruction * & pc ) {
        if ( DEBUG ) std::cout << "DONE!";
        exit( EXIT_SUCCESS );
    }

    void runFile( std::string_view filename, bool header_needed ) {
        if ( header_needed ) {
            std::cerr << "# Executing: " << filename << std::endl;
        }

        opcode_map = {
            { '+', &Engine::INCR },
            { '-', &Engine::DECR },
            { '<', &Engine::LEFT },
            { '>', &Engine::RIGHT },
            { '[', &Engine::OPEN },
            { ']', &Engine::CLOSE },
            { '.', &Engine::PUT },
            { ',', &Engine::GET },
            { '\0', &Engine::HALT }
        };
        
        CodePlanter planter( filename, opcode_map, program );
        planter.plantProgram();

        std::noskipws( std::cin );

        this->program_data = program.data();
        Instruction * pc = &program_data[0];
        loc = &memory.data()[0];
        
        for (;;) {
            OpCode s = pc++->opcode;
            (this->*s)( pc );
        }
    }
};

int main( int argc, char * argv[] ) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    for (auto arg : args) {
        Engine engine;
        engine.runFile( arg, args.size() > 1 );
    }
    exit( EXIT_SUCCESS );
}
