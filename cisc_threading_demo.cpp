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
#include <optional>
#include <stdexcept>
#include <deque>
#include <cstdlib>

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
    int operand1;
    int operand2;
} Dyad;

//  The instruction stream is mainly OpCodes but there are some
//  integer arguments interspersed. Strictly speaking this makes this
//  interpreter a hybrid between direct/indirect threading.
typedef union {
    OpCode opcode;
    int operand;
    Dyad dyad;
} Instruction;

class PeekableProgramInput {
    std::ifstream input;                //  The source code to be read in.
    std::deque< char > buffer;
public:
    PeekableProgramInput( std::string_view filename ) :
        input( filename.data(), std::ios::in )
    {}
private:
    std::optional<char> nextChar() {
        for (;;) {
            char ch = input.get();
            if ( input.good() ) {
                switch ( ch ) {
                    case '>':
                    case '<':
                    case '+':
                    case '-':
                    case '.':
                    case ',':
                    case '[':
                    case ']':
                        return std::optional<char>( ch );
                }
            } else {
                return std::nullopt;
            }
        }
    }
public:
    std::optional<char> peekN( size_t n ) {
        while ( this->buffer.size() <= n ) {
            auto ch = nextChar();
            return_unless( ch )( std::nullopt );
            this->buffer.push_front( *ch );
        }
        return this->buffer[ n ];
    }    
public:
    std::optional<char> peek() {
        if ( this->buffer.empty() ) {
            auto ch = nextChar();
            if ( ch ) {
                this->buffer.push_back( *ch );
            }
            return ch;
        } else {
            return this->buffer.back();
        }
    }
public:
    std::optional<char> pop() {
        if ( this->buffer.empty() ) {
            return nextChar();
        } else {
            char ch = this->buffer.back();
            this->buffer.pop_back();
            return ch;
        }
    }
private:
    void drop() {
        if ( this->buffer.empty() ) {
            input.get();
        } else {
            this->buffer.pop_back();
        }
    }
public:
    bool tryPop( char ch ) {
        if ( this->peek() == ch ) {
            this->drop();
            return true;
        } else {
            return false;
        }
    }
public:
    bool tryPopString( std::string str ) {
        int n = 0;
        for ( auto ch : str ) {
            return_if( this->peekN( n++ ) != ch )( false );
        }
        return true;
    }
};

struct MoveAddMove {
    int lhs;
    int by;
    int rhs;
public:
    MoveAddMove( int lhs, int by, int rhs ) {
        this->lhs = lhs;
        this->by = by;
        this->rhs = rhs;
    }
public:
    bool matches( int L, int N, int R ) {
        return L == this->lhs && N == this->by && R == this->rhs;
    }
};

typedef struct InstructionSet {
    OpCode SET_ZERO;
    OpCode INCR;
    OpCode DECR;
    OpCode ADD;
    OpCode ADD_OFFSET;
    OpCode LEFT;
    OpCode RIGHT;
    OpCode MOVE;
    OpCode OPEN;
    OpCode CLOSE;
    OpCode GET;
    OpCode PUT;
    OpCode HALT;
} InstructionSet;

//  This class is responsible for translating the stream of source code
//  into a vector<Instruction>. It is passed a mapping from characters
//  to the addresses-of-labels, so it can plant (aka append) the exact
//  pointer to the implementing code. 
class CodePlanter {
    PeekableProgramInput input;         //  The source code to be read in, stripped of comment characters.
    const std::map<char, OpCode> & opcode_map;
    const std::map<std::string, OpCode> extra_opcodes_map;
    const InstructionSet & instruction_set;
    std::vector<Instruction> & program; 
    std::vector<int> indexes;           //  Responsible for managing [ ... ] loops.

public:
    CodePlanter( 
        std::string_view filename, 
        const std::map<char, OpCode> & opcode_map, 
        std::map<std::string, OpCode> extra_opcodes_map,
        const InstructionSet & instruction_set,
        std::vector<Instruction> & program 
    ) :
        input( filename.data() ),
        opcode_map( opcode_map ),
        extra_opcodes_map( extra_opcodes_map ),
        instruction_set( instruction_set ), 
        program( program )
    {}

private:
    void plantOpen() {
        program.push_back( { opcode_map.at( '[' ) } );
        if ( DUMP ) std::cerr << "OPEN" << std::endl;
        //  If we are dealing with loops, we plant the absolute index of the
        //  operation in the program we want to jump to. This can be improved
        //  fairly easily.
        indexes.push_back( program.size() );
        program.push_back( {nullptr} );         //  Dummy value, will be overwritten.
    }

