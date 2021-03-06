/*
This code demonstrates how to implement direct-threaded code using GNU C++ by
exploiting a little-known extension - the ability to take the address of a label,
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
#include <optional>
#include <stdexcept>
#include <deque>
#include <cstdlib>


#include "json.hpp"

using namespace nlohmann;

//  Use this to turn on or off some debug-level tracing.
#define DEBUG 0
#define DUMP 0

//  Syntactic sugar to emphasise the 'break'.
#define break_if( E ) if ( E ) break
#define break_unless( E ) if (!(E)) break

//  Syntactic sugar to emphasise the 'return'. Usage:
//      return_if( E );
//      return_if( E )( Result )
#define return_if( E ) if (E) return
#define return_unless( E ) if (!(E)) return

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
};

//  We use the address of a label to play the role of an operation-code.
typedef void * OpCode;

typedef struct Dyad {
    int32_t operand1;
    int32_t operand2;
} Dyad;

//  The instruction stream is mainly OpCodes but there are some
//  integer arguments interspersed. Strictly speaking this makes this
//  interpreter a hybrid between direct/indirect threading.
typedef union {
    OpCode opcode;
    int64_t operand;
    Dyad dyad;
} Instruction;

//  Horrible hack to more-or-less switch on string-literals.
//      https://www.rioki.org/2016/03/31/cpp-switch-string.html
constexpr 
unsigned int hash(const char* str, int h = 0)
{
    return !str[h] ? 5381 : (hash(str, h+1)*33) ^ str[h];
}

typedef struct InstructionSet {
    OpCode SET_ZERO;
    OpCode INCR;
    OpCode DECR;
    OpCode ADD;
    OpCode ADD_OFFSET;
    OpCode XFR_MULTIPLE;
    OpCode LEFT;
    OpCode RIGHT;
    OpCode SEEK_LEFT;
    OpCode SEEK_RIGHT;
    OpCode MOVE;
    OpCode OPEN;
    OpCode CLOSE;
    OpCode GET;
    OpCode PUT;
    OpCode HALT;
public:
    OpCode byName( const std::string & name ) const {
        switch ( hash( name.c_str() ) ) {
            case hash( "SET_ZERO" ): return SET_ZERO;
            case hash( "INCR" ): return INCR;
            case hash( "DECR" ): return DECR;
            case hash( "ADD" ): return ADD;
            case hash( "ADD_OFFSET" ): return ADD_OFFSET;
            case hash( "XFR_MULTIPLE" ): return XFR_MULTIPLE;
            case hash( "LEFT" ): return LEFT;
            case hash( "RIGHT" ): return RIGHT;
            case hash( "SEEK_LEFT" ): return SEEK_LEFT;
            case hash( "SEEK_RIGHT" ): return SEEK_RIGHT;
            case hash( "MOVE" ): return MOVE;
            case hash( "OPEN" ): return OPEN;
            case hash( "CLOSE" ): return CLOSE;
            case hash( "GET" ): return GET;
            case hash( "PUT" ): return PUT;
            case hash( "HALT" ): return HALT;
        };
        throw std::runtime_error( "Unrecognised opcode: " + name );
    }
} InstructionSet;

//  This class is responsible for translating the stream of source code
//  into a vector<Instruction>. It is passed a mapping from characters
//  to the addresses-of-labels, so it can plant (aka append) the exact
//  pointer to the implementing code. 
class CodePlanter {
    const std::string filename;  
    const InstructionSet & instruction_set;
    std::vector<Instruction> & program; 

public:
    CodePlanter( 
        const std::string filename,
        const InstructionSet & instruction_set,
        std::vector<Instruction> & program 
    ) :
        filename( filename ),
        instruction_set( instruction_set ), 
        program( program )
    {}

private:

    void plantDyad( const json & joperand ) {
        int32_t high = joperand[ "High" ];
        int32_t low = joperand[ "Low" ];
        struct Dyad d = { .operand1=high, .operand2=low };
        program.push_back( { .dyad=d } );
    }

    void plantOperand( const json & joperand ) {
        int64_t n = joperand[ "Operand" ];
        program.push_back( { .operand=n } );
    }

    void plantOpCode( const json & jopcode ) {
        const std::string name = jopcode[ "OpCode" ];
        OpCode opcode( instruction_set.byName( name ) );
        program.push_back( { opcode } );
    }

public:
    void plantProgram() {
        std::ifstream input( filename.c_str(), std::ios::in );
        json jprogram;
        input >> jprogram;
        for ( auto& i : jprogram ) {
            if ( i.contains( "OpCode" ) ) {
                plantOpCode( i );
            } else if ( i.contains( "Operand" ) ) {
                plantOperand( i );
            } else if ( i.contains( "High" ) ) {
                plantDyad( i );
            }
        }
        program.push_back( { instruction_set.HALT } );
    }
};

typedef unsigned char num;

class Engine {
    std::map<char, OpCode> opcode_map;
    std::map<std::string, OpCode> extra_opcodes_map;
    std::vector<Instruction> program;
    std::vector<num> memory;
public:
    Engine() : 
        memory( 30000 )
    {}

public:
    void runFile( const std::string filename, bool header_needed ) {
        if ( header_needed ) {
            std::cerr << "# Executing: " << filename << std::endl;
        }

        InstructionSet instruction_set;
        instruction_set.INCR = &&INCR;
        instruction_set.DECR = &&DECR;
        instruction_set.LEFT = &&LEFT;
        instruction_set.RIGHT = &&RIGHT;
        instruction_set.OPEN = &&OPEN;
        instruction_set.CLOSE = &&CLOSE;
        instruction_set.PUT = &&PUT;
        instruction_set.GET = &&GET;
        instruction_set.ADD = &&ADD;
        instruction_set.MOVE = &&MOVE;
        instruction_set.SET_ZERO = &&SET_ZERO;
        instruction_set.ADD_OFFSET = &&ADD_OFFSET;
        instruction_set.XFR_MULTIPLE = &&XFR_MULTIPLE;
        instruction_set.SEEK_LEFT = &&SEEK_LEFT;
        instruction_set.SEEK_RIGHT = &&SEEK_RIGHT;
        instruction_set.HALT = &&HALT;
        
        CodePlanter planter( filename, instruction_set, program );
        planter.plantProgram();

        std::noskipws( std::cin );

        auto program_data = program.data();
        Instruction * pc = &program_data[0];
        num * loc = &memory.data()[0];
        goto *(pc++->opcode);

        ////////////////////////////////////////////////////////////////////////
        //  Control flow does not reach this position! 
        //  Instructions are listed below.
        ////////////////////////////////////////////////////////////////////////

    INCR:
        if ( DEBUG ) std::cout << "INCR" << std::endl;
        *loc += 1;
        goto *(pc++->opcode);
    DECR:
        if ( DEBUG ) std::cout << "DECR" << std::endl;
        *loc -= 1;
        goto *(pc++->opcode);
    ADD:
        if ( DEBUG ) std::cout << "ADD" << std::endl;
        {
            int n = pc++->operand;
            *loc += n;
        }
        goto *(pc++->opcode);
    ADD_OFFSET:
        if ( DEBUG ) std::cout << "ADD_OFFSET" << std::endl;
        {
            struct Dyad d = pc++->dyad;
            int32_t offset = d.operand1;
            int32_t by = d.operand2;
            *( loc + offset ) += by;
        }
        goto *(pc++->opcode);
    RIGHT:
        if ( DEBUG ) std::cout << "RIGHT" << std::endl;
        loc += 1;
        goto *(pc++->opcode);
    LEFT:
        if ( DEBUG ) std::cout << "LEFT" << std::endl;
        loc -= 1;
        goto *(pc++->opcode);
    MOVE:
        if ( DEBUG ) std::cout << "MOVE" << std::endl;
        {
            int n = pc++->operand;
            loc += n;
        }
        goto *(pc++->opcode);
    PUT:
        if ( DEBUG ) std::cout << "PUT" << std::endl;
        {
            num i = *loc;
            std::cout << i;
        }
        goto *(pc++->opcode);
    GET:
        if ( DEBUG ) std::cout << "GET" << std::endl;
        {
            char ch;
            std::cin.get( ch );
            if (std::cin.good()) {
                *loc = ch;
            }
        }
        goto *(pc++->opcode);
    OPEN:
        if ( DEBUG ) std::cout << "OPEN" << std::endl;
        {
            int n = pc++->operand;
            if ( *loc == 0 ) {
                pc = &program_data[n];
            }
            goto *(pc++->opcode);
        }
    CLOSE:
        if ( DEBUG ) std::cout << "CLOSE" << std::endl;
        {
            int n = pc++->operand;
            if ( *loc != 0 ) {
                pc = &program_data[n];
            }
            goto *(pc++->opcode);
        }
    SET_ZERO:
        if ( DEBUG ) std::cout << "SET_ZERO" << std::endl;
        *loc = 0;
        goto *(pc++->opcode);
    XFR_MULTIPLE:
        if ( DEBUG ) std::cout << "XFR_MULTIPLE" << std::endl;
        {
            struct Dyad d = pc++->dyad;
            int offset = d.operand1;
            int by = d.operand2;
            int n = *loc;
            if ( DEBUG ) std::cout << "XFR_MULTIPLE offset=" << offset << " n=" << n << " by=" << by << std::endl;
            *( loc + offset ) += n * by;
            *loc = 0;
        }
        goto *(pc++->opcode);
    SEEK_LEFT:
        if ( DEBUG ) std::cout << "SEEK_LEFT" << std::endl;
        {
            while ( *loc ) {
                loc -= 1;
            }
        }
        goto *(pc++->opcode);
    SEEK_RIGHT:
        if ( DEBUG ) std::cout << "SEEK_RIGHT" << std::endl;
        {
            while ( *loc ) {
                loc += 1;
            }
        }
        goto *(pc++->opcode);
    HALT:
        if ( DEBUG ) std::cout << "DONE!" << std::endl;
        return;
    }
};

/*
Each argument is the name of a Brainf*ck source file to be compiled into
threaded coded and executed.
*/
int main( int argc, char * argv[] ) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    for (auto arg : args) {
        Engine engine;
        engine.runFile( arg, args.size() > 1 );
    }
    exit( EXIT_SUCCESS );
}
