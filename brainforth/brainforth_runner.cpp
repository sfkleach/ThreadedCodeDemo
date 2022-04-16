/*
We extend the CISC Brainf*ck machine with new instructions:
    ?   Push the item at the current location onto the stack
    !   Pop the top item of the stack into the current location (or pop 0
        if the stack is empty)
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
    void * reference;
} Instruction;

//  Horrible hack to more-or-less switch on string-literals.
//      https://www.rioki.org/2016/03/31/cpp-switch-string.html
constexpr 
unsigned int hash(const char* str, int h = 0)
{
    return !str[h] ? 5381 : (hash(str, h+1)*33) ^ str[h];
}

typedef struct InstructionSet {
    OpCode PUSH;
    OpCode POP;
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
    OpCode SAVE;
    OpCode RESTORE;
    OpCode CALL;
    OpCode RETURN;
    OpCode HALT;
public:
    OpCode byName( const std::string & name ) const {
        switch ( hash( name.c_str() ) ) {
            case hash( "PUSH" ): return PUSH;
            case hash( "POP" ): return POP;
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
            case hash( "CALL" ): return CALL;
            case hash( "SAVE" ): return SAVE;
            case hash( "RESTORE" ): return RESTORE;
            case hash( "RETURN" ): return RETURN;
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
    std::map<std::string, std::vector<Instruction>> & bindings; 
    std::vector< std::tuple< std::string, size_t, std::string > > backfill;

public:
    CodePlanter( 
        const std::string filename,
        const InstructionSet & instruction_set,
        std::map<std::string, std::vector<Instruction>> & bindings 
    ) :
        filename( filename ),
        instruction_set( instruction_set ), 
        bindings( bindings )
    {}

private:

    void plantDyad( const json & joperand, std::vector< Instruction > & program ) {
        int32_t high = joperand[ "High" ];
        int32_t low = joperand[ "Low" ];
        struct Dyad d = { .operand1=high, .operand2=low };
        program.push_back( { .dyad=d } );
    }

    void plantOperand( const json & joperand, std::vector< Instruction > & program ) {
        int64_t n = joperand[ "Operand" ];
        program.push_back( { .operand=n } );
    }

    void plantReference( const json & joperand, std::vector< Instruction > & program, const std::string & enclosing ) {
        std::string name = joperand[ "Ref" ];
        program.push_back( { .operand=0 } );    //  Insert placeholder.
        backfill.push_back( { enclosing, program.size() - 1, name } );
    }

    void plantOpCode( const json & jopcode, std::vector< Instruction > & program ) {
        const std::string name = jopcode[ "OpCode" ];
        OpCode opcode( instruction_set.byName( name ) );
        program.push_back( { opcode } );
    }

public:
    void plantProgram() {
        std::ifstream input( filename.c_str(), std::ios::in );
        json jprogram;
        input >> jprogram;
        //  Ensure the bindings map is filled up - we don't want anything moving.
        for ( auto & [name, jcode] : jprogram.items() ) {
            this->bindings[ name ] = std::vector<Instruction>();
        }
        //  Now populate the individual bindings - collecting up references to backfill.
        for ( auto & [name, jcode] : jprogram.items() ) {
            std::vector< Instruction > & program = this->bindings[ name ];
            for ( auto & i : jcode ) {
                // std::cerr << "Code: " << i << std::endl;
                if ( i.contains( "OpCode" ) ) {
                    plantOpCode( i, program );
                } else if ( i.contains( "Operand" ) ) {
                    plantOperand( i, program );
                } else if ( i.contains( "High" ) ) {
                    plantDyad( i, program );
                } else if ( i.contains( "Ref" ) ) {
                    plantReference( i, program, name );
                }
            }
        }
        //  Now backfill the references.
        for ( auto & [ enclosing, index, refname ] : backfill ) {
            this->bindings[ enclosing ][ index ].reference = this->bindings[ refname ].data();
        }
    }
};

typedef unsigned char num;

typedef union CallStackSlot {
    Instruction * return_address;
    struct SavedLocation {
        num saved;
        num * location;

    public:
        void restore() {
            *location = saved;
        }
    } saved_location;
} CallStackSlot;

class Engine {
    std::map<char, OpCode> opcode_map;
    std::map<std::string, OpCode> extra_opcodes_map;
    std::map<std::string, std::vector<Instruction>> bindings;
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
        instruction_set.PUSH = &&PUSH;
        instruction_set.POP = &&POP;
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
        instruction_set.CALL = &&CALL;
        instruction_set.RETURN = &&RETURN;
        instruction_set.SAVE = &&SAVE;
        instruction_set.RESTORE = &&RESTORE;
        instruction_set.HALT = &&HALT;
        
        CodePlanter planter( filename, instruction_set, bindings );
        planter.plantProgram();

        std::noskipws( std::cin );

        auto program_data = bindings["main"].data();
        Instruction * pc = &program_data[0];
        num * loc = &memory.data()[0];
        std::vector< num > data_stack( 30000 );
        num * stack = &data_stack.data()[ 0 ];
        std::vector< CallStackSlot > call_stack;
        goto *(pc++->opcode);

        ////////////////////////////////////////////////////////////////////////
        //  Control flow does not reach this position! 
        //  Instructions are listed below.
        ////////////////////////////////////////////////////////////////////////

    PUSH:
        *stack++ = *loc;
        goto *(pc++->opcode);
    POP:
        *loc = *(--stack);
        goto *(pc++->opcode);
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
                pc += n;
            }
            goto *(pc++->opcode);
        }
    CLOSE:
        if ( DEBUG ) std::cout << "CLOSE" << std::endl;
        {
            int n = pc++->operand;
            if ( *loc != 0 ) {
                pc += n;
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
    CALL:
        Instruction * nextpc = static_cast< Instruction * >( (pc++)->reference );
        call_stack.push_back( { .return_address = pc } );
        pc = nextpc;
        goto *(pc++->opcode);
    RETURN:
        pc = call_stack.back().return_address;
        call_stack.pop_back();
        goto *(pc++->opcode);
    SAVE:
        call_stack.push_back( { .saved_location = { .saved = *loc, .location = loc } } );
        *loc = 0;
        goto *(pc++->opcode);
    RESTORE:
        call_stack.back().saved_location.restore();
        call_stack.pop_back();
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