    void plantClose() {
        if ( DUMP ) std::cerr << "CLOSE" << std::endl;
        program.push_back( { opcode_map.at( ']' ) } );
        //  If we are dealing with loops, we plant the absolute index of the
        //  operation in the program we want to jump to. This can be improved
        //  fairly easily.
        int end = program.size();
        int start = indexes.back();
        indexes.pop_back();
        program[ start ].operand = end + 1;     //  Overwrite the dummy value.
        program.push_back( { .operand={ start + 1 } } );
    }

    void plantPut() {
        if ( DUMP ) std::cerr << "PUT" << std::endl;
        program.push_back( { opcode_map.at( '.' ) } );
    }

    void plantGet() {
        if ( DUMP ) std::cerr << "GET" << std::endl;
        program.push_back( { opcode_map.at( ',' ) } );
    }

    void plantMoveBy( int n ) {
        if ( n == 1 ) {
            if ( DUMP ) std::cerr << "RIGHT" << std::endl;
            program.push_back( { opcode_map.at( '>' ) } );
        } else if ( n == -1 ) {
            if ( DUMP ) std::cerr << "LEFT" << std::endl;
            program.push_back( { opcode_map.at( '<' ) } );
        } else if ( n != 0 ) {
            if ( DUMP ) std::cerr << "MOVE " << n << std::endl;
            program.push_back( { extra_opcodes_map.at( "MOVE_BY" ) } );
            program.push_back( { .operand=n } );
        }
    }

    void plantAdd( int n ) {
        if ( n == 1 ) {
            if ( DUMP ) std::cerr << "INCR" << std::endl;
            program.push_back( { opcode_map.at( '+' ) } );
        } else if ( n == -1 ) {
            if ( DUMP ) std::cerr << "DECR" << std::endl;
            program.push_back( { opcode_map.at( '-' ) } );
        } else if ( n != 0 ) {
            if ( DUMP ) std::cerr << "ADD " << n << std::endl;
            program.push_back( { extra_opcodes_map.at( "INCR_BY" ) } );
            program.push_back( { .operand=n } );
        }
    }

    int scanAdd( int n ) {
        for (;;) {
            if ( input.tryPop( '+' ) ) {
                n += 1;
            } else if ( input.tryPop( '-' ) ) {
                n -= 1;
            } else {
                break;
            }
        }
        return n;
    }

    int scanMoveBy( int n ) {
        for (;;) {
            if ( input.tryPop( '>' ) ) {
                n += 1;
            } else if ( input.tryPop( '<' ) ) {
                n -= 1;
            } else {
                break;
            }
        }
        return n;
    }

    void plantAddOffset( int offset, int by ) {
        if ( DUMP ) std::cerr << "ADD offset=" << offset << " by=" << by << std::endl;
        program.push_back( { extra_opcodes_map.at( "ADD_OFFSET" ) } );
        struct Dyad d = { .operand1=offset, .operand2=by };
        program.push_back( { .dyad=d } );
    }

    void plantMoveAddMove( const MoveAddMove & mim ) {
        if ( mim.by == 0 ) {
            if ( mim.rhs == 0 ) {
                plantMoveBy( mim.lhs );
            } else if ( mim.lhs == 0 ) {
                const MoveAddMove nextmam = scanMoveIncrMove( mim.rhs );
                plantMoveAddMove( nextmam );
            } else {
                const MoveAddMove nextmam = scanMoveIncrMove( mim.lhs + mim.rhs );
                plantMoveAddMove( nextmam );
            }
        } else if (
            ( mim.lhs != 0 && mim.rhs != 0 ) &&     
            ( sgn( mim.lhs ) != sgn( mim.rhs ) ) //  And they have opposite signs
        ) {
            int abs_lhs = abs( mim.lhs ); 
            int abs_rhs = abs( mim.rhs );
            if ( abs_lhs == abs_rhs ) {
                plantAddOffset( mim.lhs, mim.by );
            } else if ( abs_lhs > abs_rhs ) {
                plantMoveBy( sgn( mim.lhs ) * ( abs_lhs - abs_rhs ) );
                plantAddOffset( sgn( mim.lhs ) * abs_rhs, mim.by );
            } else /* if ( abs_lhs < abs_rhs ) */ {
                plantAddOffset( mim.lhs, mim.by );
                const MoveAddMove nextmam = scanMoveIncrMove( sgn( mim.rhs ) * ( abs_rhs - abs_lhs ) );
                plantMoveAddMove( nextmam );
           }
        } else {
            plantMoveBy( mim.lhs );
            plantAdd( mim.by );
            const MoveAddMove nextmam = scanMoveIncrMove( mim.rhs );
            plantMoveAddMove( nextmam );
        }
    }

    void plantSetZero() {
        if ( DUMP ) std::cerr << "SET_ZERO" << std::endl;
        program.push_back( { extra_opcodes_map.at( "SET_ZERO" ) } );
    }

    MoveAddMove scanMoveIncrMove( int initial ) {
        int move_lhs = scanMoveBy( initial );
        int n = scanAdd( 0 );
        int move_rhs = scanMoveBy( 0 );   
        return MoveAddMove( move_lhs, n, move_rhs );
    }

    bool plantExpr() {
        auto ch = input.pop();
        return_unless( ch )( false );

        switch ( *ch ) {
            case '+':
                plantAdd( scanAdd( 1 ) );
                break;
            case '-':
                plantAdd( scanAdd( -1 ) );
                break;
            case '>':
            case '<':
                {
                    MoveAddMove mim = scanMoveIncrMove( ch == '>' ? 1 : -1 );
                    plantMoveAddMove( mim );
                }
                break;
            case '[':
                {
                    MoveAddMove mim = scanMoveIncrMove( 0 );
                    bool bump = mim.matches( 0, 1, 0 ) || mim.matches( 0, -1, 0 );
                    if ( bump && input.tryPop( ']' ) ) {
                        plantSetZero();
                    } else {
                        plantOpen();
                        plantMoveAddMove( mim );
                    }   
                }
                break;
            case ']':
                plantClose();
                break;
            case '.':
                plantPut();
                break;
            case ',':
                plantGet();
                break;
        }
        return true;
    }

public:
    void plantProgram() {
        while ( plantExpr() ) {}
        program.push_back( { this->extra_opcodes_map.at( "HALT" ) } );
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
    void runFile( std::string_view filename, bool header_needed ) {
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
        instruction_set.ADD = &&INCR_BY;
        instruction_set.MOVE = &&MOVE_BY;
        instruction_set.SET_ZERO = &&SET_ZERO;
        instruction_set.ADD_OFFSET = &&ADD_OFFSET;
        instruction_set.HALT = &&HALT;
        
        opcode_map = {
            { '+', &&INCR },
            { '-', &&DECR },
            { '<', &&LEFT },
            { '>', &&RIGHT },
            { '[', &&OPEN },
            { ']', &&CLOSE },
            { '.', &&PUT },
            { ',', &&GET },
        };

        extra_opcodes_map = {
            { "INCR_BY", &&INCR_BY },
            { "MOVE_BY", &&MOVE_BY },
            { "SET_ZERO", &&SET_ZERO },
            { "ADD_OFFSET", &&ADD_OFFSET },
            { "HALT", &&HALT }
        };
        
        CodePlanter planter( filename, opcode_map, extra_opcodes_map, instruction_set, program );
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
    INCR_BY:
        if ( DEBUG ) std::cout << "INCR_BY" << std::endl;
        {
            int n = pc++->operand;
            *loc += n;
        }
        goto *(pc++->opcode);
    ADD_OFFSET:
        if ( DEBUG ) std::cout << "ADD_OFFSET" << std::endl;
        {
            struct Dyad d = pc++->dyad;
            int offset = d.operand1;
            int by = d.operand2;
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
    MOVE_BY:
        if ( DEBUG ) std::cout << "MOVE_BY" << std::endl;
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
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    for (auto arg : args) {
        Engine engine;
        engine.runFile( arg, args.size() > 1 );
    }
    exit( EXIT_SUCCESS );
}
